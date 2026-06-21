#pragma once
#include <lvgl.h>
#include "ble_spam_engine.h"

class UiManager {
public:
    static UiManager& instance();

    void begin();
    void loop();  // call from main loop to refresh stats label etc.

private:
    UiManager() = default;

    void buildMainMenu();
    void buildLogScreen();
    void buildSettingsScreen();

    static void onAttackBtnClicked(lv_event_t* e);
    static void onStopBtnClicked(lv_event_t* e);
    static void onBackBtnClicked(lv_event_t* e);
    static void onSettingsBtnClicked(lv_event_t* e);
    static void onIntervalSliderChanged(lv_event_t* e);
    static void onMacRotationToggled(lv_event_t* e);

    void appendLogLine(const String& line);

    lv_obj_t* mainScreen   = nullptr;
    lv_obj_t* logScreen    = nullptr;
    lv_obj_t* settingsScreen = nullptr;

    lv_obj_t* statusLabel  = nullptr;
    lv_obj_t* txCounterLabel = nullptr;
    lv_obj_t* logTextArea  = nullptr;
    lv_obj_t* intervalLabel = nullptr;

    uint32_t lastUiRefresh = 0;
};
