#include <string.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "credits.h"
#include "theme.h"
#include "lvgl.h"
#include "nav.h"
#include "input.h"
#include "modal.h"
#include "api.h"
#include "badge.h"
#include "../content.h"

static const char *TAG = "screens/main [apps/credits]";

// -------------------------------------------------------------------------------------------------
// State Management
// -------------------------------------------------------------------------------------------------

typedef enum {
    CREDITS_STATE_IDLE,
    CREDITS_STATE_SELECTING_AMOUNT,
    CREDITS_STATE_REQUESTING_TRANSFER,
    CREDITS_STATE_TRANSFER_PENDING,
} credits_state_t;

typedef struct {
    credits_state_t state;

    // Amount selection modal
    lv_obj_t *amount_overlay;
    lv_obj_t *amount_label;
    lv_obj_t *plus_btn;
    lv_obj_t *minus_btn;
    lv_obj_t *transfer_btn;
    lv_obj_t *cancel_btn;
    uint32_t selected_amount;
    int64_t hold_start_time;
    int64_t last_increment_time;
    lv_obj_t *held_btn;
    nav_scope_guard_t amount_nav_guard;
    nav_ctx_t *amount_nav_ctx;

    // Transfer pending modal
    lv_obj_t *pending_overlay;
    lv_obj_t *code_label;
    lv_obj_t *countdown_label;
    char transfer_code[6];
    char expires_at[32];
    lv_timer_t *status_poll_timer;
    bool polling_active;
    bool timer_callback_running; // Guard against re-entry
    nav_scope_guard_t pending_nav_guard;
    nav_ctx_t *pending_nav_ctx;

    // Cancel state
    bool cancel_requested;

    // Background poll task
    TaskHandle_t poll_task_handle;
    bool poll_task_running;
    bool poll_task_busy;

    // Main screen
    lv_obj_t *credits_value_label;
} credits_ctx_t;

static credits_ctx_t s_ctx = {0};

// Forward declaration for timer callback (used by recreate helper)
static void status_poll_timer_cb(lv_timer_t *timer);

// -------------------------------------------------------------------------------------------------
// Timestamp Parsing
// -------------------------------------------------------------------------------------------------

/**
 * Parse ISO 8601 timestamp to time_t, handling various formats.
 * Handles: "2025-10-19T22:07:32.540169", "2025-10-19T22:07:32Z", "2025-10-19T22:07:32"
 * Note: API returns local time even if 'Z' suffix is present (ignore it)
 */
static bool parse_iso_timestamp(const char *timestamp, struct tm *tm_out) {
    if (!timestamp || !tm_out) {
        return false;
    }

    memset(tm_out, 0, sizeof(*tm_out));

    // Parse: YYYY-MM-DDTHH:MM:SS[.microseconds][Z]
    int year, month, day, hour, min, sec;
    int parsed = sscanf(timestamp, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec);

    if (parsed != 6) {
        ESP_LOGE(TAG, "Failed to parse timestamp: %s", timestamp);
        return false;
    }

    tm_out->tm_year  = year - 1900;
    tm_out->tm_mon   = month - 1;
    tm_out->tm_mday  = day;
    tm_out->tm_hour  = hour;
    tm_out->tm_min   = min;
    tm_out->tm_sec   = sec;
    tm_out->tm_isdst = -1;

    return true;
}

/**
 * Calculate seconds remaining until expiry timestamp
 */
static int32_t seconds_until_expiry(const char *expires_at) {
    struct tm expiry_tm;
    if (!parse_iso_timestamp(expires_at, &expiry_tm)) {
        return -1;
    }

    time_t expiry_time = mktime(&expiry_tm);
    time_t now;
    time(&now);

    int32_t remaining = (int32_t)difftime(expiry_time, now);
    return remaining;
}

// -------------------------------------------------------------------------------------------------
// NVS Persistence for Pending Transfers
// -------------------------------------------------------------------------------------------------

#define NVS_NAMESPACE   "credits"
#define NVS_KEY_CODE    "pending_code"
#define NVS_KEY_AMOUNT  "pending_amt"
#define NVS_KEY_EXPIRES "pending_exp"

static void save_pending_transfer(const char *code, uint32_t amount, const char *expires_at) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for write: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_str(handle, NVS_KEY_CODE, code);
    nvs_set_u32(handle, NVS_KEY_AMOUNT, amount);
    nvs_set_str(handle, NVS_KEY_EXPIRES, expires_at);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved pending transfer: %s (%d credits)", code, amount);
}

static bool load_pending_transfer(char *code_out, uint32_t *amount_out, char *expires_out) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t code_len    = 6;
    size_t expires_len = 32;

    err = nvs_get_str(handle, NVS_KEY_CODE, code_out, &code_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_get_u32(handle, NVS_KEY_AMOUNT, amount_out);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_get_str(handle, NVS_KEY_EXPIRES, expires_out, &expires_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded pending transfer: %s (%d credits)", code_out, *amount_out);
    return true;
}

static void clear_pending_transfer(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    }

    nvs_erase_key(handle, NVS_KEY_CODE);
    nvs_erase_key(handle, NVS_KEY_AMOUNT);
    nvs_erase_key(handle, NVS_KEY_EXPIRES);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Cleared pending transfer from NVS");
}

