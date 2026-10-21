// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui_garden.h"
typedef void (*tt_pet_ui_action_t)(tt_pet_action_t action);
void tt_pet_ui_open(tt_pet_t type,uint8_t language,lv_event_cb_t back,tt_pet_ui_action_t action);
void tt_pet_ui_update(const tt_pet_state_t *state,const tt_focus_progress_t *focus,bool pending);
void tt_pet_ui_feedback(tt_pet_result_t result,tt_pet_action_t action,const tt_pet_state_t *state);
lv_obj_t *tt_pet_ui_garden(void);
