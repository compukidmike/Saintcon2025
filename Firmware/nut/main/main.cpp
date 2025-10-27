#include "Arduino.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"

#include "ethernet_init.h"

#include "nut.h"
#include "nut/code_manager.h"
#include "i2c_manager.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "secure_element.h"
#include "wifi_manager.h"
#include "network_manager.h"
#include "led.h"
#include "IotReader.h"
#include <Logging.h>
#include "mcp23x17.h"
#include "led_patterns.h"
#include "api.h"

#define CW_PIN      (gpio_num_t)1
#define CCW_PIN     (gpio_num_t)0
#define LEVEL_PIN   (gpio_num_t)21
#define NFC_SEN_PIN (gpio_num_t)20

#define DEBOUNCE_TIME_US       50000   // 50ms
#define ACTIVATION_THRESHOLD   3       // must hit 2 valid activations
#define DOUBLE_PRESS_WINDOW_US 2000000 // 2 seconds

#define LED_COUNT      6
#define FADE_STEPS     30 // Number of fade frames per transition
#define FRAME_DELAY_MS 10 // Delay between fade frames (approx 100Hz update)

#define BUTTON1_I2C_PIN     10
#define BUTTON2_I2C_PIN     11
#define BUTTONLIGHT_I2C_PIN 12

// Struct for ISR -> task events
typedef struct {
    int switch_id;
} switch_event_t;

static QueueHandle_t switch_evt_queue = NULL;

// State tracking for each switch
typedef struct {
    gpio_num_t gpio_num;
    int id;
    int activation_count;
    int64_t last_time_us;
} switch_state_t;

static switch_state_t switches[] = {
    {CW_PIN, 1, 0, 0},
    {CCW_PIN, 2, 0, 0},
};

// Tracks which switch was last activated
static volatile int last_switch_id = -1;

static const char *TAG = "main";

TaskHandle_t nfcTaskHandle;

static led_pattern_t current_pattern;
static SemaphoreHandle_t pattern_mutex;

uint32_t code_level              = 1;
volatile bool code_level_changed = false;

bool doorIsOpen = false;

// lv_timer_t *ledtimer;

void led_update();
void nfcupdate(void);
esp_err_t nfcWriteCode(uint16_t timeout);
void patchPanelUpdate();
void checkPortExpanderIO();
extern "C" void openDoor();
void checkButtons();
void checkInputs();
static void setup_rotation_interrupts(void);

static void leds_off(void);
void led_pattern_set(const led_pattern_t *pattern);
static void fade_in_all(uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms);
static void fade_out_all(uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms);
static void fade_in_led(int index, uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms);
static void run_blink_pattern(const led_pattern_t *p);
static void run_chase_pattern(const led_pattern_t *p);
static void led_task(void *arg);
void led_task_start(void);

static void IRAM_ATTR level_isr_handler(void *arg);

using namespace PtxIotReader;
bool nfcstatus;

// Helper function to log stack usage for a given task
static void log_stack_usage(const char *location, TaskHandle_t task = NULL) {
    if (task == NULL) {
        task = xTaskGetCurrentTaskHandle();
    }

    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(task);

    ESP_LOGI(TAG, "[STACK] %s - High water mark: %u bytes free (min remaining)", location, high_water_mark * sizeof(StackType_t));
}

static std::vector<uint8_t> create_text_ndef_record(const char *text) {
    size_t text_len = strlen(text);
    std::vector<uint8_t> record;

    record.push_back(0xD1);
    record.push_back(0x01);
    record.push_back(3 + text_len);
    record.push_back(0x54);
    record.push_back(0x02);
    record.push_back('e');
    record.push_back('n');

    for (size_t i = 0; i < text_len; i++) {
        record.push_back(text[i]);
    }

    return record;
}

/*
If true, the loop will wait until a character is received
over the console, before reading the next card.
If false, the loop will continuously read any card placed
into the field.
*/
const bool readOneByOne = false;

// with 500 ms idle time between cycles
// Low Power Card Detection enabled with regular polling at every 10th cycle
// and stand-by mode enabled
const PollingConfig pollConfig = {
    .pollTypeA     = 1U,
    .pollTypeB     = 1U,
    .pollTypeF212  = 1U,
    .pollTypeV     = 1U,
    .idleTime      = 500U,
    .discoverMode  = 10U,
    .enableStandBy = 1U,
};

static mcp23x17_t mcp23017Dev           = {0};
static mcp23x17_handle_t mcp23017Handle = NULL;

// #define PATCHPANELPIXELSTART 150

// uint8_t patchPanelPixels[6] = {
//   PATCHPANELPIXELSTART,
//   PATCHPANELPIXELSTART+3,
//   PATCHPANELPIXELSTART+5,
//   PATCHPANELPIXELSTART+8,
//   PATCHPANELPIXELSTART+10,
//   PATCHPANELPIXELSTART+13
//   };
//   uint8_t connectedports = 0;
//   uint16_t connections = 0;

#define PATCHPANELPIXELSTART 6

uint8_t patchPanelPixels[6] = {PATCHPANELPIXELSTART,     PATCHPANELPIXELSTART + 3,  PATCHPANELPIXELSTART + 5,
                               PATCHPANELPIXELSTART + 8, PATCHPANELPIXELSTART + 10, PATCHPANELPIXELSTART + 13};
uint8_t connectedports      = 0;
uint16_t connections        = 0;
bool nfc_write_in_progress  = false; // Flag to disable patch panel updates during NFC writes

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event           = (ip_event_got_ip_t *)event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;

        ESP_LOGI(TAG, "Ethernet Got IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP) {
        ESP_LOGI(TAG, "Ethernet Lost IP Address");
    }
}

