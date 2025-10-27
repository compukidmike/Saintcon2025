#pragma once

#include <expected>
#include <format>
#include <map>
#include <variant>
#include "esp_mac.h"
#include "esp_http_client.h"

#include "api.h"
#include "secure_element.h"

#define JSON_NOEXCEPTION

#include "nlohmann/json.hpp"

using json = nlohmann::json;

class ApiClient {
  public:
    // clang-format off
    #define API_ERROR_LIST          \
        X(GENERIC_ERROR)            \
        X(REQUEST_VALIDATION_ERROR) \
        X(REQUEST_INIT_ERROR)       \
        X(REQUEST_AUTH_ERROR)       \
        X(REQUEST_SIGN_ERROR)
    // API result types
    enum class ApiError {
        #define X(name) name,
        API_ERROR_LIST
        #undef X
    };
    static inline const char *to_string(ApiError e) {
        switch (e) {
            #define X(name) \
            case ApiError::name: return #name;
                    API_ERROR_LIST
            #undef X
            default: return "UNKNOWN_ERROR";
        }
    }
    // clang-format on
    struct ApiResponse {
        std::string body;
        int status_code = -1;
        std::map<std::string, std::string> headers;
        json body_json() const {
            return json::parse(body, nullptr, false, true);
        }
    };
    using ApiResult = std::variant<ApiResponse, ApiError>;

    // Collection of helper functions
    struct ApiHelpers {
        // JSON helpers
        template <typename T> static std::optional<T> json_get(const json &json, const std::string_view &path);
        template <typename T> static T json_get(const json &json, const std::string_view &path, const T &default_value);

        // Timestamp parsing helper
        static std::optional<time_t> parse_iso8601(const std::string &iso_str);

        // Hex <-> bytes helpers
        enum class HexError { NONE, INVALID_LENGTH, INVALID_CHARACTER };
        static std::expected<std::vector<uint8_t>, HexError> hex_bytes(const std::string &hex);
        static bool hex_bytes(const std::string &hex, uint8_t *bytes, size_t len);
        static std::string bytes_hex(const std::vector<uint8_t> &bytes, bool lowercase = true);
    };

    // API endpoint auth levels
    enum class AuthLevel {
        NONE,     // No authentication
        BASIC,    // L1: Only API key via X-API-Auth header is required
        SIGNED,   // L2: API key + HMAC signature via X-Signature header
        CHALLENGE // L3: API key + HMAC signature + server-generated challenge-response nonce via X-Nonce header
    };
    using EndpointKey = std::pair<std::string, esp_http_client_method_t>;

    static bool urlMatch(const std::string_view pattern, const std::string_view url);
    static AuthLevel getAuthLevel(const EndpointKey &key);
    static AuthLevel getAuthLevel(const std::string_view &endpoint, esp_http_client_method_t method);
    static AuthLevel getAuthLevel(const std::string_view endpoint, const std::string_view method);

    ApiClient() {
        // API key comes from the MAC address
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        api_key = std::format("{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // Provisioning API endpoints
    ApiResult provisionSecrets();
    ApiResult provisionWirelessPSK();
    ApiResult provisionVerify();

    // Badge API endpoints
    ApiResult getStatus();
    ApiResult requestCode(int level);
    ApiResult getCodeStatus(const std::string_view code);
    ApiResult sendSyslog(syslog_level_t level, const std::string_view message);

    // Firmware API endpoints
    ApiResult getFirmwareVersion();
    ApiResult getFirmwareHead();
    api_err_t doFirmwareUpdate();

    // Telecom API endpoints
    ApiResult sendTelecomPortStatus(const std::vector<api_telecom_port_status_t> &port_statuses);
    ApiResult sendTelecomStatus(bool online);
    ApiResult sendTelecomHeartbeat();
    ApiResult sendTelecomLocked();
    ApiResult sendTelecomSyslog(const std::string_view message, syslog_level_t level = SYSLOG_LEVEL_INFO,
                                const std::string_view category = "SYSTEM");

  private:
    std::string api_key;
    static const std::map<EndpointKey, AuthLevel> api_metadata;
    struct RequestContext {
        std::string response_buffer;
        std::map<std::string, std::string> response_headers;
        bool log_data        = false;   // Print data to console log (usually for debugging)
        FILE *output_file    = nullptr; // For streaming downloads directly to SPIFFS
        size_t bytes_written = 0;       // Track bytes written to file
    };

    // Security helpers
    std::string canonicalizedRequest(const std::string &method, const std::string &path,
                                     const std::map<std::string, std::string> &headers, const std::string &body);
    std::string getSignature(const std::string &canonical_payload);
    esp_err_t applyAuthHeaders(esp_http_client_handle_t client, const std::string_view method, const std::string_view path,
                               const std::string_view payload = "");

    // Request handling
    static esp_err_t httpEventHandler(esp_http_client_event_t *evt);
    ApiResult doRequest(const std::string_view endpoint, const std::string_view method, const std::string_view payload = "");
};

// Include template definitions
#include "api_client.hpp"
