#include <algorithm>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include "api.h"
#include "api_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

constexpr static const char *TAG = "api";

// Alias for helpers to make less verbose
#define bytes_hex ApiClient::ApiHelpers::bytes_hex
#define hex_bytes ApiClient::ApiHelpers::hex_bytes
#define json_get  ApiClient::ApiHelpers::json_get

// Global instance of ApiClient
const static std::unique_ptr<ApiClient> apiClient = std::make_unique<ApiClient>();

// Mutex to serialize API calls
static SemaphoreHandle_t api_mutex = xSemaphoreCreateMutex();

// RAII mutex guard
class ApiMutexGuard {
  public:
    ApiMutexGuard() {
        if (api_mutex) {
            xSemaphoreTake(api_mutex, portMAX_DELAY);
        }
    }
    ~ApiMutexGuard() {
        if (api_mutex) {
            xSemaphoreGive(api_mutex);
        }
    }
    ApiMutexGuard(const ApiMutexGuard &)            = delete;
    ApiMutexGuard &operator=(const ApiMutexGuard &) = delete;
};

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
            nullsafe_free(status_data->player_name);
            nullsafe_free(status_data->team_name);
            nullsafe_free(status_data->registered_at);
            nullsafe_free(status_data->firmware_info.current_version);
            nullsafe_free(status_data->firmware_info.latest_version);
            break;
        }
        case api_result_type_t::API_MESSAGES: {
            auto messages_data = (api_messages_response_t *)data;
            // Free each message's strings
            if (messages_data != nullptr) {
                for (int i = 0; i < messages_data->message_count; i++) {
                    nullsafe_free(messages_data->messages[i].title);
                    nullsafe_free(messages_data->messages[i].content);
                    nullsafe_free(messages_data->messages[i].created_at);
                }
                nullsafe_free(messages_data);
            }
            break;
        }
        case api_result_type_t::API_MARK_MESSAGE_READ: {
            auto mark_read_data = (api_mark_message_read_response_t *)data;
            nullsafe_free(mark_read_data->badge_id);
            nullsafe_free(mark_read_data->timestamp);
            break;
        }
        case api_result_type_t::API_LIST_FILES: {
            auto list_files_data = (api_list_files_response_t *)data;
            // Free each file's strings
            if (list_files_data != nullptr) {
                for (int i = 0; i < list_files_data->file_count; i++) {
                    nullsafe_free(list_files_data->files[i].filename);
                    nullsafe_free(list_files_data->files[i].sha256);
                    nullsafe_free(list_files_data->files[i].url);
                }
                nullsafe_free(list_files_data);
            }
            break;
        }
        case api_result_type_t::API_ACHIEVEMENTS: {
            auto achievements_data = (api_achievements_response_t *)data;
            // Free each achievement's strings
            if (achievements_data != nullptr) {
                for (uint32_t i = 0; i < achievements_data->achievement_count; i++) {
                    nullsafe_free(achievements_data->achievements[i].short_name);
                    nullsafe_free(achievements_data->achievements[i].description);
                    nullsafe_free(achievements_data->achievements[i].completed_at);
                }
                nullsafe_free(achievements_data);
            }
            break;
        }
        case api_result_type_t::API_CREDIT_SWAP: {
            auto credit_swap_data = (api_credit_swap_response_t *)data;
            nullsafe_free(credit_swap_data->code);
            nullsafe_free(credit_swap_data->expires_at);
            nullsafe_free(credit_swap_data->status);
            break;
        }
        case api_result_type_t::API_CREDIT_SWAP_STATUS: {
            auto credit_swap_status_data = (api_credit_swap_status_response_t *)data;
            nullsafe_free(credit_swap_status_data->code);
            nullsafe_free(credit_swap_status_data->status);
            nullsafe_free(credit_swap_status_data->redeemed_at);
            nullsafe_free(credit_swap_status_data->redeemed_by);
            nullsafe_free(credit_swap_status_data->expires_at);
            nullsafe_free(credit_swap_status_data->issued_at);
            break;
        }
        case api_result_type_t::API_TOPOLOGY_DATA: {
            auto topology_data = (api_topology_data_t *)data;

            // Free nodes array and their strings
            if (topology_data->nodes != nullptr) {
                for (int i = 0; i < topology_data->node_count; i++) {
                    nullsafe_free(topology_data->nodes[i].node_type);
                    nullsafe_free(topology_data->nodes[i].color);
                    nullsafe_free(topology_data->nodes[i].halo_color);
                }
                nullsafe_free(topology_data->nodes);
            }

            // Free connections array and their strings
            if (topology_data->connections != nullptr) {
                for (int i = 0; i < topology_data->connection_count; i++) {
                    nullsafe_free(topology_data->connections[i].connection_type);
                }
                nullsafe_free(topology_data->connections);
            }

            // Free port status array and their strings
            if (topology_data->port_status != nullptr) {
                for (int i = 0; i < topology_data->port_status_count; i++) {
                    nullsafe_free(topology_data->port_status[i].link_status);
                    nullsafe_free(topology_data->port_status[i].updated_at);
                }
                nullsafe_free(topology_data->port_status);
            }

            // Free path status array and their strings
            if (topology_data->path_status != nullptr) {
                for (int i = 0; i < topology_data->path_status_count; i++) {
                    nullsafe_free(topology_data->path_status[i].path_nodes);
                    nullsafe_free(topology_data->path_status[i].updated_at);
                }
                nullsafe_free(topology_data->path_status);
            }

            // Free team path scores array and their strings
            if (topology_data->team_path_scores != nullptr) {
                for (int i = 0; i < topology_data->team_path_score_count; i++) {
                    nullsafe_free(topology_data->team_path_scores[i].created_at);
                }
                nullsafe_free(topology_data->team_path_scores);
            }

            // Free timestamp
            nullsafe_free(topology_data->timestamp);
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
    ApiMutexGuard guard;

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
    auto *provision_data            = static_cast<api_provision_response_t *>(result->data);
    std::string hmac_secret_hex     = result_json["hmac_secret"];
    std::string hash_prefix_hex     = result_json["hash_prefix"];
    std::string ctf_hmac_secret_hex = result_json["ctf_hmac_secret"];
    std::string ctf_hash_prefix_hex = result_json["ctf_hash_prefix"];

    if (!hex_bytes(hmac_secret_hex.c_str(), provision_data->hmac_secret, sizeof(provision_data->hmac_secret)) ||
        !hex_bytes(hash_prefix_hex.c_str(), provision_data->hash_prefix, sizeof(provision_data->hash_prefix)) ||
        !hex_bytes(ctf_hmac_secret_hex.c_str(), provision_data->ctf_hmac_secret, sizeof(provision_data->ctf_hmac_secret)) ||
        !hex_bytes(ctf_hash_prefix_hex.c_str(), provision_data->ctf_hash_prefix, sizeof(provision_data->ctf_hash_prefix))) {
        api_free_result(result, true);
        return nullptr;
    }

    return result;
}

