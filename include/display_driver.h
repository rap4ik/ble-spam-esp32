#pragma once
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "board_config.h"

class DisplayDriver {
public:
    static DisplayDriver& instance();

    void begin();
    void loop();  // call lv_timer_handler internally

private:
    DisplayDriver() = default;

    static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p);
    static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);

    TFT_eSPI tft;
    XPT2046_Touchscreen* touch = nullptr;
    SPIClass touchSpi{HSPI};

    lv_disp_draw_buf_t drawBuf;
    lv_color_t* buf1 = nullptr;

    lv_disp_drv_t dispDrv;
    lv_indev_drv_t indevDrv;
};
