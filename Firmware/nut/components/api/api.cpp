#include <algorithm>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include "api.h"
#include "api_client.h"
#include "esp_log.h"
#include "esp_timer.h"

constexpr static const char *TAG = "api";

// Alias for helpers to make less verbose
#define bytes_hex ApiClient::ApiHelpers::bytes_hex
#define hex_bytes ApiClient::ApiHelpers::hex_bytes
#define json_get  ApiClient::ApiHelpers::json_get

// Global instance of ApiClient
const static std::unique_ptr<ApiClient> apiClient = std::make_unique<ApiClient>();

// Function prototypes
char *getstr(const json &json, const char *key);
void nullsafe_free(void *ptr);
std::string get_header_nocase(const std::map<std::string, std::string> &headers, const std::string &key);

extern "C" void api_free_result_data(void *data, api_result_type_t type) {
    if (data == nullptr) {
        return;
    }

    switch (type) {
        case api_result_type_t::API_FIRMWARE_DATA: {
            auto firmware_data = (api_firmware_data_t *)data;
            nullsafe_free(firmware_data->setting);
            nullsafe_free(firmware_data->value);
            break;
        }
        case api_result_type_t::API_FIRMWARE_HEAD: {
            auto firmware_head_data = (api_firmware_head_t *)data;
            nullsafe_free(firmware_head_data->content_type);
            nullsafe_free(firmware_head_data->firmware_version);
            nullsafe_free(firmware_head_data->last_modified);
            break;
        }
        case api_result_type_t::API_STATUS: {
            auto status_data = (api_status_response_t *)data;
            nullsafe_free(status_data->nut_id);
            nullsafe_free(status_data->firmware_info.current_version);
            nullsafe_free(status_data->firmware_info.latest_version);
            break;
        }
        case api_result_type_t::API_REQUEST_CODE: {
            auto code_data = (api_request_code_response_t *)data;
            nullsafe_free(code_data->code);
            break;
        }
        case api_result_type_t::API_CODE_STATUS: {
            auto code_status_data = (api_code_status_response_t *)data;
            nullsafe_free(code_status_data->code);
            nullsafe_free(code_status_data->redeemed_by_badge_id);
            nullsafe_free(code_status_data->invalidated_reason);
            nullsafe_free(code_status_data->nut_id);
            nullsafe_free(code_status_data->issued_by_nut_id);
            nullsafe_free(code_status_data->unlocked_by_badge_id);
            nullsafe_free(code_status_data->failure_reason);
            nullsafe_free(code_status_data->attempted_by_badge_id);
            break;
        }
        default: //
            break;
    }

    nullsafe_free(data);
}

extern "C" void api_free_result(api_result_t *result, bool free_data) {
    if (result == nullptr) {
        return;
    }

    nullsafe_free(result->detail);

    if (result->data != nullptr && free_data) {
        api_free_result_data(result->data, result->type);
    }

    nullsafe_free(result);
}

api_result_t *base_result(const ApiClient::ApiResult &api_result) {
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred");
        return nullptr;
    }

    const auto &response = std::get<ApiClient::ApiResponse>(api_result);

    // All valid responses should have a body with a JSON object or array
    if (response.body.empty()) {
        return nullptr;
    }
    // ESP_LOGD(TAG, "Response Body: %s", response.body.c_str());

    // Check if the response body is valid JSON
    if (!json::accept(response.body)) {
        ESP_LOGE(TAG, "Invalid JSON response: %s", response.body.c_str());
        return nullptr;
    }

    // Parse the response body as JSON
    json response_json = response.body_json();
    if (response_json.is_null() || !response_json.is_object()) {
        ESP_LOGE(TAG, "JSON is null or not an object");
        if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
            ESP_LOGE(TAG, "Response Body: %s", response.body.c_str());
        }
        return nullptr;
    }
    // ESP_LOGD(TAG, "Response JSON: %s", response_json.dump().c_str());

    // Create a new result struct to return
    auto result = (api_result_t *)malloc(sizeof(api_result_t));
    if (result == nullptr) {
        return nullptr;
    }

    // Populate the basic fields
    result->type = api_result_type_t::API_BASE;
    auto status  = json_get<std::string>(response_json, "status", "");
    result->ok   = status == "ok" ? true : status == "error" ? false : response.status_code >= 200 && response.status_code < 300;
    result->detail = nullptr;
    result->data   = nullptr;

    // Get the detail field
    auto detail_msg = json_get<std::string>(response_json, "detail.0.msg", json_get<std::string>(response_json, "detail", ""));
    if (!detail_msg.empty()) {
        result->detail = strdup(detail_msg.c_str());
    }

    return result;
}

