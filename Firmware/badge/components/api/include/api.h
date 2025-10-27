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
 * @brief Redeem a code
 *
 * @param[in] code The code to redeem
 *
 * @return A result struct containing the redemption status
 */
api_result_t *api_redeem_code(const char *code);

/**
 * @brief Register the badge for a given player name
 *
 * @param[in] player_name The player name to register
 *
 * @return A result struct containing the player status
 */
api_result_t *api_register(const char *player_name);

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
 * @brief Get any messages for the player
 *
 * @return A result struct containing the messages
 */
api_result_t *api_get_messages();

/**
 * @brief Mark a message as read
 *
 * @param[in] message_id The ID of the message to mark as read
 *
 * @return A result struct containing the response
 */
api_result_t *api_mark_message_read(int message_id);

/**
 * @brief List files available for download
 *
 * @return A result struct containing the list of files
 */
api_result_t *api_list_files();
/**
 * @brief Download a file by its SHA256 hash
 *
 * @param[in] sha256 The SHA256 hash of the file to download
 * @param[in] file_path The path to save the downloaded file to
 *
 * @return A result struct containing the download status
 */
api_result_t *api_download_file(const char *sha256, const char *file_path);

/**
 * @brief Mark the Konami code as entered to gets the achievement
 *
 * @return A result struct containing the status of the operation
 */
api_result_t *api_konami_entered();

/**
 * @brief Report Dawn Accord protection status
 *
 * @param[in] ai_inhibitor_enabled True if AI inhibitor resistor is present
 * @param[in] ai_security_strip_enabled True if security strip trace is intact
 *
 * @return A result struct containing the status of the operation
 */
api_result_t *api_report_dawn_accord(bool ai_inhibitor_enabled, bool ai_security_strip_enabled);

/**
 * @brief Get the list of achievements
 *
 * @return A result struct containing the list of achievements
 */
api_result_t *api_get_achievements();

/**
 * @brief Get CTF hints password
 *
 * @return A result struct containing the CTF hints password
 */
api_result_t *api_get_ctf_hints_password();

/**
 * @brief Request a credit swap
 *
 * @param[in] amount The number of credits to swap
 *
 * @return A result struct containing the credit swap status
 */
api_result_t *api_request_credit_swap(uint32_t amount);

/**
 * @brief Check the status of an initiated credit swap
 *
 * @param[in] code The credit swap code to check the status for
 *
 * @return A result struct containing the credit swap status
 */
api_result_t *api_credit_swap_status(const char *code);

/**
 * @brief Cancel a credit swap that is in progress
 *
 * @param[in] code The code for the credit swap to cancel
 *
 * @return A basic result struct for request status
 */
api_result_t *api_cancel_credit_swap(const char *code);

/**
 * @brief Retrieve topology data for the badge game
 *
 * @return A result struct containing the current topology data
 */
api_result_t *api_topology_data();

#ifdef __cplusplus
}
#endif
