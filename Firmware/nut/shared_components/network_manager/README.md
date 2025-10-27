# Network Manager

A unified network interface abstraction layer for ESP-IDF that provides seamless WiFi and Ethernet support with automatic failover.

## Features

- **Unified Interface**: Single API for both WiFi and Ethernet connections
- **Automatic Preference**: Prefers Ethernet over WiFi when both are available
- **Seamless Failover**: Automatically uses WiFi when Ethernet is unavailable
- **Event System**: Status callbacks compatible with existing WiFi-based code
- **ESP-IDF Routing**: Leverages ESP-IDF's automatic routing priority (Ethernet > WiFi)

## Architecture

The network manager sits on top of `wifi_manager` and `ethernet_init` components, providing:

1. **Status Abstraction**: Translates WiFi/Ethernet events into unified `NETWORK_STATUS_*` events
2. **Connection Priority**: Automatically selects the best available connection
3. **Callback System**: Notifies application components of network status changes
4. **Default Interface**: Provides the correct `esp_netif_t` for HTTP clients and other services

## Usage

### Basic Initialization

```c
#include "network_manager.h"

void app_main() {
    // Determine configuration based on hardware
    bool has_ethernet = (nut_config.hw_type == NUT_HW_TYPE_TELECOM);

    network_manager_config_t nm_config = {
        .enable_wifi = true,                      // Always enable WiFi
        .enable_ethernet = has_ethernet,          // Enable Ethernet for TELECOM hardware
        .ethernet_fallback_to_wifi = true,        // Use WiFi if Ethernet fails
        .wifi_fallback_to_ethernet = has_ethernet,// Keep trying Ethernet even after WiFi connects
        .ethernet_retry_interval_ms = 30000,      // Retry Ethernet every 30 seconds
    };

    ESP_ERROR_CHECK(network_manager_init(&nm_config));
}
```

### Registering Status Callbacks

```c
void network_status_changed(network_status_t status, network_type_t type) {
    if (status == NETWORK_STATUS_CONNECTED) {
        const char *type_str = (type == NETWORK_TYPE_ETHERNET) ? "Ethernet" : "WiFi";
        ESP_LOGI(TAG, "Connected via %s", type_str);

        // Perform network-dependent tasks
        sync_with_server();
        check_for_updates();
    } else {
        ESP_LOGW(TAG, "Network disconnected");
    }
}

// In your init function:
network_manager_add_status_callback(network_status_changed);
```

### Checking Connection Status

```c
// Simple boolean check
if (network_manager_is_connected()) {
    // Make HTTP request, etc.
}

// Get detailed information
network_info_t info;
network_manager_get_info(&info);

ESP_LOGI(TAG, "Status: %d, Primary: %s, WiFi: %d, Ethernet: %d",
         info.status,
         info.primary_type == NETWORK_TYPE_ETHERNET ? "Ethernet" : "WiFi",
         info.wifi_available,
         info.ethernet_available);
```

### Using with HTTP Clients

The network manager automatically handles ESP-IDF's routing priority. When both connections are active, traffic will prefer Ethernet:

```c
esp_http_client_config_t config = {
    .url = "https://example.com/api",
    // No need to specify interface - ESP-IDF routes via Ethernet automatically
};

esp_http_client_handle_t client = esp_http_client_init(&config);
```

## How ESP-IDF Routing Works

ESP-IDF automatically prioritizes network interfaces based on their **route metrics**:

1. **Ethernet**: Metric = 10 (lower is better, so preferred)
2. **WiFi**: Metric = 20

When both interfaces are up:
- Default route uses Ethernet (metric 10)
- All outbound connections automatically use Ethernet
- No application code changes needed

You can verify this with:
```c
esp_netif_t *default_netif = esp_netif_get_default_netif();
// This returns Ethernet netif when both are connected
```

## Migration from wifi_manager

### Before (wifi_manager only):
```c
// Old code
void wifi_status_callback(wifi_status_t status) {
    if (status == WIFI_STATUS_CONNECTED) {
        // Do network tasks
    }
}

add_wifi_status_callback(wifi_status_callback);

if (nut_state.wifi_status == WIFI_STATUS_CONNECTED) {
    // Make API call
}
```

### After (network_manager):
```c
// New code
void network_status_callback(network_status_t status, network_type_t type) {
    if (status == NETWORK_STATUS_CONNECTED) {
        // Do network tasks (works for both WiFi and Ethernet!)
        ESP_LOGI(TAG, "Connected via %s",
                 type == NETWORK_TYPE_ETHERNET ? "Ethernet" : "WiFi");
    }
}

network_manager_add_status_callback(network_status_callback);

if (nut_state.network_status == NETWORK_STATUS_CONNECTED) {
    // Make API call (works regardless of connection type!)
}
```

## Configuration Hardware Types

### WiFi-Only Devices (Generic NUT)
```c
network_manager_config_t config = {
    .enable_wifi = true,
    .enable_ethernet = false,
    .ethernet_fallback_to_wifi = false,
    .wifi_fallback_to_ethernet = false,
    .ethernet_retry_interval_ms = 0,
};
```

### Telecom Devices (Ethernet + WiFi Fallback)
```c
network_manager_config_t config = {
    .enable_wifi = true,
    .enable_ethernet = true,
    .ethernet_fallback_to_wifi = true,     // WiFi is backup for Ethernet
    .wifi_fallback_to_ethernet = true,     // Keep trying Ethernet even when WiFi works
    .ethernet_retry_interval_ms = 30000,   // Retry every 30 seconds
};
```

## API Reference

### Types

```c
typedef enum {
    NETWORK_TYPE_NONE,
    NETWORK_TYPE_WIFI,
    NETWORK_TYPE_ETHERNET,
} network_type_t;

typedef enum {
    NETWORK_STATUS_DISCONNECTED,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
} network_status_t;

typedef void (*network_status_callback_t)(network_status_t status, network_type_t type);
```

### Functions

- `esp_err_t network_manager_init(const network_manager_config_t *config)`
- `esp_err_t network_manager_add_status_callback(network_status_callback_t cb)`
- `network_status_t network_manager_get_status()`
- `network_type_t network_manager_get_primary_type()`
- `bool network_manager_is_connected()`
- `esp_err_t network_manager_force_reconnect()`
- `esp_netif_t *network_manager_get_default_netif()`

## Benefits

1. **Simplified Code**: One API for all network operations
2. **Hardware Abstraction**: Same code works on WiFi-only and Ethernet+WiFi devices
3. **Automatic Failover**: No manual switching logic needed
4. **Future-Proof**: Easy to add new network types (e.g., cellular)
5. **Event Consistency**: All network events follow the same pattern

## Implementation Notes

- WiFi manager is still used internally for WiFi credential management
- Ethernet initialization (`ethernet_init`) is wrapped but not replaced
- ESP-IDF's netif system handles actual packet routing
- Callbacks are fired from event task context (don't block!)
- Up to 5 status callbacks can be registered simultaneously
