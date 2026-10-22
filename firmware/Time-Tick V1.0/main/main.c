// SPDX-License-Identifier: GPL-3.0-only
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bsp/esp_mosaico.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"
#include "settings.h"
#include "services.h"
#include "ui.h"
#include "pet_storage.h"
#include "diagnostics.h"
#include "power_monitor.h"
#include "ui_power.h"
#include <stdatomic.h>

static const char *TAG = "time_tick";

typedef struct {
    tt_timer_t timer;
    tt_face_t face;
    tt_settings_t settings;
    tt_services_view_t services;
    tt_pet_book_t pets;
    tt_interaction_view_t interaction;
    char notice[112];
    bool alarm;
    int rotation;
    unsigned brightness,idle_ms;
} ui_snapshot_t;
static QueueHandle_t ui_updates;
static ui_snapshot_t pending_ui, displayed_ui;
static atomic_uint display_idle_ms;

/* LVGL consumes the latest snapshot on its own task. The 50 Hz timer/IMU
 * loop never waits behind a full-screen render or a panel transfer. */
static void apply_ui_snapshot(lv_timer_t *unused)
{
    (void)unused;
    tt_diagnostics_ui_heartbeat();
    atomic_store(&display_idle_ms,lv_display_get_inactive_time(NULL));
    bool received=xQueueReceive(ui_updates,&displayed_ui,0)==pdTRUE;
    static bool snapshot_ready;
    if (received) snapshot_ready=true;
    if (!snapshot_ready) return;
    tt_power_ui_activity(displayed_ui.idle_ms,displayed_ui.alarm ||
        displayed_ui.interaction.alarm_active || tt_ui_editing());
    unsigned brightness=tt_ui_display_brightness(displayed_ui.brightness,
        displayed_ui.alarm || displayed_ui.interaction.alarm_active || displayed_ui.timer.state==TT_FINISHED);
    tt_diagnostics_display_request(&displayed_ui.rotation,&brightness);
    /* Panel commands share the LVGL worker with QSPI flushing. Sending a
     * brightness command from the sensor task races the driver's transaction
     * pool and can block both tasks while waiting for the same completion. */
    static unsigned applied_brightness=101;
    if (displayed_ui.rotation!=(int)bsp_display_get_rotation())
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_display_set_rotation((bsp_display_rotation_t)displayed_ui.rotation));
    /* Coalesce slider events on this 40ms worker, even without a new sensor
     * snapshot. Never send panel commands from a widget callback or write NVS. */
    if (brightness!=applied_brightness && bsp_display_brightness_set(brightness)==ESP_OK)
        applied_brightness=brightness;
    if (!received) return;
    tt_ui_pet_snapshot(&displayed_ui.pets);
    tt_ui_interaction_snapshot(&displayed_ui.interaction);
    tt_ui_render(&displayed_ui.timer,displayed_ui.face,&displayed_ui.settings,
                 &displayed_ui.services,displayed_ui.notice,displayed_ui.alarm);
}

static bool submit_command(const tt_command_t *command, void *context)
{
    return xQueueSend((QueueHandle_t)context, command, 0) == pdTRUE;
}

/* Explicit, bounded speaker check through the Iris gateway, never on boot.
 * Private RPC 0x5454/2: "speaker", "speaker:N" (0..8) or "stop". Uses the normal preview path;
 * no settings are saved, and the test never enables the vibration motor. */
static esp_err_t speaker_test_rpc(const esp_iris_rpc_request_t *request,uint8_t *response,
                                 size_t capacity,size_t *used,void *context)
{
    if (capacity<96) return ESP_ERR_INVALID_SIZE;
    tt_command_t command={0};
    bool default_tone=request->payload_size==7 && !memcmp(request->payload,"speaker",7);
    bool selected_tone=request->payload_size==9 && !memcmp(request->payload,"speaker:",8) &&
        request->payload[8]>='0' && request->payload[8]<'0'+TT_RING_COUNT;
    if (default_tone || selected_tone) {
        command.type=TT_CMD_FEEDBACK;
        tt_settings_default(&command.settings);
        command.settings.alarm.ringtone=selected_tone?request->payload[8]-'0':TT_RING_MELODY;
        command.settings.vibration=false;
    } else if (request->payload_size==4 && !memcmp(request->payload,"stop",4)) {
        command.type=TT_CMD_FEEDBACK_STOP;
    } else return ESP_ERR_INVALID_ARG;
    if (!submit_command(&command,context)) return ESP_ERR_TIMEOUT;
    *used=snprintf((char *)response,capacity,"{\"queued\":true,\"seconds\":3,\"volume\":65,\"vibration\":false}");
    return ESP_OK;
}

