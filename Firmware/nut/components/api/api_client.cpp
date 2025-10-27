#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <set>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <boost/config.hpp>
#include <boost/bimap.hpp>

#include "api_client.h"
#include "nut.h"
#include "nut/version.h"
#include "secure_element.h"
#include "secure_element.h"

#include "led_patterns.h"

#define OTA_BUFFER_SIZE (8 * 1024) // 8 KB

constexpr static const char *TAG = "api_client";

// Syslog level strings array
const char *const syslog_level_strs[] = {
#define X(name) #name,
    SYSLOG_LEVEL_LIST
#undef X
};
static_assert(sizeof(syslog_level_strs) / sizeof(syslog_level_strs[0]) == static_cast<size_t>(SYSLOG_LEVEL_MAX),
              "syslog_level_strs size mismatch");

// The ISRG Root X1 certificate embedded in the binary
extern const uint8_t isrgrootx1_cert[] asm("_binary_isrgrootx1_pem_start");
extern const uint8_t isrgrootx1_cert_end[] asm("_binary_isrgrootx1_pem_end");

// ------------------------------------------------------------------------------------------------
// Helpful mappings
// ------------------------------------------------------------------------------------------------
// clang-format off

// Define known API metadata
const std::map<ApiClient::EndpointKey, ApiClient::AuthLevel> ApiClient::api_metadata = {
    {{"/api/nut/challenge",                   HTTP_METHOD_POST},   ApiClient::AuthLevel::SIGNED},
    {{"/api/nut/provision",                   HTTP_METHOD_POST},   ApiClient::AuthLevel::BASIC},
    {{"/api/nut/provision/verify",            HTTP_METHOD_POST},   ApiClient::AuthLevel::SIGNED},
    {{"/api/nut/provision/wireless-psk",      HTTP_METHOD_POST},   ApiClient::AuthLevel::SIGNED},
    {{"/api/nut/status",                      HTTP_METHOD_GET},    ApiClient::AuthLevel::CHALLENGE},
    {{"/api/nut/code/request",                HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
    {{"/api/nut/code/status/<code>",          HTTP_METHOD_GET},    ApiClient::AuthLevel::CHALLENGE},
    {{"/api/nut/syslog",                      HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
    {{"/api/firmware/version",                HTTP_METHOD_GET},    ApiClient::AuthLevel::SIGNED},
    {{"/api/firmware/nut",                    HTTP_METHOD_GET},    ApiClient::AuthLevel::SIGNED},
    {{"/api/firmware/nut",                    HTTP_METHOD_HEAD},   ApiClient::AuthLevel::SIGNED},
    {{"/api/telecom/connections/update",      HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
    {{"/api/telecom/status",                  HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
    {{"/api/telecom/heartbeat",               HTTP_METHOD_GET},    ApiClient::AuthLevel::CHALLENGE},
    {{"/api/telecom/lock",                    HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
    {{"/api/telecom/syslog",                  HTTP_METHOD_POST},   ApiClient::AuthLevel::CHALLENGE},
};

// Mapping of HTTP methods to their names for reverse or forward lookups
typedef boost::bimap<esp_http_client_method_t, std::string> MethodBimap;
typedef MethodBimap::value_type MethodPair;
static const MethodBimap method_names = []() {
    MethodBimap m;
    m.insert(MethodPair(HTTP_METHOD_GET,     "GET"));
    m.insert(MethodPair(HTTP_METHOD_POST,    "POST"));
    m.insert(MethodPair(HTTP_METHOD_PUT,     "PUT"));
    m.insert(MethodPair(HTTP_METHOD_PATCH,   "PATCH"));
    m.insert(MethodPair(HTTP_METHOD_DELETE,  "DELETE"));
    m.insert(MethodPair(HTTP_METHOD_HEAD,    "HEAD"));
    m.insert(MethodPair(HTTP_METHOD_OPTIONS, "OPTIONS"));
    return m;
}();

// clang-format on

// ---------------------------------------------------------------------------------------------
// API helper function definitions
// ---------------------------------------------------------------------------------------------

std::expected<std::vector<uint8_t>, ApiClient::ApiHelpers::HexError> ApiClient::ApiHelpers::hex_bytes(const std::string &hex) {
    if (hex.length() % 2 != 0) {
        return std::unexpected(HexError::INVALID_LENGTH);
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t val;
        const char *start = hex.data() + i;
        const char *end   = start + 2;

        if (auto [ptr, ec] = std::from_chars(start, end, val, 16); ec != std::errc{} || ptr != end) {
            return std::unexpected(HexError::INVALID_CHARACTER);
        }

        bytes.push_back(val);
    }

    return bytes;
}

bool ApiClient::ApiHelpers::hex_bytes(const std::string &hex, uint8_t *bytes, size_t len) {
    if (hex.length() % 2 != 0) {
        return false;
    }
    size_t hex_bytes = hex.length() / 2;
    if (hex_bytes > len) {
        return false;
    }

    for (size_t i = 0; i < hex_bytes; ++i) {
        uint8_t val;
        const char *start = hex.data() + i * 2;
        const char *end   = start + 2;

        if (auto [ptr, ec] = std::from_chars(start, end, val, 16); ec != std::errc{} || ptr != end) {
            return false;
        }

        bytes[i] = val;
    }

    return true;
}

std::string ApiClient::ApiHelpers::bytes_hex(const std::vector<uint8_t> &bytes, bool lowercase) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (uint8_t byte : bytes) {
        if (lowercase) {
            hex += std::format("{:02x}", byte);
        } else {
            hex += std::format("{:02X}", byte);
        }
    }
    return hex;
}

std::optional<time_t> ApiClient::ApiHelpers::parse_iso8601(const std::string &iso_str) {
    if (iso_str.empty()) {
        return std::nullopt;
    }

    // Strip fractional seconds if present (e.g., "2025-10-16T00:23:17.940298" -> "2025-10-16T00:23:17")
    std::string stripped = iso_str;
    size_t dot_pos       = stripped.find('.');
    if (dot_pos != std::string::npos) {
        // Find the end of fractional seconds (either 'Z', '+', '-', or end of string)
        size_t end_pos = stripped.find_first_of("Z+-", dot_pos);
        if (end_pos != std::string::npos) {
            stripped = stripped.substr(0, dot_pos) + stripped.substr(end_pos);
        } else {
            stripped = stripped.substr(0, dot_pos);
        }
    }

    // Sometimes the API returns a timestamp ending with 'Z' to indicate UTC, but the time is actually local time
    if (!stripped.empty() && stripped.back() == 'Z') {
        stripped.pop_back();
    }

    // Parse directly into tm struct to deal with timezone issues
    struct tm tm_time        = {};
    const char *parse_result = strptime(stripped.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_time);
    if (parse_result == nullptr) {
        ESP_LOGW("api_client", "Failed to parse ISO8601 timestamp: %s", stripped.c_str());
        return std::nullopt;
    }

    // API returns local time instead of UTC so use mktime() to interpret the parsed time as local time (applies TZ offset
    // automatically)
    tm_time.tm_isdst = -1; // Let mktime() determine DST status automatically
    time_t result    = mktime(&tm_time);
    if (result == -1) {
        ESP_LOGW("api_client", "Failed to convert time to timestamp: %s", stripped.c_str());
        return std::nullopt;
    }

    // // Debug: show what time this timestamp represents
    // struct tm timeinfo_utc;
    // gmtime_r(&result, &timeinfo_utc);
    // struct tm timeinfo_local;
    // localtime_r(&result, &timeinfo_local);

    // ESP_LOGD("api_client", "Parsed ISO8601 '%s' -> timestamp %ld (interpreted as local time)", iso_str.c_str(), (long)result);
    // ESP_LOGD("api_client", "  As UTC:   %04d-%02d-%02d %02d:%02d:%02d", timeinfo_utc.tm_year + 1900, timeinfo_utc.tm_mon + 1,
    //          timeinfo_utc.tm_mday, timeinfo_utc.tm_hour, timeinfo_utc.tm_min, timeinfo_utc.tm_sec);
    // ESP_LOGD("api_client", "  As Local: %04d-%02d-%02d %02d:%02d:%02d", timeinfo_local.tm_year + 1900, timeinfo_local.tm_mon +
    // 1,
    //          timeinfo_local.tm_mday, timeinfo_local.tm_hour, timeinfo_local.tm_min, timeinfo_local.tm_sec);

    return result;
}

// Safe string_view to C-string helper (non-null)
const char *sv_cstr(const std::string_view &sv) {
    return sv.data() ? sv.data() : "";
}

// ---------------------------------------------------------------------------------------------
// API endpoint auth levels
// ---------------------------------------------------------------------------------------------

namespace url {
static inline std::string_view next_segment(std::string_view s, size_t &pos) noexcept {
    while (pos < s.size() && s[pos] == '/') {
        ++pos;
    }
    if (pos >= s.size()) {
        return {};
    }
    size_t start = pos;
    while (pos < s.size() && s[pos] != '/') {
        ++pos;
    }
    return s.substr(start, pos - start);
}

static inline void strip_slashes(std::string_view &v) noexcept {
    if (!v.empty() && v.front() == '/') {
        v.remove_prefix(1);
    }
    if (!v.empty() && v.back() == '/') {
        v.remove_suffix(1);
    }
}
} // namespace url

bool ApiClient::urlMatch(std::string_view pattern, std::string_view url) {
    url::strip_slashes(pattern);
    url::strip_slashes(url);

    for (size_t p = 0, u = 0;;) {
        std::string_view pseg = url::next_segment(pattern, p);
        std::string_view useg = url::next_segment(url, u);

        const bool pend = (pseg.empty() && p >= pattern.size());
        const bool uend = (useg.empty() && u >= url.size());
        if (pend || uend) {
            return pend && uend;
        }

        if (pseg.size() >= 3 && pseg.front() == '<' && pseg.back() == '>') {
            if (useg.empty()) {
                return false;
            }
        } else if (pseg != useg) {
            return false;
        }
    }
}

ApiClient::AuthLevel ApiClient::getAuthLevel(const EndpointKey &key) {
    auto it = api_metadata.find(key);
    if (it != api_metadata.end()) {
        return it->second;
    }

    // If exact match failed, try pattern matching for URLs with path parameters
    for (const auto &[endpoint_key, auth_level] : api_metadata) {
        if (endpoint_key.second == key.second && urlMatch(endpoint_key.first, key.first)) {
            return auth_level;
        }
    }

    return AuthLevel::NONE;
}
ApiClient::AuthLevel ApiClient::getAuthLevel(const std::string_view &endpoint, esp_http_client_method_t method) {
    // Strip any query parameters from the endpoint
    std::string_view path = endpoint;
    if (auto pos = endpoint.find('?'); pos != std::string_view::npos) {
        path = endpoint.substr(0, pos);
    }

    // Strip any extra trailing slash if present
    if (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }

    return ApiClient::getAuthLevel(ApiClient::EndpointKey{path, method});
}
ApiClient::AuthLevel ApiClient::getAuthLevel(const std::string_view endpoint, const std::string_view method) {
    esp_http_client_method_t req_method = HTTP_METHOD_GET;

    // Normalize method string to uppercase for lookup
    std::string method_upper(method.data(), method.size());
    std::transform(method_upper.begin(), method_upper.end(), method_upper.begin(), ::toupper);

    auto it = method_names.right.find(method_upper);
    if (it != method_names.right.end()) {
        req_method = it->second;
    }

    return ApiClient::getAuthLevel(std::string(endpoint), req_method);
}

esp_err_t ApiClient::httpEventHandler(esp_http_client_event_t *evt) {
    auto client = static_cast<RequestContext *>(evt->user_data);
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR: //
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED: //
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT: //
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER: //
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            client->response_headers.emplace(evt->header_key, evt->header_value);

            // Pre-allocate response buffer based on Content-Length header
            if (strcmp(evt->header_key, "Content-Length") == 0) {
                try {
                    size_t content_length = std::stoul(evt->header_value);
                    if (content_length > 0 && content_length < 1024 * 1024) {   // Limit to 1MB for safety
                        client->response_buffer.reserve(content_length + 1024); // Add some padding
                        ESP_LOGD(TAG, "Pre-allocated response buffer for %zu bytes", content_length);
                    }
                } catch (const std::exception &e) { ESP_LOGW(TAG, "Failed to parse Content-Length: %s", evt->header_value); }
            }
            break;
        case HTTP_EVENT_ON_DATA: //
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (client->log_data) {
                    std::cout.write(static_cast<const char *>(evt->data), evt->data_len);
                }
                client->response_buffer.append(static_cast<const char *>(evt->data), evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH: //
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED: //
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT: //
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
        default: //
            ESP_LOGD(TAG, "Unknown event id: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

// ------------------------------------------------------------------------------------------------
// Security Helpers
// ------------------------------------------------------------------------------------------------

std::string ApiClient::canonicalizedRequest(const std::string &method, const std::string &path,
                                            const std::map<std::string, std::string> &headers, const std::string &body) {
    // Create a set of headers to sign
    static const std::set<std::string> sign_headers = {
        "host",        //
        "x-api-auth",  //
        "x-timestamp", //
        "x-nonce"      //
    };
    std::map<std::string, std::string> canonical_headers;
    for (const auto &[key, value] : headers) {
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), [](unsigned char c) { return std::tolower(c); });
        if (sign_headers.count(lower_key) > 0) {
            canonical_headers[lower_key] = value;
        }
    }

    // Create the canonical request string
    std::stringstream ss;

    // Get the hash prefix from the secure element and add it
    esp_err_t err;
    uint8_t hash_prefix[32] = {0};
    if ((err = se_read_slot(SE_KEY_HASH_PREFIX, hash_prefix, sizeof(hash_prefix))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read hash prefix: %s", esp_err_to_name(err));
        return "";
    }
    ss << std::string(reinterpret_cast<const char *>(hash_prefix), 16); // Real size is only 16 bytes

    // Make sure we only get the URI path component regardless of what was passed in since we are not signing request parameters
    std::string clean_path = path;
    size_t protocol_pos    = clean_path.find("://");
    if (protocol_pos != std::string::npos) {
        size_t path_start = clean_path.find('/', protocol_pos + 3);
        if (path_start != std::string::npos) {
            clean_path = clean_path.substr(path_start);
        } else {
            clean_path = "/";
        }
    }
    size_t query_pos = clean_path.find('?');
    if (query_pos != std::string::npos) {
        clean_path.erase(query_pos);
    }
    ss << method << clean_path;

    // // If we also needed to include a set of canonical headers at some point
    // for (const auto &[key, value] : canonical_headers) {
    //     ss << key << ":" << value;
    // }

    // Add nonce if present
    auto nonce_it = canonical_headers.find("x-nonce");
    if (nonce_it != canonical_headers.end()) {
        ss << nonce_it->second;
    }

    ss << body;

    return ss.str();
}

std::string ApiClient::getSignature(const std::string &canonical_payload) {
    esp_err_t err;

    ESP_LOGD(TAG, "[getSignature] canonical_payload: %s",
             ApiHelpers::bytes_hex(std::vector<uint8_t>(canonical_payload.begin(), canonical_payload.end()), false).c_str());

    // Get a SHA-256 digest of the data that will be passed to se_hmac
    uint8_t sha256_digest[32] = {0};
    if ((err = sha256(reinterpret_cast<const uint8_t *>(canonical_payload.data()), canonical_payload.size(), sha256_digest)) !=
        ESP_OK) {
        ESP_LOGE(TAG, "Failed to compute SHA-256: %s", esp_err_to_name(err));
        return "";
    }
    ESP_LOGD(TAG, "[getSignature] sha256_digest: %s",
             ApiHelpers::bytes_hex(std::vector<uint8_t>(sha256_digest, sha256_digest + sizeof(sha256_digest)), false).c_str());

    // Now compute the HMAC against the SHA-256 digest
    uint8_t hmac_digest[32] = {0};
    err                     = se_hmac(sha256_digest, 32, hmac_digest);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to compute HMAC: %s", esp_err_to_name(err));
        return "";
    }
    ESP_LOGD(TAG, "[getSignature] hmac_digest: %s",
             ApiHelpers::bytes_hex(std::vector<uint8_t>(hmac_digest, hmac_digest + sizeof(hmac_digest)), false).c_str());

    return ApiHelpers::bytes_hex(std::vector<uint8_t>(hmac_digest, hmac_digest + sizeof(hmac_digest)), true);
}

esp_err_t ApiClient::applyAuthHeaders(esp_http_client_handle_t client, const std::string_view method, const std::string_view path,
                                      const std::string_view payload) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }

    // Look up the method from the given method string
    esp_http_client_method_t http_method = HTTP_METHOD_GET;
    if (method_names.right.find(std::string(method)) != method_names.right.end()) {
        http_method = method_names.right.at(std::string(method));
    }

    // Determine the required auth level
    AuthLevel auth_level = getAuthLevel(path, http_method);

    // If we need more than API key auth then we need to make sure the secure element is ready
    if (auth_level > AuthLevel::BASIC && !se_ready()) {
        ESP_LOGE(TAG, "Secure element not ready - cannot make authenticated requests");
        return ESP_FAIL;
    }

    // Base headers for all requests
    std::map<std::string, std::string> headers = {
        {"X-API-Auth", api_key},
        {"X-Firmware-Version", FIRMWARE_VERSION_STRING},
    };

    // Handle challenge-response auth by requesting the challenge nonce
    if (auth_level == AuthLevel::CHALLENGE) {
        ESP_LOGD(TAG, "Requesting challenge nonce for endpoint: %s", sv_cstr(path));
        ApiResult challenge_response = doRequest("/api/nut/challenge", "POST");
        if (std::holds_alternative<ApiError>(challenge_response)) {
            ESP_LOGE(TAG, "Failed to get challenge nonce: %s", to_string(std::get<ApiError>(challenge_response)));
            return ESP_FAIL;
        }

        if (auto nonce = ApiHelpers::json_get<std::string>(std::get<ApiResponse>(challenge_response).body_json(), "data.nonce")) {
            headers["X-Nonce"] = *nonce;
            ESP_LOGD(TAG, "Got challenge nonce: %s", headers["X-Nonce"].c_str());
        } else {
            ESP_LOGE(TAG, "Failed to parse challenge nonce from response");
            return ESP_FAIL;
        }
    }

    // Get Host header from the client for signing
    char *host_ptr = nullptr;
    esp_http_client_get_header(client, "Host", &host_ptr);
    if (host_ptr != nullptr) {
        headers["Host"] = host_ptr;
    }

    // Compute HMAC signature if required
    if (auth_level >= AuthLevel::SIGNED) {
        std::string canonical_data = canonicalizedRequest(sv_cstr(method), sv_cstr(path), headers, sv_cstr(payload));
        if (canonical_data.empty()) {
            ESP_LOGE(TAG, "Failed to create canonical request data");
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "Canonical request data: %s", canonical_data.c_str());

        std::string signature = getSignature(canonical_data);
        if (signature.empty()) {
            ESP_LOGE(TAG, "Failed to compute HMAC signature");
            return ESP_FAIL;
        }
        headers["X-Signature"] = signature;
        ESP_LOGD(TAG, "Computed signature: %s", signature.c_str());
    }

    // Apply headers to the client
    for (const auto &[key, value] : headers) {
        char *existing_value = nullptr;
        esp_http_client_get_header(client, key.c_str(), &existing_value);
        if (existing_value == nullptr) {
            esp_err_t result = esp_http_client_set_header(client, key.c_str(), value.c_str());
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set header %s: %s", key.c_str(), esp_err_to_name(result));
                return result;
            }
            ESP_LOGD(TAG, "Set auth header %s: %s", key.c_str(), value.c_str());
        }
    }

    return ESP_OK;
}

ApiClient::ApiResult ApiClient::doRequest(const std::string_view endpoint, const std::string_view method,
                                          const std::string_view payload) {
    // Look up the method from the given method string
    esp_http_client_method_t http_method = HTTP_METHOD_GET;
    if (method_names.right.find(std::string(method)) != method_names.right.end()) {
        http_method = method_names.right.at(std::string(method));
    }

    // Per-request context
    RequestContext context;
    if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
        context.log_data = true;
    }

    // Set up the HTTP client configuration
    esp_http_client_config_t config = {};
    std::string url                 = std::string(API_BASE_URL) + endpoint.data();
    config.url                      = url.c_str();
    config.cert_pem                 = reinterpret_cast<const char *>(isrgrootx1_cert);
    config.cert_len                 = isrgrootx1_cert_end - isrgrootx1_cert;
    config.event_handler            = httpEventHandler;
    config.user_data                = &context;
    config.method                   = http_method;

    // Optimize better for large payloads to handle things like the topology data quicker
    config.buffer_size           = 8192;  // Larger 8KB buffer
    config.timeout_ms            = 10000; // 10 second timeout for large payloads
    config.keep_alive_enable     = true;  // Enable keep-alive for connection reuse
    config.keep_alive_idle       = 5;     // Keep connection alive for 5 seconds
    config.keep_alive_interval   = 1;     // Send keep-alive packets every 1 second
    config.keep_alive_count      = 3;     // Send 3 keep-alive packets before giving up
    config.disable_auto_redirect = true;  // Disable redirects to avoid extra round trips

    // Initialize the HTTP client
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ApiError::REQUEST_INIT_ERROR;
    }

    AuthLevel auth_level = getAuthLevel(endpoint, http_method);
    ESP_LOGD(TAG, "Making %s request to: %s", method.data(), url.c_str());
    ESP_LOGD(TAG, "Auth level required: %d", (int)auth_level);

    // Set the payload if it exists for POST requests
    if (!payload.empty()) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload.data(), payload.length());
    }

    // Set Accept header for JSON responses
    esp_http_client_set_header(client, "Accept", "application/json");

    // Apply authentication headers based on auth level
    esp_err_t auth_result = applyAuthHeaders(client, method, endpoint, payload);
    if (auth_result != ESP_OK) {
        esp_http_client_cleanup(client);
        return ApiError::REQUEST_AUTH_ERROR;
    }

    ESP_LOGD(TAG, "HTTP Request: %s %s -- %s", config.method == HTTP_METHOD_POST ? "POST" : "GET", config.url,
             payload.empty() ? "" : payload.data());

    // Perform the HTTP request
    ApiResponse response;
    int64_t start_time = esp_timer_get_time(); // Start timing the request
    if (esp_err_t err = esp_http_client_perform(client); err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s (0x%x)", esp_err_to_name(err), err);

        // Get more details about the failure
        int status_code    = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGE(TAG, "HTTP status: %d, content_length: %d", status_code, content_length);

        if (err == ESP_FAIL) {
            ESP_LOGE(TAG, "HTTP client failure");
        }
    } else {
        int64_t end_time    = esp_timer_get_time(); // Grab the end time
        int64_t duration_ms = (end_time - start_time) / 1000;

        response.status_code = esp_http_client_get_status_code(client);
        response.body        = context.response_buffer;
        response.headers     = context.response_headers;
        int content_length   = esp_http_client_get_content_length(client);

        ESP_LOGD(TAG, "HTTP Status = %d, content_length = %d, duration = %lldms, body_size = %zu", response.status_code,
                 content_length, duration_ms, response.body.size());

        // Log performance info for larger payloads
        if (content_length > 10000) {
            float throughput_kbps = content_length > 0 && duration_ms > 0 ? (float)content_length / (float)duration_ms : 0.0f;
            ESP_LOGD(TAG, "Large payload performance: %.1f KB/s throughput", throughput_kbps);
        }

        if (response.status_code >= 400) {
            ESP_LOGW(TAG, "HTTP request returned error status: %d", response.status_code);
            if (!context.response_buffer.empty()) {
                ESP_LOGW(TAG, "Response body: %s", response.body.c_str());
            }
        }
    }

    // Clean up the HTTP client
    esp_http_client_cleanup(client);

    return response;
}

// ---------------------------------------------------------------------------------------------
// Provisioning API endpoints
// ---------------------------------------------------------------------------------------------

ApiClient::ApiResult ApiClient::provisionSecrets() {
    return doRequest("/api/nut/provision", "POST");
}

ApiClient::ApiResult ApiClient::provisionWirelessPSK() {
    return doRequest("/api/nut/provision/wireless-psk", "POST");
}

ApiClient::ApiResult ApiClient::provisionVerify() {
    return doRequest("/api/nut/provision/verify", "POST");
}

// ------------------------------------------------------------------------------------------------
// Nut API endpoints
// ------------------------------------------------------------------------------------------------

ApiClient::ApiResult ApiClient::getStatus() {
    return doRequest("/api/nut/status", "GET");
}

ApiClient::ApiResult ApiClient::requestCode(int level) {
    if (nut_config.type == NUT_TYPE_COMMUNITY && (level < 1 || level > 3)) {
        ESP_LOGW(TAG, "Community nuts can only request code levels 1-3");
        return ApiError::REQUEST_VALIDATION_ERROR;
    }
    if (nut_config.type != NUT_TYPE_COMMUNITY && level != 1) {
        ESP_LOGW(TAG, "Non-community nuts can only request code level 1");
        return ApiError::REQUEST_VALIDATION_ERROR;
    }
    json payload = {{"level", level}};
    return doRequest("/api/nut/code/request", "POST", payload.dump());
}

ApiClient::ApiResult ApiClient::getCodeStatus(const std::string_view code) {
    return doRequest(std::format("/api/nut/code/status/{}", code), "GET");
}

ApiClient::ApiResult ApiClient::sendSyslog(syslog_level_t level, const std::string_view message) {
    if (level <= SYSLOG_LEVEL_NONE || level >= SYSLOG_LEVEL_MAX) {
        return ApiError::REQUEST_VALIDATION_ERROR;
    }
    json payload = {{"message", message}, {"level", syslog_level_strs[static_cast<int>(level)]}};
    return doRequest("/api/nut/syslog", "POST", payload.dump());
}

// ------------------------------------------------------------------------------------------------
// Firmware API endpoints
// ------------------------------------------------------------------------------------------------

ApiClient::ApiResult ApiClient::getFirmwareVersion() {
    return doRequest("/api/firmware/version", "GET");
}

ApiClient::ApiResult ApiClient::getFirmwareHead() {
    return doRequest("/api/firmware/nut", "HEAD");
}

api_err_t ApiClient::doFirmwareUpdate() {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element not ready - cannot perform signed OTA");
        return api_err_t::API_FAIL;
    }

    // Set up the HTTP client configuration
    esp_http_client_config_t config = {};
    std::string url                 = std::string(API_BASE_URL) + "/api/firmware/nut";
    config.url                      = url.c_str();
    config.cert_pem                 = (const char *)isrgrootx1_cert;
    config.cert_len                 = isrgrootx1_cert_end - isrgrootx1_cert;
    config.method                   = HTTP_METHOD_GET;
    config.user_data                = this;
    config.keep_alive_enable        = true;
    config.timeout_ms               = 10000;
    config.buffer_size              = OTA_BUFFER_SIZE;
    config.buffer_size_tx           = OTA_BUFFER_SIZE / 2;

    // Set up the OTA configuration
    esp_https_ota_config_t ota_config = {};
    ota_config.bulk_flash_erase       = true;
    ota_config.partial_http_download  = true;
    ota_config.max_http_request_size  = OTA_BUFFER_SIZE;
    ota_config.http_config            = &config;
    ota_config.http_client_init_cb    = [](esp_http_client_handle_t client) -> esp_err_t {
        ApiClient *self = nullptr;
        esp_err_t ret   = esp_http_client_get_user_data(client, (void **)&self);
        if (ret != ESP_OK || self == nullptr) {
            ESP_LOGE(TAG, "OTA init: missing ApiClient user_data");
            return ESP_FAIL;
        }

        ret = self->applyAuthHeaders(client, "GET", "/api/firmware/nut");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set OTA signed headers: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGD(TAG, "OTA signed headers applied");
        return ESP_OK;
    };

    // Error var to capture any errors during the OTA process
    esp_err_t err;
    bool checking = false;

    // Get the current power save mode and disable it for the duration OTA process
    wifi_ps_type_t orig_wifi_ps_type;
    esp_wifi_get_ps(&orig_wifi_ps_type);
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Start the OTA process
    esp_https_ota_handle_t ota_handle = nullptr;
    if (esp_https_ota_begin(&ota_config, &ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to begin HTTPS OTA");
        return api_err_t::API_FAIL;
    } else {
        checking = true;
        set_ota_status(ota_status_t::OTA_STATUS_CHECKING);
    }

    // Abort helper function for OTA process to clean up and return an error
    auto ota_abort = [&checking, orig_wifi_ps_type, &ota_handle](std::string_view message = "", auto... args) {
        if (!message.empty()) {
            esp_log_write(ESP_LOG_WARN, TAG,
                          (std::string(LOG_COLOR_W "W (%lu) %s: ") + message.data() + LOG_RESET_COLOR + "\n").c_str(),
                          esp_log_timestamp(), TAG, args...);
        }
        esp_wifi_set_ps(orig_wifi_ps_type);
        esp_https_ota_abort(ota_handle);
        set_ota_message(message.empty() ? "Update failed" : message.data());
        set_ota_status(checking ? ota_status_t::OTA_STATUS_CHECK_FAILED : ota_status_t::OTA_STATUS_FAILED);
        checking = false;
        return api_err_t::API_FAIL;
    };

    // Get the OTA image header
    esp_app_desc_t ota_app_desc;
    if ((err = esp_https_ota_get_img_desc(ota_handle, &ota_app_desc)) != ESP_OK) {
        return ota_abort("Failed to get image description: %s", esp_err_to_name(err));
    }

    // Validate the OTA image version against the current firmware version
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_app_desc;
    if (esp_ota_get_partition_description(running_partition, &running_app_desc) != ESP_OK) {
        return ota_abort("Failed to get running partition description");
    } else {
        version_t running_version;
        version_t ota_version;
        version_parse(running_app_desc.version, &running_version);
        version_parse(ota_app_desc.version, &ota_version);
        if (version_compare(&running_version, &ota_version) >= 0) {
            return ota_abort("Firmware is already up to date");
        }
        checking = false;
        set_ota_status(ota_status_t::OTA_STATUS_DOWNLOADING);
    }

    led_pattern_t blink_otaupdate = {.type        = LED_PATTERN_BLINK,
                                     .red         = 0,
                                     .green       = 0,
                                     .blue        = 255,
                                     .on_ms       = 250,
                                     .off_ms      = 250,
                                     .blink_count = 0, // blink forever
                                     .speed_ms    = 50,
                                     .leds_on     = 1,
                                     .chase_count = 0,
                                     .chase_dir   = true,
                                     .fade        = false};
    led_pattern_set(&blink_otaupdate);

    // Main update loop
    while (true) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        int bytes_read = esp_https_ota_get_image_len_read(ota_handle);
        int total_size = esp_https_ota_get_image_size(ota_handle);
        ESP_LOGD(TAG, "OTA in progress... read: %d/%d bytes [%.2f%%]", bytes_read, total_size,
                 double(bytes_read * 100) / total_size);
        set_ota_progress({bytes_read, total_size});
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (esp_https_ota_is_complete_data_received(ota_handle) != true) {
        return ota_abort("Failed to receive complete data");
    } else {
        esp_err_t fin_err = esp_https_ota_finish(ota_handle);
        if ((err == ESP_OK) && (fin_err == ESP_OK)) {
            set_ota_message("Upgrade complete... rebooting");
            ESP_LOGI(TAG, "OTA upgrade finished successfully... rebooting");
            vTaskDelay(pdMS_TO_TICKS(1000));
            set_ota_status(ota_status_t::OTA_STATUS_SUCCESS);
            esp_restart();
        } else {
            if (fin_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                return ota_abort("Failed to validate OTA image");
            }
            return ota_abort("Failed to finish OTA: %s", esp_err_to_name(fin_err));
        }
    }

    // Restore the original WiFi power save mode
    esp_wifi_set_ps(orig_wifi_ps_type);

    return api_err_t::API_OK;
}

// ---------------------------------------------------------------------------------------------
// Telecom API endpoints
// ---------------------------------------------------------------------------------------------

ApiClient::ApiResult ApiClient::sendTelecomPortStatus(const std::vector<api_telecom_port_status_t> &port_statuses) {
    json connections_json = json::array();
    for (const auto &port_status : port_statuses) {
        connections_json.push_back({
            {"port_number", port_status.port_number},
            {"connected", port_status.connected},
        });
    }
    json payload = {{"connections", connections_json}};
    return doRequest("/api/telecom/connections/update", "POST", payload.dump());
}

ApiClient::ApiResult ApiClient::sendTelecomStatus(bool online) {
    json payload = {{"is_online", online}};
    return doRequest("/api/telecom/status", "POST", payload.dump());
}

ApiClient::ApiResult ApiClient::sendTelecomHeartbeat() {
    return doRequest("/api/telecom/heartbeat", "GET");
}

ApiClient::ApiResult ApiClient::sendTelecomLocked() {
    return doRequest("/api/telecom/lock", "POST", "{}");
}

ApiClient::ApiResult ApiClient::sendTelecomSyslog(const std::string_view message, syslog_level_t level,
                                                  const std::string_view category) {
    json payload = {{"message", message}, {"message_type", syslog_level_to_string(level)}, {"category", category}};
    return doRequest("/api/telecom/syslog", "POST", payload.dump());
}
