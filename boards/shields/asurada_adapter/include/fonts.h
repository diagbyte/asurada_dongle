#pragma once

#include <lvgl.h>

/* Fonts declared for the asurada layout (the only layout in this repo). The
 * font .c data under src/fonts/ is compiled by glob; unused faces are dropped
 * by the linker's --gc-sections. */
LV_FONT_DECLARE(PPF_NarrowThin_64);
LV_FONT_DECLARE(FR_Regular_48);
LV_FONT_DECLARE(FG_Medium_20);
LV_FONT_DECLARE(FG_Medium_21);
LV_FONT_DECLARE(FG_Medium_26);
LV_FONT_DECLARE(DINish_Medium_24);
