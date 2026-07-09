if(CONFIG_SHIELD_ASURADA_ADAPTER)
  zephyr_library()
  file(GLOB font_sources src/fonts/*.c)
  zephyr_library_sources(${ZEPHYR_BASE}/misc/empty_file.c)
  zephyr_library_include_directories(${ZEPHYR_LVGL_MODULE_DIR})
  zephyr_library_include_directories(${ZEPHYR_BASE}/lib/gui/lvgl/)
  zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
  zephyr_library_include_directories(${ZEPHYR_CURRENT_CMAKE_DIR}/include)
  zephyr_library_include_directories(include)
  zephyr_library_sources(src/brightness.c)
  zephyr_library_sources(src/custom_status_screen.c)
  zephyr_library_sources(src/display_rotate_init.c)

  if(CONFIG_ASURADA_TRACKBALL)
    zephyr_library_sources(src/trackball_input.c)
  endif()

  zephyr_library_sources(src/screens.c)

  if(CONFIG_ASURADA_TOUCH)
    zephyr_library_sources(src/touch.c)
  endif()

  if(CONFIG_ASURADA_SCREENSAVER)
    zephyr_library_sources(src/screensaver.c)
  endif()

  if(CONFIG_ASURADA_AUTO_PAGE_FOLLOW)
    zephyr_library_sources(src/page_follow.c)
  endif()

  # Only the asurada layout ships in this repo now (the classic/radii/field/
  # operator layouts from the Prospector fork were removed). status_screen.c is
  # #included by custom_status_screen.c, so it stays excluded from the glob.
  if(CONFIG_ASURADA_STATUS_SCREEN_ASURADA)
    file(GLOB layout_sources src/layouts/asurada/*.c)
    list(FILTER layout_sources EXCLUDE REGEX "status_screen\\.c$")
    if(NOT CONFIG_ASURADA_TRACKBALL)
      list(FILTER layout_sources EXCLUDE REGEX "(ball|pointing_mode|trackball_battery)\\.c$")
    endif()
    zephyr_library_sources(${layout_sources})
  endif()

  zephyr_library_sources(${font_sources})
endif()