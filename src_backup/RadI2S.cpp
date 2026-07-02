#include "RadI2S.h"
#include <rom/gpio.h>

#include "RadDSP.h" // Untuk mendapatkan akses global systemSampleRate

namespace RadDSP {

float systemSampleRate = 48000.0f;

I2S::I2S() : _port(I2S_NUM_0), _initialized(false), 
             _bufferLen(0), _bitsPerSample(16), 
             _leftBuffer(nullptr), _rightBuffer(nullptr), _dmaBuffer(nullptr) {
}

I2S::~I2S() {
    if (_leftBuffer) free(_leftBuffer);
    if (_rightBuffer) free(_rightBuffer);
    if (_dmaBuffer) free(_dmaBuffer);
    if (_initialized) {
        end();
    }
}

bool I2S::begin(i2s_port_t port, int sampleRate, int bitsPerSample, bool isMaster, int bckPin, int wsPin, int dataOutPin, int dataInPin, int mclkPin, bool useInternalDAC, int dmaBufCount, int dmaBufLen) {
    if (_initialized) return true; // Already initialized

    _port = port;
    RadDSP::setSampleRate((float)sampleRate);

    i2s_mode_t mode = (i2s_mode_t)(isMaster ? I2S_MODE_MASTER : I2S_MODE_SLAVE);
    if (dataOutPin >= 0 || useInternalDAC) {
        mode = (i2s_mode_t)(mode | I2S_MODE_TX);
    }
    if (dataInPin >= 0) {
        mode = (i2s_mode_t)(mode | I2S_MODE_RX);
    }
    
    if (useInternalDAC) {
        mode = (i2s_mode_t)(mode | I2S_MODE_DAC_BUILT_IN);
    }

    i2s_bits_per_sample_t bps;
    switch(bitsPerSample) {
        case 16: bps = I2S_BITS_PER_SAMPLE_16BIT; break;
        case 24: bps = I2S_BITS_PER_SAMPLE_24BIT; break;
        case 32: bps = I2S_BITS_PER_SAMPLE_32BIT; break;
        default: bps = I2S_BITS_PER_SAMPLE_16BIT; break;
    }

    i2s_config_t i2s_config = {
        .mode = mode,
        .sample_rate = (uint32_t)sampleRate,
        .bits_per_sample = bps,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = dmaBufCount,
        .dma_buf_len = dmaBufLen,
        .use_apll = (mclkPin >= 0),
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        return false;
    }

    if (!useInternalDAC) {
        i2s_pin_config_t pin_config = {
            .bck_io_num = bckPin,
            .ws_io_num = wsPin,
            .data_out_num = dataOutPin,
            .data_in_num = dataInPin
        };
        err = i2s_set_pin(_port, &pin_config);
        if (err != ESP_OK) {
            i2s_driver_uninstall(_port);
            return false;
        }

        // ESP32 MCLK hardware routing via ROM (CLK_OUT)
        if (mclkPin == 0) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL) & 0xFFFFFFF0);
        } else if (mclkPin == 1) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
            WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL) & 0xFFFFFFF0);
        } else if (mclkPin == 3) {
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
            WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL) & 0xFFFFFFF0);
        }
    } else {
        i2s_set_pin(_port, NULL);
    }

    _bitsPerSample = bitsPerSample;
    _bufferLen = dmaBufLen;

    // Allocate Internal Buffers for Block Processing
    if (_leftBuffer) free(_leftBuffer);
    if (_rightBuffer) free(_rightBuffer);
    if (_dmaBuffer) free(_dmaBuffer);

    _leftBuffer = (float*)malloc(_bufferLen * sizeof(float));
    _rightBuffer = (float*)malloc(_bufferLen * sizeof(float));
    
    // DMA buffer needs space for 2 channels (L+R), and bytes per sample
    int bytesPerSample = (_bitsPerSample == 32 || _bitsPerSample == 24) ? 4 : 2;
    _dmaBuffer = (uint8_t*)malloc(_bufferLen * 2 * bytesPerSample);

    if (!_leftBuffer || !_rightBuffer || !_dmaBuffer) {
        // Fallback or error handling
    }

    _initialized = true;
    return true;
}

bool I2S::readBlock() {
    if (!_initialized || !_dmaBuffer) return false;
    
    int bytesPerSample = (_bitsPerSample == 32 || _bitsPerSample == 24) ? 4 : 2;
    size_t bytesToRead = _bufferLen * 2 * bytesPerSample;
    size_t bytesRead = 0;
    
    esp_err_t err = i2s_read(_port, _dmaBuffer, bytesToRead, &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead == 0) return false;

    // Convert raw DMA data to float -1.0 to 1.0
    if (_bitsPerSample == 32 || _bitsPerSample == 24) {
        int32_t* ptr = (int32_t*)_dmaBuffer;
        for (int i = 0; i < _bufferLen; i++) {
            _leftBuffer[i] = (float)ptr[i * 2] * 4.656612873077393e-10f;     
            _rightBuffer[i] = (float)ptr[i * 2 + 1] * 4.656612873077393e-10f;
        }
    } else {
        int16_t* ptr = (int16_t*)_dmaBuffer;
        for (int i = 0; i < _bufferLen; i++) {
            _leftBuffer[i] = (float)ptr[i * 2] * 3.0517578125e-5f;          
            _rightBuffer[i] = (float)ptr[i * 2 + 1] * 3.0517578125e-5f;
        }
    }
    return true;
}

void I2S::writeBlock() {
    if (!_initialized || !_dmaBuffer) return;
    
    // Convert float -1.0 to 1.0 back to raw DMA data
    if (_bitsPerSample == 32 || _bitsPerSample == 24) {
        int32_t* ptr = (int32_t*)_dmaBuffer;
        for (int i = 0; i < _bufferLen; i++) {
            ptr[i * 2] = (int32_t)(_leftBuffer[i] * 2147483647.0f);
            ptr[i * 2 + 1] = (int32_t)(_rightBuffer[i] * 2147483647.0f);
        }
    } else {
        int16_t* ptr = (int16_t*)_dmaBuffer;
        for (int i = 0; i < _bufferLen; i++) {
            ptr[i * 2] = (int16_t)(_leftBuffer[i] * 32767.0f);
            ptr[i * 2 + 1] = (int16_t)(_rightBuffer[i] * 32767.0f);
        }
    }

    int bytesPerSample = (_bitsPerSample == 32 || _bitsPerSample == 24) ? 4 : 2;
    size_t bytesToWrite = _bufferLen * 2 * bytesPerSample;
    size_t bytesWritten = 0;
    
    i2s_write(_port, _dmaBuffer, bytesToWrite, &bytesWritten, portMAX_DELAY);
}

size_t I2S::read(void* dest, size_t size) {
    if (!_initialized) return 0;
    size_t bytes_read = 0;
    i2s_read(_port, dest, size, &bytes_read, portMAX_DELAY);
    return bytes_read;
}

size_t I2S::write(const void* src, size_t size) {
    if (!_initialized) return 0;
    size_t bytes_written = 0;
    i2s_write(_port, src, size, &bytes_written, portMAX_DELAY);
    return bytes_written;
}

void I2S::end() {
    if (_initialized) {
        i2s_driver_uninstall(_port);
        _initialized = false;
    }
}

} // namespace RadDSP
