#include <stddef.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_rom_crc.h"

#include "atca_config.h"
#include "cryptoauthlib.h"
#include "host/atca_host.h"

#include "i2c_manager.h"
#include "nvs.h"
#include "secure_element.h"

#include "./se_debug.h"
#include "./utils.h"

static const char *TAG = "secure_element";

// API secrets list
#define X(key, name) {key, name},
const se_secret_title_t api_secrets[] = {API_SLOTS_LIST};
#undef X

// Cryptoauthlib I2C interface configuration
static ATCAIfaceCfg atca_cfg = {
    .iface_type = ATCA_I2C_IFACE,
    .devtype    = ATECC608B,
    .atcai2c =
        {
            .address = CONFIG_ATCA_I2C_ADDRESS,
            .bus     = I2C_BUS_A,
            .baud    = 1000000, // Full speed is 1 MHz
            // .baud = 400000, // Use standard I2C speed of 400 kHz
            // .baud = 100000, // Use slower I2C speed of 100 kHz
        },
    .wake_delay = 2000,
    .rx_retries = 20,
    .cfg_data   = NULL,
};

#define SE_NVS_NAMESPACE "security"
#define SE_NVS_KEY_NAME  "io_key_seed"

// Custom error code to indicate a locked slot
#define SE_ERR_SLOT_LOCKED (0x04 << 16)

// I/O key setup and derivation functions
static void scrubmem(void *v, size_t n) {
    volatile unsigned char *p = v;
    while (n--) {
        *p++ = 0;
    }
}
static ATCA_STATUS io_key_seed(uint8_t seed_out[32]);
static ATCA_STATUS compute_io_key(uint8_t key_out[ATCA_KEY_SIZE]);
static ATCA_STATUS create_io_key();
static ATCA_STATUS host_sha_hmac(const uint8_t key[ATCA_KEY_SIZE], const uint8_t *data, size_t data_len,
                                 uint8_t hmac[ATCA_SHA2_256_DIGEST_SIZE]);
// Obfuscated derivation key in two parts (lives in .rodata in two parts)
static const uint8_t dkey_frag_a[32] = { // A: HeyWhatDoYouThinkYou'reDoingHere
    'H' ^ 0xAA, 'e' ^ 0xAA, 'y' ^ 0xAA, 'W' ^ 0xAA, 'h' ^ 0xAA,  'a' ^ 0xAA, 't' ^ 0xAA, 'D' ^ 0xAA,
    'o' ^ 0xAA, 'Y' ^ 0xAA, 'o' ^ 0xAA, 'u' ^ 0xAA, 'T' ^ 0xAA,  'h' ^ 0xAA, 'i' ^ 0xAA, 'n' ^ 0xAA,
    'k' ^ 0xAA, 'Y' ^ 0xAA, 'o' ^ 0xAA, 'u' ^ 0xAA, '\'' ^ 0xAA, 'r' ^ 0xAA, 'e' ^ 0xAA, 'D' ^ 0xAA,
    'o' ^ 0xAA, 'i' ^ 0xAA, 'n' ^ 0xAA, 'g' ^ 0xAA, 'H' ^ 0xAA,  'e' ^ 0xAA, 'r' ^ 0xAA, 'e' ^ 0xAA};
static const uint8_t dkey_frag_b[32] = { // B: IBetPlumWishesHeKnewThisFragment
    'I' ^ 0xAA, 'B' ^ 0xAA, 'e' ^ 0xAA, 't' ^ 0xAA, 'P' ^ 0xAA, 'l' ^ 0xAA, 'u' ^ 0xAA, 'm' ^ 0xAA,
    'W' ^ 0xAA, 'i' ^ 0xAA, 's' ^ 0xAA, 'h' ^ 0xAA, 'e' ^ 0xAA, 's' ^ 0xAA, 'H' ^ 0xAA, 'e' ^ 0xAA,
    'K' ^ 0xAA, 'n' ^ 0xAA, 'e' ^ 0xAA, 'w' ^ 0xAA, 'T' ^ 0xAA, 'h' ^ 0xAA, 'i' ^ 0xAA, 's' ^ 0xAA,
    'F' ^ 0xAA, 'r' ^ 0xAA, 'a' ^ 0xAA, 'g' ^ 0xAA, 'm' ^ 0xAA, 'e' ^ 0xAA, 'n' ^ 0xAA, 't' ^ 0xAA};
// Tiny xorshift32 PRNG
static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

// OTP payload helpers
static void build_otp_payload(char otp[64]);

// Config/lock prototypes
static ATCA_STATUS write_config(atecc608_config_t *config);
static ATCA_STATUS lock_config();
static ATCA_STATUS lock_data();
static ATCA_STATUS lock_slot(uint16_t slot);
static ATCA_STATUS verify_slot_hmac(uint16_t slot, const uint8_t expected_key[32]);
static ATCA_STATUS verify_slot_mac(uint16_t slot, const uint8_t expected_key[32]);

// Internal state
static bool config_locked = false;
static bool data_locked   = false;
static uint8_t config_buffer[sizeof(atecc608_config_t)];
static atecc608_config_t *config = NULL;
static bool atcab_initialized    = false;
static bool se_init_done         = false;

// Mutex to protect cryptoauthlib state (not I2C bus - i2c_manager handles that)
static SemaphoreHandle_t cryptoauth_mutex = NULL;

// Slot utility macros
#define SLOT_LOCKED(slot)   (((config->SlotLocked) & (1u << (slot))) == 0)
#define SLOT_UNLOCKED(slot) (((config->SlotLocked) & (1u << (slot))) != 0)

/**
 * @brief Macros for protecting cryptoauthlib state since it is not thread-safe
 */