// void app_main(void) {
extern "C" void app_main() {
    esp_err_t err;
    int64_t start_time = esp_timer_get_time();

    esp_timer_init();

    // Global log level override
    // esp_log_set_level_master(ESP_LOG_DEBUG);
    // esp_log_level_set("*", ESP_LOG_DEBUG);

    // Initialize the NVS storage component
    err = nvs_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }

    // vTaskDelay(pdMS_TO_TICKS(1000));

    // Initialize LEDs
    led_init();
    led_gamma_enable(true);
    // // Simple LED Test - remove later
    // led_clear();
    // // ledtimer = lv_timer_create(led_update, 50, 0);
    // for (int x = 0; x < 6; x++) {
    //     led_set(x, 10, 10, 10);
    //     led_show();
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    // for (int x = 0; x < 6; x++) {
    //     led_set(x, 0, 0, 0);
    //     led_show();
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

    led_task_start();

    led_pattern_t fade_blink = {
        .type        = LED_PATTERN_CHASE,
        .red         = 255,
        .green       = 255,
        .blue        = 255,
        .on_ms       = 500,
        .off_ms      = 500,
        .blink_count = 0, // blink forever
        .speed_ms    = 50,
        .leds_on     = 1,
        .chase_count = 0,
        .chase_dir   = 0,
        .fade        = false,
    };
    led_pattern_set(&fade_blink);

    // Initialize the default event loop (needed for ethernet)
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(err));
        return;
    }

    // Install the ISR service if needed
    err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    }

    // Initialize the I2C manager
    err = i2c_manager_init_auto();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C manager: %s", esp_err_to_name(err));
        return;
    }

    // Initialize the secure element
    err = se_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize secure element: %s", esp_err_to_name(err));
        return;
    }

    // Button/Switch Init
    gpio_reset_pin(CW_PIN);
    gpio_set_direction(CW_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CW_PIN, GPIO_PULLUP_ONLY);
    gpio_reset_pin(CCW_PIN);
    gpio_set_direction(CCW_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CCW_PIN, GPIO_PULLUP_ONLY);
    gpio_reset_pin(LEVEL_PIN);
    gpio_set_direction(LEVEL_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(LEVEL_PIN, GPIO_PULLUP_ONLY);

    // Register ethernet and IP event handlers
    err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
    }

    // Load existing config first (including any migrations) before making ANY changes
    err = load_nut_config();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Loading default nut config due to error");
        nut_config = NUT_DEFAULTS;
    }

    // Detect hardware configuration BEFORE initializing network manager
    // This scans I2C bus for MCP23017 to determine if we have TELECOM hardware
    err = nut_detect_hardware(false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to detect hardware configuration: %s", esp_err_to_name(err));
        return;
    }

    // Determine if we have TELECOM hardware (MCP23017 + W5500)
    bool has_ethernet = (nut_config.hw_type == NUT_HW_TYPE_TELECOM);

    // Initialize the network manager with correct hardware configuration
    network_manager_config_t nm_config = {
        .enable_wifi                = true,         // Always enable WiFi
        .enable_ethernet            = has_ethernet, // Only enable Ethernet on TELECOM hardware
        .ethernet_fallback_to_wifi  = true,         // Use WiFi if Ethernet fails
        .wifi_fallback_to_ethernet  = true,         // Keep trying Ethernet even after WiFi connects
        .ethernet_retry_interval_ms = 30000,        // Retry Ethernet every 30 seconds
    };
    ESP_LOGI(TAG, "Initializing network manager (WiFi=%d, Ethernet=%d)", nm_config.enable_wifi, nm_config.enable_ethernet);
    err = network_manager_init(&nm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network manager: %s", esp_err_to_name(err));
        return;
    }

    // Quiet noisy W5500 driver debug logs
    esp_log_level_set("w5500.mac", ESP_LOG_WARN);

    // Initialize the nut (this will register a callback with network_manager)
    err = nut_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize nut component: %s", esp_err_to_name(err));
        return;
    }

    switch_evt_queue = xQueueCreate(10, sizeof(switch_event_t));
    switch_event_t evt;

    // Defer rotation interrupt setup to a separate task to allow other init to proceed
    auto rotation_init_task_fn = [](void *arg) __attribute__((used)) {
        const TickType_t check_delay = pdMS_TO_TICKS(200);
        for (;;) {
            if (code_mgr_is_ready()) {
                break;
            }
            vTaskDelay(check_delay);
        }
        setup_rotation_interrupts();
        vTaskDelete(NULL);
    };
    BaseType_t r = xTaskCreate((TaskFunction_t)rotation_init_task_fn, "rotation_init", 2048, NULL, 5, NULL);
    if (r != pdPASS) {
        ESP_LOGW(TAG, "Failed to create rotation init task; enabling interrupts immediately");
        setup_rotation_interrupts();
    }

    // Initialize LEDs
    // led_init();
    // // Simple LED Test - remove later
    // led_clear();
    // // ledtimer = lv_timer_create(led_update, 50, 0);
    // for (int x = 0; x < 6; x++) {
    //     led_set(x, 10, 10, 10);
    //     led_show();
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    // for (int x = 0; x < 6; x++) {
    //     led_set(x, 0, 0, 0);
    //     led_show();
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

    // Enable NFC Chip
    gpio_reset_pin((gpio_num_t)20);
    gpio_set_direction((gpio_num_t)20, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)20, 1);

    // Initialize MCP23017 if we have TELECOM hardware
    if (has_ethernet) {
        ESP_LOGI(TAG, "Initializing MCP23017 I/O expander...");
        err = mcp23x17_init_desc(&mcp23017Dev, 0x20, I2C_NUM_0, &mcp23017Handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize MCP23017: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "MCP23017 initialized successfully");
        mcp23x17_port_set_mode(&mcp23017Dev, 0xFFFF); // Set all as inputs
        for (int x = 0; x < 8; x++) {
            mcp23x17_set_pullup(&mcp23017Dev, x, true); // Enable pullups on pins 0-7
        }
        mcp23x17_set_pullup(&mcp23017Dev, 8, true); // Latch Sensor
        mcp23x17_set_level(&mcp23017Dev, 9, 0);
        mcp23x17_set_mode(&mcp23017Dev, 9, MCP23X17_GPIO_OUTPUT); // Latch Solenoid
        mcp23x17_set_level(&mcp23017Dev, 9, 0);
        mcp23x17_set_mode(&mcp23017Dev, BUTTON1_I2C_PIN, MCP23X17_GPIO_INPUT);
        mcp23x17_set_pullup(&mcp23017Dev, BUTTON1_I2C_PIN, true);
        mcp23x17_set_mode(&mcp23017Dev, BUTTON2_I2C_PIN, MCP23X17_GPIO_INPUT);
        mcp23x17_set_pullup(&mcp23017Dev, BUTTON2_I2C_PIN, true);
        mcp23x17_set_mode(&mcp23017Dev, BUTTONLIGHT_I2C_PIN, MCP23X17_GPIO_OUTPUT);
        mcp23x17_set_level(&mcp23017Dev, BUTTONLIGHT_I2C_PIN, 0);
    }

    // Initialize ethernet if we have TELECOM hardware (MCP23017 + W5500)
    if (has_ethernet) {
        ESP_LOGI(TAG, "TELECOM hardware detected - ethernet initialization temporarily disabled for testing");

        esp_netif_init();
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        assert(eth_netif);

        // Initialize all configured ethernet interfaces (W5500)
        esp_eth_handle_t *eth_handles = NULL;
        uint8_t eth_cnt               = 0;
        err                           = ethernet_init_all(&eth_handles, &eth_cnt);
        if (err == ESP_OK && eth_cnt > 0) {
            ESP_LOGI(TAG, "Ethernet initialized successfully (%d interface%s)", eth_cnt, eth_cnt > 1 ? "s" : "");

            // Start the ethernet interface(s)
            for (uint8_t i = 0; i < eth_cnt; i++) {
                esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i]));
                err = esp_eth_start(eth_handles[i]);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Ethernet interface %d started", i);

                    // Get the netif handle and start DHCP client
                    esp_netif_t *eth_netif = esp_netif_get_default_netif(); // esp_netif_get_handle_from_ifkey("ETH_DEF");
                    if (eth_netif) {
                        err = esp_netif_dhcpc_start(eth_netif);
                        if (err == ESP_OK) {
                            ESP_LOGI(TAG, "DHCP client started for ethernet interface %d", i);
                        } else if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
                            ESP_LOGI(TAG, "DHCP client already running for ethernet interface %d", i);
                        } else {
                            ESP_LOGW(TAG, "Failed to start DHCP client for ethernet interface %d: %s", i, esp_err_to_name(err));
                        }
                    } else {
                        ESP_LOGW(TAG, "Failed to get netif handle for ethernet interface %d", i);
                    }
                } else {
                    ESP_LOGW(TAG, "Failed to start ethernet interface %d: %s", i, esp_err_to_name(err));
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize ethernet: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "WIRELESS hardware detected - doing SPI2 initialization for NFC");

        // Initialize SPI bus
        ESP_LOGI(TAG, "Initializing SPI2 bus for NFC...");
        spi_bus_config_t buscfg = {
            .mosi_io_num     = 6,
            .miso_io_num     = 7,
            .sclk_io_num     = 5,
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = 4096,
        };
        err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SPI2 bus: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "SPI2 bus initialized successfully");
    }

    Serial.begin(115200);
    // while (!Serial)
    //     ; // wait for the port to be opened
    ESP_LOGI(TAG, "Starting NFC Reader...");

    // Retry NFC initialization up to 3 times
    int nfc_retry_count       = 0;
    const int max_nfc_retries = 65535;
    nfcstatus                 = false;

    while (!nfcstatus && nfc_retry_count < max_nfc_retries) {
        nfc_retry_count++;
        ESP_LOGI(TAG, "NFC initialization attempt %d/%d", nfc_retry_count, max_nfc_retries);

        // initialize the reader instance
        nfcstatus = IotReader::getReader().begin();

        if (!nfcstatus) {
            ESP_LOGW(TAG, "NFC initialization attempt %d failed", nfc_retry_count);
            if (nfc_retry_count < max_nfc_retries) {
                ESP_LOGI(TAG, "Waiting 2 seconds before retry...");
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }

    if (!nfcstatus) {
        ESP_LOGE(TAG, "ERROR: Failed to initialize NFC reader after %d attempts", max_nfc_retries);
    } else {
        ESP_LOGI(TAG, "NFC Reader Started successfully on attempt %d", nfc_retry_count);
        bool status = IotReader::getReader().pollingStop();
        if (!status) {
            ESP_LOGE(TAG, "ERROR: failed to disable nfc polling.", NULL);
        }
    }

    Serial.println("Place a card on the antenna to read it.");
    Serial.println();

    // int ledcounter = 0;
    // fade_blink.type        = LED_PATTERN_BLINK;
    // fade_blink.blink_count = 1;
    // fade_blink.on_ms       = 1000;
    // fade_blink.off_ms      = 1000;
    // fade_blink.fade        = true;
    // fade_blink.red         = 0;
    // fade_blink.green       = 255;
    // fade_blink.blue        = 0;
    // led_pattern_set(&fade_blink);
    // fade_blink.type = LED_PATTERN_NONE;
    // fade_blink.blink_count = 1;
    // led_pattern_set(&fade_blink);

    // Stack monitoring - log every 30 seconds
    static uint32_t last_stack_log_time  = 0;
    const uint32_t STACK_LOG_INTERVAL_MS = 30000;

    while (1) {
        // Periodic stack usage logging
        uint32_t now = millis();
        if (now - last_stack_log_time >= STACK_LOG_INTERVAL_MS) {
            log_stack_usage("Main loop - periodic check");
            last_stack_log_time = now;
        }

        // led_set(ledcounter, 10,10,10);
        // if(ledcounter > 0) led_set(ledcounter-1, 0,0,0); else led_set(5, 0,0,0);
        // ledcounter ++;
        // if(ledcounter > 5) ledcounter = 0;
        // led_show();

        // nfcupdate(); // Check for NFC tag

        // Only update patch panel for telecom hardware (which has MCP23017)
        // Skip patch panel updates during NFC writes to avoid timing interference
        // Reduced scan frequency to 2 Hz (every 500ms) to minimize I2C bus contention
        if (nut_config.hw_type == NUT_HW_TYPE_TELECOM && !nfc_write_in_progress) {
            patchPanelUpdate();
            checkPortExpanderIO();
        }
        if (code_level_changed) {
            code_level++;
            if (code_level > 3) {
                code_level = 1;
            }
            led_pattern_t blink_wrench_white = {
                .type        = LED_PATTERN_BLINK,
                .red         = 255,
                .green       = 255,
                .blue        = 255,
                .on_ms       = 250,
                .off_ms      = 250,
                .blink_count = code_level,
                .speed_ms    = 50,
                .leds_on     = 1,
                .chase_count = 0,
                .chase_dir   = true,
                .fade        = false,
            };
            led_pattern_set(&blink_wrench_white);
            esp_err_t err = ESP_ERR_INVALID_STATE;
            if (nut_config.type != NUT_TYPE_UNKNOWN) {
                err = request_new_code();
            } else {
                static int64_t last_skip_log_ms = 0;
                int64_t now_ms                  = esp_timer_get_time() / 1000;
                // Only log this warning at most once every 30 seconds to avoid spam
                if (now_ms - last_skip_log_ms > 30000) {
                    ESP_LOGW(TAG, "Not requesting code because nut type is UNKNOWN");
                    last_skip_log_ms = now_ms;
                }
                // Clear the flag so we don't repeatedly attempt until user action
                code_level_changed = false;
            }
            if (err == ESP_OK) {
                code_level_changed             = false;
                led_pattern_t blink_wrench_red = {
                    .type        = LED_PATTERN_BLINK,
                    .red         = 0,
                    .green       = 255,
                    .blue        = 0,
                    .on_ms       = 250,
                    .off_ms      = 250,
                    .blink_count = 1,
                    .speed_ms    = 50,
                    .leds_on     = 1,
                    .chase_count = 0,
                    .chase_dir   = true,
                    .fade        = true,
                };
                led_pattern_set(&blink_wrench_red);
            }
        }
        // checkInputs();
        if (xQueueReceive(switch_evt_queue, &evt, pdMS_TO_TICKS(50))) {
            ESP_LOGI(TAG, "Switch %d triggered (two consecutive presses detected)", evt.switch_id);
            led_pattern_t chase_wrench = {
                .type        = LED_PATTERN_CHASE,
                .red         = 255,
                .green       = 165,
                .blue        = 0,
                .on_ms       = 500,
                .off_ms      = 500,
                .blink_count = 0,
                .speed_ms    = 50,
                .leds_on     = 1,
                .chase_count = 0,
                .chase_dir   = true,
                .fade        = false,
            };
            led_pattern_t blink_wrench_red = {
                .type        = LED_PATTERN_BLINK,
                .red         = 255,
                .green       = 0,
                .blue        = 0,
                .on_ms       = 250,
                .off_ms      = 250,
                .blink_count = 5,
                .speed_ms    = 50,
                .leds_on     = 1,
                .chase_count = 0,
                .chase_dir   = true,
                .fade        = true,
            };
            led_pattern_t blink_wrench_green = {
                .type        = LED_PATTERN_BLINK,
                .red         = 0,
                .green       = 255,
                .blue        = 0,
                .on_ms       = 250,
                .off_ms      = 250,
                .blink_count = 5,
                .speed_ms    = 50,
                .leds_on     = 1,
                .chase_count = 0,
                .chase_dir   = true,
                .fade        = true,
            };
            if (evt.switch_id == 1) {
                chase_wrench.chase_dir = true;
            } else {
                chase_wrench.chase_dir = false;
            }
            if (doorIsOpen == true) {
                led_pattern_set(&blink_wrench_red);
            } else {
                led_pattern_set(&chase_wrench);
                esp_err_t err = nfcWriteCode(2000);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Code written to badge!");
                    led_pattern_set(&blink_wrench_green);
                    // TODO: NFC write OK, request a new code from API
                } else {
                    ESP_LOGI(TAG, "NFC write failed.");
                    led_pattern_set(&blink_wrench_red);
                }
            }
            // Clear any queued switch events that happened while waiting for NFC write
            while (xQueueReceive(switch_evt_queue, &evt, 0))
                ;
        }

        // vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void led_update() {
    static uint32_t counter = 0;

    if (counter < NUM_LEDS) {
        led_set(counter, 10, 10, 10);
    } else {
        led_set(counter - 1, 0, 0, 0);
        // lv_timer_delete(ledtimer);
        // ledtimer = NULL;
        counter = 0;
    }
    if (counter > 0) {
        led_set(counter - 1, 0, 0, 0);
    }
    led_show();
    counter++;
}

void nfcupdate(void) {
    if (!nfcstatus || !code_mgr_is_ready()) {
        return;
    }

    if (IotReader::getReader().detectCard(pollConfig)) {
        Serial.println("Card found");
        printCardInfo(IotReader::getReader().getCardInfo());

        const char *code     = code_mgr_get_current_code();
        const char *nut_type = code_mgr_get_nut_type_record();

        std::vector<uint8_t> type_record = create_text_ndef_record(nut_type);
        std::vector<uint8_t> code_record = create_text_ndef_record(code);

        type_record.insert(type_record.end(), code_record.begin(), code_record.end());
        type_record[0] |= 0x80;
        code_record[0] |= 0x40;

        ESP_LOGI(TAG, "Writing NFC: %s, Code: %s", nut_type, code);
        Serial.printf("Writing NFC: %s, Code: %s\n", nut_type, code);

        nfc_write_in_progress = true;

        if (IotReader::getReader().ndefWrite(type_record)) {
            Serial.println("NDEF message written successfully");
            ESP_LOGI(TAG, "NDEF message written successfully");
            code_mgr_mark_code_written(code);
        } else {
            Serial.println("Could not write NDEF message");
            ESP_LOGW(TAG, "Failed to write NDEF message");
        }

        nfc_write_in_progress = false;
        Serial.println();
        Serial.println("Place another card on the antenna to write it.");
        Serial.println();
        Serial.flush();
        Serial.println("Scanning for card ...");
        Serial.flush();
    }
}

esp_err_t nfcWriteCode(uint16_t timeout) {
    log_stack_usage("nfcWriteCode - START");

    if (!code_mgr_is_ready()) {
        ESP_LOGW(TAG, "Code manager not ready");
        return ESP_ERR_INVALID_STATE;
    }

    const char *code            = code_mgr_get_current_code();
    const char *nut_type_record = code_mgr_get_nut_type_record();

    if (!code || !nut_type_record) {
        ESP_LOGE(TAG, "No valid code available");
        return ESP_ERR_INVALID_STATE;
    }

    // Create both NDEF records (these are raw NDEF records without TLV wrapping)
    std::vector<uint8_t> nut_type_record_data = create_text_ndef_record(nut_type_record);
    std::vector<uint8_t> code_record_data     = create_text_ndef_record(code);

    // Update the flags in the record headers to indicate this is a multi-record message
    // First record: MB=1, ME=0 (Message Begin, not Message End) = 0x91
    // Last record: MB=0, ME=1 (not Message Begin, Message End) = 0x51
    nut_type_record_data[0] = 0x91; // First record in message
    code_record_data[0]     = 0x51; // Last record in message

    // Combine both records into a single NDEF message
    // NOTE: ndefWrite() adds the TLV wrapper automatically, so we just pass the raw NDEF records
    std::vector<uint8_t> combined_message;

    // Add both NDEF records (ndefWrite will add TLV header and terminator)
    combined_message.insert(combined_message.end(), nut_type_record_data.begin(), nut_type_record_data.end());
    combined_message.insert(combined_message.end(), code_record_data.begin(), code_record_data.end());

    bool writeSuccess     = false;
    unsigned long endTime = millis() + timeout;

    nfc_write_in_progress = true;

    while (!writeSuccess && millis() < endTime) {
        if (nfcstatus && IotReader::getReader().detectCard(pollConfig)) {
            ESP_LOGI(TAG, "Badge detected, writing: %s + %s", nut_type_record, code);

            if (IotReader::getReader().ndefWrite(combined_message)) {
                ESP_LOGI(TAG, "Code written successfully: %s", code);
                writeSuccess = true;
                // code_mgr_write_to_nfc();
                code_mgr_mark_code_written(code);
            } else {
                ESP_LOGW(TAG, "NFC write failed");
            }
        }
    }

    IotReader::getReader().pollingStop();
    nfc_write_in_progress = false;

    log_stack_usage("nfcWriteCode - END");

    return writeSuccess ? ESP_OK : ESP_ERR_TIMEOUT;
}

void patchPanelUpdate() {
    // Initialize all pins as inputs with pull-ups before starting the patch panel scan
    for (int pin = 0; pin < 8; pin++) {
        // mcp23x17_set_mode(&mcp23017Dev, pin, MCP23X17_GPIO_OUTPUT);
        //  Enable pull-up resistors on all input pins
        mcp23x17_set_pullup(&mcp23017Dev, pin, true);
    }

    // for (int x = 0; x < 6; x++) {
    //     led_set(patchPanelPixels[x], 0, 0, 0);
    // }
    uint8_t oldconnections = connectedports;
    connectedports         = 0;
    if (nut_config.node_id < 19) {
        // Check port 1
        mcp23x17_set_mode(&mcp23017Dev, 0, MCP23X17_GPIO_OUTPUT); // Change pin A0 to Output
        mcp23x17_set_level(&mcp23017Dev, 0, 0);
        mcp23x17_port_read(&mcp23017Dev, &connections);
        // Serial.print("P1:");
        // Serial.print(connections & 0x3F, BIN);
        if ((connections & (1 << 1)) == 0) {
            connectedports |= 0x2;
            connectedports |= 0x1;
        } // Port 2
        if ((connections & (1 << 2)) == 0) {
            connectedports |= 0x4;
            connectedports |= 0x1;
        } // Port 3
        if ((connections & (1 << 3)) == 0) {
            connectedports |= 0x8;
            connectedports |= 0x1;
        } // Port 4
        if ((connections & (1 << 4)) == 0) {
            connectedports |= 0x10;
            connectedports |= 0x1;
        } // Port 5
        if ((connections & (1 << 5)) == 0) {
            connectedports |= 0x20;
            connectedports |= 0x1;
        } // Port 6
        mcp23x17_set_mode(&mcp23017Dev, 0, MCP23X17_GPIO_INPUT); // Change pin A0 to Input
        mcp23x17_set_pullup(&mcp23017Dev, 0, true);
        // Check port 2
        mcp23x17_set_mode(&mcp23017Dev, 1, MCP23X17_GPIO_OUTPUT); // Change pin A0 to Output
        mcp23x17_set_level(&mcp23017Dev, 1, 0);
        mcp23x17_port_read(&mcp23017Dev, &connections);
        // Serial.print(" P2:");
        // Serial.print(connections, BIN);
        if ((connections & (1 << 2)) == 0) {
            connectedports |= 0x4;
            connectedports |= 0x2;
        } // Port 3
        if ((connections & (1 << 3)) == 0) {
            connectedports |= 0x8;
            connectedports |= 0x2;
        } // Port 4
        if ((connections & (1 << 4)) == 0) {
            connectedports |= 0x10;
            connectedports |= 0x2;
        } // Port 5
        if ((connections & (1 << 5)) == 0) {
            connectedports |= 0x20;
            connectedports |= 0x2;
        } // Port 6
        mcp23x17_set_mode(&mcp23017Dev, 1, MCP23X17_GPIO_INPUT); // Change pin A0 to Input
        mcp23x17_set_pullup(&mcp23017Dev, 1, true);
        // Check port 3
        mcp23x17_set_mode(&mcp23017Dev, 2, MCP23X17_GPIO_OUTPUT); // Change pin A0 to Output
        mcp23x17_set_level(&mcp23017Dev, 2, 0);
        mcp23x17_port_read(&mcp23017Dev, &connections);
        // Serial.print(" P3:");
        // Serial.print(connections, BIN);
        if ((connections & (1 << 3)) == 0) {
            connectedports |= 0x8;
            connectedports |= 0x4;
        } // Port 4
        if ((connections & (1 << 4)) == 0) {
            connectedports |= 0x10;
            connectedports |= 0x4;
        } // Port 5
        if ((connections & (1 << 5)) == 0) {
            connectedports |= 0x20;
            connectedports |= 0x4;
        } // Port 6
        mcp23x17_set_mode(&mcp23017Dev, 2, MCP23X17_GPIO_INPUT); // Change pin A0 to Input
        mcp23x17_set_pullup(&mcp23017Dev, 2, true);
        // Check port 4
        mcp23x17_set_mode(&mcp23017Dev, 3, MCP23X17_GPIO_OUTPUT); // Change pin A0 to Output
        mcp23x17_set_level(&mcp23017Dev, 3, 0);
        mcp23x17_port_read(&mcp23017Dev, &connections);
        // Serial.print(" P4:");
        // Serial.print(connections, BIN);
        if ((connections & (1 << 4)) == 0) {
            connectedports |= 0x10;
            connectedports |= 0x8;
        } // Port 5
        if ((connections & (1 << 5)) == 0) {
            connectedports |= 0x20;
            connectedports |= 0x8;
        } // Port 6
        mcp23x17_set_mode(&mcp23017Dev, 3, MCP23X17_GPIO_INPUT); // Change pin A0 to Input
        mcp23x17_set_pullup(&mcp23017Dev, 3, true);
        // Check port 5
        mcp23x17_set_mode(&mcp23017Dev, 4, MCP23X17_GPIO_OUTPUT); // Change pin A0 to Output
        mcp23x17_set_level(&mcp23017Dev, 4, 0);
        mcp23x17_port_read(&mcp23017Dev, &connections);
        // Serial.print(" P5:");
        // Serial.print(connections, BIN);
        if ((connections & (1 << 5)) == 0) {
            connectedports |= 0x20;
            connectedports |= 0x10;
        } // Port 6
        mcp23x17_set_mode(&mcp23017Dev, 4, MCP23X17_GPIO_INPUT); // Change pin A0 to Input
        mcp23x17_set_pullup(&mcp23017Dev, 4, true);

        // Serial.print(" Conns:");
        // Serial.println(connectedports, BIN);

        if (connectedports != oldconnections) {
            for (int x = 0; x < 6; x++) {
                led_set(patchPanelPixels[x], 0, 0, 0);
            }
            uint8_t setbits = __builtin_popcount(connectedports);

            // Rate limiting for debug messages... only once per second
            static uint32_t last_debug_time = 0;
            uint32_t current_time           = esp_timer_get_time() / 1000;
            bool should_debug               = (current_time - last_debug_time) > 1000;

            if (setbits == 0) {
                // for (int x = 0; x < 6; x++) {
                //     led_set(patchPanelPixels[x], 0, 0, 0);
                // }
            }
            if (setbits == 1) {
                if (should_debug) {
                    ESP_LOGI(TAG, "Only one port connected. How does that even happen???");
                }
            }
            if (setbits == 2) {
                for (int x = 0; x < 6; x++) {
                    if (connectedports >> x & 0x1) {
                        led_set(patchPanelPixels[x], 0, 255, 0);
                    }
                }
            }
            if (setbits > 2) {
                if (should_debug) {
                    ESP_LOGI(TAG, "Too many connections!!! (setbits=%d, connectedports=0x%02X)\n", setbits, connectedports);
                }
                for (int x = 0; x < 6; x++) {
                    if (connectedports >> x & 0x1) {
                        led_set(patchPanelPixels[x], 255, 0, 0);
                    }
                }
            }

            // Update debug time after all conditions are checked
            if (should_debug && (setbits == 1 || setbits > 2)) {
                last_debug_time = current_time;
            }
        }
    } else {
        mcp23x17_port_read(&mcp23017Dev, &connections);
        if ((connections & (1 << 0)) == 0) {
            connectedports |= 0x1;
        } // Port 1
        if ((connections & (1 << 1)) == 0) {
            connectedports |= 0x2;
        } // Port 2
        if ((connections & (1 << 2)) == 0) {
            connectedports |= 0x4;
        } // Port 3
        if ((connections & (1 << 3)) == 0) {
            connectedports |= 0x8;
        } // Port 4
        // if ((connections & (1 << 4)) == 0) {
        //     connectedports |= 0x10;
        // } // Port 5
        // if ((connections & (1 << 5)) == 0) {
        //     connectedports |= 0x20;
        // } // Port 6

        if (connectedports != oldconnections) {
            for (int x = 0; x < 4; x++) {
                led_set(patchPanelPixels[x], 0, 0, 0);
            }
            uint8_t setbits = __builtin_popcount(connectedports);

            // Rate limiting for debug messages... only once per second
            static uint32_t last_debug_time = 0;
            uint32_t current_time           = esp_timer_get_time() / 1000;
            bool should_debug               = (current_time - last_debug_time) > 1000;

            if (setbits == 0) {
                // for (int x = 0; x < 6; x++) {
                //     led_set(patchPanelPixels[x], 0, 0, 0);
                // }
            }
            if (setbits == 1) {
                for (int x = 0; x < 6; x++) {
                    if (connectedports >> x & 0x1) {
                        led_set(patchPanelPixels[x], 0, 255, 0);
                    }
                }
                // if (should_debug) {
                //     ESP_LOGI(TAG, "Only one port connected. How does that even happen???");
                // }
            }
            // if (setbits == 2) {
            //     for (int x = 0; x < 6; x++) {
            //         if (connectedports >> x & 0x1) {
            //             led_set(patchPanelPixels[x], 0, 255, 0);
            //         }
            //     }
            // }
            if (setbits > 1) {
                if (should_debug) {
                    ESP_LOGI(TAG, "Too many connections!!! (setbits=%d, connectedports=0x%02X)\n", setbits, connectedports);
                }
                for (int x = 0; x < 6; x++) {
                    if (connectedports >> x & 0x1) {
                        led_set(patchPanelPixels[x], 255, 0, 0);
                    }
                }
            }
        }
    }

    led_show();
}

void checkPortExpanderIO() {
    bool button1 = false;
    bool button2 = false;
    mcp23x17_get_level(&mcp23017Dev, BUTTON1_I2C_PIN, &button1);
    mcp23x17_get_level(&mcp23017Dev, BUTTON2_I2C_PIN, &button2);
    if (button1 == true && button2 == false) {
        mcp23x17_set_level(&mcp23017Dev, BUTTONLIGHT_I2C_PIN, 1);
    } else if (button1 == false && button2 == true) {
        mcp23x17_set_level(&mcp23017Dev, BUTTONLIGHT_I2C_PIN, 1);
    } else {
        mcp23x17_set_level(&mcp23017Dev, BUTTONLIGHT_I2C_PIN, 0);
    }
    static bool lastLatchState = false;
    bool latchSwitch;
    mcp23x17_get_level(&mcp23017Dev, 8, &latchSwitch);
    if (latchSwitch != lastLatchState) {
        lastLatchState = latchSwitch;
        if (latchSwitch == false) {
            vTaskDelay(500); // Make sure door stays latched
            mcp23x17_get_level(&mcp23017Dev, 8, &latchSwitch);
            if (lastLatchState == latchSwitch) {
                bool conn[6];
                conn[0] = (connectedports >> 0 & 0x1);
                conn[1] = (connectedports >> 1 & 0x1);
                conn[2] = (connectedports >> 2 & 0x1);
                conn[3] = (connectedports >> 3 & 0x1);
                conn[4] = (connectedports >> 4 & 0x1);
                conn[5] = (connectedports >> 5 & 0x1);

                if (nut_config.node_id > 19) {
                    // 4 connections
                    const api_telecom_port_status_t statuses[4]{
                        {.port_number = 1, .connected = conn[0]},
                        {.port_number = 2, .connected = conn[1]},
                        {.port_number = 3, .connected = conn[2]},
                        {.port_number = 4, .connected = conn[3]},
                    };
                    api_err_t err = api_send_telecom_port_status(statuses, 4);
                }
                if (nut_config.node_id == 9 || nut_config.node_id == 11 || nut_config.node_id == 13 || nut_config.node_id == 15 ||
                    nut_config.node_id == 17 || nut_config.node_id == 19) {
                    // 5 connections
                    const api_telecom_port_status_t statuses[5]{
                        {.port_number = 1, .connected = conn[0]}, {.port_number = 2, .connected = conn[1]},
                        {.port_number = 3, .connected = conn[2]}, {.port_number = 4, .connected = conn[3]},
                        {.port_number = 5, .connected = conn[4]},
                    };
                    api_err_t err = api_send_telecom_port_status(statuses, 5);
                }
                if (nut_config.node_id < 9 || nut_config.node_id == 10 || nut_config.node_id == 12 || nut_config.node_id == 14 ||
                    nut_config.node_id == 16 || nut_config.node_id == 18) {
                    // 6 connections
                    const api_telecom_port_status_t statuses[6]{
                        {.port_number = 1, .connected = conn[0]}, {.port_number = 2, .connected = conn[1]},
                        {.port_number = 3, .connected = conn[2]}, {.port_number = 4, .connected = conn[3]},
                        {.port_number = 5, .connected = conn[4]}, {.port_number = 6, .connected = conn[5]},
                    };
                    api_err_t err = api_send_telecom_port_status(statuses, 6);
                }
            }

            api_err_t err = api_send_telecom_locked();
            doorIsOpen    = false;
        }
    }
}

extern "C" void openDoor() {
    bool latchSwitch = true; // Pin is pulled up
    mcp23x17_get_level(&mcp23017Dev, 8, &latchSwitch);
    if (latchSwitch == false) {
        bool closed = true;
        while (closed) { // Latch is closed, so we can open it
            ESP_LOGI(TAG, "Turning on Relay");
            mcp23x17_set_level(&mcp23017Dev, 9, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            mcp23x17_set_level(&mcp23017Dev, 9, 0);
            ESP_LOGI(TAG, "Turning off Relay");
            mcp23x17_get_level(&mcp23017Dev, 8, &latchSwitch); // Check latch switch
            if (latchSwitch == true) {
                ESP_LOGI(TAG, "Success opening door!");
                closed     = false;
                doorIsOpen = true;
            } else {
                ESP_LOGE(TAG, "Latch switch failed to open. Is the door stuck?");
                vTaskDelay(2000); // Wait 2 seconds for capacitor to recharge
            }
        }
    } else {
        ESP_LOGE(TAG, "Latch switch is open but tried to actuate door latch");
    }
}

void checkButtons() { // For Extra Buttons connected to Port Expander, not critical
}

void checkInputs() { // CW/CCW switch and Level button
    if ((gpio_get_level(CW_PIN) == 0) || (gpio_get_level(CCW_PIN) == 0)) {
        if (!code_mgr_is_ready()) {
            return;
        }

        esp_err_t err = nfcWriteCode(1000);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Code written to badge!");
        } else if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Nut not ready for code write");
        } else {
            ESP_LOGW(TAG, "NFC write failed or timed out");
        }
    }
}