char *getstr(const json &json, const char *key) {
    if (json.contains(key) && json[key] != nullptr) {
        return strdup(json[key].get<std::string>().c_str());
    }
    return nullptr;
}

void nullsafe_free(void *ptr) {
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
}

std::string get_header_nocase(const std::map<std::string, std::string> &headers, const std::string &key) {
    for (const auto &header : headers) {
        std::string header_key_lower = header.first;
        std::string key_lower        = key;
        std::transform(header_key_lower.begin(), header_key_lower.end(), header_key_lower.begin(), ::tolower);
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

        if (header_key_lower == key_lower) {
            return header.second;
        }
    }
    return "";
}

extern "C" api_result_t *api_provision_secrets() {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->provisionSecrets();
    auto result     = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    result->data = malloc(sizeof(api_provision_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_PROVISION;

    // Copy the provision response data
    auto *provision_data        = static_cast<api_provision_response_t *>(result->data);
    std::string hmac_secret_hex = result_json["hmac_secret"];
    std::string hash_prefix_hex = result_json["hash_prefix"];

    if (!hex_bytes(hmac_secret_hex.c_str(), provision_data->hmac_secret, sizeof(provision_data->hmac_secret)) ||
        !hex_bytes(hash_prefix_hex.c_str(), provision_data->hash_prefix, sizeof(provision_data->hash_prefix))) {
        api_free_result(result, true);
        return nullptr;
    }

    return result;
}

extern "C" api_result_t *api_provision_wireless_psk() {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->provisionWirelessPSK();
    auto result     = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    result->data = malloc(sizeof(api_provision_psk_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_PROVISION_PSK;

    // Copy the provision PSK into the response
    auto *provision_psk_data = static_cast<api_provision_psk_response_t *>(result->data);
    std::string psk          = result_json["psk"];
    snprintf(provision_psk_data->psk, sizeof(provision_psk_data->psk), "%s", psk.c_str());

    return result;
}

extern "C" api_result_t *api_provision_verify() {
    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->provisionVerify());
}

extern "C" api_result_t *api_get_status() {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->getStatus();
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred");
        return nullptr;
    }

    auto result = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    result->data = malloc(sizeof(api_status_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_STATUS;

    // Copy the status response data
    auto *status_data                           = static_cast<api_status_response_t *>(result->data);
    status_data->nut_id                         = getstr(result_json, "nut_id");
    status_data->nut_type                       = json_get<nut_type_t>(result_json, "nut_type", NUT_TYPE_UNKNOWN);
    status_data->enabled                        = json_get<bool>(result_json, "enabled", false);
    status_data->provisioned                    = json_get<bool>(result_json, "provisioned", false);
    status_data->provisioned_at                 = json_get<time_t>(result_json, "provisioned_at", API_TIMESTAMP_NULL);
    status_data->last_seen                      = json_get<time_t>(result_json, "last_seen", API_TIMESTAMP_NULL);
    status_data->firmware_info.current_version  = getstr(result_json["firmware_info"], "current_version");
    status_data->firmware_info.latest_version   = getstr(result_json["firmware_info"], "latest_version");
    status_data->firmware_info.update_available = json_get<bool>(result_json["firmware_info"], "update_available", false);
    status_data->vendor_id                      = json_get<int16_t>(result_json, "vendor_id", -1);
    status_data->community_id                   = json_get<int16_t>(result_json, "community_id", -1);
    status_data->contest_id                     = json_get<int16_t>(result_json, "contest_id", -1);
    status_data->event_id                       = json_get<int16_t>(result_json, "event_id", -1);
    status_data->garbage_id                     = json_get<int16_t>(result_json, "garbage_id", -1);
    status_data->system_id                      = json_get<int16_t>(result_json, "system_id", -1);
    status_data->node_id                        = json_get<int16_t>(result_json, "node_id", -1);

    return result;
}

extern "C" api_result_t *api_request_code(int level) {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->requestCode(level);
    auto result     = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    result->data = malloc(sizeof(api_request_code_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_REQUEST_CODE;

    // Copy the request code response data
    auto *code_data       = static_cast<api_request_code_response_t *>(result->data);
    code_data->code       = getstr(result_json, "code");
    code_data->code_type  = json_get<code_type_t>(result_json, "code_type", CODE_TYPE_UNKNOWN);
    code_data->expires_at = json_get<time_t>(result_json, "expires_at", API_TIMESTAMP_NULL);

    return result;
}

extern "C" api_result_t *api_get_code_status(const char *code) {
    if (apiClient == nullptr) {
        return nullptr;
    }
    if (code == nullptr || strlen(code) == 0) {
        ESP_LOGE(TAG, "Code is null or empty");
        return nullptr;
    }

    auto api_result = apiClient->getCodeStatus(code);
    auto result     = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    result->data = malloc(sizeof(api_code_status_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_CODE_STATUS;

    // Helper to get code status enum from string
    auto get_code_status = [](std::string status_str) -> code_status_t {
        std::transform(status_str.begin(), status_str.end(), status_str.begin(), ::tolower);
        if (status_str == "pending") {
            return CODE_STATUS_PENDING;
        } else if (status_str == "valid") {
            return CODE_STATUS_VALID;
        } else if (status_str == "failed") {
            return CODE_STATUS_FAILED;
        } else if (status_str == "expired") {
            return CODE_STATUS_EXPIRED;
        } else if (status_str == "invalidated") {
            return CODE_STATUS_INVALIDATED;
        } else {
            return CODE_STATUS_UNKNOWN;
        }
    };

    // Copy the request code response data
    auto *code_data                   = static_cast<api_code_status_response_t *>(result->data);
    code_data->code                   = getstr(result_json, "code");
    code_data->code_type              = json_get<code_type_t>(result_json, "code_type", CODE_TYPE_UNKNOWN);
    code_data->status                 = get_code_status(json_get<std::string>(result_json, "status", "unknown"));
    code_data->issued_at              = json_get<time_t>(result_json, "issued_at", API_TIMESTAMP_NULL);
    code_data->expires_at             = json_get<time_t>(result_json, "expires_at", API_TIMESTAMP_NULL);
    code_data->redeemed               = json_get<bool>(result_json, "redeemed", false);
    code_data->redeemed_at            = json_get<time_t>(result_json, "redeemed_at", API_TIMESTAMP_NULL);
    code_data->redeemed_by_badge_id   = getstr(result_json, "redeemed_by_badge_id");
    code_data->invalidated            = json_get<bool>(result_json, "invalidated", false);
    code_data->invalidated_reason     = getstr(result_json, "invalidated_reason");
    code_data->nut_id                 = getstr(result_json, "nut_id");
    code_data->issued_by_nut_id       = getstr(result_json, "issued_by_nut_id");
    code_data->issued_by_nut_type     = json_get<nut_type_t>(result_json, "issued_by_nut_type", NUT_TYPE_UNKNOWN);
    code_data->node_id                = json_get<int>(result_json, "node_id", -1);
    code_data->node_type              = json_get<node_type_t>(result_json, "node_type", NODE_TYPE_UNKNOWN);
    code_data->unlocked_at            = json_get<time_t>(result_json, "unlocked_at", API_TIMESTAMP_NULL);
    code_data->unlocked_by_badge_id   = getstr(result_json, "unlocked_by_badge_id");
    code_data->team_id                = json_get<int>(result_json, "team_id", -1);
    code_data->credits_deducted       = json_get<int>(result_json, "credits_deducted", 0);
    code_data->remaining_credits      = json_get<int>(result_json, "remaining_credits", 0);
    code_data->node_team_id_at_unlock = json_get<int>(result_json, "node_team_id_at_unlock", -1);
    code_data->failure_reason         = getstr(result_json, "failure_reason");
    code_data->failed_at              = json_get<time_t>(result_json, "failed_at", API_TIMESTAMP_NULL);
    code_data->attempted_by_badge_id  = getstr(result_json, "attempted_by_badge_id");

    return result;
}

extern "C" api_result_t *api_send_syslog(syslog_level_t level, const char *message) {
    if (apiClient == nullptr) {
        return nullptr;
    }
    if (message == nullptr || strlen(message) == 0) {
        ESP_LOGE(TAG, "Syslog message is null or empty");
        return nullptr;
    }

    return base_result(apiClient->sendTelecomSyslog(message, level, "SYSTEM"));
}

extern "C" api_result_t *api_get_firmware_version() {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->getFirmwareVersion();

    // Create a new result struct to return
    auto result = base_result(api_result);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(api_result).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_firmware_data_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_FIRMWARE_DATA;

    // Copy the firmware data
    auto *firmware_data        = static_cast<api_firmware_data_t *>(result->data);
    std::string device_type    = json_get<std::string>(result_json, "device_type", "unknown");
    firmware_data->device_type = device_type == "badge" ? API_DEVICE_TYPE_BADGE
                                 : device_type == "nut" ? API_DEVICE_TYPE_NUT
                                                        : API_DEVICE_TYPE_UNKNOWN;
    firmware_data->setting     = getstr(result_json, "setting");
    firmware_data->value       = getstr(result_json, "value");

    return result;
}

extern "C" api_result_t *api_get_firmware_head() {
    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = apiClient->getFirmwareHead();
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred");
        return nullptr;
    }

    // Get the response and check for errors
    const auto &response = std::get<ApiClient::ApiResponse>(api_result);
    if (response.status_code < 200 || response.status_code >= 300) {
        ESP_LOGE(TAG, "Firmware HEAD request failed with status code %d", response.status_code);
        return nullptr;
    }
    // Check headers
    auto headers = response.headers;
    if (headers.empty()) {
        ESP_LOGE(TAG, "No headers in firmware HEAD response");
        return nullptr;
    }

    // Check for required headers case-insensitively
    std::string content_type     = get_header_nocase(headers, "Content-Type");
    std::string content_length   = get_header_nocase(headers, "Content-Length");
    std::string firmware_version = get_header_nocase(headers, "X-Firmware-Version");
    std::string device_type      = get_header_nocase(headers, "X-Device-Type");
    std::string last_modified    = get_header_nocase(headers, "Last-Modified");
    if (content_type.empty() || content_length.empty() || firmware_version.empty() || device_type.empty() ||
        last_modified.empty()) {
        ESP_LOGE(TAG, "Missing required headers in firmware HEAD response");
        return nullptr;
    }

    // Create a new result struct to return
    auto result = (api_result_t *)malloc(sizeof(api_result_t));
    if (result == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware HEAD response");
        return nullptr;
    }

    // Create a new struct to return in the data field
    result->data = malloc(sizeof(api_firmware_head_t));
    if (result->data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware HEAD response");
        api_free_result(result, true);
        return nullptr;
    }

    // Initialize some other initial fields
    result->detail = nullptr;
    result->ok     = true;

    // Set the result type
    result->type = api_result_type_t::API_FIRMWARE_HEAD;

    // Get the firmware information from the headers
    auto *firmware_info             = static_cast<api_firmware_head_t *>(result->data);
    firmware_info->content_type     = strdup(content_type.c_str());
    firmware_info->firmware_size    = std::stoull(content_length);
    firmware_info->firmware_version = strdup(firmware_version.c_str());
    firmware_info->device_type      = device_type == "badge" ? API_DEVICE_TYPE_BADGE
                                      : device_type == "nut" ? API_DEVICE_TYPE_NUT
                                                             : API_DEVICE_TYPE_UNKNOWN;
    firmware_info->last_modified    = strdup(last_modified.c_str());

    return result;
}

extern "C" api_err_t api_do_firmware_update() {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }
    return apiClient->doFirmwareUpdate();
}

extern "C" api_err_t api_send_telecom_port_status(const api_telecom_port_status_t statuses[], size_t count) {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }
    if (statuses == nullptr) {
        ESP_LOGE(TAG, "Port statuses array is null");
        return api_err_t::API_FAIL;
    }

    std::vector<api_telecom_port_status_t> port_statuses(statuses, statuses + count);
    auto api_result = apiClient->sendTelecomPortStatus(port_statuses);
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred while sending telecom port status");
        return api_err_t::API_FAIL;
    }

    return api_err_t::API_OK;
}

extern "C" api_err_t api_send_telecom_status(bool online) {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }

    auto api_result = apiClient->sendTelecomStatus(online);
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred while sending telecom status");
        return api_err_t::API_FAIL;
    }

    return api_err_t::API_OK;
}

extern "C" api_err_t api_send_telecom_heartbeat() {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }

    auto api_result = apiClient->sendTelecomHeartbeat();
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred while sending telecom heartbeat");
        return api_err_t::API_FAIL;
    }

    return api_err_t::API_OK;
}

extern "C" api_err_t api_send_telecom_locked() {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }

    auto api_result = apiClient->sendTelecomLocked();
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred while sending telecom locked notification");
        return api_err_t::API_FAIL;
    }

    return api_err_t::API_OK;
}

extern "C" api_err_t api_send_telecom_syslog(const char *message, syslog_level_t level, const char *category) {
    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }

    auto api_result = apiClient->sendTelecomSyslog(message, level, category);
    if (std::holds_alternative<ApiClient::ApiError>(api_result)) {
        ESP_LOGE(TAG, "API error occurred while sending telecom syslog");
        return api_err_t::API_FAIL;
    }

    return api_err_t::API_OK;
}