extern "C" api_result_t *api_provision_wireless_psk() {
    ApiMutexGuard guard;

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
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->provisionVerify());
}

extern "C" api_result_t *api_get_firmware_version() {
    ApiMutexGuard guard;

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
    ApiMutexGuard guard;

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
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return api_err_t::API_FAIL;
    }
    return apiClient->doFirmwareUpdate();
}

extern "C" api_result_t *api_redeem_code(const char *code) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->redeemCode(code));
}

extern "C" api_result_t *api_register(const char *player_name) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto api_result = player_name == nullptr ? apiClient->getStatus() // Status if no player name is provided
                                             : apiClient->registerBadge(player_name);
    auto result     = base_result(api_result);
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
    result->data = malloc(sizeof(api_status_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_STATUS;

    // Copy the status response data
    auto *status_data          = static_cast<api_status_response_t *>(result->data);
    status_data->player_name   = getstr(result_json, "player_name");
    status_data->team_id       = result_json["team_id"];
    status_data->team_name     = getstr(result_json, "team_name");
    status_data->credits       = result_json["credits"];
    status_data->level         = result_json["level"];
    status_data->registered_at = getstr(result_json, "registered_at");
    status_data->sorting_hat   = result_json["sorting_hat"];
    status_data->has_messages  = result_json["has_messages"];

    // Handle firmware_info which can be null
    if (result_json.contains("firmware_info") && !result_json["firmware_info"].is_null()) {
        const auto &firmware_info                  = result_json["firmware_info"];
        status_data->firmware_info.current_version = getstr(firmware_info, "current_version");
        status_data->firmware_info.latest_version  = getstr(firmware_info, "latest_version");
        status_data->firmware_info.update_available =
            firmware_info.contains("update_available") && !firmware_info["update_available"].is_null()
                ? firmware_info["update_available"].get<bool>()
                : false;
    } else {
        status_data->firmware_info.current_version  = nullptr;
        status_data->firmware_info.latest_version   = nullptr;
        status_data->firmware_info.update_available = false;
    }

    return result;
}

extern "C" api_result_t *api_get_status() {
    return api_register(nullptr);
}

extern "C" api_result_t *api_get_messages() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->getMessages();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_messages_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_MESSAGES;

    // Copy the messages data
    auto *messages_data      = static_cast<api_messages_response_t *>(result->data);
    messages_data->badge_id  = getstr(result_json, "badge_id");
    messages_data->timestamp = getstr(result_json, "timestamp");

    auto messages_json           = result_json["messages"];
    messages_data->message_count = messages_json.is_array() ? messages_json.size() : 0;
    messages_data->messages      = (api_message_t *)calloc(messages_data->message_count, sizeof(api_message_t));
    if (messages_data->messages == nullptr && messages_data->message_count > 0) {
        api_free_result(result, true);
        return nullptr;
    }
    for (int i = 0; i < messages_data->message_count; i++) {
        auto message                          = messages_json[i];
        messages_data->messages[i].id         = message["id"];
        messages_data->messages[i].title      = getstr(message, "title");
        messages_data->messages[i].content    = getstr(message, "content");
        messages_data->messages[i].priority   = message["priority"];
        messages_data->messages[i].created_at = getstr(message, "created_at");
    }

    return result;
}