// Interrupt handler
static void IRAM_ATTR switch_isr_handler(void *arg) {
    switch_state_t *sw = (switch_state_t *)arg;
    int64_t now        = esp_timer_get_time();

    // Debounce filter
    if (now - sw->last_time_us < DEBOUNCE_TIME_US) {
        return;
    }

    // Check for inter-switch interruption
    if (last_switch_id != -1 && last_switch_id != sw->id) {
        // Different switch triggered -> reset all
        for (int i = 0; i < 2; i++) {
            switches[i].activation_count = 0;
        }
    }

    // Reset counter if too much time has passed since last activation
    if (now - sw->last_time_us > DOUBLE_PRESS_WINDOW_US) {
        sw->activation_count = 0;
    }

    // Record activation
    sw->activation_count++;
    sw->last_time_us = now;
    last_switch_id   = sw->id;

    // Trigger event when threshold reached
    if (sw->activation_count >= ACTIVATION_THRESHOLD) {
        switch_event_t evt = {.switch_id = sw->id};
        xQueueSendFromISR(switch_evt_queue, &evt, NULL);
        sw->activation_count = 0;
        last_switch_id       = -1; // reset sequence
    }
}

// Configure GPIOs and attach interrupts
static void setup_rotation_interrupts(void) {
    // Configure the GPIOs as inputs with pull-ups
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CW_PIN) | (1ULL << CCW_PIN) | (1ULL << LEVEL_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE, // falling edge (active low)
    };
    gpio_config(&io_conf);

    // gpio_install_isr_service(0);

    for (int i = 0; i < 2; i++) {
        gpio_isr_handler_add(switches[i].gpio_num, switch_isr_handler, &switches[i]);
    }

    // Level Button Interrupt
    if (nut_config.type == NUT_TYPE_COMMUNITY) {
        gpio_isr_handler_add(LEVEL_PIN, level_isr_handler, NULL);
    }
}