void app_main(void)
{
    /* Sensor/timer deadlines precede background HTTPS certificate/JSON work.
     * This task sleeps between samples; Wi-Fi and audio keep their priorities. */
    vTaskPrioritySet(NULL,2);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    iris_ota_support_start();
    /* Ready tasks still run at 320 MHz. Only idle intervals may drop to
     * XTAL / the driver's APB lock; do not enable automatic light sleep. */
    const esp_pm_config_t pm={.max_freq_mhz=320,.min_freq_mhz=40,.light_sleep_enable=false};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));
#if CONFIG_ESP_CRYPTO_CLK_ON_DEMAND
    ESP_LOGI(TAG,"power: CPU 40..320MHz, display frame guard, crypto clocks on demand");
#endif
#if CONFIG_PM_TICKLESS_IDLE_WAITI
    ESP_LOGI(TAG,"power: tickless WFI enabled; automatic light sleep remains disabled");
#endif
    bsp_board_variant_t variant;
    ESP_ERROR_CHECK(bsp_board_variant_get(&variant));
    if (variant != BSP_BOARD_VARIANT_V1_2) {
        ESP_LOGE(TAG, "CoreBoard V1.2 required; no pin override applied");
        return;
    }
    tt_settings_t settings;
    esp_err_t load = tt_settings_load(&settings);
    if (load != ESP_OK && load != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "Settings unavailable; using defaults (%s)", esp_err_to_name(load));
    ESP_LOGI("tt_alarm","settings schema=%u tone=%u volume=%u seconds=%u enabled=%d vibration=%d",
        (unsigned)settings.schema,settings.alarm.ringtone,settings.alarm.volume,settings.alarm.duration_s,settings.alarm.enabled,settings.vibration);
    tt_pet_book_t pets;
    tt_pet_runtime_t pet_runtime={0};
    esp_err_t pet_load=tt_pet_load(&pets);
    if (pet_load!=ESP_OK && pet_load!=ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG,"Pet save unavailable; using defaults (%s)",esp_err_to_name(pet_load));
    setenv("TZ", settings.timezone, 1);
    tzset();
    QueueHandle_t commands = xQueueCreate(8, sizeof(tt_command_t));
    ui_updates=xQueueCreate(1,sizeof(ui_snapshot_t));
    if (!commands || !ui_updates || !bsp_display_start()) {
        ESP_LOGE(TAG, "Display/command queue unavailable; Recovery remains reachable");
        return;
    }
    ESP_ERROR_CHECK(bsp_display_brightness_set(settings.brightness));
    if (!bsp_display_lock(1000)) return;
    tt_ui_init(submit_command, commands, &settings);
    tt_diagnostics_start(bsp_display_get());
    tt_ui_pet_snapshot(&pets);
    lv_timer_create(apply_ui_snapshot,40,NULL);
    bsp_display_unlock();
    bool imu_ready = bsp_imu_init() == ESP_OK;
    if (imu_ready) imu_ready = bsp_imu_start(NULL) == ESP_OK;
    bool motor_ready = bsp_motor_init() == ESP_OK;
    ESP_ERROR_CHECK_WITHOUT_ABORT(tt_power_start());
    (void)tt_interaction_start();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_rpc_register(0x5454,2,speaker_test_rpc,commands));
    char notice[112] = "";
    if (!imu_ready) snprintf(notice, sizeof(notice), "Motion sensor unavailable");
    else if (!settings.timer.calibrated) snprintf(notice, sizeof(notice), "Open Settings: set upright, then right turn");
    if (tt_services_start(&settings) != ESP_OK)
        snprintf(notice, sizeof(notice), "Network unavailable; offline timers still work");

    tt_orientation_t orientation = {0};
    tt_timer_t timer;
    tt_timer_init(&timer);
    tt_face_t face = TT_UNKNOWN;
    int pending_home = 0, rotation = 0;
    int64_t next_ui = 0, touch_until=0;
    tt_alarm_t reminder={0};
    tt_state_t previous_state=TT_IDLE; tt_face_t previous_active=TT_HOME;
    bool alarm = false, motor_on = false, pet_dirty=false;
    int64_t last_motion=0;
    tt_face_t power_face=TT_UNKNOWN;
    int64_t next_pet_save=60000;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        int64_t now = esp_timer_get_time() / 1000;
        tt_diagnostics_tick(now);
        float accel[3] = {0}, gyro[3] = {0};
        bool sample_ok = imu_ready && bsp_imu_get_motion(accel,gyro) == ESP_OK;
        tt_orientation_sample(&orientation, accel, gyro, sample_ok, now);
        if (tt_timer_tick(&timer, now)) {
            alarm = true;
            tt_interaction_alarm_active(false); /* End any in-progress preview first. */
            tt_alarm_start(&reminder,&settings.alarm,settings.vibration,false,now);
            reminder.origin_axis=orientation.stable; /* Last reliable pose, even if a turn is in progress. */
            ESP_LOGI("tt_alarm","timer expired: tone=%u volume=%u seconds=%u enabled=%d vibration=%d",
                settings.alarm.ringtone,settings.alarm.volume,settings.alarm.duration_s,settings.alarm.enabled,settings.vibration);
            if (timer.duration_s>=60) {
                (void)tt_pet_focus_complete(&pets,settings.pet,timer.duration_s);
                pet_dirty=tt_pet_save(&pets)!=ESP_OK; next_pet_save=now+60000;
                if (pet_dirty) snprintf(notice,sizeof(notice),"Focus progress awaiting save");
            }
        }

        pet_dirty|=tt_pet_tick(&pets,&pet_runtime,settings.pet,now);
        tt_command_t command;
        while (xQueueReceive(commands, &command, 0) == pdTRUE) {
            if (command.type==TT_CMD_FEEDBACK) {
                if (!alarm && tt_settings_valid(&command.settings)) {
                    tt_interaction_alarm_active(false);
                    tt_alarm_start(&reminder,&command.settings.alarm,command.settings.vibration,true,now);
                    ESP_LOGI("tt_alarm","preview: tone=%u volume=%u enabled=%d vibration=%d",
                        command.settings.alarm.ringtone,command.settings.alarm.volume,
                        command.settings.alarm.enabled,command.settings.vibration);
                }
            } else if (command.type==TT_CMD_FEEDBACK_STOP) {
                if (reminder.preview) tt_alarm_stop(&reminder);
            } else if (command.type == TT_CMD_STOP) {
                tt_timer_stop(&timer); alarm = false; tt_alarm_stop(&reminder);
                snprintf(notice, sizeof(notice), "Stopped; flip to another face to start");
            } else if (command.type == TT_CMD_START) {
                /* Manual start uses the retained display face: touching the
                 * dial can unsettle the IMU. Only automatic flips require a
                 * fresh stable sample; disabled/uncalibrated faces still fail. */
                bool started=tt_timer_start(&timer,&settings.timer,face,now);
                ESP_LOGI("tt_dial","start: face=%d accepted=%d imu_valid=%d settled=%d",
                    face,started,sample_ok,orientation.settled);
                if (started) {
                    alarm=false; tt_alarm_stop(&reminder); notice[0]='\0';
                }
            } else if (command.type == TT_CMD_TOGGLE) {
                tt_timer_toggle(&timer, now);
            } else if (command.type == TT_CMD_SAVE) {
                /* Calibration commands are processed separately and always win over a stale form. */
                command.settings.timer.home_axis = settings.timer.home_axis;
                command.settings.timer.right_axis = settings.timer.right_axis;
                command.settings.timer.calibrated = settings.timer.calibrated;
                if (tt_settings_save(&command.settings) == ESP_OK) {
                    settings = command.settings;
                    setenv("TZ", settings.timezone, 1); tzset();
                    tt_services_configure(&settings);
                    tt_ui_save_result(true);
                    snprintf(notice, sizeof(notice), "Saved; new durations apply on the next flip");
                } else {
                    tt_ui_save_result(false);
                    snprintf(notice, sizeof(notice), "Could not save settings");
                }
            } else if (command.type == TT_CMD_PET) {
                tt_pet_book_t candidate=pets; tt_pet_runtime_t runtime=pet_runtime;
                tt_pet_result_t result=tt_pet_act(&candidate,&runtime,command.settings.pet,command.pet_action,now);
                if (result==TT_PET_OK) {
                    if (tt_pet_save(&candidate)==ESP_OK) { pets=candidate; pet_runtime=runtime; pet_dirty=false; next_pet_save=now+60000; }
                    else result=TT_PET_SAVE_FAILED;
                }
                if (result==TT_PET_OK) {
                    touch_until=now+35;
                    if (settings.sound) tt_interaction_sound(TT_SOUND_TAP,settings.sound_volume);
                }
                tt_ui_pet_result(result);
            } else if (command.type == TT_CMD_NETWORK) {
                tt_services_configure(&command.settings);
            } else if (command.type == TT_CMD_SCAN) {
                tt_services_scan();
            } else if (command.type == TT_CMD_LOCATION) {
                tt_services_locate(&command.location);
            } else if (command.type == TT_CMD_HOME || command.type == TT_CMD_RIGHT) {
                if (!sample_ok || !orientation.settled) {
                    snprintf(notice, sizeof(notice), "Hold still on a flat side, then tap again");
                } else if (command.type == TT_CMD_HOME) {
                    pending_home = orientation.stable;
                    snprintf(notice, sizeof(notice), "Turn clockwise 90 deg; tap Set right turn");
                } else if (!pending_home || abs(pending_home) == abs(orientation.stable)) {
                    snprintf(notice, sizeof(notice), "Set upright first, then turn exactly 90 deg");
                } else {
                    tt_settings_t calibrated = settings;
                    calibrated.timer.home_axis = pending_home;
                    calibrated.timer.right_axis = orientation.stable;
                    calibrated.timer.calibrated = true;
                    if (tt_settings_save(&calibrated) == ESP_OK) {
                        settings = calibrated;
                        pending_home = 0;
                        tt_timer_init(&timer);
                        alarm = false; tt_alarm_stop(&reminder);
                        snprintf(notice, sizeof(notice), "Calibration saved");
                    } else snprintf(notice, sizeof(notice), "Could not save calibration");
                }
            }
        }
        if (pet_dirty && now>=next_pet_save) {
            if (tt_pet_save(&pets)==ESP_OK) pet_dirty=false;
            else ESP_LOGW(TAG,"Pet autosave failed; retry in 60 seconds");
            next_pet_save=now+60000;
        }
        if (sample_ok && orientation.settled) {
            tt_face_t sensed_face = tt_face_from_axis(&settings.timer, orientation.stable);
            /* Settings rotate too; editing only suppresses flip-to-time actions.
             * The display worker changes panel and touch mapping between frames. */
            rotation = tt_rotation(sensed_face, settings.timer.reverse_rotation, rotation);
            if (!tt_ui_editing() || timer.state==TT_FINISHED) {
                face = sensed_face;
                if (timer.state==TT_FINISHED) {
                    if (tt_alarm_turn(&reminder,orientation.stable)) {
                        tt_timer_stop(&timer); timer.observed=face; touch_until=0;
                        ESP_LOGI("tt_alarm","dismissed by turn: axis=%d face=%d; next flip may start a timer",orientation.stable,face);
                    }
                } else tt_timer_face(&timer, &settings.timer, face, now);
            }
        }
        if (timer.state != TT_FINISHED) { alarm = false; if (!reminder.preview) tt_alarm_stop(&reminder); }
        tt_alarm_output_t alert=tt_alarm_poll(&reminder,now);
        tt_interaction_alarm_active(alert.active);
        if (alert.ring) tt_interaction_alarm_pulse(reminder.config.ringtone,reminder.config.volume);
        if (timer.state!=previous_state || timer.active!=previous_active) {
            if (timer.state==TT_RUNNING || timer.state==TT_PAUSED) {
                tt_sound_t cue=timer.state==TT_RUNNING?TT_SOUND_START:TT_SOUND_PAUSE;
                if (settings.sound) tt_interaction_sound(cue,settings.sound_volume);
                if (timer.state!=TT_FINISHED) touch_until=now+45;
            }
            previous_state=timer.state; previous_active=timer.active;
        }
        bool vibrate = motor_ready && (alert.motor || (!alert.active && settings.vibration && now<touch_until));
        if (vibrate != motor_on) {
            if (bsp_motor_set(vibrate) == ESP_OK) motor_on = vibrate;
        }
        if (now >= next_ui) {
            pending_ui.timer=timer; pending_ui.face=face; pending_ui.settings=settings;
            pending_ui.pets=pets; pending_ui.alarm=alarm;
            tt_services_snapshot(&pending_ui.services);
            tt_power_snapshot(&pending_ui.services.power);
            if (face!=power_face) { power_face=face; last_motion=now; }
            unsigned idle=atomic_load(&display_idle_ms);
            if ((uint64_t)(now-last_motion)<idle) idle=(unsigned)(now-last_motion);
            pending_ui.idle_ms=idle;
            pending_ui.rotation=rotation;
            pending_ui.brightness=tt_power_brightness(&pending_ui.services.power,settings.brightness,idle,alarm || tt_ui_editing());
            tt_interaction_snapshot(&pending_ui.interaction);
            snprintf(pending_ui.notice,sizeof(pending_ui.notice),"%s",
                     sample_ok?notice:"Motion sensor unavailable; timer is retained");
            tt_diagnostics_ui(xQueueOverwrite(ui_updates,&pending_ui)==pdPASS);
            next_ui = now + 100;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
    }
}
