#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

esp_err_t badge_nfc_init(void);
void badge_nfc_deinit(void);

#ifdef __cplusplus
}
#endif