static void IRAM_ATTR level_isr_handler(void *arg) {
    code_level_changed = true;
}

static void leds_off(void) {
    for (int i = 0; i < LED_COUNT; i++) {
        led_set(i, 0, 0, 0);
    }
    led_show();
}

void led_pattern_set(const led_pattern_t *pattern) {
    if (xSemaphoreTake(pattern_mutex, portMAX_DELAY)) {
        current_pattern = *pattern;
        xSemaphoreGive(pattern_mutex);
    }
}

// --- Fade helpers ---
static void fade_in_all(uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms) {
    int steps = duration_ms / FRAME_DELAY_MS;
    if (steps < 1) {
        steps = 1;
    }
    for (int s = 0; s <= steps; s++) {
        float t    = (float)s / steps;
        uint32_t r = (uint32_t)(red * t);
        uint32_t g = (uint32_t)(green * t);
        uint32_t b = (uint32_t)(blue * t);
        for (int i = 0; i < LED_COUNT; i++) {
            led_set(i, r, g, b);
        }
        led_show();
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }
}

static void fade_out_all(uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms) {
    int steps = duration_ms / FRAME_DELAY_MS;
    if (steps < 1) {
        steps = 1;
    }
    for (int s = 0; s <= steps; s++) {
        float t    = 1.0f - (float)s / steps;
        uint32_t r = (uint32_t)(red * t);
        uint32_t g = (uint32_t)(green * t);
        uint32_t b = (uint32_t)(blue * t);
        for (int i = 0; i < LED_COUNT; i++) {
            led_set(i, r, g, b);
        }
        led_show();
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }
}

