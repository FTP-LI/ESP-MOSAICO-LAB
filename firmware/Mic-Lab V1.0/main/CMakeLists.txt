idf_component_register(SRCS "main.c" "mic_core.c" "clip_file.c" "mic_storage.c" "mic_audio.c"
    "usb_layout.c" "usb_mic.c" "mic_ui.c" "mic_font.c"
    INCLUDE_DIRS "."
    REQUIRES esp-mosaico-bsp esp_timer esp_iris esp_mosaico_app_recovery nvs_flash
             mic_storage_backend espressif__esp_tinyusb)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=tinyusb_driver_install")
