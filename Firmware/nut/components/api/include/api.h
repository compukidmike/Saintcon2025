#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "../types.h"

#define API_BASE_URL "https://sc25.redactd.dev"

/**
 * @brief API result data free function to free allocated memory for result data structs
 *
 * @param[in] data The result data struct to free
 * @param[in] type The type of the result data struct
 */
void api_free_result_data(void *data, api_result_type_t type);

/**
 * @brief API result free function to free allocated memory for result structs
 *
 * @param[in] result The result struct to free
 * @param[in] free_data Whether to free the result data as well
 */
void api_free_result(api_result_t *result, bool free_data);

/**
 * @brief Start the provisioning process.
 *        This will fetch the needed HMAC secret and hash prefix
 *
 * @return A result struct containing the secrets
 */
api_result_t *api_provision_secrets();

/**
 * @brief Provision the wireless PSK
 *
 * @return A result struct containing the PSK to write to the secure element
 */
api_result_t *api_provision_wireless_psk();

/**
 * @brief Attempt to verify the provisioned secrets
 *
 * @return A base result with code/status
 */
api_result_t *api_provision_verify();

/**
 * @brief Get the player status.
 *
 *   Note that the response is the same as the register API so this just calls api_register with a
 *   null player name.
 *
 * @return A result struct containing the player status
 */
api_result_t *api_get_status();

/**
 * @brief Request a new code for the given level
 *
 * @param[in] level The level to request a code for (only levels 1-3 are valid)
 * @return A result struct containing the new code
 */
api_result_t *api_request_code(int level);

/** @brief Get the status of a previously requested code
 *
 * @param[in] code The code to check the status of
 * @return A result struct containing the code status
 */
api_result_t *api_get_code_status(const char *code);

/**
 * @brief Send a syslog message
 *
 * @param[in] message The syslog message to send
 * @param[in] level The syslog level of the message
 * @return A result struct containing the syslog status
 */
api_result_t *api_send_syslog(syslog_level_t level, const char *message);

/**
 * @brief Retrieve firmware version data
 *
 * @return A result struct containing the latest firmware version
 */
api_result_t *api_get_firmware_version();

/**
 * @brief Retrieve firmware version via HEAD request
 *
 * @return A result struct containing the latest firmware version
 */
api_result_t *api_get_firmware_head();

/**
 * @brief Retrieve firmware binary
 *
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_do_firmware_update();

/**
 * @brief Send port connection status to the telecom API
 *
 * @param[in] statuses An array of port statuses for all 6 ports
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_send_telecom_port_status(const api_telecom_port_status_t statuses[], size_t count);

/**
 * @brief Send telecom status (online/offline)
 *
 * @param[in] online Whether the device is online or offline
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_send_telecom_status(bool online);

/**
 * @brief Send a telecom heartbeat
 *
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_send_telecom_heartbeat();

/**
 * @brief Send a telecom locked notification
 *
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_send_telecom_locked();

/**
 * @brief Send a telecom syslog message
 *
 * @param[in] message The syslog message to send
 * @param[in] level The syslog level of the message
 * @param[in] category The syslog category of the message
 * @return API_OK if the request was successful, API_FAIL otherwise
 */
api_err_t api_send_telecom_syslog(const char *message, syslog_level_t level, const char *category);

#ifdef __cplusplus
}
#endif
