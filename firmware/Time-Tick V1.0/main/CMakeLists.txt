include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
idf_component_register(SRCS "main.c" "platform/settings.c" "platform/services.c" "platform/pet_storage.c" "platform/interaction.c" "platform/diagnostics.c" "platform/power_monitor.c" "platform/sdk_compat.c" ${TT_SHARED_SOURCES}
    INCLUDE_DIRS ${TT_INCLUDE_DIRS}
    REQUIRES esp-mosaico-bsp esp_timer esp_pm esp_iris esp_mosaico_app_recovery
        nvs_flash esp_wifi esp_netif esp_event esp_http_client mbedtls lwip espressif__cjson)

target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_clk_tree_initialize")
