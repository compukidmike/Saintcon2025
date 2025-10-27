#include "esp_log.h"
#include "se_debug.h"

static const char *TAG = "secure_element";

void decode_slot_config(atecc608_config_t *config, int slot_num) {
    uint16_t slot_config = config->SlotConfig[slot_num];
    ESP_LOGI(TAG, "Slot %d Config: 0x%04X", slot_num, slot_config);

    // Extract and decode WriteConfig
    uint8_t write_config = (slot_config >> 12) & 0xF;
    ESP_LOGI(TAG, "  WriteConfig: 0x%X", write_config);
    // Write command interpretation
    if (write_config == 0x0) {
        ESP_LOGI(TAG, "    Write:     Always - Clear text writes always permitted");
    } else if (write_config == 0x1) {
        ESP_LOGI(TAG, "    Write:     PubInvalid - Writes prohibited if validated public key stored");
    } else if ((write_config & 0x6) == 0x2) { // 0010 or 0011
        ESP_LOGI(TAG, "    Write:     Never - Writes never permitted");
    } else if ((write_config & 0x8) == 0x8) { // 10XX
        ESP_LOGI(TAG, "    Write:     Never - Writes never permitted");
    } else if ((write_config & 0x4) == 0x4) { // X1XX
        ESP_LOGI(TAG, "    Write:     Encrypt - Writes require MAC and encryption with WriteKey");
    } else {
        ESP_LOGI(TAG, "    Write:     Unknown mode");
    }
    // DeriveKey command interpretation
    if ((write_config & 0x3) == 0x2) { // XX10
        if ((write_config & 0x8) == 0x0) {
            ESP_LOGI(TAG, "    DeriveKey: Target (Roll) - No MAC required");
        } else {
            ESP_LOGI(TAG, "    DeriveKey: Target (Roll) - MAC required");
        }
    } else if ((write_config & 0x3) == 0x3) { // XX11
        if ((write_config & 0x8) == 0x0) {
            ESP_LOGI(TAG, "    DeriveKey: Parent (Create) - No MAC required");
        } else {
            ESP_LOGI(TAG, "    DeriveKey: Parent (Create) - MAC required");
        }
    } else if ((write_config & 0x2) == 0x0) { // XX0X
        ESP_LOGI(TAG, "    DeriveKey: Not usable as DeriveKey target");
    } else {
        ESP_LOGI(TAG, "    DeriveKey: Unknown mode");
    }
    // GenKey command interpretation
    if ((write_config & 0x2) == 0x0) { // XX0X
        ESP_LOGI(TAG, "    GenKey:    Not usable as GenKey target");
    } else { // XX1X
        ESP_LOGI(TAG, "    GenKey:    Usable as GenKey target");
    }
    // PrivWrite command interpretation
    if ((write_config & 0x4) == 0x0) { // X0XX
        ESP_LOGI(TAG, "    PrivWrite: Forbidden - Error if target key slot has this value");
    } else { // X1XX
        ESP_LOGI(TAG, "    PrivWrite: Encrypt - Requires MAC and encryption with WriteKey");
    }

    ESP_LOGI(TAG, "  WriteKey:    0x%X", (slot_config >> 8) & 0xF);
    ESP_LOGI(TAG, "  IsSecret:    0x%X", (slot_config >> 7) & 0x1);
    ESP_LOGI(TAG, "  EncryptRead: 0x%X", (slot_config >> 6) & 0x1);
    ESP_LOGI(TAG, "  LimitedUse:  0x%X", (slot_config >> 5) & 0x1);
    ESP_LOGI(TAG, "  NoMac:       0x%X", (slot_config >> 4) & 0x1);
    ESP_LOGI(TAG, "  ReadKey:     0x%X", slot_config & 0xF);
}