#define CRYPTOAUTH_LOCK()                                                               \
    do {                                                                                \
        if (!cryptoauth_mutex) {                                                        \
            ESP_LOGE(TAG, "cryptoauth mutex is NULL!");                                 \
            return ESP_ERR_INVALID_STATE;                                               \
        }                                                                               \
        if (xSemaphoreTakeRecursive(cryptoauth_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) { \
            ESP_LOGE(TAG, "Failed to take cryptoauth mutex within 5 seconds");          \
            return ESP_ERR_TIMEOUT;                                                     \
        }                                                                               \
    } while (0)
#define CRYPTOAUTH_UNLOCK()                        \
    do {                                           \
        xSemaphoreGiveRecursive(cryptoauth_mutex); \
    } while (0)

/**
 * @brief Slot info registry
 */
typedef union {
    struct {
        uint32_t magic;       // 0xDEADBEEF
        uint16_t version;     // 0x0001
        uint16_t touch_flags; // 1 bit per slot to indicate if it has ever been written to
        uint32_t crc32;       // CRC32 of the structure up to this point
    } f;
    uint8_t raw[SLOT_CAP(SE_KEY_REGISTRY)];
} slot_registry_t;
static_assert(sizeof(slot_registry_t) == SLOT_CAP(SE_KEY_REGISTRY), "Slot registry does not fit in registry slot");
#define SLOT_REGISTRY_MAGIC 0xDEADBEEF
#define SLOT_REGISTRY_VER   0x0001
static slot_registry_t slot_registry;

// clang-format off
static esp_err_t se_registry_save() {
    if (!data_locked) {
        ESP_LOGW(TAG, "Data zone is not locked... cannot save slot registry");
        return ESP_ERR_INVALID_STATE;
    }
    slot_registry.f.crc32 = esp_rom_crc32_le(UINT32_MAX, (const uint8_t *)&slot_registry, offsetof(slot_registry_t, f.crc32));
    return se_write_slot(SE_KEY_REGISTRY, (uint8_t *)&slot_registry, sizeof(slot_registry));
}
static bool se_registry_valid() {
    if (slot_registry.f.magic != SLOT_REGISTRY_MAGIC) { return false; }
    if (slot_registry.f.version != SLOT_REGISTRY_VER) { return false; }
    uint32_t crc = esp_rom_crc32_le(UINT32_MAX, (const uint8_t *)&slot_registry, offsetof(slot_registry_t, f.crc32));
    if (crc != slot_registry.f.crc32) {
        ESP_LOGW(TAG, "Slot registry CRC32 mismatch: calc=0x%08X read=0x%08X", crc, slot_registry.f.crc32);
        ESP_LOG_BUFFER_HEXDUMP(TAG, &slot_registry, sizeof(slot_registry), ESP_LOG_DEBUG);
        return false;
    }
    return true;
}
static void se_touch_slot(se_slot_id_t slot) {
    if (slot > SE_KEY_MAX)                         { return; }
    if (!se_registry_valid())                      { return; }
    if (slot_registry.f.touch_flags & (1 << slot)) { return; }
    slot_registry.f.touch_flags |= (1 << slot);
    se_registry_save();
}
static void se_untouch_slot(se_slot_id_t slot) {
    if (slot > SE_KEY_MAX)                                { return; }
    if (!se_registry_valid())                             { return; }
    if ((slot_registry.f.touch_flags & (1 << slot)) == 0) { return; }
    slot_registry.f.touch_flags &= ~(1 << slot);
    se_registry_save();
}
bool se_slot_touched(se_slot_id_t slot) {
    if (slot > SE_KEY_MAX)    { return false; }
    if (!se_registry_valid()) { return false; }
    return (slot_registry.f.touch_flags & (1 << slot)) != 0;
}
// clang-format on
static esp_err_t se_registry_reset() {
    memset(&slot_registry, 0, sizeof(slot_registry));
    slot_registry.f.magic       = SLOT_REGISTRY_MAGIC;
    slot_registry.f.version     = SLOT_REGISTRY_VER;
    slot_registry.f.touch_flags = 0x0000 | (1 << SE_KEY_REGISTRY);
    if (data_locked) {
        // Some slots are obviously "touched" since they get locked immediately after write
        for (se_slot_id_t slot = SE_KEY_IO_KEY; slot <= SE_KEY_MAX; slot++) {
            if (SLOT_LOCKED(slot)) {
                slot_registry.f.touch_flags |= (1 << slot);
            }
        }
        // See if the WiFi SSID "looks" like it has had the actual SSID written to it
        uint8_t ssid_data[SLOT_CAP(SE_KEY_WIFI_SSID)] = {0};
        if (se_read_slot(SE_KEY_WIFI_SSID, ssid_data, sizeof(ssid_data)) == ESP_OK) {
            enum { SSID_LEN = sizeof(CONFIG_CONFERENCE_WIFI_SSID) - 1 };
            enum { OFFSET = 5 }; // Compare from bytes after the first 5 bytes ("Llama" vs. "Badge" will differ)
            const uint8_t compare_len = ((int)SSID_LEN > (int)OFFSET) ? (uint8_t)(SSID_LEN - OFFSET) : 0;
            bool differs              = false;
            if (compare_len > 0) {
                if (OFFSET + compare_len <= sizeof(ssid_data)) {
                    differs = memcmp(ssid_data + OFFSET, (const uint8_t *)CONFIG_CONFERENCE_WIFI_SSID + OFFSET, compare_len) != 0;
                } else {
                    differs = true;
                }
            }
            if (!differs) {
                ESP_LOGD(TAG, "WiFi SSID slot appears to contain a matching SSID (%s)... marking SSID/password slots as touched",
                         ssid_data);
                slot_registry.f.touch_flags |= (1 << SE_KEY_WIFI_SSID);
                slot_registry.f.touch_flags |= (1 << SE_KEY_WIFI_PASSWORD);
            } else {
                ESP_LOGD(TAG, "WiFi SSID slot appears untouched");
            }
        }
    }
    ESP_LOGD(TAG, "Slot registry reset: magic=0x%08X version=0x%04X touched=0x%04X", slot_registry.f.magic,
             slot_registry.f.version, slot_registry.f.touch_flags);
    return se_registry_save();
}
static esp_err_t se_registry_load() {
    uint8_t buf[SLOT_CAP(SE_KEY_REGISTRY)] = {0};
    ESP_RETURN_ON_ERROR(se_read_slot(SE_KEY_REGISTRY, buf, sizeof(buf)), TAG, "Failed to read registry slot");
    memcpy(&slot_registry.raw, buf, sizeof(slot_registry_t));
    if (!se_registry_valid()) {
        ESP_LOGW(TAG, "Slot registry is invalid... resetting now");
        return se_registry_reset();
    }
    ESP_LOGD(TAG, "Slot registry loaded: magic=0x%08X version=0x%04X touched=0x%04X", slot_registry.f.magic,
             slot_registry.f.version, slot_registry.f.touch_flags);
    return ESP_OK;
}

esp_err_t se_init() {
    // Check if already initialized
    if (se_init_done) {
        ESP_LOGD(TAG, "Secure element already initialized");
        return ESP_OK;
    }

    // Create the mutex
    if (cryptoauth_mutex == NULL) {
        cryptoauth_mutex = xSemaphoreCreateMutex();
        if (cryptoauth_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create cryptoauth mutex");
            return ESP_FAIL;
        }
    }

    CRYPTOAUTH_LOCK();

    ATCA_STATUS status = atcab_init(&atca_cfg);
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize ATECC608B chip: %d", status);
        CRYPTOAUTH_UNLOCK();
        return ESP_FAIL;
    }

    // Mark atcab as initialized so other functions can use atcab_* calls
    atcab_initialized = true;

    // DEBUG: Set the log level for the I2C interface to verbose for I2C debugging
    // ATCAIface iface = atGetIFace(atcab_get_device());
    // if (iface != NULL) {
    //     ((i2c_manager_device_config_t *)iface->hal_data)->log_level = ESP_LOG_VERBOSE;
    // }

    // Read the chip config
    if (atcab_read_config_zone(config_buffer) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to read ATECC608B config zone");
        return ESP_FAIL;
    }
    config = (atecc608_config_t *)config_buffer;
    // if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
    //     se_dump_config(config);
    // }

    // Get the config and data zone lock states (0x00 - locked, 0x55 - unlocked)
    config_locked = config->LockConfig == ATCA_LOCKED;
    data_locked   = config->LockValue == ATCA_LOCKED;

    // ---------------------------------------------------------------------------------------------
    // Write the config zone
    // ---------------------------------------------------------------------------------------------

    if (!config_locked) {
        // Write the config zone
        ESP_LOGI(TAG, "Config zone is not locked... writing configuration");
        status = write_config(config);
        if (status != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to write ATECC608B config zone: %d", status);
            return ESP_FAIL;
        }

        // Lock it so that we can write to data slots and use the chip
        ESP_LOGW(TAG, "Locking config zone");
        status = lock_config();
        if (status != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to lock ATECC608B config zone: %d", status);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Config zone locked successfully... the chip is now cryptographically enabled");

        // Reload config now that it is locked
        if (atcab_read_config_zone(config_buffer) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to read ATECC608B config zone after locking");
            return ESP_FAIL;
        }
        config = (atecc608_config_t *)config_buffer;
        if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
            se_dump_config(config);
        }

        // Re-check the lock states
        config_locked = config->LockConfig == ATCA_LOCKED;
        data_locked   = config->LockValue == ATCA_LOCKED;
    }

    // ---------------------------------------------------------------------------------------------
    // Write the Data/OTP zone
    // ---------------------------------------------------------------------------------------------

    if (!data_locked) {
        // Populate the OTP data
        uint8_t otp_data[64] = {0};
        build_otp_payload((char *)otp_data);
        ESP_LOGI(TAG, "Writing OTP data: %s", otp_data);
        status = atcab_write_bytes_zone(ATCA_ZONE_OTP, 0, 0, otp_data, sizeof(otp_data));
        if (status != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to write OTP data zone: %d", status);
            return ESP_FAIL;
        }

        // Lock the data zone
        ESP_LOGW(TAG, "Data zone is not locked... locking it now");
        status = lock_data();
        if (status != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to lock ATECC608B data zone: %d", status);
            return ESP_FAIL;
        }
    }

    // // DEBUG: Fetch and print the OTP data
    // if (data_locked) {
    //     uint8_t otp_data[64] = {0};
    //     if ((status = atcab_read_bytes_zone(ATCA_ZONE_OTP, 0, 0, otp_data, sizeof(otp_data))) != ATCA_SUCCESS) {
    //         ESP_LOGE(TAG, "Failed to read OTP data zone: %d", status);
    //         return ESP_FAIL;
    //     }
    //     ESP_LOGI(TAG, "OTP data:");
    //     ESP_LOG_BUFFER_HEXDUMP(TAG, otp_data, sizeof(otp_data), ESP_LOG_INFO);

    //     // DEBUG: Extract and dump the easter egg from the OTP data
    //     char *easter_egg_b64 = strrchr((char *)otp_data, '|');
    //     if (easter_egg_b64 != NULL) {
    //         easter_egg_b64++;
    //         ESP_LOGI(TAG, "Base64 encoded easter egg: %s", easter_egg_b64);

    //         // Decode the base64-encoded easter egg
    //         uint8_t decoded_data[64] = {0};
    //         size_t decoded_len       = sizeof(decoded_data);
    //         if (base64_decode(easter_egg_b64, strlen(easter_egg_b64), decoded_data, &decoded_len) == ESP_OK) {
    //             ESP_LOGI(TAG, "Decoded easter egg:");
    //             ESP_LOG_BUFFER_HEXDUMP(TAG, decoded_data, decoded_len, ESP_LOG_INFO);
    //         }
    //     }
    // }

    // ---------------------------------------------------------------------------------------------
    // Initialize the I/O protection key
    // ---------------------------------------------------------------------------------------------

    // Create/initialize the key
    ESP_LOGI(TAG, "Initializing I/O Protection Key");
    status = create_io_key(); // Calling this _should_ be idempotent
    if (status == (SE_ERR_SLOT_LOCKED | ATCA_SLOT_LOCKED(SE_KEY_IO_KEY))) {
        ESP_LOGI(TAG, "I/O Protection Key is already locked... continuing");
    } else if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize I/O Protection Key: %d", status);
        return ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Verifying I/O Protection Key");

        // Re-derive the I/O Protection Key for verification
        uint8_t io_key[32] = {0};
        if ((status = compute_io_key(io_key)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to get configured I/O Protection Key for verification: %d", status);
            return ESP_FAIL;
        }
        if ((status = verify_slot_mac(SE_KEY_IO_KEY, io_key)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to verify I/O Protection Key via MAC: %d", status);
            return ESP_FAIL;
        }

        // Slot contents have been verified so we can lock them as long as the device is in a secure state
        bool io_locked = SLOT_LOCKED(SE_KEY_IO_KEY) && SLOT_LOCKED(SE_KEY_IO_SEED);
        if (config_locked && data_locked && !io_locked) {
            ESP_LOGW(TAG, "Locking I/O Protection Key and Seed slots");
            lock_slot(SE_KEY_IO_KEY);
            lock_slot(SE_KEY_IO_SEED);
        } else {
            ESP_LOGW(TAG, "Skipping locking of I/O Protection Key and Seed slots because the device is not yet secured");
        }
    }

    // Make sure we have erased the I/O key seed from NVS if the slots are locked
    nvs_handle_t nvs_handle;
    esp_err_t err = ESP_OK;
    if (!nvs_ready() || (err = nvs_open(SE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", SE_NVS_NAMESPACE, esp_err_to_name(err));
    } else if (SLOT_LOCKED(SE_KEY_IO_KEY) && SLOT_LOCKED(SE_KEY_IO_SEED)) {
        size_t required_size = 0;
        err                  = nvs_get_blob(nvs_handle, SE_NVS_KEY_NAME, NULL, &required_size);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "I/O key seed not found in NVS... nothing to delete");
            nvs_close(nvs_handle);
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to check for existence of NVS key '%s': %s", SE_NVS_KEY_NAME, esp_err_to_name(err));
            nvs_close(nvs_handle);
        } else {
            // Key exists - erase it
            ESP_LOGI(TAG, "Deleting I/O key seed from NVS");
            if ((status = nvs_erase_key(nvs_handle, SE_NVS_KEY_NAME)) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to delete NVS key '%s': %d", SE_NVS_KEY_NAME, status);
                nvs_close(nvs_handle);
            } else {
                if ((status = nvs_commit(nvs_handle)) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to commit NVS changes: %d", status);
                } else {
                    ESP_LOGI(TAG, "I/O key seed deleted from NVS successfully");
                }
                nvs_close(nvs_handle);
            }
        }
    }

    // Now update the initialization status
    se_init_done = true;
    CRYPTOAUTH_UNLOCK();

    // Load the slot registry
    if (se_registry_load() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load slot registry");
    }

    return ESP_OK;
}

