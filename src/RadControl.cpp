#include "RadControl.h"
#include <Wire.h>

namespace RadDSP {

Controller* _globalController = nullptr;

void onI2CReceive(int numBytes) {
    if (!_globalController || numBytes < 6) {
        while(Wire.available()) Wire.read(); // Flush
        return;
    }
    
    uint8_t id = Wire.read();
    uint8_t param = Wire.read();
    
    union {
        uint8_t b[4];
        float f;
    } val;
    val.b[0] = Wire.read();
    val.b[1] = Wire.read();
    val.b[2] = Wire.read();
    val.b[3] = Wire.read();

    _globalController->executeCommand(id, param, val.f);
    
    while(Wire.available()) Wire.read(); // Flush sisanya
}

Controller::Controller() : _serialEnabled(false), _bufLen(0) {
    _processStartTs[0] = 0;
    _processStartTs[1] = 0;
    _dspLoad[0] = 0.0f;
    _dspLoad[1] = 0.0f;
    for (int i = 0; i < 256; i++) {
        _modules[i] = nullptr;
    }
    _schemaJson = nullptr;
    _globalController = this;
}

void Controller::attach(uint8_t id, Controllable* module) {
    if (id < 256) {
        _modules[id] = module;
    }
}

void Controller::setSchema(const char* schemaJson) {
    _schemaJson = schemaJson;
}

void Controller::beginSerial(long baudRate) {
    // Asumsi Serial.begin sudah dipanggil user atau kita panggil jika perlu.
    // Tapi amannya user yang memanggil Serial.begin di setup()
    _serialEnabled = true;
}

void Controller::beginI2C(uint8_t address, int sdaPin, int sclPin) {
    Wire.begin(address, sdaPin, sclPin, 400000);
    Wire.onReceive(onI2CReceive);
}

void Controller::executeCommand(uint8_t id, uint8_t paramID, float value) {
    if (id < 256 && _modules[id] != nullptr) {
        _modules[id]->setParameter(paramID, value);
    }
}

void Controller::markProcessStart(uint8_t coreID) {
    if (coreID < 2) _processStartTs[coreID] = esp_timer_get_time();
}

void Controller::markProcessEnd(uint8_t coreID, int sampleCount, int sampleRate) {
    if (coreID >= 2) return;
    uint32_t diff = esp_timer_get_time() - _processStartTs[coreID];
    float maxTimeUs = ((float)sampleCount / (float)sampleRate) * 1000000.0f;
    float load = ((float)diff / maxTimeUs) * 100.0f;
    
    // Low-pass filter for smooth UI display
    _dspLoad[coreID] = (_dspLoad[coreID] * 0.95f) + (load * 0.05f);
}

void Controller::poll() {
    if (!_serialEnabled) return;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (_bufLen > 0) {
                _serialBuffer[_bufLen] = '\0';
                
                char* idPtr = strstr(_serialBuffer, "\"id\":");
                char* pPtr = strstr(_serialBuffer, "\"p\":");
                char* vPtr = strstr(_serialBuffer, "\"v\":");
                char* reqPtr = strstr(_serialBuffer, "\"req\":");
                
                if (reqPtr != nullptr && idPtr != nullptr) {
                    uint8_t id = atoi(idPtr + 5);
                    uint8_t param = atoi(reqPtr + 6);
                    
                    if (id == 254) { // Get Schema
                        if (_schemaJson != nullptr) {
                            Serial.println(_schemaJson);
                        } else {
                            Serial.println("{\"error\":\"No schema defined\"}");
                        }
                    } else if (id == 255) { // System Telemetry
                        Serial.print("{\"sys\":1,\"c0\":");
                        Serial.print(_dspLoad[0]);
                        Serial.print(",\"c1\":");
                        Serial.print(_dspLoad[1]);
                        Serial.print(",\"ramF\":");
                        Serial.print(ESP.getFreeHeap());
                        Serial.print(",\"ramT\":");
                        Serial.print(ESP.getHeapSize());
                        Serial.println("}");
                    } else if (id < 256 && _modules[id] != nullptr) {
                        float val = _modules[id]->getParameter(param);
                        Serial.print("{\"ack\":1,\"id\":");
                        Serial.print(id);
                        Serial.print(",\"p\":");
                        Serial.print(param);
                        Serial.print(",\"v\":");
                        Serial.print(val);
                        Serial.println("}");
                    }
                } else if (idPtr != nullptr && pPtr != nullptr && vPtr != nullptr) {
                    uint8_t id = atoi(idPtr + 5);
                    uint8_t param = atoi(pPtr + 4);
                    float val = atof(vPtr + 4);
                    
                    executeCommand(id, param, val);
                    
                    Serial.print("{\"ack\":1,\"id\":");
                    Serial.print(id);
                    Serial.print(",\"p\":");
                    Serial.print(param);
                    Serial.print(",\"v\":");
                    Serial.print(val);
                    Serial.println("}");
                }
                
                _bufLen = 0;
            }
        } else {
            if (_bufLen < 127) {
                _serialBuffer[_bufLen++] = c;
            }
        }
    }
}

} // namespace RadDSP