// -------------------------------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------------------------------

static void cleanup_amount_modal(void);
static void cleanup_pending_modal(void);
static void show_error_modal(const char *title, const char *message, bool allow_retry);
static void credits_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data);
static void show_transfer_pending_modal(const char *code, uint32_t amount, const char *expires_at);

// -------------------------------------------------------------------------------------------------
// Transfer Pending Modal
// -------------------------------------------------------------------------------------------------

static void update_countdown_display(void) {
    lv_obj_t *label = s_ctx.countdown_label;
    if (!label || !s_ctx.expires_at[0] || !s_ctx.polling_active) {
        return;
    }

    int32_t remaining_secs = seconds_until_expiry(s_ctx.expires_at);

    if (remaining_secs <= 0) {
        lv_label_set_text(label, "Expired");
        return;
    }

    int mins = remaining_secs / 60;
    int secs = remaining_secs % 60;

    static char buf[32];
    snprintf(buf, sizeof(buf), "Expires in: %d:%02d", mins, secs);
    lv_label_set_text(label, buf);
}

static void status_poll_timer_cb(lv_timer_t *timer) {
    ESP_LOGD(TAG, "Timer tick: polling=%d, busy=%d, cancel=%d", s_ctx.polling_active, s_ctx.poll_task_busy,
             s_ctx.cancel_requested);

    if (s_ctx.cancel_requested || !s_ctx.polling_active || !timer) {
        return;
    }

    // Update countdown display on LVGL thread
    update_countdown_display();

    // If poll task not running, nothing to do
    if (!s_ctx.poll_task_handle || !s_ctx.poll_task_running) {
        return;
    }

    // Avoid queuing a new poll if one is already running
    if (s_ctx.poll_task_busy) {
        ESP_LOGD(TAG, "Poll task busy, skipping notify");
        return;
    }

    // Notify background task to perform the network poll
    xTaskNotifyGive(s_ctx.poll_task_handle);
}

// -----------------------------------------------------------------------------
// Background poll task and LVGL async result handler
// -----------------------------------------------------------------------------

typedef enum {
    POLL_RESULT_NONE = 0,
    POLL_RESULT_REDEEMED,
    POLL_RESULT_EXPIRED,
    POLL_RESULT_CANCELLED,
    POLL_RESULT_ERROR,
    POLL_RESULT_ESCROWED,
} poll_result_type_t;

typedef struct {
    poll_result_type_t type;
    char detail[128];
} poll_task_msg_t;

typedef struct {
    bool success;
    char detail[128];
} cancel_task_msg_t;

typedef struct {
    char code[16];
} cancel_task_args_t;

static void async_handle_poll_result(void *user_data) {
    poll_task_msg_t *msg = (poll_task_msg_t *)user_data;
    if (!msg) {
        return;
    }

    switch (msg->type) {
        case POLL_RESULT_REDEEMED:
            cleanup_pending_modal();
            try_status_sync();
            {
                modal_message_config_t mc = {
                    .title     = "Complete",
                    .message   = "Credits successfully transferred!",
                    .buttons   = {"OK", NULL},
                    .success   = true,
                    .callback  = NULL,
                    .user_data = NULL,
                };
                modal_message_open(&mc);
                if (s_ctx.credits_value_label) {
                    static char credits_buf[32];
                    snprintf(credits_buf, sizeof(credits_buf), "%d", badge_config.credits);
                    lv_label_set_text(s_ctx.credits_value_label, credits_buf);
                }
            }
            break;
        case POLL_RESULT_EXPIRED:
            cleanup_pending_modal();
            show_error_modal("Expired", "The transfer request has expired.", false);
            break;
        case POLL_RESULT_CANCELLED: cleanup_pending_modal(); break;
        case POLL_RESULT_ERROR:
            cleanup_pending_modal();
            show_error_modal("Check Failed", msg->detail[0] ? msg->detail : "Failed to check transfer status", true);
            break;
        case POLL_RESULT_ESCROWED:
        default:
            // nothing to do, keep polling
            break;
    }

    vPortFree(msg);
}

