#include <string.h>
#include "esp_log.h"

#include "config.h"
#include "migrate.h"

static const char *TAG __attribute__((unused)) = "nut/config [migrate]";

/**
 * @brief Migrates the nut configuration to the latest version
 *
 * This function should handle any necessary transformations required
 * to bring an older version of the configuration struct up to date
 * with the latest version.
 *
 * @param stored_version The version of the stored configuration
 */
void migrate_nut_config(nvs_handle_t nvs_handle, uint32_t stored_version) {
    // bool migrated_last = false;

    // Create a new config struct with the latest version to migrate to
    nut_config_v1_t config_v1 = NUT_DEFAULTS_V1;

    // ----------------------------------------------------------------------- //
    //                        MIGRATION IMPLEMENTATION                         //
    // ----------------------------------------------------------------------- //

    // ---- PLACEHOLDER FOR PER-VERSION MIGRATION LOGIC ---- //

    // Save the new config
    nut_config = config_v1;
    save_nut_config();
}
