#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "api.h"
#include "badge.h"
#include "badge_game.h"
#include "display.h"
#include "modal.h"
#include "text_input.h"
#include "led.h"
#include "esp_timer.h"
#include "badge/led_patterns.h"

#include "redeem_code.h"

typedef struct {
    char code[6];
    lv_obj_t *overlay;
    redeem_code_cb_t cb;
    void *user_data;
    api_result_t *result;
    redeem_ui_texts_t ui;
    bool has_ui_overrides;
} redeem_code_task_args_t;

static void redeem_code_result_modal_cb(uint8_t btn_idx, void *user_data) {
    redeem_code_task_args_t *args = (redeem_code_task_args_t *)user_data;
    if (args && args->cb) {
        args->cb(btn_idx, args->result, args->user_data);
    }
    if (args && args->result) {
        api_free_result(args->result, true);
    }
    if (args) {
        vPortFree(args);
    }
}

static void redeem_code_task(void *pv) {
    redeem_code_task_args_t *args = (redeem_code_task_args_t *)pv;
    if (!args) {
        vTaskDelete(NULL);
        return;
    }
    args->result = api_redeem_code(args->code);
    if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        if (args->overlay) {
            modal_async_close(args->overlay);
        }
        modal_message_config_t mc = {0};
        if (args->result && args->result->ok) {
            mc.title = args->has_ui_overrides && args->ui.success_title ? args->ui.success_title : "Success";
            mc.message =
                args->has_ui_overrides && args->ui.success_message ? args->ui.success_message : "Code redeemed successfully!";

            // Special handling for Sorting Hat assignment
            if (args->has_ui_overrides && args->ui.success_title && strcmp(args->ui.success_title, "Assignment") == 0) {
                // This is a Sorting Hat redemption - customize message with faction name
                if (badge_config.team_id <= NUM_FACTIONS) {
                    faction_id_t faction_id = faction_from_team_id(badge_config.team_id);
                    if (faction_id != FACTION_NONE) {
                        const char *faction_name = get_faction_name(faction_id);
                        static char faction_msg[64];
                        snprintf(faction_msg, sizeof(faction_msg), "Welcome to %s", faction_name);
                        mc.message = faction_msg;
                    }
                }
            }

            mc.buttons[0] = "Continue";
            mc.success    = true;
            led_pattern_sparkles();
        } else if (args->result) {
            mc.title      = args->has_ui_overrides && args->ui.failure_title ? args->ui.failure_title : "Failed";
            mc.message    = args->result->detail ? args->result->detail : "Failed to redeem code.";
            mc.buttons[0] = "OK";
            mc.success    = false;
            led_pattern_flash_red();
        } else {
            mc.title      = "Error";
            mc.message    = "Could not contact server.";
            mc.buttons[0] = "OK";
            mc.success    = false;
            led_pattern_flash_red();
        }
        mc.callback  = redeem_code_result_modal_cb;
        mc.user_data = args;
        modal_message_open(&mc);
        lvgl_unlock(__FILE__, __LINE__);
    }
    vTaskDelete(NULL);
}

esp_err_t try_redeem_code_ctx(const char *code, size_t code_len, const redeem_ui_texts_t *ui, redeem_code_cb_t callback,
                              void *user_data) {
    if (!code || code_len != 5) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *p = code;
    while (*p) {
        if (!strchr(TI_BASE32_NC, *p)) {
            return ESP_ERR_INVALID_ARG;
        }
        p++;
    }
    lv_obj_t *overlay = NULL;
    if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        overlay = modal_async_open(ui && ui->spinner_text ? ui->spinner_text : "Redeeming code...");
        lvgl_unlock(__FILE__, __LINE__);
    }
    redeem_code_task_args_t *args = pvPortMalloc(sizeof(redeem_code_task_args_t));
    if (!args) {
        if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
            if (overlay) {
                modal_async_close(overlay);
            }
            modal_message_config_t mc = {
                .title     = "Error",
                .message   = "Out of memory.",
                .buttons   = {"OK", NULL},
                .success   = false,
                .callback  = NULL,
                .user_data = NULL,
            };
            modal_message_open(&mc);
            lvgl_unlock(__FILE__, __LINE__);
        }
        return ESP_ERR_NO_MEM;
    }
    memset(args, 0, sizeof(*args));
    memcpy(args->code, code, 5);
    args->overlay   = overlay;
    args->cb        = callback;
    args->user_data = user_data;
    args->result    = NULL;
    if (ui) {
        args->ui               = *ui;
        args->has_ui_overrides = true;
    } else {
        args->has_ui_overrides = false;
    }
    BaseType_t ok = xTaskCreate(redeem_code_task, "redeem_code_task", 8192, args, tskIDLE_PRIORITY + 2, NULL);
    if (ok != pdPASS) {
        vPortFree(args);
        if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
            if (overlay) {
                modal_async_close(overlay);
            }
            modal_message_config_t mc = {
                .title     = "Error",
                .message   = "Could not start task.",
                .buttons   = {"OK", NULL},
                .success   = false,
                .callback  = NULL,
                .user_data = NULL,
            };
            modal_message_open(&mc);
            lvgl_unlock(__FILE__, __LINE__);
        }
    }
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t try_redeem_code(const char *code, size_t code_len, redeem_code_cb_t callback, void *user_data) {
    return try_redeem_code_ctx(code, code_len, NULL, callback, user_data);
}
