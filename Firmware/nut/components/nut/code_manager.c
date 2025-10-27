#include "nut/code_manager.h"
#include "config.h"
#include "types.h"
#include "api.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "led_patterns.h"

static const char *TAG = "code_manager";

#define NVS_NAMESPACE   "code_mgr"
#define NVS_KEY_CODE    "code"
#define NVS_KEY_TYPE    "code_type"
#define NVS_KEY_EXPIRES "expires_at"

#define CODE_REQUEST_RETRY_INTERVAL_MS (30 * 1000)
#define CODE_REQUEST_MAX_RETRIES       10

extern int code_level;

typedef struct {
    char code[6];
    code_type_t code_type;
    time_t expires_at;
} stored_code_t;

static code_mgr_state_t state         = CODE_MGR_STATE_IDLE;
static stored_code_t current_code     = {0};
static bool has_valid_code            = false;
static bool ready_state_shown         = false;
static char nut_type_record[32]       = {0};
static esp_timer_handle_t retry_timer = NULL;
static int retry_count                = 0;
static TaskHandle_t code_worker_task  = NULL;

// Background poller state for written codes
static char last_written_code[8]       = {0};
static TaskHandle_t status_poller_task = NULL;

// Helper function to log stack usage
static void log_stack_usage(const char *location) {
    TaskHandle_t task           = xTaskGetCurrentTaskHandle();
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(task);

    ESP_LOGI(TAG, "[STACK] %s - High water mark: %u bytes free (min remaining)", location, high_water_mark * sizeof(StackType_t));
}

static void show_ready_state() {
    if (ready_state_shown) {
        return;
    }
    ready_state_shown = true;

    led_pattern_t ready = {
        .type        = LED_PATTERN_BLINK,
        .red         = 0,
        .green       = 255,
        .blue        = 0,
        .on_ms       = 1000,
        .off_ms      = 1000,
        .blink_count = 1,
        .fade        = true,
    };
    led_pattern_set(&ready);
}

static esp_err_t save_code_to_nvs(const stored_code_t *code) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, NVS_KEY_CODE, code->code);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_TYPE, (uint8_t)code->code_type);
    }
    if (err == ESP_OK) {
        err = nvs_set_i64(handle, NVS_KEY_EXPIRES, (int64_t)code->expires_at);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save code to NVS: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t load_code_from_nvs(stored_code_t *code) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t code_len = sizeof(code->code);
    err             = nvs_get_str(handle, NVS_KEY_CODE, code->code, &code_len);
    if (err == ESP_OK) {
        uint8_t type;
        err = nvs_get_u8(handle, NVS_KEY_TYPE, &type);
        if (err == ESP_OK) {
            code->code_type = (code_type_t)type;
            int64_t expires;
            err = nvs_get_i64(handle, NVS_KEY_EXPIRES, &expires);
            if (err == ESP_OK) {
                code->expires_at = (time_t)expires;
            }
        }
    }

    nvs_close(handle);
    return err;
}

