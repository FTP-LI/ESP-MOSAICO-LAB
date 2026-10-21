// SPDX-License-Identifier: GPL-3.0-only
#include "pet_storage.h"
#include "nvs.h"
esp_err_t tt_pet_load(tt_pet_book_t *book)
{
    tt_pet_default(book);
    nvs_handle_t h; esp_err_t err=nvs_open("time_tick",NVS_READONLY,&h);
    if (err!=ESP_OK) return err;
    tt_pet_book_t saved; size_t size=sizeof(saved);
    err=nvs_get_blob(h,"pet_book",&saved,&size); nvs_close(h);
    if (err==ESP_OK && !tt_pet_decode(book,&saved,size)) return ESP_ERR_INVALID_STATE;
    return err;
}
esp_err_t tt_pet_save(const tt_pet_book_t *book)
{
    if (!tt_pet_valid(book)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h; esp_err_t err=nvs_open("time_tick",NVS_READWRITE,&h);
    if (err!=ESP_OK) return err;
    err=nvs_set_blob(h,"pet_book",book,sizeof(*book));
    if (err==ESP_OK) err=nvs_commit(h);
    nvs_close(h); return err;
}
