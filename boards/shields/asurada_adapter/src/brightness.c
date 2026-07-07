#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asurada_bl, LOG_LEVEL_WRN);

#include "asurada_brightness.h"

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

/* The user's preferred (non-dimmed) brightness. */
static uint8_t preferred_brightness = CONFIG_ASURADA_FIXED_BRIGHTNESS;
/* The brightness currently applied to the panel. */
static uint8_t applied_brightness = CONFIG_ASURADA_FIXED_BRIGHTNESS;

static void apply(uint8_t percent) {
    if (percent < 1) {
        percent = 1;
    } else if (percent > 100) {
        percent = 100;
    }
    applied_brightness = percent;
    if (led_set_brightness(pwm_leds_dev, DISP_BL, percent)) {
        LOG_WRN("failed to set brightness");
    }
}

void asurada_brightness_set(uint8_t percent) {
    if (percent < 1) {
        percent = 1;
    } else if (percent > 100) {
        percent = 100;
    }
    preferred_brightness = percent;
    apply(percent);
}

uint8_t asurada_brightness_get(void) {
    return applied_brightness;
}

void asurada_brightness_adjust(int8_t delta) {
    int value = (int)preferred_brightness + delta;
    if (value < 1) {
        value = 1;
    } else if (value > 100) {
        value = 100;
    }
    asurada_brightness_set((uint8_t)value);
}

void asurada_brightness_dim(uint8_t percent) {
    apply(percent); /* does not change preferred_brightness */
}

void asurada_brightness_restore(void) {
    apply(preferred_brightness);
}

static int asurada_brightness_init(void) {
    if (!device_is_ready(pwm_leds_dev)) {
        LOG_WRN("pwm_leds device not ready");
        return 0;
    }
    apply(preferred_brightness);
    return 0;
}

SYS_INIT(asurada_brightness_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
