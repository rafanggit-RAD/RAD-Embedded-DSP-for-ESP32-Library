// BLUETOOTH DIAGNOSTIC TEST
// Upload ini SENDIRIAN untuk menguji apakah Bluetooth ESP32 Anda bekerja.
// Buka Serial Monitor di 115200 baud setelah upload.

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include <nvs_flash.h>

// PENTING: Arduino ESP32 membebaskan memori BT secara default jika tidak ada library BT standard yang di-link.
// Kita override fungsi btInUse() agar bernilai true secara eksplisit untuk mencegah pelepasan memori BT!
extern "C" bool btInUse(void) {
    return true;
}

// Callback kosong
void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            Serial.println(">>> CONNECTED!");
        }
    }
}
void a2d_data_cb(const uint8_t *data, uint32_t len) {}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=================================");
    Serial.println(" BLUETOOTH DIAGNOSTIC TEST");
    Serial.println("=================================");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Chip model: %s\n", ESP.getChipModel());
    Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
    Serial.println();

    // Print Preprocessor Macros
#ifdef CONFIG_BT_ENABLED
    Serial.println("[CFG] CONFIG_BT_ENABLED: YES");
#else
    Serial.println("[CFG] CONFIG_BT_ENABLED: NO");
#endif

#ifdef CONFIG_BT_CLASSIC_ENABLED
    Serial.println("[CFG] CONFIG_BT_CLASSIC_ENABLED: YES");
#else
    Serial.println("[CFG] CONFIG_BT_CLASSIC_ENABLED: NO");
#endif

#ifdef CONFIG_BLUEDROID_ENABLED
    Serial.println("[CFG] CONFIG_BLUEDROID_ENABLED: YES");
#else
    Serial.println("[CFG] CONFIG_BLUEDROID_ENABLED: NO");
#endif

#ifdef CONFIG_NIMBLE_ENABLED
    Serial.println("[CFG] CONFIG_NIMBLE_ENABLED: YES");
#else
    Serial.println("[CFG] CONFIG_NIMBLE_ENABLED: NO");
#endif

#ifdef CONFIG_BTDM_CTRL_MODE_BLE_ONLY
    Serial.println("[CFG] Controller Mode: BLE ONLY");
#endif
#ifdef CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY
    Serial.println("[CFG] Controller Mode: CLASSIC ONLY");
#endif
#ifdef CONFIG_BTDM_CTRL_MODE_BTDM
    Serial.println("[CFG] Controller Mode: DUAL MODE (BTDM)");
#endif

    // Step 1: NVS
    Serial.print("[1] NVS Flash Init... ");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    Serial.printf("%s (code %d)\n", ret == ESP_OK ? "OK" : "FAIL", ret);
    if (ret != ESP_OK) { Serial.println(">>> STOPPED AT NVS"); while(1) delay(1000); }

    // Step 2: btStartMode (Classic BT)
    Serial.print("[2] btStartMode(BT_MODE_CLASSIC_BT)... ");
    bool bt_ok = btStartMode(BT_MODE_CLASSIC_BT);
    Serial.println(bt_ok ? "OK" : "FAIL");
    if (!bt_ok) { 
        Serial.print("[2a] Try btStartMode(BT_MODE_BTDM) as fallback... ");
        bt_ok = btStartMode(BT_MODE_BTDM);
        Serial.println(bt_ok ? "OK" : "FAIL");
        if (!bt_ok) { Serial.println(">>> STOPPED AT btStartMode"); while(1) delay(1000); }
    }

    // Step 3: Bluedroid Init
    Serial.print("[3] Bluedroid Init... ");
    ret = esp_bluedroid_init();
    Serial.printf("%s (code %d)\n", ret == ESP_OK ? "OK" : "FAIL", ret);
    if (ret != ESP_OK) { Serial.println(">>> STOPPED AT BLUEDROID INIT"); while(1) delay(1000); }

    // Step 4: Bluedroid Enable
    Serial.print("[4] Bluedroid Enable... ");
    ret = esp_bluedroid_enable();
    Serial.printf("%s (code %d)\n", ret == ESP_OK ? "OK" : "FAIL", ret);
    if (ret != ESP_OK) { Serial.println(">>> STOPPED AT BLUEDROID ENABLE"); while(1) delay(1000); }

    // Step 5: Device Name
    Serial.print("[5] Set name 'RAD-DSP-LIB'... ");
    esp_bt_dev_set_device_name("RAD-DSP-LIB");
    Serial.println("OK");

    // Step 6: SSP
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    // Step 7: Discoverable
    Serial.print("[6] Set Discoverable... ");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    Serial.println("OK");

    // Step 8: A2DP Sink
    Serial.print("[7] A2DP Sink Init... ");
    ret = esp_a2d_sink_init();
    Serial.printf("%s (code %d)\n", ret == ESP_OK ? "OK" : "FAIL", ret);
    if (ret != ESP_OK) { Serial.println(">>> STOPPED AT A2DP SINK"); while(1) delay(1000); }
    
    esp_a2d_register_callback(a2d_cb);
    esp_a2d_sink_register_data_callback(a2d_data_cb);

    Serial.println();
    Serial.println("=================================");
    Serial.println(" ALL STEPS PASSED!");
    Serial.printf(" Free heap after BT: %u bytes\n", ESP.getFreeHeap());
    Serial.println(" 'RAD-DSP-LIB' should appear on");
    Serial.println(" your phone's Bluetooth scan now!");
    Serial.println("=================================");
}

void loop() {
    delay(5000);
    Serial.printf("[ALIVE] Free heap: %u bytes\n", ESP.getFreeHeap());
}