static void fade_in_led(int index, uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms) {
    int steps = duration_ms / FRAME_DELAY_MS;
    if (steps < 1) {
        steps = 1;
    }
    for (int s = 0; s <= steps; s++) {
        float t = (float)s / steps;
        led_set(index, (uint32_t)(red * t), (uint32_t)(green * t), (uint32_t)(blue * t));
        led_show();
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }
}

// --- Blink pattern ---
static void run_blink_pattern(const led_pattern_t *p) {
    uint32_t count       = 0;
    bool pattern_changed = false;

    while (p->blink_count == 0 || count < p->blink_count) {
        if (p->fade) {
            uint32_t half_on  = p->on_ms / 2;
            uint32_t half_off = p->off_ms / 2;
            if (half_on == 0) {
                half_on = p->on_ms; // avoid zero-duration halves
            }
            if (half_off == 0) {
                half_off = p->off_ms;
            }

            fade_in_all(p->red, p->green, p->blue, half_on);
            led_show();
            vTaskDelay(pdMS_TO_TICKS(p->on_ms - half_on)); // remaining on time (if any)
            fade_out_all(p->red, p->green, p->blue, half_off);
            led_show();
            vTaskDelay(pdMS_TO_TICKS(p->off_ms - half_off));
        } else {
            for (int i = 0; i < LED_COUNT; i++) {
                led_set(i, p->red, p->green, p->blue);
            }
            led_show();
            vTaskDelay(pdMS_TO_TICKS(p->on_ms));
            leds_off();
            led_show();
            vTaskDelay(pdMS_TO_TICKS(p->off_ms));
        }

        count++;

        // Check for pattern change (non-blocking)
        led_pattern_t check;
        if (xSemaphoreTake(pattern_mutex, 0)) {
            check = current_pattern;
            xSemaphoreGive(pattern_mutex);
            if (check.type != LED_PATTERN_BLINK) {
                pattern_changed = true;
                break;
            }
        }
    }

    // If the loop ended because the pattern completed (not because it was changed),
    // clear the global pattern so the task doesn't restart it.
    if (!pattern_changed) {
        if (xSemaphoreTake(pattern_mutex, portMAX_DELAY)) {
            // Only clear if current pattern still matches the one we ran.
            if (current_pattern.type == LED_PATTERN_BLINK && current_pattern.red == p->red && current_pattern.green == p->green &&
                current_pattern.blue == p->blue && current_pattern.blink_count == p->blink_count &&
                current_pattern.on_ms == p->on_ms && current_pattern.off_ms == p->off_ms && current_pattern.fade == p->fade) {
                current_pattern.type = LED_PATTERN_NONE;
            }
            xSemaphoreGive(pattern_mutex);
        }
    }
}

