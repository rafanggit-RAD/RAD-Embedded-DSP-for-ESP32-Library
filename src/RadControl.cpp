#include "RadControl.h"
#include <Wire.h>
#include "RadBluetooth.h"

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

Controller::Controller() : _serialEnabled(false), _bufLen(0), _sampleRate(48000), _bitDepth(16) {
    _processStartTs[0] = 0;
    _processStartTs[1] = 0;
    _dspLoad[0] = -1.0f;
    _dspLoad[1] = -1.0f;
    for (int i = 0; i < 256; i++) {
        _modules[i] = nullptr;
        _moduleNames[i] = nullptr;
        _linkID[i] = -1;
    }
    _schemaJson = nullptr;
    _globalController = this;
}

void Controller::attach(uint8_t id, Controllable* module, const char* name) {
    if (id < 256) {
        _modules[id] = module;
        _moduleNames[id] = name;
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
        
        // Parameter Linking dua arah
        int16_t linkedID = _linkID[id];
        if (linkedID >= 0 && linkedID < 256 && _modules[linkedID] != nullptr) {
            if (_modules[linkedID]->getParameter(paramID) != value) {
                _modules[linkedID]->setParameter(paramID, value);
                
                // Kirimkan respon serial agar RadStudio GUI dapat menyinkronkan slider secara real-time
                if (_serialEnabled) {
                    Serial.print("{\"ack\":1,\"id\":");
                    Serial.print(linkedID);
                    Serial.print(",\"p\":");
                    Serial.print(paramID);
                    Serial.print(",\"v\":");
                    Serial.print(value);
                    Serial.println("}");
                }
            }
        }
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
    if (_dspLoad[coreID] < 0.0f) {
        _dspLoad[coreID] = load;
    } else {
        _dspLoad[coreID] = (_dspLoad[coreID] * 0.95f) + (load * 0.05f);
    }
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
                        Serial.print("{\"routing\":[],\"modules\":{");
                        bool first = true;
                        for (int i = 0; i < 256; i++) {
                            if (_modules[i] != nullptr) {
                                if (!first) {
                                    Serial.print(",");
                                }
                                first = false;
                                
                                Serial.print("\"");
                                  Serial.print(i);
                                  Serial.print("\":{\"name\":\"");
                                  Serial.print(_moduleNames[i] ? _moduleNames[i] : "Unnamed");
                                  Serial.print("\",\"type\":\"");
                                  const char* mType = _modules[i]->getType();
                                  Serial.print(mType);
                                  Serial.print("\",\"params\":[");
                                  
                                  printParamsForType(mType);
                                  
                                  Serial.print("]}");
                              }
                          }
                          Serial.println("}}");
                      } else if (id == 255) { // System Telemetry
                          Serial.print("{\"sys\":1,\"c0\":");
                          Serial.print(_dspLoad[0]);
                          Serial.print(",\"c1\":");
                          Serial.print(_dspLoad[1]);
                          Serial.print(",\"ramF\":");
                          Serial.print(ESP.getFreeHeap());
                          Serial.print(",\"ramT\":");
                          Serial.print(ESP.getHeapSize());
                          Serial.print(",\"sr\":");
                          Serial.print(_sampleRate);
                          Serial.print(",\"bd\":");
                          Serial.print(_bitDepth);
                          Serial.print(",\"cpu\":");
                          Serial.print(getCpuFrequencyMhz());
                          Serial.print(",\"temp\":");
                          Serial.print(temperatureRead(), 1);
                           
                           // Tambahkan info RingBuffer Bluetooth jika di-inisialisasi
                           size_t btCap = Bluetooth::getRingBufferCapacity();
                           if (btCap > 0) {
                               size_t btFill = Bluetooth::getRingBufferFillBytes();
                               float btPct = (float)btFill / (float)btCap * 100.0f;
                               Serial.print(",\"btBuf\":");
                               Serial.print(btPct, 1);
                           }
                           
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
  
  
  
  void Controller::printParamsForType(const char* mType) {
      // Bersihkan template parameter: "MatrixRouter<3,2>" -> "MatrixRouter"
      char baseType[32];
      strncpy(baseType, mType, sizeof(baseType));
      baseType[sizeof(baseType)-1] = '\0';
      char* angleBracket = strchr(baseType, '<');
      if (angleBracket) {
          *angleBracket = '\0';
      }
      
      if (strcmp(baseType, "Biquad") == 0) {
          Serial.print("\"0: Filter Type\",\"1: Frequency (Hz)\",\"2: Gain (dB)\",\"3: Q-Factor\",\"100: Bypass (0/1)\"");
      } else if (strcmp(baseType, "Dynamics") == 0) {
          Serial.print("\"0: Dynamics Type\",\"1: Threshold (dB)\",\"2: Ratio\",\"3: Attack (ms)\",\"4: Hold (ms)\",\"5: Release (ms)\",\"6: Makeup Gain (dB)\",\"7: SC Filter Type\",\"8: SC Freq (Hz)\",\"100: Bypass (0/1)\"");
      } else if (strcmp(baseType, "Mixer") == 0) {
          int n = 2;
          if (angleBracket) {
              n = atoi(angleBracket + 1);
          }
          for (int i = 0; i < n; i++) {
              if (i > 0) Serial.print(",");
              Serial.print("\"");
              Serial.print(i);
              Serial.print(": Gain In");
              Serial.print(i);
              Serial.print(" (dB)\"");
          }
          for (int i = 0; i < n; i++) {
              Serial.print(",\"");
              Serial.print(100 + i);
              Serial.print(": Mute In");
              Serial.print(i);
              Serial.print(" (0/1)\"");
          }
      } else if (strcmp(baseType, "Splitter") == 0) {
          int mVal = 2;
          if (angleBracket) {
              mVal = atoi(angleBracket + 1);
          }
          for (int i = 0; i < mVal; i++) {
              if (i > 0) Serial.print(",");
              Serial.print("\"");
              Serial.print(i);
              Serial.print(": Gain Out");
              Serial.print(i);
              Serial.print(" (dB)\"");
          }
          for (int i = 0; i < mVal; i++) {
              Serial.print(",\"");
              Serial.print(100 + i);
              Serial.print(": Mute Out");
              Serial.print(i);
              Serial.print(" (0/1)\"");
          }
      } else if (strcmp(baseType, "MonoGain") == 0 || strcmp(baseType, "Gain") == 0) {
          Serial.print("\"0: Gain (dB)\",\"1: Mute (0/1)\",\"2: Phase Invert (0/1)\"");
      } else if (strcmp(baseType, "StereoGain") == 0) {
          Serial.print("\"0: Gain (dB)\",\"1: Mute (0/1)\",\"2: Phase Invert (0/1)\",\"3: Balance (-1/1)\"");
      } else if (strcmp(baseType, "FIR") == 0) {
          Serial.print("\"3: Taps Number (16-512)\",\"4: Gain (dB)\",\"100: Bypass (0/1)\"");
      } else if (strcmp(baseType, "MatrixRouter") == 0) {
          int numIn = 3, numOut = 2;
          if (angleBracket) {
              numIn = atoi(angleBracket + 1);
              char* comma = strchr(angleBracket, ',');
              if (comma) {
                  numOut = atoi(comma + 1);
              }
          }
          bool innerFirst = true;
          for (int i = 0; i < numIn; i++) {
              for (int j = 0; j < numOut; j++) {
                  if (!innerFirst) Serial.print(",");
                  innerFirst = false;
                  int idx = i * numOut + j;
                  Serial.print("\"");
                  Serial.print(idx);
                  Serial.print(": In");
                  Serial.print(i);
                  Serial.print("->Out");
                  Serial.print(j);
                  Serial.print(" (Lin)\"");
              }
          }
      } else if (strcmp(baseType, "Meter") == 0) {
          Serial.print("\"0: Level (dB)\",\"1: Decay Factor\"");
      } else if (strcmp(baseType, "GraphicEQ") == 0) {
          int nVal = 10;
          if (angleBracket) {
              nVal = atoi(angleBracket + 1);
          }
          float fMin = 20.0f;
          float fMax = 20000.0f;
          for (int i = 0; i < nVal; i++) {
              if (i > 0) Serial.print(",");
              Serial.print("\"");
              Serial.print(i);
              Serial.print(": ");
              
              float freq;
              if (nVal == 1) {
                  freq = 1000.0f;
              } else {
                  freq = fMin * powf(fMax / fMin, (float)i / (nVal - 1));
              }
              
              if (freq < 1000.0f) {
                  Serial.print((int)(freq + 0.5f));
                  Serial.print("Hz");
              } else {
                  float khz = freq / 1000.0f;
                  if (khz - (int)khz < 0.05f) {
                      Serial.print((int)khz);
                  } else {
                      Serial.print(khz, 1);
                  }
                  Serial.print("kHz");
              }
              Serial.print(" (dB)\"");
          }
          Serial.print(",\"100: Bypass (0/1)\"");
      } else {
          Serial.print("\"0: Custom Value\"");
      }
  }
  
  void Controller::link(Controllable* m1, Controllable* m2) {
      int id1 = -1;
      int id2 = -1;
      for (int i = 0; i < 256; i++) {
          if (_modules[i] == m1) id1 = i;
          if (_modules[i] == m2) id2 = i;
      }
      if (id1 != -1 && id2 != -1) {
          _linkID[id1] = id2;
          _linkID[id2] = id1;
      }
  }
  
  void Controller::setSystemMetrics(int sampleRate, int bitDepth) {
      _sampleRate = sampleRate;
      _bitDepth = bitDepth;
  }
  
  } // namespace RadDSP
