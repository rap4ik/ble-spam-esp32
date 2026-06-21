#include "ui_manager.h"
#include <Preferences.h>

static Preferences prefs;

// Map each menu button to the SpamType it triggers, stored as user_data
struct AttackBtnDef {
    const char* label;
    SpamType type;
    lv_color_t color;
};

static const AttackBtnDef kAttacks[] = {
    {"All Apple",          SpamType::ALL_APPLE,            lv_color_hex(0x0A84FF)},
    {"AirPods",            SpamType::APPLE_AIRPODS,        lv_color_hex(0x0A84FF)},
    {"AirPods Pro",        SpamType::APPLE_AIRPODS_PRO,    lv_color_hex(0x0A84FF)},
    {"AirPods Max",        SpamType::APPLE_AIRPODS_MAX,    lv_color_hex(0x0A84FF)},
    {"AirTag",             SpamType::APPLE_AIRTAG,         lv_color_hex(0x0A84FF)},
    {"Apple TV Setup",     SpamType::APPLE_TV_SETUP,       lv_color_hex(0x0A84FF)},
    {"Apple Keyboard",     SpamType::APPLE_KEYBOARD,       lv_color_hex(0x0A84FF)},
    {"Action Modal",       SpamType::APPLE_ACTION_MODAL,   lv_color_hex(0x0A84FF)},
    {"iOS17 Crash",        SpamType::APPLE_CRASH_IOS17,    lv_color_hex(0xFF3B30)},
    {"All Android",        SpamType::ALL_ANDROID,          lv_color_hex(0x34C759)},
    {"Android FastPair",   SpamType::ANDROID_FASTPAIR,     lv_color_hex(0x34C759)},
    {"Samsung Buds",       SpamType::SAMSUNG_BUDS,         lv_color_hex(0x34C759)},
    {"Samsung Watch",      SpamType::SAMSUNG_WATCH,        lv_color_hex(0x34C759)},
    {"Xiaomi QuickConn.",  SpamType::XIAOMI_QUICKCONNECT,  lv_color_hex(0x34C759)},
    {"Windows SwiftPair",  SpamType::WINDOWS_SWIFTPAIR,    lv_color_hex(0x5856D6)},
    {"ALL DEVICES",        SpamType::ALL_DEVICES,          lv_color_hex(0xFF9F0A)},
};
static const size_t kAttackCount = sizeof(kAttacks) / sizeof(kAttacks[0]);

UiManager& UiManager::instance() {
    static UiManager inst;
    return inst;
}

void UiManager::begin() {
    prefs.begin("blespam", false);

    buildMainMenu();
    buildLogScreen();
    buildSettingsScreen();

    lv_scr_load(mainScreen);
}

