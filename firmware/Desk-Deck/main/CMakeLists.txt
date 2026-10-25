idf_component_register(SRCS "main.c" "usb_hid.c" "deck_font.c"
    INCLUDE_DIRS "."
    REQUIRES esp-mosaico-bsp esp_timer esp_iris esp_mosaico_app_recovery nvs_flash espressif__esp_tinyusb)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=tinyusb_driver_install")
