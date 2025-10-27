#include "network_manager.h"
#include "wifi_manager.h"
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "network_manager";

// Network manager state
static struct {
    bool initialized;
    network_manager_config_t config;
    network_status_t status;
    network_type_t primary_type;

    // Connection states
    bool wifi_connected;
    bool ethernet_connected;
    esp_netif_t *wifi_netif;
    esp_netif_t *ethernet_netif;

    // Callbacks
    network_status_callback_t callbacks[NETWORK_STATUS_CALLBACK_MAX];
    uint8_t callback_count;

    // Ethernet retry timer
    TimerHandle_t ethernet_retry_timer;

} nm_state = {
    .initialized          = false,
    .status               = NETWORK_STATUS_DISCONNECTED,
    .primary_type         = NETWORK_TYPE_NONE,
    .wifi_connected       = false,
    .ethernet_connected   = false,
    .wifi_netif           = NULL,
    .ethernet_netif       = NULL,
    .callback_count       = 0,
    .ethernet_retry_timer = NULL,
};

// Forward declarations
static void update_network_status();
static void notify_status_change();
static void wifi_status_change_handler(wifi_status_t wifi_status);
static void ethernet_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ethernet_retry_timer_callback(TimerHandle_t timer);
static esp_err_t init_sntp();
static void time_sync_notification_cb(struct timeval *tv);

/**
 * @brief SNTP time synchronization callback
 */
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "SNTP time sync complete");

    // Print the current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current date/time: %s", asctime(&timeinfo));
}

/**
 * @brief Initialize the SNTP service and set the timezone
 *
 * @return esp_err_t
 */
static esp_err_t init_sntp() {
    // Set the timezone to Mountain Time
    setenv("TZ", "MST7MDT,M3.2.0,M11.1.0", 1);
    tzset();

    // Initialize SNTP with multiple servers
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("time.aws.com", "pool.ntp.org"));
    config.sync_cb           = time_sync_notification_cb;
    return esp_netif_sntp_init(&config);
}

esp_err_t network_manager_init(const network_manager_config_t *config) {
    if (nm_state.initialized) {
        ESP_LOGW(TAG, "Network manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    if (!config->enable_wifi && !config->enable_ethernet) {
        ESP_LOGE(TAG, "At least one network interface must be enabled");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing network manager (WiFi=%d, Ethernet=%d)", config->enable_wifi, config->enable_ethernet);

    // Store configuration
    nm_state.config = *config;

    // Initialize WiFi if enabled
    if (config->enable_wifi) {
        ESP_LOGI(TAG, "Initializing WiFi...");
        esp_err_t err = wifi_manager_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(err));
            return err;
        }

        // Register WiFi status callback
        add_wifi_status_callback(wifi_status_change_handler);

        // Get WiFi netif
        nm_state.wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }

    // Initialize Ethernet if enabled
    if (config->enable_ethernet) {
        ESP_LOGI(TAG, "Initializing Ethernet...");

        // Register Ethernet event handlers
        ESP_LOGI(TAG, "Registering ETH_EVENT handler");
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethernet_event_handler, NULL));
        ESP_LOGI(TAG, "Registering IP_EVENT_ETH_GOT_IP handler");
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL));
        ESP_LOGI(TAG, "Ethernet event handlers registered successfully");

        // Create retry timer if configured
        if (config->ethernet_retry_interval_ms > 0) {
            nm_state.ethernet_retry_timer = xTimerCreate("eth_retry", pdMS_TO_TICKS(config->ethernet_retry_interval_ms),
                                                         pdTRUE, // Auto-reload
                                                         NULL, ethernet_retry_timer_callback);
        }
    }

    // Register IP event handler for WiFi
    if (config->enable_wifi) {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_event_handler, NULL));
    }

    // Initialize SNTP for time synchronization (works for both WiFi and Ethernet)
    esp_err_t sntp_err = init_sntp();
    if (sntp_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize SNTP: %s (continuing anyway)", esp_err_to_name(sntp_err));
    }

    nm_state.initialized = true;
    update_network_status();

    ESP_LOGI(TAG, "Network manager initialized successfully");
    return ESP_OK;
}