// --- Chase pattern ---
static void run_chase_pattern(const led_pattern_t *p) {
    uint32_t count       = 0;
    int pos              = 0;
    bool pattern_changed = false;

    while (p->chase_count == 0 || count < p->chase_count) {
        leds_off();

        for (int i = 0; i < p->leds_on; i++) {
            int led = (pos + i) % LED_COUNT;
            if (p->fade) {
                // trailing fade effect (simple linear falloff)
                float brightness = 1.0f - ((float)i / (float)p->leds_on);
                if (brightness < 0.0f) {
                    brightness = 0.0f;
                }
                led_set(led, (uint32_t)(p->red * brightness), (uint32_t)(p->green * brightness),
                        (uint32_t)(p->blue * brightness));
            } else {
                led_set(led, p->red, p->green, p->blue);
            }
        }

        led_show();

        vTaskDelay(pdMS_TO_TICKS(p->speed_ms));
        if (p->chase_dir) {
            pos = (pos + 1) % LED_COUNT;
        } else {
            if (pos == 0) {
                pos = LED_COUNT - 1;
            } else {
                pos = (pos - 1) % LED_COUNT;
            }
        }
        if (pos == 0) {
            count++;
        }

        // Check for pattern change (non-blocking)
        led_pattern_t check;
        if (xSemaphoreTake(pattern_mutex, 0)) {
            check = current_pattern;
            xSemaphoreGive(pattern_mutex);
            if (check.type != LED_PATTERN_CHASE) {
                pattern_changed = true;
                break;
            }
        }
    }

    // If the loop ended because the pattern completed (not because it was changed),
    // clear the global pattern so the task doesn't restart it.
    if (!pattern_changed) {
        if (xSemaphoreTake(pattern_mutex, portMAX_DELAY)) {
            // Only clear if current pattern still matches the one we ran.
            if (current_pattern.type == LED_PATTERN_CHASE && current_pattern.red == p->red && current_pattern.green == p->green &&
                current_pattern.blue == p->blue && current_pattern.speed_ms == p->speed_ms &&
                current_pattern.leds_on == p->leds_on && current_pattern.chase_count == p->chase_count &&
                current_pattern.fade == p->fade) {
                current_pattern.type = LED_PATTERN_NONE;
            }
            xSemaphoreGive(pattern_mutex);
        }
    }
}

static void led_task(void *arg) {
    led_pattern_t pattern = {
        .type        = LED_PATTERN_NONE,
        .red         = 0,
        .green       = 0,
        .blue        = 0,
        .on_ms       = 500,
        .off_ms      = 500,
        .blink_count = 1, // blink forever
        .speed_ms    = 100,
        .leds_on     = 1,
        .chase_count = 1,
        .chase_dir   = false,
        .fade        = true,
    };

    while (1) {
        if (xSemaphoreTake(pattern_mutex, portMAX_DELAY)) {
            pattern = current_pattern;
            xSemaphoreGive(pattern_mutex);
        }

        switch (pattern.type) {
            case LED_PATTERN_BLINK: run_blink_pattern(&pattern); break;
            case LED_PATTERN_CHASE: run_chase_pattern(&pattern); break;
            default:
                leds_off();
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

void led_task_start(void) {
    pattern_mutex = xSemaphoreCreateMutex();
    leds_off();
    current_pattern.type = LED_PATTERN_NONE;
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
}