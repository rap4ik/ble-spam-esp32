#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0

#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (48U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 30

#define LV_TICK_CUSTOM     1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LOG      0

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_LIST 1
#define LV_USE_SWITCH 1
#define LV_USE_SLIDER 1
#define LV_USE_TABVIEW 1
#define LV_USE_BAR 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_ROLLER 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1
#define LV_USE_MSGBOX 1
#define LV_USE_SPINNER 1
#define LV_USE_LED 1
#define LV_USE_LINE 1
#define LV_USE_ARC 1
#define LV_USE_ANIMIMG 0

#endif
