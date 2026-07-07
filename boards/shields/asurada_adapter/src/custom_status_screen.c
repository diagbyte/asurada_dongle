#include <lvgl.h>

#if defined(CONFIG_ASURADA_STATUS_SCREEN_ASURADA)
#include "layouts/asurada/status_screen.c"
#elif defined(CONFIG_ASURADA_STATUS_SCREEN_CLASSIC)
#include "layouts/classic/status_screen.c"
#elif defined(CONFIG_ASURADA_STATUS_SCREEN_RADII)
#include "layouts/radii/status_screen.c"
#elif defined(CONFIG_ASURADA_STATUS_SCREEN_FIELD)
#include "layouts/field/status_screen.c"
#elif defined(CONFIG_ASURADA_STATUS_SCREEN_OPERATOR)
#include "layouts/operator/status_screen.c"
#else
#error "No status screen layout selected"
#endif