// ── Main menu: scrollable list of attack buttons + status bar ──
void UiManager::buildMainMenu() {
    mainScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(mainScreen, lv_color_hex(0x111111), 0);

    // Top status bar
    lv_obj_t* topBar = lv_obj_create(mainScreen);
    lv_obj_set_size(topBar, LV_PCT(100), 40);
    lv_obj_align(topBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_width(topBar, 0, 0);
    lv_obj_set_style_radius(topBar, 0, 0);

    statusLabel = lv_label_create(topBar);
    lv_label_set_text(statusLabel, "BLE Spam - Idle");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x00FF88), 0);
    lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* settingsBtn = lv_btn_create(topBar);
    lv_obj_set_size(settingsBtn, 34, 30);
    lv_obj_align(settingsBtn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(settingsBtn, onSettingsBtnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* gearLabel = lv_label_create(settingsBtn);
    lv_label_set_text(gearLabel, LV_SYMBOL_SETTINGS);
    lv_obj_center(gearLabel);

    // Scrollable button list
    lv_obj_t* list = lv_obj_create(mainScreen);
    lv_obj_set_size(list, LV_PCT(100), 320 - 40 - 36);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (size_t i = 0; i < kAttackCount; i++) {
        lv_obj_t* btn = lv_btn_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 38);
        lv_obj_set_style_bg_color(btn, kAttacks[i].color, 0);
        lv_obj_set_user_data(btn, (void*)&kAttacks[i]);
        lv_obj_add_event_cb(btn, onAttackBtnClicked, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, kAttacks[i].label);
        lv_obj_center(lbl);
    }

    // Bottom bar: TX counter + stop button
    lv_obj_t* bottomBar = lv_obj_create(mainScreen);
    lv_obj_set_size(bottomBar, LV_PCT(100), 36);
    lv_obj_align(bottomBar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottomBar, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_width(bottomBar, 0, 0);
    lv_obj_set_style_radius(bottomBar, 0, 0);

    txCounterLabel = lv_label_create(bottomBar);
    lv_label_set_text(txCounterLabel, "TX: 0");
    lv_obj_set_style_text_color(txCounterLabel, lv_color_hex(0xFFD700), 0);
    lv_obj_align(txCounterLabel, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* logBtn = lv_btn_create(bottomBar);
    lv_obj_set_size(logBtn, 50, 28);
    lv_obj_align(logBtn, LV_ALIGN_RIGHT_MID, -54, 0);
    lv_obj_add_event_cb(logBtn, [](lv_event_t* e) {
        UiManager* self = (UiManager*)lv_event_get_user_data(e);
        lv_scr_load_anim(self->logScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* logLbl = lv_label_create(logBtn);
    lv_label_set_text(logLbl, "Log");
    lv_obj_center(logLbl);

    lv_obj_t* stopBtn = lv_btn_create(bottomBar);
    lv_obj_set_size(stopBtn, 50, 28);
    lv_obj_align(stopBtn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(stopBtn, lv_color_hex(0xFF3B30), 0);
    lv_obj_add_event_cb(stopBtn, onStopBtnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* stopLbl = lv_label_create(stopBtn);
    lv_label_set_text(stopLbl, "Stop");
    lv_obj_center(stopLbl);
}

// ── Log screen: scrolling text area of TX events ──
void UiManager::buildLogScreen() {
    logScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(logScreen, lv_color_hex(0x000000), 0);

    lv_obj_t* topBar = lv_obj_create(logScreen);
    lv_obj_set_size(topBar, LV_PCT(100), 36);
    lv_obj_align(topBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_width(topBar, 0, 0);
    lv_obj_set_style_radius(topBar, 0, 0);

    lv_obj_t* title = lv_label_create(topBar);
    lv_label_set_text(title, "BLE Packet Log");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF88), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* backBtn = lv_btn_create(topBar);
    lv_obj_set_size(backBtn, 50, 28);
    lv_obj_align(backBtn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(backBtn, onBackBtnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "Back");
    lv_obj_center(backLbl);

    logTextArea = lv_textarea_create(logScreen);
    lv_obj_set_size(logTextArea, LV_PCT(100), 320 - 36);
    lv_obj_align(logTextArea, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_textarea_set_text(logTextArea, "Waiting for packets...\n");
    lv_obj_set_style_text_font(logTextArea, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(logTextArea, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(logTextArea, lv_color_hex(0xAAFFAA), 0);
    lv_textarea_set_cursor_click_pos(logTextArea, false);
}

// ── Settings screen: interval slider + MAC rotation toggle ──
void UiManager::buildSettingsScreen() {
    settingsScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(settingsScreen, lv_color_hex(0x111111), 0);

    lv_obj_t* topBar = lv_obj_create(settingsScreen);
    lv_obj_set_size(topBar, LV_PCT(100), 36);
    lv_obj_align(topBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_width(topBar, 0, 0);
    lv_obj_set_style_radius(topBar, 0, 0);

    lv_obj_t* title = lv_label_create(topBar);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* backBtn = lv_btn_create(topBar);
    lv_obj_set_size(backBtn, 50, 28);
    lv_obj_align(backBtn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(backBtn, onBackBtnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "Back");
    lv_obj_center(backLbl);

    // Interval slider (5ms - 500ms)
    lv_obj_t* intervalTitle = lv_label_create(settingsScreen);
    lv_label_set_text(intervalTitle, "TX Interval");
    lv_obj_align(intervalTitle, LV_ALIGN_TOP_LEFT, 10, 50);

    intervalLabel = lv_label_create(settingsScreen);
    uint32_t curInterval = BleSpamEngine::instance().getIntervalMs();
    lv_label_set_text_fmt(intervalLabel, "%lu ms", curInterval);
    lv_obj_align(intervalLabel, LV_ALIGN_TOP_RIGHT, -10, 50);

    lv_obj_t* slider = lv_slider_create(settingsScreen);
    lv_obj_set_width(slider, LV_PCT(90));
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 80);
    lv_slider_set_range(slider, 5, 500);
    lv_slider_set_value(slider, curInterval, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, onIntervalSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    // MAC rotation toggle
    lv_obj_t* macRow = lv_obj_create(settingsScreen);
    lv_obj_set_size(macRow, LV_PCT(90), 40);
    lv_obj_align(macRow, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_bg_color(macRow, lv_color_hex(0x1c1c1e), 0);

    lv_obj_t* macLabel = lv_label_create(macRow);
    lv_label_set_text(macLabel, "Randomize MAC each TX");
    lv_obj_align(macLabel, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* macSwitch = lv_switch_create(macRow);
    lv_obj_align(macSwitch, LV_ALIGN_RIGHT_MID, -8, 0);
    if (BleSpamEngine::instance().getMacRotation()) lv_obj_add_state(macSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(macSwitch, onMacRotationToggled, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* note = lv_label_create(settingsScreen);
    lv_label_set_text(note,
        "MAC rotation defeats iOS/Android\n"
        "device caching so popups keep\n"
        "appearing instead of stopping\n"
        "after the first hit.");
    lv_obj_set_style_text_color(note, lv_color_hex(0x888888), 0);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 190);
}

// ── Event handlers ──────────────────────────────────────────────
void UiManager::onAttackBtnClicked(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    UiManager* self = (UiManager*)lv_event_get_user_data(e);
    AttackBtnDef* def = (AttackBtnDef*)lv_obj_get_user_data(btn);
    if (!def) return;

    BleSpamEngine::instance().start(def->type);
    lv_label_set_text_fmt(self->statusLabel, "Running: %s", def->label);

    String line = "*** Started: " + String(def->label) + " ***\n";
    self->appendLogLine(line);
}

void UiManager::onStopBtnClicked(lv_event_t* e) {
    UiManager* self = (UiManager*)lv_event_get_user_data(e);
    BleSpamEngine::instance().stop();
    lv_label_set_text(self->statusLabel, "BLE Spam - Idle");
    self->appendLogLine("*** Stopped ***\n");
}

void UiManager::onBackBtnClicked(lv_event_t* e) {
    UiManager* self = (UiManager*)lv_event_get_user_data(e);
    lv_scr_load_anim(self->mainScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

void UiManager::onSettingsBtnClicked(lv_event_t* e) {
    UiManager* self = (UiManager*)lv_event_get_user_data(e);
    lv_scr_load_anim(self->settingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void UiManager::onIntervalSliderChanged(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    UiManager* self = (UiManager*)lv_event_get_user_data(e);
    int32_t val = lv_slider_get_value(slider);
    BleSpamEngine::instance().setIntervalMs(val);
    lv_label_set_text_fmt(self->intervalLabel, "%ld ms", (long)val);
    prefs.putUInt("interval", val);
}

void UiManager::onMacRotationToggled(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    BleSpamEngine::instance().setMacRotation(on);
    prefs.putBool("macRotation", on);
}

// ── Log helper ───────────────────────────────────────────────────
void UiManager::appendLogLine(const String& line) {
    if (!logTextArea) return;
    lv_textarea_add_text(logTextArea, line.c_str());

    // Keep log from growing forever: trim if too long
    const char* txt = lv_textarea_get_text(logTextArea);
    if (strlen(txt) > 4000) {
        lv_textarea_set_text(logTextArea, "");
    }
}

// ── Periodic UI refresh (TX counter etc.) ────────────────────────
void UiManager::loop() {
    uint32_t now = millis();
    if (now - lastUiRefresh < 150) return;
    lastUiRefresh = now;

    const SpamStats& stats = BleSpamEngine::instance().getStats();
    if (txCounterLabel) {
        lv_label_set_text_fmt(txCounterLabel, "TX: %lu", (unsigned long)stats.txCount);
    }
}
