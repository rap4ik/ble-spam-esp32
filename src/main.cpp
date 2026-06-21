#include <Arduino.h>
#include "display_driver.h"
#include "ui_manager.h"
#include "ble_spam_engine.h"
#include <Preferences.h>

static Preferences bootPrefs;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== BLE Spam ESP32 (CYD) booting ===");

    // Display + touch
    DisplayDriver::instance().begin();

    // BLE spam engine
    BleSpamEngine::instance().begin();

    // Restore saved settings
    bootPrefs.begin("blespam", true);
    uint32_t savedInterval = bootPrefs.getUInt("interval", 20);
    bool savedMacRotation = bootPrefs.getBool("macRotation", true);
    bootPrefs.end();

    BleSpamEngine::instance().setIntervalMs(savedInterval);
    BleSpamEngine::instance().setMacRotation(savedMacRotation);

    // Wire TX events into the on-screen log
    BleSpamEngine::instance().setTxCallback(
        [](SpamType type, const uint8_t* mac, const uint8_t* payload, size_t len) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            Serial.printf("[TX] type=%d mac=%s len=%u\n", (int)type, macStr, (unsigned)len);
        }
    );

    // UI (built after engine so it can read saved settings)
    UiManager::instance().begin();

    Serial.println("=== Boot complete ===");
}

void loop() {
    DisplayDriver::instance().loop();   // lv_timer_handler — keep this frequent
    BleSpamEngine::instance().loop();   // paced BLE TX
    UiManager::instance().loop();       // refresh TX counter label

    delay(1); // yield to RTOS / WiFi-BT coexistence scheduler
}