static void get_device_mask(uint8_t mask[32]) {
    // Get factory MAC address and ATECC608 serial
    uint8_t esp_mac[6];
    esp_efuse_mac_get_default(esp_mac);
    uint8_t se_serial[ATCA_SERIAL_NUM_SIZE];
    atcab_read_serial_number(se_serial);

    uint8_t data[6 + ATCA_SERIAL_NUM_SIZE];
    memcpy(data + ATCA_SERIAL_NUM_SIZE, esp_mac, 6);
    memcpy(data, se_serial, ATCA_SERIAL_NUM_SIZE);
    sha256(data, sizeof(data), mask);
    ESP_LOG_BUFFER_HEXDUMP(TAG, mask, 32, ESP_LOG_DEBUG);
}

ATCA_STATUS io_key_seed(uint8_t seed_out[32]) {
    ATCA_STATUS status       = ATCA_SUCCESS;
    esp_err_t err            = ESP_OK;
    static bool seed_slotted = false; // Seed should have been written to the slot already

    nvs_handle_t nvs_handle = 0;
    uint8_t masked[32]      = {0};
    size_t masked_len       = sizeof(masked);
    uint8_t mask[32]        = {0};
    uint8_t seed[32]        = {0};
    bool nvs_opened         = false;
    bool seed_loaded        = false;

    // clang-format off
    // Local helper macros to simplify the code
    #define SCRUB(buf)          do { scrubmem((buf), sizeof(buf));                                   } while (0)
    #define FAIL(st, fmt, ...)  do { status = (st); ESP_LOGE(TAG, fmt, ##__VA_ARGS__); goto cleanup; } while (0)
    // clang-format on

    // Data zone is locked - try to read from chip slot before fallback to NVS
    if (data_locked) {
        if ((status = atcab_read_bytes_zone(ATCA_ZONE_DATA, SE_KEY_IO_SEED, 0, masked, sizeof(masked))) == ATCA_SUCCESS) {
            seed_slotted = true;

            // Get the device mask and unmask the seed
            get_device_mask(mask);
            for (size_t i = 0; i < sizeof(masked); i++) {
                seed[i] = masked[i] ^ mask[i];
            }
            seed_loaded = true;
            ESP_LOGD(TAG, "Retrieved I/O key seed from slot %d", SE_KEY_IO_SEED);
            goto done;
        } else {
            ESP_LOGW(TAG, "Could not read I/O key seed from slot %d: %d", SE_KEY_IO_SEED, status);
        }
    }

    // Data zone is unlocked OR chip read failed - use NVS storage as fallback
    if (!nvs_ready() || (err = nvs_open(SE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle)) != ESP_OK) {
        FAIL(ATCA_COMM_FAIL, "Failed to open NVS namespace: %s", esp_err_to_name(err));
    }
    nvs_opened = true;

    if ((err = nvs_get_blob(nvs_handle, SE_NVS_KEY_NAME, masked, &masked_len)) == ESP_ERR_NVS_NOT_FOUND) {
        // Get the device mask and generate seed + masked seed
        get_device_mask(mask);
        esp_fill_random(seed, sizeof(seed));
        seed_loaded = true;
        for (size_t i = 0; i < sizeof(masked); i++) {
            masked[i] = seed[i] ^ mask[i];
        }

        // Store the masked seed in NVS
        if ((err = nvs_set_blob(nvs_handle, SE_NVS_KEY_NAME, masked, sizeof(masked))) != ESP_OK) {
            FAIL(ATCA_COMM_FAIL, "Failed to write I/O key seed to NVS: %s", esp_err_to_name(err));
        }
        if ((err = nvs_commit(nvs_handle)) != ESP_OK) {
            FAIL(ATCA_COMM_FAIL, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "Generated new I/O key seed and wrote to NVS");
    } else if (err != ESP_OK) {
        FAIL(ATCA_COMM_FAIL, "Failed to read seed from NVS: %s", esp_err_to_name(err));
    } else {
        get_device_mask(mask);
        for (size_t i = 0; i < sizeof(masked); ++i) {
            seed[i] = masked[i] ^ mask[i];
        }
        seed_loaded = true;

        ESP_LOGD(TAG, "Retrieved I/O key seed from NVS");
    }

    bool slot_locked = false;
    if ((status = atcab_is_slot_locked(SE_KEY_IO_SEED, &slot_locked)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to check if I/O key seed slot %d is locked: %d", SE_KEY_IO_SEED, status);
        goto done;
    }
    if (!slot_locked && !seed_slotted) {
        if ((status = atcab_write_bytes_zone(ATCA_ZONE_DATA, SE_KEY_IO_SEED, 0, masked, sizeof(masked))) == ATCA_SUCCESS) {
            seed_slotted = true;
            ESP_LOGD(TAG, "Wrote existing I/O key seed to chip slot %d for good measure", SE_KEY_IO_SEED);
        } else {
            ESP_LOGW(TAG, "Failed to write I/O key seed to chip slot %d: %d (will remain in NVS)", SE_KEY_IO_SEED, status);
            status = ATCA_SUCCESS; // Don't fail if chip write fails, we have NVS copy
        }
    }

done:
    // clang-format off
    if (!seed_loaded) { FAIL(ATCA_GEN_FAIL, "No I/O key seed available"); }
    memcpy(seed_out, seed, sizeof(seed));
    // clang-format on

cleanup:
    // clang-format off
    if (nvs_opened) { nvs_close(nvs_handle); }
    SCRUB(masked);
    SCRUB(mask);
    SCRUB(seed);

    #undef SCRUB
    #undef FAIL
    return status;
    // clang-format on
}

ATCA_STATUS compute_io_key(uint8_t key_out[ATCA_KEY_SIZE]) {
    // Get the seed from NVS
    uint8_t seed[32];
    ATCA_STATUS status = io_key_seed(seed);
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to get IO key seed: %d", status);
        return status;
    }

    // Compute the derivation key
    uint8_t dkey[32]     = {0};
    uint8_t mac_bytes[6] = {0};
    esp_efuse_mac_get_default(mac_bytes);
    // clang-format off
    uint64_t preseed = 0xFEEDC0FFEE01u ^ (
        ((uint64_t)mac_bytes[0]      ) |
        ((uint64_t)mac_bytes[1] <<  8) |
        ((uint64_t)mac_bytes[2] << 16) |
        ((uint64_t)mac_bytes[3] << 24) |
        ((uint64_t)mac_bytes[4] << 32) |
        ((uint64_t)mac_bytes[5] << 40)
    );
    uint32_t prngseed = (uint32_t)(preseed ^ (preseed >> 32));
    static const uint8_t permutations[32] = {
        17, 0,  29, 8,  23, 4, 12, 31, 6,  20, 1, 26, 14, 9,  27, 3,
        7,  16, 30, 11, 22, 2, 19, 25, 28, 10, 5, 24, 21, 18, 13, 15
    };
    // clang-format on
    for (size_t i = 0; i < sizeof(dkey); i++) {
        uint32_t r  = xs32(&prngseed);
        uint8_t m   = (uint8_t)r ^ (uint8_t)(r >> 8) ^ (uint8_t)(r >> 16) ^ (uint8_t)(r >> 24);
        uint8_t idx = permutations[i];
        dkey[idx]   = (uint8_t)(dkey_frag_a[idx] ^ dkey_frag_b[31 - idx] ^ m);
    }

    // I/O key is the SHA256 hash of the seed and derivation key
    uint8_t buf[sizeof(seed) + sizeof(dkey)];
    memcpy(buf, seed, sizeof(seed));
    memcpy(buf + sizeof(seed), dkey, sizeof(dkey));
    sha256(buf, sizeof(buf), key_out);

    // Scrub sensitive data
    scrubmem(buf, sizeof(buf));
    scrubmem(dkey, sizeof(dkey));

    return ATCA_SUCCESS;
}

/**
 * @brief Compute HMAC-SHA256 on the host the same as atcab_sha_hmac would
 *        on the ATECC chip. Useful mostly for testing and validation. Requires
 *        the same key and data inputs as the ATECC chip.
 *
 * @param key 32-byte key
 * @param data pointer to the data to hash
 * @param data_len length of the data to hash
 * @param[out] hmac pointer to the output buffer for the HMAC
 */
ATCA_STATUS host_sha_hmac(const uint8_t key[ATCA_KEY_SIZE], const uint8_t *data, size_t data_len,
                          uint8_t hmac[ATCA_SHA2_256_DIGEST_SIZE]) {
    // ipad/opad key block
    uint8_t k_ipad[ATCA_SHA2_256_BLOCK_SIZE] = {0};
    uint8_t k_opad[ATCA_SHA2_256_BLOCK_SIZE] = {0};
    memset(k_ipad, 0x36, sizeof(k_ipad));
    memset(k_opad, 0x5C, sizeof(k_opad));
    for (size_t i = 0; i < ATCA_KEY_SIZE; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    // inner = SHA256(k_ipad || data)
    atcac_sha2_256_ctx_t hmac_ctx;
    uint8_t inner[ATCA_SHA2_256_DIGEST_SIZE] = {0};
    atcac_sw_sha2_256_init(&hmac_ctx);
    atcac_sw_sha2_256_update(&hmac_ctx, k_ipad, sizeof(k_ipad));
    atcac_sw_sha2_256_update(&hmac_ctx, data, data_len);
    atcac_sw_sha2_256_finish(&hmac_ctx, inner);

    // outer = SHA256(k_opad || inner)
    atcac_sw_sha2_256_init(&hmac_ctx);
    atcac_sw_sha2_256_update(&hmac_ctx, k_opad, sizeof(k_opad));
    atcac_sw_sha2_256_update(&hmac_ctx, inner, sizeof(inner));
    atcac_sw_sha2_256_finish(&hmac_ctx, hmac);

    // Scrub the key buffers
    scrubmem(k_ipad, sizeof(k_ipad));
    scrubmem(k_opad, sizeof(k_opad));
    scrubmem(inner, sizeof(inner));

    return ATCA_SUCCESS;
}

ATCA_STATUS create_io_key() {
    ATCA_STATUS status = ATCA_SUCCESS;

    // Pre-checks
    if (!config_locked) {
        ESP_LOGE(TAG, "Config zone is not locked, cannot create I/O key");
        return ATCA_NOT_LOCKED;
    }
    bool slot_locked = false;
    if ((status = atcab_is_slot_locked(SE_KEY_IO_KEY, &slot_locked)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to check if slot %d is locked: %d", SE_KEY_IO_KEY, status);
        return status;
    }
    if (slot_locked) {
        ESP_LOGD(TAG, "Slot %d is locked, skipping I/O key creation", SE_KEY_IO_KEY);
        return SE_ERR_SLOT_LOCKED | ATCA_SLOT_LOCKED(SE_KEY_IO_KEY);
    }

    // ---------------------------------------------------------------------------------------------
    // Generate (and write masked seed to NVS) I/O key and write to slot 0
    // ---------------------------------------------------------------------------------------------

    // Generate a new IO key
    uint8_t io_key[32];
    if ((status = compute_io_key(io_key)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to derive IO key: %d", status);
        return status;
    }

    // Write the IO key to slot 0
    uint8_t num_in[NONCE_NUMIN_SIZE] = {0};
    esp_fill_random(num_in, sizeof(num_in));
    if ((status = atcab_write_bytes_zone(ATCA_ZONE_DATA, SE_KEY_IO_KEY, 0, io_key, sizeof(io_key))) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to write IO key to slot %d: %d", SE_KEY_IO_KEY, status);
        scrubmem(io_key, sizeof(io_key));
        return status;
    } else {
        ESP_LOGI(TAG, "Wrote IO key to slot %d", SE_KEY_IO_KEY);
        ESP_LOG_BUFFER_HEXDUMP(TAG, io_key, sizeof(io_key), ESP_LOG_DEBUG);
        scrubmem(io_key, sizeof(io_key));
    }

    // ---------------------------------------------------------------------------------------------
    // Do a data-lock independent pairing check to ensure the IO key was correctly written
    // ---------------------------------------------------------------------------------------------

    // Re-read the seed and reobtain the I/O key for verification
    if ((status = compute_io_key(io_key)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to derive IO key: %d", status);
        return status;
    }

    // Now verify that the secret contents written match the computed I/O key
    if ((status = verify_slot_hmac(SE_KEY_IO_KEY, io_key)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to verify slot %d: %d", SE_KEY_IO_KEY, status);
        return status;
    }

    return ATCA_SUCCESS;
}

static void unmask_append(char *dst, size_t *off, const uint8_t *masked, size_t masked_len, uint8_t key) {
    for (size_t i = 0; i < masked_len; i++) {
        dst[(*off)++] = (char)(masked[i] ^ key);
    }
}
void build_otp_payload(char otp[64]) {
    // Mask key
    enum { K = 0x5A };

    // --- Prefix fragments (obfuscated) ---
    // "Oct 2025|SAINTCON Badge Team|"
    static const uint8_t f1[] = {'2' ^ K, '0' ^ K, '2' ^ K, '5' ^ K};                                              // "2025"
    static const uint8_t f2[] = {'|' ^ K};                                                                         // "|"
    static const uint8_t f3[] = {'S' ^ K, 'A' ^ K, 'I' ^ K, 'N' ^ K, 'T' ^ K, 'C' ^ K, 'O' ^ K, 'N' ^ K, ' ' ^ K}; // "SAINTCON "
    static const uint8_t f4[] = {'B' ^ K, 'a' ^ K, 'd' ^ K, 'g' ^ K, 'e' ^ K, ' ' ^ K};                            // "Badge "
    static const uint8_t f5[] = {'T' ^ K, 'e' ^ K, 'a' ^ K, 'm' ^ K};                                              // "Team"
    static const uint8_t f6[] = {'|' ^ K};                                                                         // "|"

    // --- Base64 input payload (obfuscated) ---
    // "LEFTY_LOOSEY//RIGHTY_TIGHTY"
    static const uint8_t p1[] = {
        'L' ^ K, 'E' ^ K, 'F' ^ K, 'T' ^ K, 'Y' ^ K, '_' ^ K, 'L' ^ K, 'O' ^ K, 'O' ^ K, //
        'S' ^ K, 'E' ^ K, 'Y' ^ K, '/' ^ K, '/' ^ K, 'R' ^ K, 'I' ^ K, 'G' ^ K, 'H' ^ K, //
        'T' ^ K, 'Y' ^ K, '_' ^ K, 'T' ^ K, 'I' ^ K, 'G' ^ K, 'H' ^ K, 'T' ^ K, 'Y' ^ K  //
    };

    char buf[64]  = {0};
    size_t offset = 0;

    unmask_append(buf, &offset, f1, sizeof(f1), K);
    unmask_append(buf, &offset, f2, sizeof(f2), K);
    unmask_append(buf, &offset, f3, sizeof(f3), K);
    unmask_append(buf, &offset, f4, sizeof(f4), K);
    unmask_append(buf, &offset, f5, sizeof(f5), K);
    unmask_append(buf, &offset, f6, sizeof(f6), K);

    // Decode p1 to plaintext then base64-encode into the OTP buffer.
    uint8_t phrase[sizeof(p1)];
    for (size_t i = 0; i < sizeof(p1); i++) {
        phrase[i] = (uint8_t)(p1[i] ^ K);
    }

    char b64[48]  = {0}; // Should be big enough for base64-encoded output from the 27 bytes in
    size_t b64len = sizeof(b64);
    ESP_LOGD(TAG, "Base64 input [%d bytes]: %s", sizeof(phrase), phrase);
    base64_encode(phrase, sizeof(phrase), b64, &b64len);
    ESP_LOGD(TAG, "Base64 output [%d bytes]: %s", b64len, b64);
    ESP_LOGD(TAG, "Current OTP buffer length: %d", offset);
    if (b64len + offset > 64) {
        ESP_LOGW(TAG, "Base64 output truncated to fit OTP buffer");
        b64len = 64 - offset;
    }

    // Append base64 to buf
    memcpy(buf + offset, b64, b64len);
    offset += b64len;
    memset(otp, 0, 64);
    memcpy(otp, buf, offset);

    // Scrub stuff
    memset(buf, 0, sizeof(buf));
    memset(phrase, 0, sizeof(phrase));
    memset(b64, 0, sizeof(b64));
}

ATCA_STATUS write_config(atecc608_config_t *config) {
    ATCA_STATUS status = ATCA_SUCCESS;

    // Clear the slot and key config buffers
    memset(config->SlotConfig, 0, sizeof(config->SlotConfig));
    memset(config->KeyConfig, 0, sizeof(config->KeyConfig));

    // Basic device config
    config->AES_Enable  = ATCA_AES_ENABLE_EN_MASK; // Enable AES
    config->ChipOptions = (                        //
        ATCA_CHIP_OPT_IO_PROT_EN_MASK              // Enable IO protection
        | ATCA_CHIP_OPT_KDF_AES_EN_MASK            // Enable AES KDF
        | ATCA_CHIP_OPT_AUTO_CLEAR_FAIL_MASK       // Auto clear health check first fail
        | ATCA_CHIP_OPT_ECDH_PROT(0b00)            // ECDH output protection - clear text allowed
        | ATCA_CHIP_OPT_KDF_PROT(0b00)             // KDF output protection - clear text allowed
        | ATCA_CHIP_OPT_IO_PROT_KEY(0x0)           // IO protection key slot 0
    );

    // ----- Slot 0 [36 bytes]: IO Protection Key (secret, clear text writable, no read, lockable) -----
    config->SlotConfig[0] = (                   //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                                //   - Write: Always
    );
    config->KeyConfig[0]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 1 [36 bytes]: I/O Protection Key masked seed (non-secret, clear text R/W, lockable) -----
    config->SlotConfig[1] = (                 //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[1]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
    );

    // ----- Slot 2 [36 bytes]: (H)MAC Pre-shared Key (secret, encrypted write via key 0, lockable) -----
    config->SlotConfig[2] = (                   //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_WRITE_KEY(0)         // Write key 0
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0100) // Write config 0b0100
                                                //   - Write: Encrypt
    );
    config->KeyConfig[2]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 3 [36 bytes]: Hash prefix (secret, encrypted R/W via key 0, lockable) -----
    config->SlotConfig[3] = (                   //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_ENC_READ_MASK        // Encrypted read
        | ATCA_SLOT_CONFIG_READKEY(0)           // Read key 0
        | ATCA_SLOT_CONFIG_WRITE_KEY(0)         // Write key 0
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0100) // Write config 0b0100
                                                //   - Write: Encrypt
    );
    config->KeyConfig[3]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 4 [36 bytes]: ECC P-256 private key (secret, Sign + ECDH + GenKey, ECDH output clear, regenerable) -----
    config->SlotConfig[4] = (             //
        ATCA_SLOT_CONFIG_EXT_SIG_MASK     // Enable external signature
        | ATCA_SLOT_CONFIG_INT_SIG_MASK   // Enable internal signature
        | ATCA_SLOT_CONFIG_ECDH_MASK      // Enable ECDH
        | ATCA_SLOT_CONFIG_IS_SECRET_MASK // Slot contents are secret
        | ATCA_SLOT_CONFIG_GEN_KEY_MASK   // Generate key
    );
    config->KeyConfig[4]  = (                           //
        ATCA_KEY_CONFIG_PRIVATE_MASK                   // Private key
        | ATCA_KEY_CONFIG_PUB_INFO_MASK                // Public key info
        | ATCA_KEY_CONFIG_KEY_TYPE(ATCA_P256_KEY_TYPE) // Key type
        | ATCA_KEY_CONFIG_LOCKABLE_MASK                // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK              // Require random nonce for operations
    );

    // ----- Slot 5 [36 bytes]: AES-128 symmetric key (secret, encrypted write via key 0, no read, lockable) -----
    config->SlotConfig[5] = (                   //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_WRITE_KEY(0)         // Write key 0
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0111) // Write config 0b0111
                                                //   - Write: Encrypt
                                                //   - DeriveKey: Parent + no MAC auth
    );
    config->KeyConfig[5]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_AES_KEY_TYPE) // Key type AES
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 6 [36 bytes]: Slot Registry (non-secret, clear text R/W, not lockable) -----
    config->SlotConfig[6] = (                 //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[6]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
    );

    // ----- Slot 7 [36 bytes]: SHA-256 debug key (non-secret, clear text R/W, not lockable) -----
    config->SlotConfig[7] = (                 //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[7]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
    );

    // ----- Slot 8 [416 bytes]: General purpose - large data slot (clear text R/W, lockable) -----
    config->SlotConfig[8] = (                 //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[8]  = (                        //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA (or data)
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable
    );

    // ----- Slot 9 [72 bytes]: ECC P-256 public key (non-secret, clear text R/W, lockable) -----
    config->SlotConfig[9] = (                 //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[9]  = (                         //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_P256_KEY_TYPE) // Key type P-256
        | ATCA_KEY_CONFIG_LOCKABLE_MASK              // Lockable key
    );

    // ----- Slot 10 [72 bytes]: Data - WiFi SSID (secret, encrypted R/W via key 0, lockable) -----
    config->SlotConfig[10] = (                  //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_ENC_READ_MASK        // Reads encrypted
        | ATCA_SLOT_CONFIG_READKEY(0)           // Read key 0
        | ATCA_SLOT_CONFIG_WRITE_KEY(0)         // Write key 0
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0100) // Write config 0b0100
                                                //   - Write: Encrypt
    );
    config->KeyConfig[10]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 11 [72 bytes]: Data - WiFi Password (secret, encrypted R/W via key 0, lockable) -----
    config->SlotConfig[11] = (                  //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_ENC_READ_MASK        // Reads encrypted
        | ATCA_SLOT_CONFIG_READKEY(0)           // Read key 0
        | ATCA_SLOT_CONFIG_WRITE_KEY(0)         // Write key 0
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0100) // Write config 0b0100
                                                //   - Write: Encrypt
    );
    config->KeyConfig[11]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 12 [72 bytes]: CTF HMAC -----
    config->SlotConfig[12] = (                  //
        ATCA_SLOT_CONFIG_IS_SECRET_MASK         // Slot contents are secret
        | ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                                //   - Write: Always
    );
    config->KeyConfig[12]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
    );

    // ----- Slot 13 [72 bytes]: CTF hash prefix -----
    config->SlotConfig[13] = (                //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[13]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
    );

    // ----- Slot 14 [72 bytes]: CTF Hints Password (non-secret, clear text R/W, lockable) -----
    config->SlotConfig[14] = (                //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[14]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // ----- Slot 15 [72 bytes]: Open for future use (non-secret, clear text R/W, lockable) -----
    config->SlotConfig[15] = (                //
        ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0000) // Write config 0b0000
                                              //   - Write: Always
    );
    config->KeyConfig[15]  = (                       //
        ATCA_KEY_CONFIG_KEY_TYPE(ATCA_SHA_KEY_TYPE) // Key type SHA
        | ATCA_KEY_CONFIG_LOCKABLE_MASK             // Lockable key
        | ATCA_KEY_CONFIG_REQ_RANDOM_MASK           // Require random nonce for operations
    );

    // Write the config to the chip
    if ((status = atcab_write_config_zone(config_buffer)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to write ATECC608B config zone: %d", status);
    }
    return status;
}

