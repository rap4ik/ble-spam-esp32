#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>

enum class SpamType {
    NONE,
    APPLE_AIRPODS,
    APPLE_AIRPODS_PRO,
    APPLE_AIRPODS_MAX,
    APPLE_AIRTAG,
    APPLE_TV_SETUP,
    APPLE_KEYBOARD,
    APPLE_ACTION_MODAL,
    APPLE_CRASH_IOS17,
    ANDROID_FASTPAIR,
    SAMSUNG_BUDS,
    SAMSUNG_WATCH,
    WINDOWS_SWIFTPAIR,
    XIAOMI_QUICKCONNECT,
    ALL_APPLE,
    ALL_ANDROID,
    ALL_DEVICES
};

struct SpamStats {
    uint32_t txCount = 0;
    uint32_t lastTxTime = 0;
    uint8_t lastMac[6] = {0};
    SpamType activeType = SpamType::NONE;
    bool running = false;
};

class BleSpamEngine {
public:
    static BleSpamEngine& instance();

    void begin();
    void start(SpamType type);
    void stop();
    void loop();  // call every frame; handles pacing/rotation

    void setIntervalMs(uint32_t ms) { intervalMs = ms; }
    uint32_t getIntervalMs() const { return intervalMs; }

    void setMacRotation(bool enabled) { rotateMac = enabled; }
    bool getMacRotation() const { return rotateMac; }

    const SpamStats& getStats() const { return stats; }

    // Callback invoked on every TX so the UI can log it
    using TxCallback = std::function<void(SpamType type, const uint8_t* mac, const uint8_t* payload, size_t len)>;
    void setTxCallback(TxCallback cb) { txCallback = cb; }

    // Internal: called by the GAP event handler (not for app use)
    void onAddrSet();
    void onAdvDataSet();
    void onAdvStarted();
    void onAdvStopped();

private:
    BleSpamEngine() = default;

    void randomizeMac();
    void beginAdvertiseSequence(SpamType type); // step 1: set random address

    // Per-protocol payload builders
    std::vector<uint8_t> buildAppleContinuity(SpamType type);
    std::vector<uint8_t> buildFastPair();
    std::vector<uint8_t> buildSamsungEasySetup(bool watch);
    std::vector<uint8_t> buildSwiftPair();
    std::vector<uint8_t> buildXiaomi();

    SpamType currentType = SpamType::NONE;
    std::vector<SpamType> cycleQueue;   // for ALL_* group modes
    size_t cycleIndex = 0;

    uint32_t intervalMs = 20;
    uint32_t lastSendMs = 0;
    bool rotateMac = true;

    // State machine for one advertise cycle: IDLE -> SETTING_ADDR -> SETTING_DATA -> ADVERTISING -> STOPPING
    enum class Step { IDLE, SETTING_ADDR, SETTING_DATA, ADVERTISING, STOPPING };
    Step step = Step::IDLE;
    std::vector<uint8_t> pendingPayload;
    SpamType pendingType = SpamType::NONE;
    uint32_t advStartedAtMs = 0;

    SpamStats stats;
    TxCallback txCallback = nullptr;
};