static void async_handle_cancel_result(void *user_data) {
    cancel_task_msg_t *msg = (cancel_task_msg_t *)user_data;
    if (!msg) {
        return;
    }

    if (msg->success) {
        // Cancel succeeded: close modal and inform user
        cleanup_pending_modal();
        modal_message_config_t mc = {
            .title     = "Cancelled",
            .message   = "Transfer successfully cancelled",
            .buttons   = {"OK", NULL},
            .success   = true,
            .callback  = NULL,
            .user_data = NULL,
        };
        modal_message_open(&mc);
    } else {
        // Cancel failed: restore polling and show error so user can retry
        ESP_LOGW(TAG, "Cancel failed: %s", msg->detail[0] ? msg->detail : "Unknown error");
        show_error_modal("Cancel Failed", msg->detail[0] ? msg->detail : "Failed to cancel transfer", true);

        // Restore polling state and recreate timer if needed
        s_ctx.cancel_requested = false;
        s_ctx.polling_active   = true;
        if (!s_ctx.status_poll_timer) {
            s_ctx.status_poll_timer = lv_timer_create(status_poll_timer_cb, 3000, NULL);
            lv_timer_set_repeat_count(s_ctx.status_poll_timer, -1);
            lv_timer_reset(s_ctx.status_poll_timer);
        }

        // Wake poll task to run an immediate check
        if (s_ctx.poll_task_handle && s_ctx.poll_task_running) {
            xTaskNotifyGive(s_ctx.poll_task_handle);
        }
    }

    vPortFree(msg);
}

static void cancel_task(void *pv) {
    cancel_task_args_t *args = (cancel_task_args_t *)pv;
    cancel_task_msg_t *msg   = NULL;
    if (!args) {
        vTaskDelete(NULL);
        return;
    }

    api_result_t *result = api_cancel_credit_swap(args->code);

    msg = pvPortMalloc(sizeof(cancel_task_msg_t));
    if (!msg) {
        if (result) {
            api_free_result(result, true);
        }
        vPortFree(args);
        vTaskDelete(NULL);
        return;
    }
    memset(msg, 0, sizeof(*msg));

    if (!result) {
        msg->success = false;
        snprintf(msg->detail, sizeof(msg->detail), "Network error");
    } else if (!result->ok) {
        msg->success = false;
        if (result->detail) {
            strncpy(msg->detail, result->detail, sizeof(msg->detail) - 1);
        } else {
            snprintf(msg->detail, sizeof(msg->detail), "Unknown error");
        }
    } else {
        msg->success = true;
    }

    if (result) {
        api_free_result(result, true);
    }

    // Notify LVGL thread with result
    lv_async_call(async_handle_cancel_result, msg);

    vPortFree(args);
    vTaskDelete(NULL);
}

static void status_poll_task(void *pv) {
    (void)pv;
    ESP_LOGD(TAG, "Status poll task started");
    while (s_ctx.poll_task_running) {
        // Wait for notification from timer
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!s_ctx.poll_task_running) {
            break;
        }
        if (s_ctx.cancel_requested || !s_ctx.polling_active) {
            continue;
        }

        s_ctx.poll_task_busy = true;

        api_result_t *result = api_credit_swap_status(s_ctx.transfer_code);
        if (!result) {
            ESP_LOGW(TAG, "Poll task: network error");
            s_ctx.poll_task_busy = false;
            continue;
        }

        poll_task_msg_t *msg = pvPortMalloc(sizeof(poll_task_msg_t));
        if (!msg) {
            api_free_result(result, true);
            s_ctx.poll_task_busy = false;
            continue;
        }
        memset(msg, 0, sizeof(*msg));

        if (!result->ok) {
            msg->type = POLL_RESULT_ERROR;
            if (result->detail) {
                strncpy(msg->detail, result->detail, sizeof(msg->detail) - 1);
            }
            api_free_result(result, true);
            lv_async_call(async_handle_poll_result, msg);
            s_ctx.poll_task_busy = false;
            continue;
        }

        api_credit_swap_status_response_t *status_data = (api_credit_swap_status_response_t *)result->data;
        if (status_data && status_data->status) {
            if (strcmp(status_data->status, "redeemed") == 0) {
                msg->type = POLL_RESULT_REDEEMED;
            } else if (strcmp(status_data->status, "expired") == 0) {
                msg->type = POLL_RESULT_EXPIRED;
            } else if (strcmp(status_data->status, "cancelled") == 0) {
                msg->type = POLL_RESULT_CANCELLED;
            } else {
                msg->type = POLL_RESULT_ESCROWED;
            }
        } else {
            msg->type = POLL_RESULT_ESCROWED;
        }

        api_free_result(result, true);

        // If not escrowed, schedule UI update; if escrowed, just free msg
        if (msg->type == POLL_RESULT_ESCROWED) {
            vPortFree(msg);
        } else {
            lv_async_call(async_handle_poll_result, msg);
        }

        s_ctx.poll_task_busy = false;
    }

    ESP_LOGD(TAG, "Status poll task exiting");
    // Clear handle to indicate stopped
    s_ctx.poll_task_handle = NULL;
    vTaskDelete(NULL);
}