// Lock the config zone
ATCA_STATUS lock_config() {
    if (config_locked) {
        ESP_LOGD(TAG, "Config zone is already locked, skipping");
        return ATCA_SUCCESS;
    }
    ATCA_STATUS status = atcab_lock_config_zone();
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to lock ATECC608B config zone: %d", status);
        return status;
    }
    config->LockConfig = ATCA_LOCKED;
    config_locked      = true;
    return ATCA_SUCCESS;
}

// Lock the data zone
ATCA_STATUS lock_data() {
    if (data_locked) {
        ESP_LOGD(TAG, "Data zone is already locked, skipping");
        return ATCA_SUCCESS;
    }
    ATCA_STATUS status = atcab_lock_data_zone();
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to lock ATECC608B data zone: %d", status);
        return status;
    }
    config->LockValue = ATCA_LOCKED;
    data_locked       = true;
    return ATCA_SUCCESS;
}

// Lock the given slot
ATCA_STATUS lock_slot(uint16_t slot) {
    ATCA_STATUS status = atcab_lock_data_slot(slot);
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to lock ATECC608B slot %d: %d", slot, status);
        return status;
    }
    config->SlotLocked &= ~(1 << slot);
    return ATCA_SUCCESS;
}

ATCA_STATUS verify_slot_hmac(uint16_t slot, const uint8_t expected_key[32]) {
    ATCA_STATUS status;
    uint8_t chip_hmac[32] = {0};
    uint8_t host_hmac[32] = {0};

    // Challenge - data to HMAC
    uint8_t challenge[32] = {0};
    esp_fill_random(challenge, sizeof(challenge));

    // Generate nonce
    uint8_t num_in[NONCE_NUMIN_SIZE]  = {0};
    uint8_t rand_out[RANDOM_NUM_SIZE] = {0};
    esp_fill_random(num_in, sizeof(num_in));
    if ((status = atcab_nonce_rand(num_in, rand_out)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to generate nonce: %d", status);
        return status;
    }

    // Calculate an on-chip HMAC using the given slot
    if ((status = atcab_sha_hmac(challenge, sizeof(challenge), slot, chip_hmac, SHA_MODE_TARGET_OUT_ONLY)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to calculate HMAC on chip: %d", status);
        return status;
    }

    // Calculate an equivalent HMAC on the host
    if ((status = host_sha_hmac(expected_key, challenge, sizeof(challenge), host_hmac)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to calculate HMAC on host: %d", status);
        return status;
    }

    // Compare the HMACs
    if (memcmp(chip_hmac, host_hmac, sizeof(chip_hmac)) != 0) {
        ESP_LOGE(TAG, "HMACs do not match for slot %d", slot);
        return ATCA_GEN_FAIL;
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS verify_slot_mac(uint16_t slot, const uint8_t expected_key[32]) {
    ATCA_STATUS status;
    uint8_t chip_mac[32] = {0};
    uint8_t host_mac[32] = {0};
    uint8_t serial[9]    = {0};
    if ((status = atcab_read_serial_number(serial)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to read serial number: %d", status);
        return status;
    }

    // Calculate Nonce + TempKey
    uint8_t num_in[NONCE_NUMIN_SIZE]  = {0};
    uint8_t rand_out[RANDOM_NUM_SIZE] = {0};
    esp_fill_random(num_in, sizeof(num_in));
    if ((status = atcab_nonce_rand(num_in, rand_out)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to generate nonce: %d", status);
        return status;
    }
    atca_temp_key_t temp_key = {0};
    atca_nonce_in_out_t ni   = {
          .mode     = NONCE_MODE_SEED_UPDATE,
          .zero     = 0,
          .num_in   = num_in,
          .rand_out = rand_out,
          .temp_key = &temp_key,
    };
    if ((status = atcah_nonce(&ni)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to compute Nonce input structure: %d", status);
        return status;
    }

    // Check the current TempKey.SourceFlag
    uint8_t output[INFO_SIZE] = {0};
    se_state_t *state         = (se_state_t *)output;
    if ((status = atcab_info_base(INFO_MODE_STATE, 0, output)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to get info: %d", status);
        return status;
    }

    // Calculate both the host-side and on-chip MACs using the given slot
    uint8_t mode = MAC_MODE_BLOCK2_TEMPKEY | MAC_MODE_INCLUDE_SN;
    if (state->TempKey_SourceFlag) {
        mode |= MAC_MODE_SOURCE_FLAG_MATCH;
    }
    atca_mac_in_out_t mi = {
        .mode      = mode,
        .key_id    = slot,
        .challenge = NULL,
        .otp       = NULL,
        .sn        = serial,
        .response  = host_mac,
        .key       = expected_key,
        .temp_key  = &temp_key,
    };
    if ((status = atcah_mac(&mi)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to compute MAC input structure: %d", status);
        return status;
    }
    if ((status = atcab_mac(mi.mode, mi.key_id, NULL, chip_mac)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to calculate MAC on chip: %d", status);
        return status;
    }

    // Compare the MACs
    if (memcmp(chip_mac, host_mac, sizeof(chip_mac)) != 0) {
        ESP_LOGE(TAG, "MACs do not match for slot %d", slot);
        return ATCA_GEN_FAIL;
    }
    return ATCA_SUCCESS;
}

// Check if the secure element is ready for operation: config + data zones locked and I/O key + seed slots locked
bool se_ready() {
    // Make sure the device/interface config reference is (still?) valid
    ATCADevice dev = atcab_get_device();
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to get secure element device instance");
        return false;
    }
    if (dev->mIface.mIfaceCFG == NULL) {
        ESP_LOGE(TAG, "Failed to get secure element interface configuration");
        return false;
    }
    if (dev->mIface.mIfaceCFG->atcai2c.address != CONFIG_ATCA_I2C_ADDRESS) {
        ESP_LOGE(TAG, "Secure element I2C address mismatch");
        return false;
    }

    // Check to see if all of the initialization has completed
    if (!atcab_initialized || !se_init_done) {
        ESP_LOGE(TAG, "Secure element not initialized... has se_init() been called yet?");
        return false;
    }
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL after init - this should not happen");
        return false;
    }

    return config_locked && data_locked && SLOT_LOCKED(SE_KEY_IO_KEY) && SLOT_LOCKED(SE_KEY_IO_SEED);
}

bool se_config_locked() {
    return config_locked;
}
bool se_data_locked() {
    return data_locked;
}

esp_err_t se_write_slot(se_slot_id_t slot, const uint8_t *value, size_t len) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for writing");
        return ESP_ERR_INVALID_STATE;
    }
    if (value == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for writing");
        return ESP_ERR_INVALID_ARG;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for writing", slot);
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0 || SLOT_CAP(slot) < len) {
        ESP_LOGE(TAG, "Invalid length %zu for writing to slot %d (max %zu)", len, slot, SLOT_CAP(slot));
        return ESP_ERR_INVALID_ARG;
    }
    if (SLOT_LOCKED(slot)) {
        ESP_LOGE(TAG, "Slot %d is locked, cannot write", slot);
        return ESP_ERR_INVALID_STATE;
    }

    ATCA_STATUS status = ATCA_SUCCESS;
    CRYPTOAUTH_LOCK();

    // If it needs to be an encrypted write
    if ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_WRITE_CONFIG(0b0100)) != 0) {
        uint8_t io_key[32] = {0};
        if ((status = compute_io_key(io_key)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to derive I/O key: %d", status);
            scrubmem(io_key, sizeof(io_key));
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
        uint8_t num_in[NONCE_NUMIN_SIZE] = {0};
        for (size_t i = 0; i < len; i += 32) {
            size_t remaining = len - i;
            size_t rlen      = (remaining >= 32) ? 32 : remaining;
            size_t block     = (i / 32);

            ESP_LOGD(TAG, "Writing slot %d block %d (offset %zu, len %zu)", slot, block, i, rlen);
            uint8_t data[32] = {0};
            memcpy(data, value + i, rlen);

            esp_fill_random(num_in, sizeof(num_in));
            if ((status = atcab_write_enc(slot, block, data, io_key, SE_KEY_IO_KEY, num_in)) != ATCA_SUCCESS) {
                ESP_LOGE(TAG, "Failed to write to slot %d block %d (offset %zu, len %zu): %d", slot, block, i, rlen, status);
                scrubmem(io_key, sizeof(io_key));
                CRYPTOAUTH_UNLOCK();
                return ESP_FAIL;
            }
            se_touch_slot(slot);
        }
        scrubmem(io_key, sizeof(io_key));
    }
    // Plaintext writes
    else if ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_WRITE_CONFIG(0b1111)) == 0) {
        if ((status = atcab_write_bytes_zone(ATCA_ZONE_DATA, slot, 0, value, len)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to write to slot %d: %d", slot, status);
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
        se_touch_slot(slot);
    } else {
        ESP_LOGE(TAG, "Slot %d is not writable", slot);
        CRYPTOAUTH_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Successfully wrote to slot %d", slot);
    CRYPTOAUTH_UNLOCK();
    return ESP_OK;
}

esp_err_t se_read_slot(se_slot_id_t slot, uint8_t *value, size_t len) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for reading");
        return ESP_ERR_INVALID_STATE;
    }
    if (value == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for reading");
        return ESP_ERR_INVALID_ARG;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for reading", slot);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate slot size against length
    if (slot < 8 && len > 32) { // 36 bytes but can only do 32 byte R/W
        ESP_LOGE(TAG, "Invalid length %zu for reading from slot %d", len, slot);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot == 8 && len > 416) { // 416 bytes max
        ESP_LOGE(TAG, "Invalid length %zu for reading from slot %d", len, slot);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot > 8 && len > 64) { // 72 bytes but we're doing 32 byte R/W
        ESP_LOGE(TAG, "Invalid length %zu for reading from slot %d", len, slot);
        return ESP_ERR_INVALID_ARG;
    }

    ATCA_STATUS status = ATCA_SUCCESS;
    CRYPTOAUTH_LOCK();

    // If it needs to be an encrypted read
    if (((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_ENC_READ_MASK) != 0) &&
        ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_IS_SECRET_MASK) != 0)) {
        uint8_t io_key[32] = {0};
        if ((status = compute_io_key(io_key)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to derive I/O key: %d", status);
            scrubmem(io_key, sizeof(io_key));
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
        uint8_t num_in[NONCE_NUMIN_SIZE] = {0};
        for (size_t i = 0; i < len; i += 32) {
            size_t remaining = len - i;
            size_t rlen      = (remaining >= 32) ? 32 : remaining;
            size_t block     = (i / 32);
            uint8_t data[32] = {0}; // We are doing 32-byte reads so capture the 32 bytes into an independent buffer in case
                                    // the value buffer is not large enough

            esp_fill_random(num_in, sizeof(num_in));
            if ((status = atcab_read_enc(slot, block, data, io_key, SE_KEY_IO_KEY, num_in)) != ATCA_SUCCESS) {
                ESP_LOGE(TAG, "Failed to read from slot %d block %d (offset %zu, len %zu): %d", slot, block, i, rlen, status);
                scrubmem(io_key, sizeof(io_key));
                CRYPTOAUTH_UNLOCK();
                return ESP_FAIL;
            }
            ESP_LOGD(TAG, "Read slot %d block %d (offset %zu, len %zu)", slot, block, i, rlen);

            // Copy the correct amount of remaining data to the value buffer
            memcpy(value + i, data, rlen);
        }
        scrubmem(io_key, sizeof(io_key));
    }
    // Plaintext reads
    else if (((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_ENC_READ_MASK) == 0) &&
             ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_IS_SECRET_MASK) == 0)) {
        if ((status = atcab_read_bytes_zone(ATCA_ZONE_DATA, slot, 0, value, len)) != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Failed to read from slot %d: %d", slot, status);
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Slot %d is not readable", slot);
        CRYPTOAUTH_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    CRYPTOAUTH_UNLOCK();
    return ESP_OK;
}

esp_err_t se_clear_slot(se_slot_id_t slot) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for clearing");
        return ESP_ERR_INVALID_STATE;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for clearing", slot);
        return ESP_ERR_INVALID_ARG;
    }
    if (SLOT_LOCKED(slot)) {
        ESP_LOGE(TAG, "Slot %d is locked, cannot clear", slot);
        return ESP_ERR_INVALID_STATE;
    }

    // Clear the slot by writing all zeros
    uint8_t zeros[SLOT_CAP(slot)];
    memset(zeros, 0, sizeof(zeros));
    esp_err_t err = se_write_slot(slot, zeros, sizeof(zeros));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear slot %d: %d", slot, err);
    } else {
        se_untouch_slot(slot);
        ESP_LOGD(TAG, "Successfully cleared slot %d", slot);
    }
    return err;
}

esp_err_t se_validate_slot(se_slot_id_t slot, const uint8_t *expected, size_t len) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for validation");
        return ESP_ERR_INVALID_STATE;
    }
    if (expected == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters for validation");
        return ESP_ERR_INVALID_ARG;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for validation", slot);
        return ESP_ERR_INVALID_ARG;
    }

    CRYPTOAUTH_LOCK();

    // Read the current value from the slot if the slot is readable
    if ((((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_ENC_READ_MASK) != 0) &&
         ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_IS_SECRET_MASK) != 0)) ||
        (((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_ENC_READ_MASK) == 0) &&
         ((config->SlotConfig[slot] & ATCA_SLOT_CONFIG_IS_SECRET_MASK) == 0))) {
        uint8_t current[len];
        memset(current, 0, len);

        esp_err_t err = se_read_slot(slot, current, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read from slot %d for validation: %d", slot, err);
            CRYPTOAUTH_UNLOCK();
            return err;
        }

        // Compare the current value with the expected value
        if (memcmp(current, expected, len) != 0) {
            ESP_LOGE(TAG, "Slot %d validation failed", slot);
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
    }
    // Attempt to validate using MAC
    else {
        ATCA_STATUS status = verify_slot_mac(slot, expected);
        if (status != ATCA_SUCCESS) {
            ESP_LOGE(TAG, "Slot %d MAC validation failed: %d", slot, status);
            CRYPTOAUTH_UNLOCK();
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Slot %d validation succeeded", slot);
    CRYPTOAUTH_UNLOCK();
    return ESP_OK;
}

esp_err_t se_lock_slot(se_slot_id_t slot) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for locking");
        return ESP_ERR_INVALID_STATE;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for locking", slot);
        return ESP_ERR_INVALID_ARG;
    }

    CRYPTOAUTH_LOCK();

    // Lock the slot using our internal lock_slot(...) function
    ATCA_STATUS status = lock_slot(slot);
    if (status != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to lock slot %d: %d", slot, status);
        CRYPTOAUTH_UNLOCK();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Slot %d locked successfully", slot);
    CRYPTOAUTH_UNLOCK();
    return ESP_OK;
}

bool se_slot_locked(se_slot_id_t slot) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready");
        return false;
    }
    if (slot < 0 || slot > SE_KEY_MAX) {
        ESP_LOGE(TAG, "Invalid slot ID %d for lock status check", slot);
        return false;
    }

    return SLOT_LOCKED(slot);
}

