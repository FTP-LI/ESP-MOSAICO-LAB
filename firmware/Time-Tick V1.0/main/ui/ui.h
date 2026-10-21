// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "settings.h"
#include "services.h"
#include "pet_model.h"
#include "interaction.h"
typedef enum { TT_CMD_TOGGLE, TT_CMD_STOP, TT_CMD_SAVE, TT_CMD_HOME, TT_CMD_RIGHT, TT_CMD_NETWORK, TT_CMD_SCAN, TT_CMD_PET, TT_CMD_START,
               TT_CMD_FEEDBACK, TT_CMD_LOCATION, TT_CMD_FEEDBACK_STOP } tt_command_type_t;
typedef struct { tt_command_type_t type; tt_settings_t settings; tt_pet_action_t pet_action; tt_location_request_t location; } tt_command_t;
typedef bool (*tt_ui_submit_t)(const tt_command_t *command, void *context);
void tt_ui_init(tt_ui_submit_t submit, void *context, const tt_settings_t *settings);
bool tt_ui_editing(void);
/* LVGL thread only. Preview the visible display form; alerts use normal brightness.
 * Leaving the form restores normal brightness without committing the draft. */
unsigned tt_ui_display_brightness(unsigned normal, bool alert);
bool tt_ui_pet_open(void);
/* Thread-safe completion notification; UI leaves the form only after persistence succeeds. */
void tt_ui_save_result(bool success);
void tt_ui_render(const tt_timer_t *timer, tt_face_t face, const tt_settings_t *settings,
                  const tt_services_view_t *services, const char *notice, bool alarm);

void tt_ui_pet_snapshot(const tt_pet_book_t *book);
void tt_ui_pet_result(tt_pet_result_t result);
void tt_ui_interaction_snapshot(const tt_interaction_view_t *view);
