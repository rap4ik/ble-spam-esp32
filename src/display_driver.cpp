#include "display_driver.h"

DisplayDriver& DisplayDriver::instance() {
    static DisplayDriver inst;
    return inst;
}

void DisplayDriver::flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    DisplayDriver* self = (DisplayDriver*)drv->user_data;
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    self->tft.startWrite();
    self->tft.setAddrWindow(area->x1, area->y1, w, h);
    self->tft.pushColors((uint16_t*)color_p, w * h, true);
    self->tft.endWrite();

    lv_disp_flush_ready(drv);
}

void DisplayDriver::touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    DisplayDriver* self = (DisplayDriver*)drv->user_data;
    if (!self->touch || !self->touch->touched()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    TS_Point p = self->touch->getPoint();

    int16_t x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    int16_t y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
    x = constrain(x, 0, SCREEN_WIDTH - 1);
    y = constrain(y, 0, SCREEN_HEIGHT - 1);

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR;
}

void DisplayDriver::begin() {
    tft.init();
    tft.setRotation(0); // portrait; flip to 2 if image is upside down
    tft.fillScreen(TFT_BLACK);

    static const uint32_t bufPixels = SCREEN_WIDTH * 40; // partial buffer, RAM-friendly
    buf1 = (lv_color_t*)malloc(bufPixels * sizeof(lv_color_t));

    lv_init();
    lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, bufPixels);

    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = SCREEN_WIDTH;
    dispDrv.ver_res = SCREEN_HEIGHT;
    dispDrv.flush_cb = flushCb;
    dispDrv.draw_buf = &drawBuf;
    dispDrv.user_data = this;
    lv_disp_drv_register(&dispDrv);

    // Touch on a separate SPI bus (HSPI) - typical CYD wiring
    touchSpi.begin(TOUCH_CLK, TOUCH_DOUT, TOUCH_DIN, TOUCH_CS);
    touch = new XPT2046_Touchscreen(TOUCH_CS, TOUCH_IRQ);
    touch->begin(touchSpi);
    touch->setRotation(0);

    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchReadCb;
    indevDrv.user_data = this;
    lv_indev_drv_register(&indevDrv);

    // Backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

void DisplayDriver::loop() {
    lv_timer_handler();
}