esp_err_t se_hmac(const uint8_t *data, size_t data_len, uint8_t *digest) {
    if (!se_ready()) {
        ESP_LOGE(TAG, "Secure element is not ready for signing");
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || data_len == 0 || digest == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for signing");
        return ESP_ERR_INVALID_ARG;
    }

    ATCA_STATUS status = ATCA_SUCCESS;
    CRYPTOAUTH_LOCK();

    ESP_LOGD(TAG, "Signing %zu bytes of data with HMAC key in slot %d", data_len, SE_KEY_HMAC);
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_DEBUG);

    // Generate nonce
    uint8_t num_in[NONCE_NUMIN_SIZE]  = {0};
    uint8_t rand_out[RANDOM_NUM_SIZE] = {0};
    esp_fill_random(num_in, sizeof(num_in));
    if ((status = atcab_nonce_rand(num_in, rand_out)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to generate nonce: %d", status);
        return status;
    }

    // Create a signature using the HMAC key in HMAC slot
    if ((status = atcab_sha_hmac(data, data_len, SE_KEY_HMAC, digest, SHA_MODE_TARGET_OUT_ONLY)) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to sign data with ATECC608B: %d", status);
        CRYPTOAUTH_UNLOCK();
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Data signed successfully");
    CRYPTOAUTH_UNLOCK();
    return ESP_OK;
}