extern "C" api_result_t *api_mark_message_read(int message_id) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->markMessageRead(message_id);

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_mark_message_read_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_MARK_MESSAGE_READ;

    // Copy the mark message read data
    auto *mark_read_data       = static_cast<api_mark_message_read_response_t *>(result->data);
    mark_read_data->badge_id   = getstr(result_json, "badge_id");
    mark_read_data->message_id = result_json["message_id"];
    mark_read_data->success    = result_json["success"];
    mark_read_data->timestamp  = getstr(result_json, "timestamp");

    return result;
}

extern "C" api_result_t *api_list_files() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->listFiles();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_list_files_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_LIST_FILES;

    // Copy the list files data
    auto *list_files_data       = static_cast<api_list_files_response_t *>(result->data);
    list_files_data->file_count = result_json["files"].is_array() ? result_json["files"].size() : 0;
    list_files_data->files      = (api_file_info_t *)calloc(list_files_data->file_count, sizeof(api_file_info_t));
    if (list_files_data->files == nullptr && list_files_data->file_count > 0) {
        api_free_result(result, true);
        return nullptr;
    }
    for (int i = 0; i < list_files_data->file_count; i++) {
        auto file                          = result_json["files"][i];
        list_files_data->files[i].filename = getstr(file, "filename");
        list_files_data->files[i].sha256   = getstr(file, "sha256");
        list_files_data->files[i].url      = getstr(file, "url");
    }

    return result;
}

extern "C" api_result_t *api_download_file(const char *sha256, const char *file_path) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->downloadFile(sha256, file_path));
}

extern "C" api_result_t *api_konami_entered() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->konamiEntered();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_konami_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Copy the konami data
    auto *konami_data       = static_cast<api_konami_response_t *>(result->data);
    konami_data->first_time = result_json["first_time"];

    return result;
}

extern "C" api_result_t *api_report_dawn_accord(bool ai_inhibitor_enabled, bool ai_security_strip_enabled) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->dawnAccord(ai_inhibitor_enabled, ai_security_strip_enabled, ""));
}

extern "C" api_result_t *api_get_achievements() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->getAchievements();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_array()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Copy the achievements data
    auto *achievements_data              = static_cast<api_achievements_response_t *>(result->data);
    achievements_data->achievement_count = result_json.size();
    achievements_data->achievements =
        (api_achievement_t *)calloc(achievements_data->achievement_count, sizeof(api_achievement_t));
    if (achievements_data->achievements == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }
    for (size_t i = 0; i < achievements_data->achievement_count; i++) {
        auto achievement                                = result_json[i];
        achievements_data->achievements[i].id           = achievement["id"];
        achievements_data->achievements[i].short_name   = getstr(achievement, "short_name");
        achievements_data->achievements[i].description  = getstr(achievement, "description");
        achievements_data->achievements[i].is_public    = achievement["is_public"];
        auto status                                     = json_get<std::string>(achievement, "status", "NOT_STARTED");
        achievements_data->achievements[i].status       = status == "NOT_STARTED"   ? ACHIEVEMENT_NOT_STARTED
                                                          : status == "IN_PROGRESS" ? ACHIEVEMENT_IN_PROGRESS
                                                          : status == "COMPLETED"   ? ACHIEVEMENT_COMPLETED
                                                          : status == "FAILED"      ? ACHIEVEMENT_FAILED
                                                                                    : ACHIEVEMENT_UNKNOWN;
        achievements_data->achievements[i].completed_at = getstr(achievement, "completed_at");
    }

    return result;
}