static void clear_code_from_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, NVS_KEY_CODE);
        nvs_erase_key(handle, NVS_KEY_TYPE);
        nvs_erase_key(handle, NVS_KEY_EXPIRES);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static bool validate_stored_code(const stored_code_t *code) {
    // If SNTP hasn't synced yet, we can't check expiration but should trust the stored code
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGW(TAG, "Cannot validate code expiration: SNTP not synced yet - trusting stored code");
        // Still check API status if we have network connectivity
        api_result_t *result = api_get_code_status(code->code);
        if (!result || !result->ok) {
            ESP_LOGW(TAG, "Failed to check code status (but SNTP not synced, trusting stored code)");
            if (result) {
                api_free_result(result, true);
            }
            return true; // Trust the code since we can't verify time
        }

        api_code_status_response_t *status = (api_code_status_response_t *)result->data;
        // A code is valid if it's either PENDING or VALID, and not redeemed or invalidated
        bool is_valid = ((status->status == CODE_STATUS_PENDING || status->status == CODE_STATUS_VALID) && !status->redeemed &&
                         !status->invalidated);

        if (!is_valid) {
            ESP_LOGI(TAG, "Stored code is no longer valid per API (status=%d, redeemed=%d, invalidated=%d)", status->status,
                     status->redeemed, status->invalidated);
        } else {
            ESP_LOGI(TAG, "Code valid per API (time expiration not checked - SNTP not synced)");
        }

        api_free_result(result, true);
        return is_valid;
    }

    // Get current time in UTC (time() should always return UTC regardless of TZ setting)
    time_t now = time(NULL);

    // For debugging: also get local time representation
    struct tm timeinfo_local;
    localtime_r(&now, &timeinfo_local);
    struct tm timeinfo_utc;
    gmtime_r(&now, &timeinfo_utc);

    ESP_LOGD(TAG, "Current time: UTC=%04d-%02d-%02d %02d:%02d:%02d, Local=%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo_utc.tm_year + 1900, timeinfo_utc.tm_mon + 1, timeinfo_utc.tm_mday, timeinfo_utc.tm_hour,
             timeinfo_utc.tm_min, timeinfo_utc.tm_sec, timeinfo_local.tm_year + 1900, timeinfo_local.tm_mon + 1,
             timeinfo_local.tm_mday, timeinfo_local.tm_hour, timeinfo_local.tm_min, timeinfo_local.tm_sec);

    long time_until_expiry = (long)(code->expires_at - now);

    ESP_LOGD(TAG, "Validating code: now=%ld, expires_at=%ld, time_until_expiry=%ld seconds", (long)now, (long)code->expires_at,
             time_until_expiry);

    if (now >= code->expires_at) {
        ESP_LOGW(TAG, "Stored code expired (expired %ld seconds ago)", -time_until_expiry);
        return false;
    }

    api_result_t *result = api_get_code_status(code->code);
    if (!result || !result->ok) {
        ESP_LOGW(TAG, "Failed to check code status");
        if (result) {
            api_free_result(result, true);
        }
        return false;
    }

    api_code_status_response_t *status = (api_code_status_response_t *)result->data;
    // A code is valid if it's either PENDING or VALID, and not redeemed or invalidated
    bool is_valid = ((status->status == CODE_STATUS_PENDING || status->status == CODE_STATUS_VALID) && !status->redeemed &&
                     !status->invalidated);

    if (!is_valid) {
        ESP_LOGI(TAG, "Stored code is no longer valid (status=%d, redeemed=%d, invalidated=%d)", status->status, status->redeemed,
                 status->invalidated);
    }

    api_free_result(result, true);
    return is_valid;
}

