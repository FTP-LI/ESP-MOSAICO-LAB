// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "pet_model.h"
#include "esp_err.h"
esp_err_t tt_pet_load(tt_pet_book_t *book);
esp_err_t tt_pet_save(const tt_pet_book_t *book);