void decode_key_config(atecc608_config_t *config, int slot_num) {
    uint16_t key_config = config->KeyConfig[slot_num];
    ESP_LOGI(TAG, "Key %d Config: 0x%04X", slot_num, key_config);

    // Gather all the KeyConfig fields
    uint8_t private_key     = key_config & 0x1;
    uint8_t pub_info        = (key_config >> 1) & 0x1;
    uint8_t key_type        = (key_config >> 2) & 0x7;
    uint8_t lockable        = (key_config >> 5) & 0x1;
    uint8_t req_random      = (key_config >> 6) & 0x1;
    uint8_t req_auth        = (key_config >> 7) & 0x1;
    uint8_t auth_key        = (key_config >> 8) & 0xF;
    uint8_t persist_disable = (key_config >> 12) & 0x1;
    uint8_t rfu             = (key_config >> 13) & 0x1;
    uint8_t x509_id         = (key_config >> 14) & 0x3;

    ESP_LOGI(TAG, "  Private:     0x%X", private_key);
    if (private_key == 0) {
        ESP_LOGI(TAG, "    Not an ECC private key - can contain ECC public key, SHA key, or data");
    } else {
        ESP_LOGI(TAG, "    ECC private key - only usable with Sign, GenKey, ECDH, PrivWrite commands");
    }

    ESP_LOGI(TAG, "  PubInfo:     0x%X", pub_info);
    if (private_key == 1) {
        // ECC private key
        if (pub_info == 0) {
            ESP_LOGI(TAG, "    Public version of this private key can never be generated (highest security)");
        } else {
            ESP_LOGI(TAG, "    Public version of this private key can always be generated");
        }
    } else {
        // This is not an ECC private key - PubInfo controls public key validity (slots 8-15 only)
        if (slot_num >= 8 && slot_num <= 15) {
            if (pub_info == 0) {
                ESP_LOGI(TAG, "    Public key can be used by Verify without validation");
            } else {
                ESP_LOGI(TAG, "    Public key can only be used by Verify if validated first");
            }
        } else {
            ESP_LOGI(TAG, "    PubInfo field (public key validity only applies to slots 8-15)");
        }
    }

    const char *key_types[] = {
        "ECC B283",                  // 0
        "ECC K283",                  // 1
        "",                          // 2 - unused
        "",                          // 3 - unused
        "ECC P-256",                 // 4
        "",                          // 5 - unused
        "AES-128",                   // 6
        "SHA-256 key OR other data", // 7
    };
    ESP_LOGI(TAG, "  KeyType:        0x%X (%s)", key_type, key_types[key_type]);

    ESP_LOGI(TAG, "  Lockable:       0x%X", lockable);
    if (lockable == 0) {
        ESP_LOGI(TAG, "    SlotConfig and KeyConfig control modification permissions");
    } else {
        ESP_LOGI(TAG, "    Slot can be individually locked using Lock command");
    }

    ESP_LOGI(TAG, "  ReqRandom:      0x%X", req_random);
    if (req_random == 0) {
        ESP_LOGI(TAG, "    Random nonce not required");
    } else {
        ESP_LOGI(TAG, "    Random nonce required for GenKey, MAC, HMAC, CheckMac, Verify, DeriveKey, GenDig");
    }

    ESP_LOGI(TAG, "  ReqAuth:        0x%X", req_auth);
    if (req_auth == 0) {
        ESP_LOGI(TAG, "    No prior authorization required");
    } else {
        ESP_LOGI(TAG, "    Prior authorization required using AuthKey slot %d", auth_key);
    }

    // AuthKey field (only relevant if ReqAuth is 1)
    ESP_LOGI(TAG, "  AuthKey:        0x%X", auth_key);
    if (req_auth == 1) {
        ESP_LOGI(TAG, "    Authorization key slot: %d", auth_key);
    } else {
        ESP_LOGI(TAG, "    Must be zero (ReqAuth is 0)");
    }

    ESP_LOGI(TAG, "  PersistDisable: 0x%X", persist_disable);
    if (persist_disable == 0) {
        ESP_LOGI(TAG, "    Key use independent of PersistLatch state");
    } else {
        ESP_LOGI(TAG, "    Key use prohibited (except GenKey) if PersistLatch is zero");
    }

    ESP_LOGI(TAG, "  RFU:            0x%X", rfu);
    if (rfu != 0) {
        ESP_LOGI(TAG, "    WARNING: RFU field should be zero");
    } else {
        ESP_LOGI(TAG, "    Reserved for Future Use (correct value)");
    }

    ESP_LOGI(TAG, "  X509id:         0x%X", x509_id);
    ESP_LOGI(TAG, "    Index into X509format array (addresses 92-95): %d", x509_id);
    if (x509_id == 0) {
        ESP_LOGI(TAG, "    Public key can be validated by any format signature");
    } else {
        ESP_LOGI(TAG, "    Validating certificate must be specific length/format");
    }
}