static void pending_cancel_btn_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // Log ALL events for debugging
    if (code == LV_EVENT_KEY || code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED) {
        // ESP_LOGD(TAG, "<<< CANCEL BUTTON EVENT: code=%d >>>", code);
    }
    // Accept cancel on KEY, CLICKED, or PRESSED
    bool is_cancel_event = false;
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        // ESP_LOGD(TAG, "<<< KEY event with key code: %d >>>", key);
        if (key == LV_KEY_ENTER || key == 18) {
            is_cancel_event = true;
        }
    } else if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        is_cancel_event = true;
    }

    if (is_cancel_event && !s_ctx.cancel_requested) {
        s_ctx.cancel_requested = true;
        // ESP_LOGD(TAG, "!!! CANCELLING TRANSFER: %s !!!", s_ctx.transfer_code);
        // Immediate visual feedback
        if (s_ctx.countdown_label) {
            lv_label_set_text(s_ctx.countdown_label, "Cancelling...");
        }
        // Stop polling timer immediately
        if (s_ctx.status_poll_timer) {
            lv_timer_delete(s_ctx.status_poll_timer);
            s_ctx.status_poll_timer = NULL;
        }
        // Set polling state to false so timer callback will not re-enter
        s_ctx.polling_active         = false;
        s_ctx.timer_callback_running = false;
        // Start cancel task in background (copy code to args to avoid races)
        cancel_task_args_t *args = pvPortMalloc(sizeof(cancel_task_args_t));
        if (args) {
            memset(args, 0, sizeof(*args));
            strncpy(args->code, s_ctx.transfer_code, sizeof(args->code) - 1);
            BaseType_t ok = xTaskCreate(cancel_task, "credit_cancel", 8192, args, 5, NULL);
            if (ok != pdPASS) {
                ESP_LOGE(TAG, "Failed to create cancel task");
                vPortFree(args);
                // Restore state so user can try again
                s_ctx.cancel_requested = false;
                s_ctx.polling_active   = true;
                if (!s_ctx.status_poll_timer) {
                    s_ctx.status_poll_timer = lv_timer_create(status_poll_timer_cb, 3000, NULL);
                    lv_timer_set_repeat_count(s_ctx.status_poll_timer, -1);
                    lv_timer_reset(s_ctx.status_poll_timer);
                }
            }
        } else {
            show_error_modal("Error", "Out of memory", false);
            s_ctx.cancel_requested = false;
            s_ctx.polling_active   = true;
            if (!s_ctx.status_poll_timer) {
                s_ctx.status_poll_timer = lv_timer_create(status_poll_timer_cb, 3000, NULL);
                lv_timer_set_repeat_count(s_ctx.status_poll_timer, -1);
                lv_timer_reset(s_ctx.status_poll_timer);
            }
        }
    }
}

