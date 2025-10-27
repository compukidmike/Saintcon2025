#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cryptoauthlib.h"

#define ATECC608B_CONFIG_SIZE 128

typedef struct ATCA_PACKED se_state_s {
    uint8_t TempKey_KeyID : 4;
    bool TempKey_SourceFlag : 1;
    bool TempKey_GenDigData : 1;
    bool TempKey_GenKeyData : 1;
    bool TempKey_NoMacFlag : 1;

    bool EEPROM_RNG : 1;
    bool SRAM_RNG : 1;
    bool AuthValid : 1;
    uint8_t AuthKeyID : 4;
    bool TempKey_Valid : 1;

    uint8_t byte2;
    uint8_t byte3;
} se_state_t;

void dump_state(se_state_t *state);

void decode_slot_config(atecc608_config_t *config, int slot_num);
void decode_key_config(atecc608_config_t *config, int slot_num);
void se_dump_config(atecc608_config_t *config);

#ifdef __cplusplus
}
#endif
