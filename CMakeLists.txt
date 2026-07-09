zephyr_include_directories(include)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library()

if(CONFIG_SHIELD_ASURADA_ADAPTER)

        # Asurada uses the in-tree GC9A01 (galaxycore,gc9x01x) display driver,
        # so the vendored ST7789V driver is no longer built.

        # Per-peripheral split connection-status event: the Connections page and
        # the battery widgets subscribe to zmk_split_central_status_changed, which
        # stock ZMK does not emit -- this module adds it.
        zephyr_library_sources(src/events/split_central_status_changed.c)
        zephyr_library_sources(src/split/bluetooth/central_status_changed_observer.c)

endif()