esp_err_t network_manager_add_status_callback(network_status_callback_t cb) {
    if (!nm_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (nm_state.callback_count >= NETWORK_STATUS_CALLBACK_MAX) {
        ESP_LOGE(TAG, "Maximum number of callbacks reached");
        return ESP_ERR_NO_MEM;
    }

    nm_state.callbacks[nm_state.callback_count++] = cb;

    // Immediately notify callback of current status
    if (cb != NULL) {
        cb(nm_state.status, nm_state.primary_type);
    }

    return ESP_OK;
}

network_status_t network_manager_get_status() {
    return nm_state.status;
}

esp_err_t network_manager_get_info(network_info_t *info) {
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    info->status             = nm_state.status;
    info->primary_type       = nm_state.primary_type;
    info->wifi_available     = nm_state.wifi_connected;
    info->ethernet_available = nm_state.ethernet_connected;
    info->wifi_netif         = nm_state.wifi_netif;
    info->ethernet_netif     = nm_state.ethernet_netif;

    return ESP_OK;
}

network_type_t network_manager_get_primary_type() {
    return nm_state.primary_type;
}

bool network_manager_is_connected() {
    return nm_state.status == NETWORK_STATUS_CONNECTED;
}

esp_err_t network_manager_force_reconnect() {
    if (!nm_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Forcing network reconnection...");

    if (nm_state.config.enable_wifi) {
        wifi_force_reconnect();
    }

    // TODO: Add ethernet reconnection if needed

    return ESP_OK;
}

esp_err_t network_manager_set_wifi_enabled(bool enable) {
    if (!nm_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!nm_state.config.enable_wifi) {
        ESP_LOGW(TAG, "WiFi is not configured");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "%s WiFi", enable ? "Enabling" : "Disabling");

    // TODO: Implement WiFi enable/disable
    // For now this is a placeholder

    return ESP_OK;
}

esp_netif_t *network_manager_get_default_netif() {
    // Prefer Ethernet over WiFi when both are available
    if (nm_state.ethernet_connected && nm_state.ethernet_netif != NULL) {
        return nm_state.ethernet_netif;
    }

    if (nm_state.wifi_connected && nm_state.wifi_netif != NULL) {
        return nm_state.wifi_netif;
    }

    return NULL;
}

// ============================================================================
// Internal functions
// ============================================================================

static void update_network_status() {
    network_status_t old_status = nm_state.status;
    network_type_t old_primary  = nm_state.primary_type;

    // Determine overall status
    if (nm_state.ethernet_connected || nm_state.wifi_connected) {
        nm_state.status = NETWORK_STATUS_CONNECTED;

        // Prefer Ethernet over WiFi
        if (nm_state.ethernet_connected) {
            nm_state.primary_type = NETWORK_TYPE_ETHERNET;
        } else {
            nm_state.primary_type = NETWORK_TYPE_WIFI;
        }
    } else {
        nm_state.status       = NETWORK_STATUS_DISCONNECTED;
        nm_state.primary_type = NETWORK_TYPE_NONE;
    }

    // Log status changes
    if (old_status != nm_state.status || old_primary != nm_state.primary_type) {
        const char *status_str = (nm_state.status == NETWORK_STATUS_CONNECTED)    ? "CONNECTED"
                                 : (nm_state.status == NETWORK_STATUS_CONNECTING) ? "CONNECTING"
                                                                                  : "DISCONNECTED";
        const char *type_str   = (nm_state.primary_type == NETWORK_TYPE_ETHERNET) ? "ETHERNET"
                                 : (nm_state.primary_type == NETWORK_TYPE_WIFI)   ? "WIFI"
                                                                                  : "NONE";

        ESP_LOGI(TAG, "Network status: %s via %s (WiFi=%d, Eth=%d)", status_str, type_str, nm_state.wifi_connected,
                 nm_state.ethernet_connected);

        notify_status_change();
    }

    // Handle fallback logic
    if (nm_state.config.enable_ethernet && nm_state.config.enable_wifi) {
        if (!nm_state.ethernet_connected && nm_state.config.ethernet_fallback_to_wifi) {
            // Ethernet is down, ensure WiFi is enabled
            if (!nm_state.wifi_connected) {
                ESP_LOGD(TAG, "Ethernet down, WiFi should be connecting...");
            }
        }

        // Start ethernet retry timer if ethernet is down and retry is configured
        if (!nm_state.ethernet_connected && nm_state.ethernet_retry_timer != NULL) {
            if (xTimerIsTimerActive(nm_state.ethernet_retry_timer) == pdFALSE) {
                ESP_LOGD(TAG, "Starting Ethernet retry timer");
                xTimerStart(nm_state.ethernet_retry_timer, 0);
            }
        } else if (nm_state.ethernet_connected && nm_state.ethernet_retry_timer != NULL) {
            // Stop retry timer if ethernet is connected
            if (xTimerIsTimerActive(nm_state.ethernet_retry_timer) == pdTRUE) {
                ESP_LOGD(TAG, "Stopping Ethernet retry timer");
                xTimerStop(nm_state.ethernet_retry_timer, 0);
            }
        }
    }
}

static void notify_status_change() {
    for (uint8_t i = 0; i < nm_state.callback_count; i++) {
        if (nm_state.callbacks[i] != NULL) {
            nm_state.callbacks[i](nm_state.status, nm_state.primary_type);
        }
    }
}

static void wifi_status_change_handler(wifi_status_t wifi_status) {
    ESP_LOGD(TAG, "WiFi status change: %d", wifi_status);

    bool was_connected      = nm_state.wifi_connected;
    nm_state.wifi_connected = (wifi_status == WIFI_STATUS_CONNECTED);

    if (was_connected != nm_state.wifi_connected) {
        update_network_status();
    }
}

static void ethernet_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case ETHERNET_EVENT_START: ESP_LOGI(TAG, "Ethernet started"); break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            nm_state.ethernet_connected = false;
            update_network_status();
            break;

        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet link up");
            // Don't mark as connected yet - wait for IP
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet link down");
            nm_state.ethernet_connected = false;
            update_network_status();
            break;

        default: break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "IP event handler called: event_id=%ld", event_id);

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "WiFi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            nm_state.wifi_netif = event->esp_netif;
            // WiFi connection is already tracked by wifi_status_change_handler
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "WiFi lost IP");
            // Connection status still tracked by wifi_status_change_handler
            break;

        case IP_EVENT_ETH_GOT_IP: {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Ethernet got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            nm_state.ethernet_netif     = event->esp_netif;
            nm_state.ethernet_connected = true;
            ESP_LOGD(TAG, "Setting ethernet_connected=true, calling update_network_status()");
            update_network_status();
            break;
        }

        default: ESP_LOGD(TAG, "Unhandled IP event: %ld", event_id); break;
    }
}

static void ethernet_retry_timer_callback(TimerHandle_t timer) {
    ESP_LOGD(TAG, "Ethernet retry timer fired");

    if (!nm_state.ethernet_connected) {
        ESP_LOGI(TAG, "Attempting to restart Ethernet...");
        // TODO: Add ethernet restart logic if needed
    }
}
