#ifndef RAD_I2S_H
#define RAD_I2S_H

#include <Arduino.h>
#include <driver/i2s.h>

namespace RadDSP {

class I2S {
public:
    I2S();
    ~I2S();

    /**
     * @brief Initialize I2S port
     * @param port I2S_NUM_0 or I2S_NUM_1
     * @param sampleRate The sampling rate (e.g. 44100, 48000)
     * @param bitsPerSample Bit depth (16, 24, 32)
     * @param isMaster True for I2S_MODE_MASTER, false for I2S_MODE_SLAVE
     * @param bckPin Bit Clock (BCLK) pin
     * @param wsPin Word Select (LRCLK) pin
     * @param dataOutPin Data Out (TX) pin (Set to -1 if unused)
     * @param dataInPin Data In (RX) pin (Set to -1 if unused)
     * @param mclkPin Master Clock (MCLK) pin (0, 1, or 3 for ESP32. Set to -1 if unused)
     * @param useInternalDAC Set to true if using internal DAC
     * @param dmaBufCount Number of DMA buffers
     * @param dmaBufLen Length of each DMA buffer (in samples per channel)
     * @return true if successful
     */
    bool begin(i2s_port_t port = I2S_NUM_0, 
               int sampleRate = 48000, 
               int bitsPerSample = 16,
               bool isMaster = true,
               int bckPin = 26, 
               int wsPin = 25, 
               int dataOutPin = 22, 
               int dataInPin = -1,
               int mclkPin = -1,
               bool useInternalDAC = false,
               int dmaBufCount = 8,
               int dmaBufLen = 64);

    /**
     * @brief Read data from I2S
     * @param dest Buffer to store the read data
     * @param size Size in bytes to read
     * @return Number of bytes actually read
     */
    size_t read(void* dest, size_t size);

    /**
     * @brief Write data to I2S
     * @param src Buffer containing data to write
     * @param size Size in bytes to write
     * @return Number of bytes actually written
     */
    size_t write(const void* src, size_t size);

    /**
     * @brief Uninstall I2S driver
     */
    void end();

    /**
     * @brief Read a block of audio data into the internal float buffers
     * @return true if data was read successfully
     */
    bool readBlock();

    /**
     * @brief Write the internal float buffers to I2S DMA
     */
    void writeBlock();

    /**
     * @brief Get pointer to the internal Left channel buffer (float format -1.0 to 1.0)
     */
    float* getLeftBuffer() { return _leftBuffer; }

    /**
     * @brief Get pointer to the internal Right channel buffer (float format -1.0 to 1.0)
     */
    float* getRightBuffer() { return _rightBuffer; }

    /**
     * @brief Get the configured buffer length (samples per channel)
     */
    int getBufferLength() { return _bufferLen; }

private:
    i2s_port_t _port;
    bool _initialized;
    
    // Internal Block Processing Buffers
    int _bufferLen;
    int _bitsPerSample;
    int _sampleRate;
    float* _leftBuffer;
    float* _rightBuffer;
    uint8_t* _dmaBuffer;
};

} // namespace RadDSP

#endif // RAD_I2S_H