static void show_transfer_pending_modal(const char *code, uint32_t amount, const char *expires_at) {
    if (s_ctx.state != CREDITS_STATE_IDLE) {
        ESP_LOGW(TAG, "Cannot show pending modal - not in idle state");
        return;
    }

    s_ctx.state            = CREDITS_STATE_TRANSFER_PENDING;
    s_ctx.cancel_requested = false;
    strncpy(s_ctx.transfer_code, code, sizeof(s_ctx.transfer_code) - 1);
    strncpy(s_ctx.expires_at, expires_at, sizeof(s_ctx.expires_at) - 1);

    // Save to NVS in case app is closed/restarted
    save_pending_transfer(code, amount, expires_at);

    // Overlay
    lv_obj_t *overlay     = lv_obj_create(lv_scr_act());
    s_ctx.pending_overlay = overlay;
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    // Navigation scope
    lv_indev_t *keypad      = input_get_device();
    lv_group_t *group       = lv_group_create();
    s_ctx.pending_nav_guard = nav_scope_push(keypad, group);
    s_ctx.pending_nav_ctx   = nav_ctx_create(keypad, group, overlay);
    nav_ctx_set_wrap(s_ctx.pending_nav_ctx, false, false);

    // Panel
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(panel, lv_color_hex(GRAY_SHADE_7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(panel, 8, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_set_width(title_row, lv_pct(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(title_row, lv_color_hex(BLUE_MAIN), LV_PART_MAIN);
    lv_obj_set_style_border_width(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_row, 10, LV_PART_MAIN);
    lv_obj_remove_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(title_row, 0);
    lv_obj_t *title_lbl = lv_label_create(title_row);
    lv_label_set_text(title_lbl, "Pending");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &white_rabbit_22, LV_PART_MAIN);

    // Body
    lv_obj_t *body = lv_obj_create(panel);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, 15, LV_PART_MAIN);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *instr = lv_label_create(body);
    lv_label_set_text(instr, "Share this code with recipient:");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(instr, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(instr, lv_pct(100));

    s_ctx.code_label = lv_label_create(body);
    lv_label_set_text(s_ctx.code_label, code);
    lv_obj_set_style_text_font(s_ctx.code_label, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ctx.code_label, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(s_ctx.code_label, 8, LV_PART_MAIN);

    static char amount_buf[32];
    snprintf(amount_buf, sizeof(amount_buf), "%" PRIu32 " credits", amount);
    lv_obj_t *amount_lbl = lv_label_create(body);
    lv_label_set_text(amount_lbl, amount_buf);
    lv_obj_set_style_text_font(amount_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(amount_lbl, lv_color_hex(WHITE), LV_PART_MAIN);

    // Start polling timer (3 seconds) - set state before creating UI that depends on it
    s_ctx.polling_active         = true;
    s_ctx.timer_callback_running = false;

    s_ctx.countdown_label = lv_label_create(body);
    update_countdown_display();
    lv_obj_set_style_text_font(s_ctx.countdown_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ctx.countdown_label, lv_color_hex(YELLOW_MAIN), LV_PART_MAIN);

    // Button row
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_set_style_pad_gap(btn_row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(btn_row, 15, LV_PART_MAIN);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cancel_btn = lv_button_create(btn_row);
    lv_obj_add_style(cancel_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(cancel_btn, &button_red, LV_PART_MAIN);
    lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(cancel_btn, pending_cancel_btn_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    nav_register(s_ctx.pending_nav_ctx, cancel_btn);

    lv_group_focus_obj(cancel_btn);

    // Create timer last after all UI is ready
    s_ctx.status_poll_timer = lv_timer_create(status_poll_timer_cb, 3000, NULL);
    lv_timer_set_repeat_count(s_ctx.status_poll_timer, -1); // Repeat indefinitely
    lv_timer_reset(s_ctx.status_poll_timer);                // Reset to ensure 3s delay before first fire

    // Start background poll task
    if (!s_ctx.poll_task_handle) {
        s_ctx.poll_task_running = true;
        s_ctx.poll_task_busy    = false;
        BaseType_t ok           = xTaskCreate(status_poll_task, "credit_poll", 8192, NULL, 5, &s_ctx.poll_task_handle);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Failed to create poll task");
            s_ctx.poll_task_handle  = NULL;
            s_ctx.poll_task_running = false;
        } else {
            s_ctx.poll_task_running = true;
        }
    }
}

static void cleanup_pending_modal(void) {
    if (s_ctx.status_poll_timer) {
        lv_timer_delete(s_ctx.status_poll_timer);
        s_ctx.status_poll_timer = NULL;
    }

    // Stop poll task
    if (s_ctx.poll_task_handle) {
        s_ctx.poll_task_running = false;
        // Wake up the task so it can exit
        xTaskNotifyGive(s_ctx.poll_task_handle);
        // Give the task a short time to exit
        vTaskDelay(pdMS_TO_TICKS(50));
        if (s_ctx.poll_task_handle) {
            // If still running, detach handle to avoid dangling pointer
            s_ctx.poll_task_handle = NULL;
        }
    }

    s_ctx.polling_active         = false;
    s_ctx.timer_callback_running = false;

    if (s_ctx.pending_nav_ctx) {
        nav_ctx_destroy(s_ctx.pending_nav_ctx);
        s_ctx.pending_nav_ctx = NULL;
    }

    lv_indev_t *keypad = input_get_device();
    nav_scope_pop(keypad, s_ctx.pending_nav_guard);
    memset(&s_ctx.pending_nav_guard, 0, sizeof(s_ctx.pending_nav_guard));

    if (s_ctx.pending_overlay) {
        lv_obj_del(s_ctx.pending_overlay);
        s_ctx.pending_overlay = NULL;
    }

    s_ctx.code_label       = NULL;
    s_ctx.countdown_label  = NULL;
    s_ctx.transfer_code[0] = '\0';
    s_ctx.expires_at[0]    = '\0';
    s_ctx.state            = CREDITS_STATE_IDLE;

    // Clear NVS when transfer is complete/expired/cancelled
    clear_pending_transfer();
}

// -------------------------------------------------------------------------------------------------
// Request Transfer Task
// -------------------------------------------------------------------------------------------------

typedef struct {
    uint32_t amount;
    lv_obj_t *spinner_overlay;
} request_transfer_args_t;

static void request_transfer_task(void *pv) {
    request_transfer_args_t *args = (request_transfer_args_t *)pv;
    if (!args) {
        vTaskDelete(NULL);
        return;
    }

    api_result_t *result = api_request_credit_swap(args->amount);

    if (!lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock in request_transfer_task");
        if (result) {
            api_free_result(result, true);
        }
        vPortFree(args);
        vTaskDelete(NULL);
        return;
    }

    // Close spinner
    if (args->spinner_overlay) {
        modal_async_close(args->spinner_overlay);
    }

    if (!result) {
        lvgl_unlock(__FILE__, __LINE__);
        vPortFree(args);
        show_error_modal("Network Error", "Failed to contact server", true);
        vTaskDelete(NULL);
        return;
    }

    if (!result->ok) {
        const char *msg = result->detail ? result->detail : "Failed to create transfer";
        lvgl_unlock(__FILE__, __LINE__);
        api_free_result(result, true);
        vPortFree(args);
        show_error_modal("Transfer Failed", msg, true);
        vTaskDelete(NULL);
        return;
    }

    api_credit_swap_response_t *swap_data = (api_credit_swap_response_t *)result->data;

    if (!swap_data || !swap_data->code || !swap_data->expires_at) {
        lvgl_unlock(__FILE__, __LINE__);
        api_free_result(result, true);
        vPortFree(args);
        show_error_modal("Invalid Response", "Server returned invalid data", false);
        vTaskDelete(NULL);
        return;
    }

    // Show pending modal
    s_ctx.state = CREDITS_STATE_IDLE; // Reset state before showing modal
    show_transfer_pending_modal(swap_data->code, args->amount, swap_data->expires_at);

    lvgl_unlock(__FILE__, __LINE__);
    api_free_result(result, true);

    vPortFree(args);
    vTaskDelete(NULL);
}

static void start_transfer_request(uint32_t amount) {
    request_transfer_args_t *args = (request_transfer_args_t *)pvPortMalloc(sizeof(request_transfer_args_t));
    if (!args) {
        show_error_modal("Error", "Out of memory", false);
        return;
    }

    args->amount = amount;

    // Show spinner
    if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        args->spinner_overlay = modal_async_open("Requesting transfer...");
        lvgl_unlock(__FILE__, __LINE__);
    }

    s_ctx.state = CREDITS_STATE_REQUESTING_TRANSFER;

    // Start task
    xTaskCreate(request_transfer_task, "credit_xfer", 1024 * 6, args, 5, NULL);
}

// -------------------------------------------------------------------------------------------------
// Amount Selection Modal
// -------------------------------------------------------------------------------------------------

static void update_amount_display(void) {
    if (!s_ctx.amount_label) {
        return;
    }

    static char buf[32];
    snprintf(buf, sizeof(buf), "%" PRIu32, s_ctx.selected_amount);
    lv_label_set_text(s_ctx.amount_label, buf);
    // Update button states
    if (s_ctx.plus_btn) {
        if (s_ctx.selected_amount >= (uint32_t)badge_config.credits) {
            lv_obj_add_state(s_ctx.plus_btn, LV_STATE_DISABLED);
            nav_focus(s_ctx.amount_nav_ctx, s_ctx.minus_btn);
        } else {
            lv_obj_remove_state(s_ctx.plus_btn, LV_STATE_DISABLED);
        }
    }

    if (s_ctx.minus_btn) {
        if (s_ctx.selected_amount <= 1) {
            lv_obj_add_state(s_ctx.minus_btn, LV_STATE_DISABLED);
            nav_focus(s_ctx.amount_nav_ctx, s_ctx.plus_btn);
        } else {
            lv_obj_remove_state(s_ctx.minus_btn, LV_STATE_DISABLED);
        }
    }
}

static void increment_amount(int32_t delta) {
    int32_t new_amount = (int32_t)s_ctx.selected_amount + delta;

    if (new_amount < 1) {
        new_amount = 1;
    }
    if (new_amount > badge_config.credits) {
        new_amount = badge_config.credits;
    }

    s_ctx.selected_amount = (uint32_t)new_amount;
    update_amount_display();
}

static void amount_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn        = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        int64_t now = esp_timer_get_time() / 1000;

        // Only process single clicks, not clicks after a hold
        bool was_hold = (s_ctx.hold_start_time != 0 && (now - s_ctx.hold_start_time) >= 150);

        if (!was_hold) {
            // This was a single click, always increment
            if (btn == s_ctx.plus_btn) {
                increment_amount(1);
            } else if (btn == s_ctx.minus_btn) {
                increment_amount(-1);
            }
            s_ctx.last_increment_time = now;
        }

        s_ctx.held_btn        = NULL;
        s_ctx.hold_start_time = 0;
    } else if (code == LV_EVENT_PRESSED) {
        // Start hold tracking
        s_ctx.held_btn        = btn;
        s_ctx.hold_start_time = esp_timer_get_time() / 1000;
        // Don't reset last_increment_time here - let first PRESSING event handle it
    } else if (code == LV_EVENT_PRESSING) {
        // Accelerate based on hold duration
        if (s_ctx.held_btn != btn) {
            return;
        }

        int64_t now           = esp_timer_get_time() / 1000;
        int64_t hold_duration = now - s_ctx.hold_start_time;

        // Initialize last_increment_time on first PRESSING event
        if (s_ctx.last_increment_time < s_ctx.hold_start_time) {
            s_ctx.last_increment_time = s_ctx.hold_start_time;
        }

        int64_t since_last = now - s_ctx.last_increment_time;

        // Rate limiting: minimum interval between increments
        int32_t min_interval = 150; // More conservative base rate
        int32_t delta        = 1;

        if (hold_duration > 2000) {
            delta        = 100;
            min_interval = 200; // Slower for large jumps
        } else if (hold_duration > 1000) {
            delta        = 10;
            min_interval = 150;
        }

        // Only increment if enough time has passed
        if (since_last >= min_interval) {
            if (btn == s_ctx.plus_btn) {
                increment_amount(delta);
            } else if (btn == s_ctx.minus_btn) {
                increment_amount(-delta);
            }
            s_ctx.last_increment_time = now;
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // Clear hold state
        if (s_ctx.held_btn == btn) {
            s_ctx.held_btn        = NULL;
            s_ctx.hold_start_time = 0;
            // Keep last_increment_time to prevent rapid re-clicks
        }
    }
}

static void amount_transfer_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    uint32_t amount = s_ctx.selected_amount;
    cleanup_amount_modal();
    start_transfer_request(amount);
}

static void amount_cancel_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    cleanup_amount_modal();
}

static void show_amount_selection_modal(void) {
    if (s_ctx.state != CREDITS_STATE_IDLE) {
        ESP_LOGW(TAG, "Cannot show amount modal - not in idle state");
        return;
    }

    s_ctx.state           = CREDITS_STATE_SELECTING_AMOUNT;
    s_ctx.selected_amount = (uint32_t)badge_config.credits; // Start at max

    // Overlay
    lv_obj_t *overlay    = lv_obj_create(lv_scr_act());
    s_ctx.amount_overlay = overlay;
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    // Navigation scope
    lv_indev_t *keypad     = input_get_device();
    lv_group_t *group      = lv_group_create();
    s_ctx.amount_nav_guard = nav_scope_push(keypad, group);
    s_ctx.amount_nav_ctx   = nav_ctx_create(keypad, group, overlay);
    nav_ctx_set_wrap(s_ctx.amount_nav_ctx, true, false);

    // Panel
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(panel, lv_color_hex(GRAY_SHADE_7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(panel, 8, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_set_width(title_row, lv_pct(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(title_row, lv_color_hex(BLUE_MAIN), LV_PART_MAIN);
    lv_obj_set_style_border_width(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_row, 10, LV_PART_MAIN);
    lv_obj_remove_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(title_row, 0);
    lv_obj_t *title_lbl = lv_label_create(title_row);
    lv_label_set_text(title_lbl, "Transfer");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &white_rabbit_22, LV_PART_MAIN);

    // Body
    lv_obj_t *body = lv_obj_create(panel);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, 10, LV_PART_MAIN);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *instr = lv_label_create(body);
    lv_label_set_text(instr, "Select amount to transfer:");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(instr, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(instr, lv_pct(100));

    // Plus button
    s_ctx.plus_btn = lv_button_create(body);
    lv_obj_add_style(s_ctx.plus_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(s_ctx.plus_btn, &button_blue, LV_PART_MAIN);
    // lv_obj_set_style_size(s_ctx.plus_btn, 30, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_ctx.plus_btn, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(s_ctx.plus_btn, amount_btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *plus_lbl = lv_label_create(s_ctx.plus_btn);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_16, LV_PART_MAIN);

    // Amount label
    s_ctx.amount_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_ctx.amount_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ctx.amount_label, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);
    update_amount_display();

    // Minus button
    s_ctx.minus_btn = lv_button_create(body);
    lv_obj_add_style(s_ctx.minus_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(s_ctx.minus_btn, &button_blue, LV_PART_MAIN);
    // lv_obj_set_style_size(s_ctx.minus_btn, 30, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_ctx.minus_btn, 5, LV_PART_MAIN);
    lv_obj_add_event_cb(s_ctx.minus_btn, amount_btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *minus_lbl = lv_label_create(s_ctx.minus_btn);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_16, LV_PART_MAIN);

    // Button row
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_set_style_pad_gap(btn_row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(btn_row, 15, LV_PART_MAIN);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    // Transfer button
    s_ctx.transfer_btn = lv_button_create(btn_row);
    lv_obj_add_style(s_ctx.transfer_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(s_ctx.transfer_btn, &button_green, LV_PART_MAIN);
    lv_obj_add_event_cb(s_ctx.transfer_btn, amount_transfer_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xfer_lbl = lv_label_create(s_ctx.transfer_btn);
    lv_label_set_text(xfer_lbl, "Transfer");

    // Cancel button
    s_ctx.cancel_btn = lv_button_create(btn_row);
    lv_obj_add_style(s_ctx.cancel_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(s_ctx.cancel_btn, &button_red, LV_PART_MAIN);
    lv_obj_add_event_cb(s_ctx.cancel_btn, amount_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lbl = lv_label_create(s_ctx.cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");

    // Register navigation
    nav_register(s_ctx.amount_nav_ctx, s_ctx.plus_btn);
    nav_register(s_ctx.amount_nav_ctx, s_ctx.minus_btn);
    nav_register(s_ctx.amount_nav_ctx, s_ctx.transfer_btn);
    nav_register(s_ctx.amount_nav_ctx, s_ctx.cancel_btn);

    // Set up vertical navigation: plus -> minus -> button_row
    // nav_autolink(s_ctx.amount_nav_ctx);
    // nav_bind_vertical(s_ctx.amount_nav_ctx, s_ctx.plus_btn, s_ctx.minus_btn, true);
    // nav_bind_vertical(s_ctx.amount_nav_ctx, s_ctx.minus_btn, btn_row, true);
    // nav_ctx_set_vert_fallback(s_ctx.amount_nav_ctx, false);

    lv_group_focus_obj(s_ctx.transfer_btn);
}

static void cleanup_amount_modal(void) {
    if (s_ctx.amount_nav_ctx) {
        nav_ctx_destroy(s_ctx.amount_nav_ctx);
        s_ctx.amount_nav_ctx = NULL;
    }

    lv_indev_t *keypad = input_get_device();
    nav_scope_pop(keypad, s_ctx.amount_nav_guard);
    memset(&s_ctx.amount_nav_guard, 0, sizeof(s_ctx.amount_nav_guard));

    if (s_ctx.amount_overlay) {
        lv_obj_del(s_ctx.amount_overlay);
        s_ctx.amount_overlay = NULL;
    }

    s_ctx.amount_label    = NULL;
    s_ctx.plus_btn        = NULL;
    s_ctx.minus_btn       = NULL;
    s_ctx.transfer_btn    = NULL;
    s_ctx.cancel_btn      = NULL;
    s_ctx.selected_amount = 0;
    s_ctx.held_btn        = NULL;
    s_ctx.hold_start_time = 0;
    s_ctx.state           = CREDITS_STATE_IDLE;
}

// -------------------------------------------------------------------------------------------------
// Error Modal with Retry
// -------------------------------------------------------------------------------------------------

typedef struct {
    char title[64];
    char message[128];
    bool can_retry;
} error_modal_ctx_t;

static void error_modal_cb(uint8_t btn_idx, void *user_data) {
    error_modal_ctx_t *ctx = (error_modal_ctx_t *)user_data;

    if (ctx && ctx->can_retry && btn_idx == 0) {
        // Retry - show amount selection again
        if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
            show_amount_selection_modal();
            lvgl_unlock(__FILE__, __LINE__);
        }
    }

    if (ctx) {
        vPortFree(ctx);
    }
}

static void show_error_modal(const char *title, const char *message, bool allow_retry) {
    error_modal_ctx_t *ctx = (error_modal_ctx_t *)pvPortMalloc(sizeof(error_modal_ctx_t));
    if (!ctx) {
        return;
    }

    strncpy(ctx->title, title, sizeof(ctx->title) - 1);
    strncpy(ctx->message, message, sizeof(ctx->message) - 1);
    ctx->can_retry = allow_retry;

    s_ctx.state = CREDITS_STATE_IDLE;

    if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        modal_message_config_t mc = {
            .title     = ctx->title,
            .message   = ctx->message,
            .buttons   = {allow_retry ? "Retry" : "OK", allow_retry ? "Cancel" : NULL},
            .success   = false,
            .callback  = error_modal_cb,
            .user_data = ctx,
        };
        modal_message_open(&mc);
        lvgl_unlock(__FILE__, __LINE__);
    } else {
        vPortFree(ctx);
    }
}

// -------------------------------------------------------------------------------------------------
// Main Credits Screen
// -------------------------------------------------------------------------------------------------

static void transfer_btn_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (badge_config.credits <= 0) {
        show_error_modal("No Credits", "You have no credits to transfer", false);
        return;
    }

    show_amount_selection_modal();
}

static lv_obj_t *s_transfer_btn = NULL;

void credits_app_create(lv_obj_t *parent) {
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = CREDITS_STATE_IDLE;

    content_register_nav_callback(APP_CREDITS, credits_nav_setup, NULL);

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(95), lv_pct(95));
    lv_obj_center(container);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(container, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, "YOUR CREDITS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(WHITE), LV_PART_MAIN);

    // Credits value
    s_ctx.credits_value_label = lv_label_create(container);
    static char credits_buf[32];
    snprintf(credits_buf, sizeof(credits_buf), "%d", badge_config.credits);
    lv_label_set_text(s_ctx.credits_value_label, credits_buf);
    lv_obj_set_style_text_font(s_ctx.credits_value_label, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ctx.credits_value_label, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);

    // Transfer button
    s_transfer_btn = lv_button_create(container);
    lv_obj_add_style(s_transfer_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(s_transfer_btn, &button_blue, LV_PART_MAIN);
    lv_obj_add_event_cb(s_transfer_btn, transfer_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(s_transfer_btn);
    lv_label_set_text(btn_lbl, "Transfer");

    // Check for pending transfer from previous session
    char pending_code[6]     = {0};
    uint32_t pending_amount  = 0;
    char pending_expires[32] = {0};

    if (load_pending_transfer(pending_code, &pending_amount, pending_expires)) {
        // Check if it's expired
        int32_t remaining = seconds_until_expiry(pending_expires);
        if (remaining > 0) {
            ESP_LOGI(TAG, "Restoring pending transfer from previous session: %s", pending_code);
            show_transfer_pending_modal(pending_code, pending_amount, pending_expires);
        } else {
            ESP_LOGI(TAG, "Pending transfer expired, clearing: %s", pending_code);
            clear_pending_transfer();
        }
    }
}

static void credits_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data) {
    if (!s_transfer_btn) {
        ESP_LOGE(TAG, "Transfer button not initialized");
        return;
    }

    nav_register(nav_ctx, s_transfer_btn);
    nav_focus(nav_ctx, s_transfer_btn);
}