set(UPSTREAM "${MOSAICO_UTILS_ROOT}/esp-mosaico-vibe-mode/firmware/vibe-mode/components/app_storage")
idf_component_register(
    SRCS "${UPSTREAM}/src/app_storage.c" "${UPSTREAM}/src/storage_metrics.c"
         "${UPSTREAM}/src/storage_blockdev.c" "${UPSTREAM}/src/storage_volume.c"
    INCLUDE_DIRS "${UPSTREAM}/include"
    PRIV_INCLUDE_DIRS "${UPSTREAM}/private"
    PRIV_REQUIRES esp-mosaico-bsp esp_blockdev espressif__spi_nand_flash
                  joltwallet__littlefs esp_driver_spi esp_driver_gpio esp_timer)
# Enable only the pinned storage backend's existing compile gates, not factory services.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CONFIG_ESP_IRIS_SYSTEM_UPDATE=1
    CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_BACKEND=1 CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE=1
    CONFIG_IRIS_FACTORY_NAND_READ_CACHE_KIB=64 CONFIG_IRIS_FACTORY_NAND_POLL_SHORT=0)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=spi_device_transmit")
