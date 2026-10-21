idf_component_register(SRCS "diagnostics.c" INCLUDE_DIRS "."
    REQUIRES esp-mosaico-bsp esp_timer esp_driver_gpio
        esp_iris esp_mosaico_app_recovery nvs_flash)
