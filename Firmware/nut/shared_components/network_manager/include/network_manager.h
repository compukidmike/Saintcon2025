#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"

/**
 * @brief Network connection type
 */
typedef enum {
    NETWORK_TYPE_NONE,     ///< No network connection
    NETWORK_TYPE_WIFI,     ///< WiFi connection
    NETWORK_TYPE_ETHERNET, ///< Ethernet connection
} network_type_t;

/**
 * @brief Network connection status
 */
typedef enum {
    NETWORK_STATUS_DISCONNECTED, ///< No network connection (neither WiFi nor Ethernet)
    NETWORK_STATUS_CONNECTING,   ///< Attempting to connect
    NETWORK_STATUS_CONNECTED,    ///< Connected (WiFi or Ethernet or both)
} network_status_t;

/**
 * @brief Network connection info
 */
typedef struct {
    network_status_t status;     ///< Overall network status
    network_type_t primary_type; ///< Primary (preferred) connection type currently active
    bool wifi_available;         ///< WiFi connection is available
    bool ethernet_available;     ///< Ethernet connection is available
    esp_netif_t *wifi_netif;     ///< WiFi network interface (if active)
    esp_netif_t *ethernet_netif; ///< Ethernet network interface (if active)
} network_info_t;

/**
 * @brief Network status callback function type
 *
 * Called whenever the network connection status changes. This callback
 * will be invoked from the event task context, so it should not block.
 *
 * @param status Current network connection status
 * @param type Primary connection type (WIFI or ETHERNET if connected)
 */
typedef void (*network_status_callback_t)(network_status_t status, network_type_t type);

#define NETWORK_STATUS_CALLBACK_MAX 5

/**
 * @brief Network manager configuration
 */
typedef struct {
    bool enable_wifi;                    ///< Enable WiFi support
    bool enable_ethernet;                ///< Enable Ethernet support (only for telecom hardware)
    bool ethernet_fallback_to_wifi;      ///< If true, enable WiFi when Ethernet fails
    bool wifi_fallback_to_ethernet;      ///< If true, keep trying Ethernet even when WiFi connects
    uint32_t ethernet_retry_interval_ms; ///< How often to retry Ethernet if it fails (0 = no retry)
} network_manager_config_t;

/**
 * @brief Initialize the network manager
 *
 * This initializes both WiFi and/or Ethernet based on the configuration.
 * For telecom hardware with W5500, both can be enabled with Ethernet preferred.
 * For other hardware, only WiFi is enabled.
 *
 * @param config Network manager configuration
 * @return
 *     - ESP_OK: Network manager initialized successfully
 *     - ESP_ERR_INVALID_ARG: Invalid configuration
 *     - ESP_ERR_INVALID_STATE: Network manager already initialized
 */
esp_err_t network_manager_init(const network_manager_config_t *config);

/**
 * @brief Add a callback for network status changes
 *
 * The callback will be called whenever the network connection status changes
 * (connected, disconnected, etc.). Multiple callbacks can be registered.
 *
 * @param cb Callback function
 * @return
 *     - ESP_OK: Callback registered successfully
 *     - ESP_ERR_NO_MEM: Maximum number of callbacks reached
 */
esp_err_t network_manager_add_status_callback(network_status_callback_t cb);

/**
 * @brief Get the current network connection status
 *
 * @return Current network connection status
 */
network_status_t network_manager_get_status();

/**
 * @brief Get detailed network connection information
 *
 * @param[out] info Network connection information
 * @return
 *     - ESP_OK: Information retrieved successfully
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t network_manager_get_info(network_info_t *info);

/**
 * @brief Get the primary (preferred) connection type
 *
 * Returns the connection type that is currently being used for network traffic.
 * When both WiFi and Ethernet are connected, this returns ETHERNET (preferred).
 *
 * @return Primary connection type (NONE, WIFI, or ETHERNET)
 */
network_type_t network_manager_get_primary_type();

/**
 * @brief Check if network is connected (WiFi or Ethernet)
 *
 * @return true if any network connection is available
 */
bool network_manager_is_connected();

/**
 * @brief Force a reconnection attempt
 *
 * This will disconnect and reconnect WiFi (and retry Ethernet if enabled).
 * Useful after credential changes or network configuration updates.
 *
 * @return
 *     - ESP_OK: Reconnection initiated successfully
 */
esp_err_t network_manager_force_reconnect();

/**
 * @brief Enable or disable WiFi interface
 *
 * @param enable True to enable WiFi, false to disable
 * @return
 *     - ESP_OK: WiFi state changed successfully
 */
esp_err_t network_manager_set_wifi_enabled(bool enable);

/**
 * @brief Get the default network interface for outbound connections
 *
 * This returns the netif that should be used for HTTP requests, etc.
 * When both WiFi and Ethernet are available, returns Ethernet (preferred).
 *
 * @return Network interface handle, or NULL if no connection available
 */
esp_netif_t *network_manager_get_default_netif();

#ifdef __cplusplus
}
#endif