static void code_worker_task_fn(void *pv) {
    (void)pv;
    const TickType_t initial_backoff = pdMS_TO_TICKS(1000);
    const TickType_t max_backoff     = pdMS_TO_TICKS(60000);

    while (1) {
        // Wait until someone notifies us to attempt getting a code
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        TickType_t backoff = initial_backoff;

        // Keep trying until we get a valid code or until a write is in progress
        while (!has_valid_code && state != CODE_MGR_STATE_WRITING) {
            if (nut_config.type == NUT_TYPE_UNKNOWN) {
                ESP_LOGW(TAG, "Code worker: nut type unknown, waiting before retrying");
                // Wait a bit and loop to check again (or wait for another notify)
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            ESP_LOGI(TAG, "Code worker: attempting request_new_code...");
            esp_err_t res = request_new_code();
            if (res == ESP_OK) {
                state = CODE_MGR_STATE_READY;
                show_ready_state();
                // Stop retry timer if running
                if (retry_timer) {
                    esp_timer_stop(retry_timer);
                }
                break;
            }

            ESP_LOGW(TAG, "Code worker: request failed (err=%d), retrying in %lu ms", res,
                     (unsigned long)(backoff * portTICK_PERIOD_MS));
            vTaskDelay(backoff);
            // Exponential backoff
            backoff = (backoff < max_backoff) ? (backoff * 2) : max_backoff;
        }
    }
}

// Ensure the persistent worker exists and notify it to start/continue fetching a code.
// Returns pdPASS on success, pdFAIL on failure to create/notify.
static BaseType_t request_code_if_needed(const char *reason) {
    if (code_worker_task == NULL) {
        BaseType_t r = xTaskCreate(code_worker_task_fn, "code_worker", 8192, NULL, 5, &code_worker_task);
        if (r != pdPASS) {
            ESP_LOGE(TAG, "Failed to create code worker task (%s)", reason ? reason : "unspecified");
            code_worker_task = NULL;
            return pdFAIL;
        }
        ESP_LOGI(TAG, "Created persistent code worker task (%s)", reason ? reason : "unspecified");
    }

    // Notify the worker to perform work
    xTaskNotifyGive(code_worker_task);
    ESP_LOGI(TAG, "Notified code worker to request code (%s)", reason ? reason : "unspecified");
    return pdPASS;
}

esp_err_t request_new_code(void) {
    log_stack_usage("request_new_code - START");

    // Log current task for diagnostics
    TaskHandle_t cur  = xTaskGetCurrentTaskHandle();
    const char *tname = pcTaskGetName(cur);
    ESP_LOGD(TAG, "request_new_code running in task: %s", tname ? tname : "(null)");

    // If the nut type hasn't been detected, don't attempt to request codes.
    if (nut_config.type == NUT_TYPE_UNKNOWN) {
        ESP_LOGW(TAG, "Nut type unknown - skipping code request");
        return ESP_ERR_INVALID_STATE;
    }

    // If we're running inside the esp_timer task, avoid performing blocking network
    // operations here. Instead spawn a FreeRTOS task to do the work.
    if (tname && strcmp(tname, "esp_timer") == 0) {
        ESP_LOGW(TAG, "request_new_code called from esp_timer task - deferring to background task");
        BaseType_t r = request_code_if_needed("esp_timer");
        if (r != pdPASS) {
            ESP_LOGE(TAG, "Failed to create background code request task from esp_timer");
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    // int level = 1; // (nut_config.type == NUT_TYPE_COMMUNITY) ? 1 : 0;

    ESP_LOGI(TAG, "Requesting code (level=%d, type=%s)...", code_level, get_nut_type_short_str(nut_config.type));

    api_result_t *result = api_request_code(code_level);

    log_stack_usage("request_new_code - AFTER API call");

    if (!result) {
        ESP_LOGE(TAG, "api_request_code returned NULL");
        return ESP_FAIL;
    }

    if (!result->ok) {
        ESP_LOGE(TAG, "api_request_code failed: ok=false, detail=%s", result->detail ? result->detail : "null");
        api_free_result(result, true);
        return ESP_FAIL;
    }

    if (!result->data) {
        ESP_LOGE(TAG, "api_request_code succeeded but data is NULL");
        api_free_result(result, true);
        return ESP_FAIL;
    }

    api_request_code_response_t *code_data = (api_request_code_response_t *)result->data;

    if (!code_data->code || strlen(code_data->code) == 0) {
        ESP_LOGE(TAG, "Received invalid code (null or empty)");
        api_free_result(result, true);
        return ESP_FAIL;
    }

    strncpy(current_code.code, code_data->code, sizeof(current_code.code) - 1);
    current_code.code[sizeof(current_code.code) - 1] = '\0';
    current_code.code_type                           = code_data->code_type;
    current_code.expires_at                          = code_data->expires_at;

    has_valid_code = true;

    esp_err_t err = save_code_to_nvs(&current_code);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save code to NVS, continuing anyway");
    }

    ESP_LOGI(TAG, "New code ready: %s (type=%s, expires in %ld seconds)", current_code.code,
             get_code_type_str(current_code.code_type), (long)(current_code.expires_at - time(NULL)));

    api_free_result(result, true);
    retry_count = 0;

    log_stack_usage("request_new_code - END (success)");

    return ESP_OK;
}

// #ifdef __cplusplus
//  extern "C" {
// C symbol for openDoor() in main.cpp
extern void openDoor();
//  }
//  #endif

static void status_poller_task_fn(void *pv) {
    (void)pv;
    const TickType_t short_delay = pdMS_TO_TICKS(5000);  // 5s
    const TickType_t long_delay  = pdMS_TO_TICKS(30000); // 30s

    char polled_code[8] = {0};
    while (1) {
        // Wait for a notification to poll immediately, or timeout to poll periodically
        if (ulTaskNotifyTake(pdTRUE, short_delay) > 0) {
            // immediate poll triggered
        }

        // copy current last_written_code atomically
        vTaskSuspendAll();
        strncpy(polled_code, last_written_code, sizeof(polled_code) - 1);
        xTaskResumeAll();

        if (polled_code[0] == '\0') {
            // nothing to do, sleep longer
            vTaskDelay(long_delay);
            continue;
        }

        api_result_t *res = api_get_code_status(polled_code);
        if (!res || !res->ok) {
            if (res) {
                api_free_result(res, true);
            }
            // try again later
            vTaskDelay(short_delay);
            continue;
        }

        api_code_status_response_t *status = (api_code_status_response_t *)res->data;
        if (status) {
            // If the code was invalidated or explicitly failed/expired, stop polling and clear it.
            if (status->invalidated || status->status == CODE_STATUS_FAILED || status->status == CODE_STATUS_EXPIRED) {
                ESP_LOGW(TAG, "Code %s is terminal (invalidated/failed/expired) - stopping poll", polled_code);
                if (status->failure_reason) {
                    ESP_LOGW(TAG, "Invalidation reason: %s", status->failure_reason ? status->failure_reason : "(none)");
                }
                // Clear last_written_code to stop polling until a new write
                vTaskSuspendAll();
                last_written_code[0] = '\0';
                xTaskResumeAll();

                // After invalidation, attempt to fetch a replacement code (mirror redemption behavior)
                if (nut_config.type != NUT_TYPE_UNKNOWN) {
                    if (!has_valid_code && state != CODE_MGR_STATE_WRITING) {
                        BaseType_t r = request_code_if_needed("invalidation");
                        if (r != pdPASS) {
                            ESP_LOGE(TAG, "Failed to create replacement code request task after invalidation");
                        }
                    } else {
                        ESP_LOGI(TAG, "No replacement request needed after invalidation (state=%d, has_valid_code=%d)", state,
                                 has_valid_code);
                    }
                } else {
                    ESP_LOGW(TAG, "Nut type unknown - skipping replacement code request after invalidation");
                }

                api_free_result(res, true);
                // Sleep longer after terminal condition
                vTaskDelay(long_delay);
                continue;
            }

            if (status->redeemed) {
                ESP_LOGI(TAG, "Code %s redeemed - triggering door open", polled_code);
                // Call openDoor in main
                openDoor();

                // Clear last_written_code to stop polling until a new write
                vTaskSuspendAll();
                last_written_code[0] = '\0';
                xTaskResumeAll();

                // After redemption, ensure we fetch a replacement code if one is not already being
                // requested. If the write path already started a replacement request this will be
                // a no-op; otherwise spawn a background request task so the node returns to READY.
                if (nut_config.type != NUT_TYPE_UNKNOWN) {
                    if (!has_valid_code && state != CODE_MGR_STATE_WRITING) {
                        BaseType_t r = request_code_if_needed("redemption");
                        if (r != pdPASS) {
                            ESP_LOGE(TAG, "Failed to create replacement code request task after redemption");
                        }
                    } else {
                        ESP_LOGI(TAG, "No replacement request needed after redemption (state=%d, has_valid_code=%d)", state,
                                 has_valid_code);
                    }
                } else {
                    ESP_LOGW(TAG, "Nut type unknown - skipping replacement code request after redemption");
                }

                api_free_result(res, true);
                // After successful action, sleep longer
                vTaskDelay(long_delay);
                continue;
            }

            api_free_result(res, true);
            // not redeemed yet; retry after short delay
            vTaskDelay(short_delay);
        }
    }
}

static void retry_timer_callback(void *arg) {
    if (has_valid_code || state != CODE_MGR_STATE_IDLE) {
        if (retry_timer) {
            esp_timer_stop(retry_timer);
        }
        return;
    }

    retry_count++;
    ESP_LOGW(TAG, "Retrying code request (attempt %d/%d)...", retry_count, CODE_REQUEST_MAX_RETRIES);

    if (retry_count > CODE_REQUEST_MAX_RETRIES) {
        ESP_LOGE(TAG, "Max retry attempts reached, giving up");
        if (retry_timer) {
            esp_timer_stop(retry_timer);
        }
        return;
    }

    if (nut_config.type == NUT_TYPE_UNKNOWN) {
        ESP_LOGW(TAG, "Nut type unknown - skipping scheduled code request");
        return;
    }

    BaseType_t r = request_code_if_needed("timer_callback");
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create code request task from timer callback");
    }
}

esp_err_t code_mgr_init(void) {
    memset(&current_code, 0, sizeof(current_code));
    has_valid_code = false;
    state          = CODE_MGR_STATE_IDLE;

    snprintf(nut_type_record, sizeof(nut_type_record), "NT:%s", get_nut_type_short_str(nut_config.type));

    stored_code_t nvs_code = {0};
    if (load_code_from_nvs(&nvs_code) == ESP_OK) {
        ESP_LOGI(TAG, "Found stored code in NVS: %s", nvs_code.code);
        memcpy(&current_code, &nvs_code, sizeof(current_code));
        has_valid_code = true;
        ESP_LOGI(TAG, "Loaded code from NVS (will validate after SNTP sync)");
    }

    esp_timer_create_args_t timer_args = {
        .callback = retry_timer_callback, .arg = NULL, .dispatch_method = ESP_TIMER_TASK, .name = "code_retry"};

    esp_err_t err = esp_timer_create(&timer_args, &retry_timer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create retry timer: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

void code_mgr_on_nut_ready(void) {
    if (state != CODE_MGR_STATE_IDLE) {
        return;
    }

    if (has_valid_code) {
        ESP_LOGI(TAG, "Validating stored code...");
        if (validate_stored_code(&current_code)) {
            ESP_LOGI(TAG, "Stored code is still valid");
            state = CODE_MGR_STATE_READY;
            show_ready_state();
            return;
        } else {
            ESP_LOGI(TAG, "Stored code no longer valid, clearing");
            has_valid_code = false;
            clear_code_from_nvs();
        }
    }

    ESP_LOGI(TAG, "Requesting new code...");
    if (nut_config.type == NUT_TYPE_UNKNOWN) {
        ESP_LOGW(TAG, "Nut type unknown - postponing initial code request");
        // Start the retry timer to attempt later when type is known
        retry_count = 1;
        if (retry_timer) {
            esp_timer_start_periodic(retry_timer, CODE_REQUEST_RETRY_INTERVAL_MS * 1000);
        }
    } else if (request_new_code() == ESP_OK) {
        state = CODE_MGR_STATE_READY;
        show_ready_state();
    } else {
        ESP_LOGW(TAG, "Initial code request failed, will retry every %d seconds", CODE_REQUEST_RETRY_INTERVAL_MS / 1000);
        retry_count = 1;
        if (retry_timer) {
            esp_timer_start_periodic(retry_timer, CODE_REQUEST_RETRY_INTERVAL_MS * 1000);
        }
    }
}

bool code_mgr_is_ready(void) {
    return (state == CODE_MGR_STATE_READY && has_valid_code);
}

esp_err_t code_mgr_write_to_nfc(void) {
    if (!code_mgr_is_ready()) {
        ESP_LOGW(TAG, "Code manager not ready for NFC write");
        return ESP_ERR_INVALID_STATE;
    }

    state          = CODE_MGR_STATE_WRITING;
    has_valid_code = false;
    clear_code_from_nvs();

    state = CODE_MGR_STATE_IDLE;

    // Spawn a background task to request a new code, instead of calling directly
    BaseType_t r = request_code_if_needed("write_to_nfc");
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create code request task from code_mgr_write_to_nfc");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

// Called when a code has been written to a badge. Accept the written code so we
// can poll its status in the background and trigger the door when redeemed.
void code_mgr_mark_code_written(const char *written_code) {
    if (!written_code || strlen(written_code) == 0) {
        ESP_LOGW(TAG, "code_mgr_mark_code_written called with empty code");
        return;
    }

    ESP_LOGI(TAG, "Code written: %s", written_code);

    // Store the last written code for the poller
    vTaskSuspendAll();
    strncpy(last_written_code, written_code, sizeof(last_written_code) - 1);
    last_written_code[sizeof(last_written_code) - 1] = '\0';
    xTaskResumeAll();

    // Ensure the status poller task is running
    if (status_poller_task == NULL) {
        BaseType_t r = xTaskCreate(status_poller_task_fn, "code_status", 8192, NULL, 5, &status_poller_task);
        if (r != pdPASS) {
            ESP_LOGE(TAG, "Failed to create status poller task");
            status_poller_task = NULL;
        }
    }

    // Notify the poller to check immediately
    if (status_poller_task != NULL) {
        xTaskNotifyGive(status_poller_task);
    }

    // Mark code manager to fetch a replacement code
    if (state == CODE_MGR_STATE_READY) {
        has_valid_code = false;
        state          = CODE_MGR_STATE_IDLE;
        memset(&current_code, 0, sizeof(current_code));
        clear_code_from_nvs();
        // spawn a background request for a new code if nut type is known
        if (nut_config.type != NUT_TYPE_UNKNOWN) {
            BaseType_t r = request_code_if_needed("mark_code_written");
            if (r != pdPASS) {
                ESP_LOGE(TAG, "Failed to create code request task from mark_code_written");
            }
        } else {
            ESP_LOGW(TAG, "Nut type unknown - skipping immediate replacement code request");
        }
    }
}

const char *code_mgr_get_current_code(void) {
    return has_valid_code ? current_code.code : NULL;
}

const char *code_mgr_get_nut_type_record(void) {
    return nut_type_record;
}

void code_mgr_update_nut_type_record(void) {
    snprintf(nut_type_record, sizeof(nut_type_record), "NT:%s", get_nut_type_short_str(nut_config.type));
    ESP_LOGI(TAG, "Updated nut type record to: %s", nut_type_record);
}