void dump_state(se_state_t *state) {
    ESP_LOGI(TAG, "Dumping secure element state:");
    ESP_LOGI(TAG, "  TempKey.KeyID:     0x%2X", state->TempKey_KeyID);
    ESP_LOGI(TAG, "  TempKey.SourceFlag:  %d", state->TempKey_SourceFlag);
    ESP_LOGI(TAG, "  TempKey.GenDigData:  %d", state->TempKey_GenDigData);
    ESP_LOGI(TAG, "  TempKey.GenKeyData:  %d", state->TempKey_GenKeyData);
    ESP_LOGI(TAG, "  TempKey.NoMacFlag:   %d", state->TempKey_NoMacFlag);
    ESP_LOGI(TAG, "  TempKey.Valid:       %d", state->TempKey_Valid);
    ESP_LOGI(TAG, "  EEPROM RNG:          %d", state->EEPROM_RNG);
    ESP_LOGI(TAG, "  SRAM RNG:            %d", state->SRAM_RNG);
    ESP_LOGI(TAG, "  Auth Valid:          %d", state->AuthValid);
    ESP_LOGI(TAG, "  Auth Key ID:       0x%2X", state->AuthKeyID);
}

void se_dump_config(atecc608_config_t *config) {
    uint8_t *sn03 = (uint8_t *)&config->SN03;
    uint8_t *sn47 = (uint8_t *)&config->SN47;
    ESP_LOGI(TAG, "Dumping ATECC608B configuration:");
    ESP_LOGI(TAG, "  Revision:      0x%08X (%s)", config->RevNum, config->RevNum & 0x0F000000 ? "ATECC608" : "ATECC508");
    ESP_LOGI(TAG, "  Serial Number: %02X%02X%02X%02X%02X%02X%02X%02X%02X", //
             sn03[0], sn03[1], sn03[2], sn03[3],                           // SN03 is the first 4 bytes of the serial number
             sn47[0], sn47[1], sn47[2], sn47[3],                           // SN47 is the next 4 bytes of the serial number
             config->SN8                                                   // SN8 is the last byte of the serial number
    );
    ESP_LOGI(TAG, "  AES Enable:    0x%02X", config->AES_Enable & 0x01);
    ESP_LOGI(TAG, "  I2C Enable:    0x%02X", config->I2C_Enable & 0x01); // [7:1] Reserved and set by Microchip, only bit 0 is
                                                                         // used
    ESP_LOGI(TAG, "  I2C Address:   0x%02X", config->I2C_Address);
    ESP_LOGI(TAG, "  Count Match:   0x%02X", config->CountMatch); // Counter match - limit to which counter 0 can be incremented
    ESP_LOGI(TAG, "  Chip Mode:     0x%02X", config->ChipMode);   // [7:3] Reserved - set to 0
                                                                  // [2]   Watchdog duration
                                                                  // [1]   TTL Enable
                                                                  // [0]   SelectorMode

    // Show the two counters
    ESP_LOGI(TAG, "  Counter 0:     %02X%02X%02X%02X%02X%02X%02X%02X", //
             config->Counter0[0], config->Counter0[1], config->Counter0[2], config->Counter0[3], config->Counter0[4],
             config->Counter0[5], config->Counter0[6], config->Counter0[7]);
    ESP_LOGI(TAG, "  Counter 1:     %02X%02X%02X%02X%02X%02X%02X%02X", //
             config->Counter1[0], config->Counter1[1], config->Counter1[2], config->Counter1[3], config->Counter1[4],
             config->Counter1[5], config->Counter1[6], config->Counter1[7]);

    ESP_LOGI(TAG, "  UseLock:       0x%02X", config->UseLock); // Transport/UseLock - General purpose usage is prohibited until
                                                               // the device is cryptographically enabled
                                                               // [7:4] UseLockKey - Key slot used to lock the device
                                                               // [3:0] UseLockEnable - Enable UseLock
    ESP_LOGI(TAG, "  VolatileKeyPermission: 0x%02X", config->VolatileKeyPermission); // Volatile key permission - volatile key is
                                                                                     // used to control the state of the
                                                                                     // Persistent Latch
                                                                                     // [7]   VolatileKeyPermitEnable
                                                                                     // [6:4] Reserved
                                                                                     // [3:0] VolatileKeyPermitSlot
    ESP_LOGI(TAG, "  SecureBoot:    0x%02X", config->SecureBoot); // Secure boot - controls secure boot features
    ESP_LOGI(TAG, "  KDFlvLoc:      0x%02X", config->KdflvLoc);   // KDF location - controls the location of the KDF key
    ESP_LOGI(TAG, "  KDFlvStr:      0x%02X", config->KdflvStr);   // KDF string - controls the string used for KDF key generation
    ESP_LOGI(TAG, "  UserExtra:     0x%02X", config->UserExtra);  // User extra - additional user-defined data
    ESP_LOGI(TAG, "  UserExtraAdd:  0x%02X", config->UserExtraAdd); // User extra add - additional user-defined data that can be
                                                                    // added

    // Show the lock values
    ESP_LOGI(TAG, "  LockValue:     0x%02X (%s)", config->LockValue,
             config->LockValue == 0x00 ? "locked" : "unlocked"); // Data/OTP zones:
                                                                 // 0x55 - unlocked (read prohibited)
                                                                 // 0x00 - locked
    ESP_LOGI(TAG, "  LockConfig:    0x%02X (%s)", config->LockConfig,
             config->LockConfig == 0x00 ? "locked" : "unlocked"); // Config zone:
                                                                  // 0x55 - unlocked (read prohibited)
                                                                  // 0x00 - locked
    ESP_LOGI(TAG, "  SlotLocked:    0x%04X", config->SlotLocked); // Bitmask of locked slots, 1 bit for each slot
                                                                  //   0 - locked
                                                                  //   1 - unlocked

    ESP_LOGI(TAG, "  ChipOptions:   0x%04X", config->ChipOptions); // [15:12] IO Protection Key Slot
                                                                   // [11:10] KDF Output Protection
                                                                   //         0x00: Clear text allowed
                                                                   //         0x01: Encrypted only
                                                                   //         0x02: None - store in TempKey and/or key slot _only_
                                                                   // [9:8]   ECDH Output Protection - Same as above
                                                                   // [7:4]   Reserved. Must be zero
                                                                   // [3]     Auto clear health test failure bit
                                                                   // [2]     KDF AES Enable
                                                                   // [1]     IO Protection Key Enable
                                                                   // [0]     Power On Self Test (POST) Enable

    // Dump the full config zone in hex format
    size_t config_size      = ATECC608B_CONFIG_SIZE;
    size_t config_text_size = (config_size * 3) + (config_size / 16) * 2 + 1; // 2 chars per byte plus a space for each byte, plus
                                                                              // newlines (\r\n), plus null terminator
    char config_text[config_text_size];
    if (atcab_bin2hex((const uint8_t *)config, sizeof(atecc608_config_t), config_text, &config_text_size) != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to convert config to hex");
        return;
    }
    ESP_LOGI(TAG,
             "Raw Config:\n-----------------------------------------------\n%s\n-----------------------------------------------",
             config_text);

    // Show the config for each slot
    for (int i = 0; i < 16; i++) {
        decode_slot_config(config, i);
        decode_key_config(config, i);
        ESP_LOGI(TAG, "-----------------------------------------------");
    }
}