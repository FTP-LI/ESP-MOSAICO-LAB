// SPDX-License-Identifier: GPL-3.0-only
#include "settings.h"
#include "nvs.h"
#include <string.h>
esp_err_t tt_settings_load(tt_settings_t *s)
{
    tt_settings_default(s);
    nvs_handle_t handle;
    esp_err_t err = nvs_open("time_tick", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    uint8_t stored[sizeof(tt_settings_t)];
    size_t size = sizeof(stored);
    err = nvs_get_blob(handle, "settings", &stored, &size);
    nvs_close(handle);
    if (err == ESP_OK && tt_settings_decode(stored, size, s)) { /* Valid or migrated. */ }
    else if (err == ESP_OK) err = ESP_ERR_INVALID_STATE;
    return err;
}
esp_err_t tt_settings_save(const tt_settings_t *s)
{
    if (!tt_settings_valid(s)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("time_tick", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(handle, "settings", s, sizeof(*s));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
