#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    CODE_MGR_STATE_IDLE,
    CODE_MGR_STATE_READY,
    CODE_MGR_STATE_WRITING,
} code_mgr_state_t;

esp_err_t code_mgr_init(void);

bool code_mgr_is_ready(void);

esp_err_t code_mgr_write_to_nfc(void);

void code_mgr_on_nut_ready(void);

const char *code_mgr_get_current_code(void);

const char *code_mgr_get_nut_type_record(void);

void code_mgr_mark_code_written(const char *written_code);

void code_mgr_update_nut_type_record(void);

esp_err_t request_new_code(void);

#ifdef __cplusplus
}
#endif