extern "C" api_result_t *api_get_ctf_hints_password() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->ctfHintsPassword();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_ctf_hints_password_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_CTF_HINTS_PASSWORD;

    // Copy the CTF hints password data
    auto *ctf_hints_password_data     = static_cast<api_ctf_hints_password_response_t *>(result->data);
    ctf_hints_password_data->password = getstr(result_json, "password");

    return result;
}

extern "C" api_result_t *api_request_credit_swap(uint32_t amount) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->requestCreditSwap(amount);

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_credit_swap_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_CREDIT_SWAP;

    // Copy the credit swap data
    auto *credit_swap_data       = static_cast<api_credit_swap_response_t *>(result->data);
    credit_swap_data->code       = getstr(result_json, "code");
    credit_swap_data->credits    = result_json["credits"];
    credit_swap_data->expires_at = getstr(result_json, "expires_at");
    credit_swap_data->status     = getstr(result_json, "status");

    return result;
}

extern "C" api_result_t *api_credit_swap_status(const char *code) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->creditSwapStatus(code);

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON
    auto result_json = std::get<ApiClient::ApiResponse>(response).body_json()["data"];
    if (result_json.is_null() || !result_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create a new result struct to return in the data field
    result->data = malloc(sizeof(api_credit_swap_status_response_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    // Set the result type
    result->type = api_result_type_t::API_CREDIT_SWAP_STATUS;

    // Copy the credit swap status data
    auto *status_data        = static_cast<api_credit_swap_status_response_t *>(result->data);
    status_data->code        = getstr(result_json, "code");
    status_data->credits     = result_json["credits"];
    status_data->status      = getstr(result_json, "status");
    status_data->redeemed    = result_json.value("redeemed", false);
    status_data->redeemed_at = getstr(result_json, "redeemed_at");
    status_data->redeemed_by = getstr(result_json, "redeemed_by");
    status_data->expires_at  = getstr(result_json, "expires_at");
    status_data->issued_at   = getstr(result_json, "issued_at");

    return result;
}

extern "C" api_result_t *api_cancel_credit_swap(const char *code) {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }
    return base_result(apiClient->cancelCreditSwap(code));
}

extern "C" api_result_t *api_topology_data() {
    ApiMutexGuard guard;

    if (apiClient == nullptr) {
        return nullptr;
    }

    auto response = apiClient->getTopologyData();

    // Create a new result struct to return
    auto result = base_result(response);
    if (result == nullptr) {
        return nullptr;
    }

    // Parse the response JSON - topology data is in the root, not in a "result" field
    auto response_json = std::get<ApiClient::ApiResponse>(response).body_json();
    if (response_json.is_null() || !response_json.is_object()) {
        api_free_result(result, true);
        return nullptr;
    }

    // Create the topology data struct
    result->data = malloc(sizeof(api_topology_data_t));
    if (result->data == nullptr) {
        api_free_result(result, true);
        return nullptr;
    }

    result->type       = api_result_type_t::API_TOPOLOGY_DATA;
    auto topology_data = (api_topology_data_t *)result->data;

    // Initialize all pointers to null
    topology_data->nodes            = nullptr;
    topology_data->connections      = nullptr;
    topology_data->port_status      = nullptr;
    topology_data->path_status      = nullptr;
    topology_data->team_path_scores = nullptr;
    topology_data->timestamp        = getstr(response_json, "timestamp");

    int64_t start_time = esp_timer_get_time();

    // Parse nodes array
    auto nodes_json = response_json["nodes"];
    if (nodes_json.is_array()) {
        topology_data->node_count = nodes_json.size();
        topology_data->nodes      = (api_topology_node_t *)calloc(topology_data->node_count, sizeof(api_topology_node_t));

        for (int i = 0; i < topology_data->node_count; i++) {
            auto node                           = nodes_json[i];
            topology_data->nodes[i].id          = json_get<int>(node, "id", 0);
            topology_data->nodes[i].node_type   = getstr(node, "node_type");
            topology_data->nodes[i].x_coord     = json_get<float>(node, "x_coord", 0.0f);
            topology_data->nodes[i].y_coord     = json_get<float>(node, "y_coord", 0.0f);
            topology_data->nodes[i].port_count  = json_get<int>(node, "port_count", 0);
            topology_data->nodes[i].team_id     = node["team_id"].is_null() ? -1 : json_get<int>(node, "team_id", -1);
            topology_data->nodes[i].color       = getstr(node, "color");
            topology_data->nodes[i].is_online   = json_get<bool>(node, "is_online", false);
            topology_data->nodes[i].halo_color  = getstr(node, "halo_color");
            topology_data->nodes[i].is_unlocked = node["is_unlocked"].is_null() ? -1 : json_get<int>(node, "is_unlocked", -1);
        }
    } else {
        topology_data->node_count = 0;
    }

    // Parse connections array
    auto connections_json = response_json["connections"];
    if (connections_json.is_array()) {
        topology_data->connection_count = connections_json.size();
        topology_data->connections =
            (api_topology_connection_t *)calloc(topology_data->connection_count, sizeof(api_topology_connection_t));

        for (int i = 0; i < topology_data->connection_count; i++) {
            auto conn                                     = connections_json[i];
            topology_data->connections[i].id              = json_get<int>(conn, "id", 0);
            topology_data->connections[i].node_a_id       = json_get<int>(conn, "node_a_id", 0);
            topology_data->connections[i].node_a_port     = json_get<int>(conn, "node_a_port", 0);
            topology_data->connections[i].node_b_id       = json_get<int>(conn, "node_b_id", 0);
            topology_data->connections[i].node_b_port     = json_get<int>(conn, "node_b_port", 0);
            topology_data->connections[i].connection_type = getstr(conn, "connection_type");
        }
    } else {
        topology_data->connection_count = 0;
    }

    // Parse port_status array
    auto port_status_json = response_json["port_status"];
    if (port_status_json.is_array()) {
        topology_data->port_status_count = port_status_json.size();
        topology_data->port_status =
            (api_topology_port_status_t *)calloc(topology_data->port_status_count, sizeof(api_topology_port_status_t));

        for (int i = 0; i < topology_data->port_status_count; i++) {
            auto port                                 = port_status_json[i];
            topology_data->port_status[i].node_id     = json_get<int>(port, "node_id", 0);
            topology_data->port_status[i].port_number = json_get<int>(port, "port_number", 0);
            topology_data->port_status[i].connected   = json_get<int>(port, "connected", 0);
            topology_data->port_status[i].link_status = getstr(port, "link_status");
            topology_data->port_status[i].updated_at  = getstr(port, "updated_at");
        }
    } else {
        topology_data->port_status_count = 0;
    }

    // Parse path_status array
    auto path_status_json = response_json["path_status"];
    if (path_status_json.is_array()) {
        topology_data->path_status_count = path_status_json.size();
        topology_data->path_status =
            (api_topology_path_status_t *)calloc(topology_data->path_status_count, sizeof(api_topology_path_status_t));

        for (int i = 0; i < topology_data->path_status_count; i++) {
            auto path                                      = path_status_json[i];
            topology_data->path_status[i].id               = json_get<int>(path, "id", 0);
            topology_data->path_status[i].team_id          = json_get<int>(path, "team_id", 0);
            topology_data->path_status[i].has_path_to_core = json_get<int>(path, "has_path_to_core", 0);
            topology_data->path_status[i].path_via_tower   = json_get<int>(path, "path_via_tower", 0);
            topology_data->path_status[i].path_nodes       = getstr(path, "path_nodes");
            topology_data->path_status[i].updated_at       = getstr(path, "updated_at");
            topology_data->path_status[i].is_scored_path   = json_get<int>(path, "is_scored_path", 0);
        }
    } else {
        topology_data->path_status_count = 0;
    }

    // Parse team_path_scores array
    auto team_path_scores_json = response_json["team_path_scores"];
    if (team_path_scores_json.is_array()) {
        topology_data->team_path_score_count = team_path_scores_json.size();
        topology_data->team_path_scores      = (api_topology_team_path_score_t *)calloc(topology_data->team_path_score_count,
                                                                                        sizeof(api_topology_team_path_score_t));

        for (int i = 0; i < topology_data->team_path_score_count; i++) {
            auto score                                    = team_path_scores_json[i];
            topology_data->team_path_scores[i].team_id    = json_get<int>(score, "team_id", 0);
            topology_data->team_path_scores[i].first_node = json_get<int>(score, "first_node", 0);
            topology_data->team_path_scores[i].last_node  = json_get<int>(score, "last_node", 0);
            topology_data->team_path_scores[i].created_at = getstr(score, "created_at");
        }
    } else {
        topology_data->team_path_score_count = 0;
    }

    int64_t end_time = esp_timer_get_time();
    ESP_LOGD(TAG, "Time taken to parse API response: %lld ms", (end_time - start_time) / 1000);

    return result;
}
