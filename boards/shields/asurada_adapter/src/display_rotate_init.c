#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asurada_disp, LOG_LEVEL_WRN);

/*
 * The GC9A01 is a symmetric 240x240 round panel, so the default (NORMAL)
 * orientation is correct. ASURADA_ROTATE_DISPLAY_180 flips it if the module
 * is mounted upside down in the case.
 */
int disp_set_orientation(void)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display))
	{
		return -EIO;
	}

#ifdef CONFIG_ASURADA_ROTATE_DISPLAY_180
	enum display_orientation orientation = DISPLAY_ORIENTATION_ROTATED_180;
#else
	enum display_orientation orientation = DISPLAY_ORIENTATION_NORMAL;
#endif

	int ret = display_set_orientation(display, orientation);
	if (ret < 0)
	{
		/* Some display drivers do not implement runtime rotation; that is
		 * fine for a symmetric round panel. Warn instead of failing init. */
		LOG_WRN("display_set_orientation not applied (%d)", ret);
	}

	return 0;
}

SYS_INIT(disp_set_orientation, APPLICATION, 60);
