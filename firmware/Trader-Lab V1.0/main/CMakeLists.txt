idf_component_register(SRCS "main.c" "model.c" "gbk.c" "network.c" "audio.c" "alarm_model.c" "trader_font.c" "sdk_compat.c" "watchlist_seed.c" "global_market.c"
    INCLUDE_DIRS "."
    REQUIRES esp-mosaico-bsp esp_timer esp_iris esp_mosaico_app_recovery nvs_flash
        esp_wifi esp_netif esp_event esp_http_client mbedtls lwip espressif__cjson)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_clk_tree_initialize")
