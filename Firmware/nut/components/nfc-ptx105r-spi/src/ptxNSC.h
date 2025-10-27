/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of
       conditions and the following disclaimer in the documentation and/or other
       materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



    THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX1K
    Module      : NSC
    File        : ptxNSC.h

    Description :
*/

/**
 * \addtogroup grp_ptx_api_nsc_core PTX NSC Stack Core
 *
 * @{
 */

#ifndef COMPS_NSC_PTXNSC_C_
#define COMPS_NSC_PTXNSC_C_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include "ptxNSC_Event.h"
#include "ptxNSC_RfConfigVal.h"
#include "ptxNSC_Hal.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RF_CONFIG_SDK_DEFAULT_SETTINGS
    #pragma message("Attention - Default SDK RF-Config settings used - please adapt values for target application platform!")
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

struct ptxNSC_System;
struct ptxNSC;

/**
 * \name PTX1K Communication parameters.
 * 
 * @{
 */
#define PTX_NSC_TYPE_FRAME_MASK                         (0xC0)                 /**< NSC Frame Mask communication type */
#define PTX_NSC_TYPE_FRAME_CMD                          (0x00)                 /**< NSC Frame Command communication type */
#define PTX_NSC_TYPE_FRAME_RSP                          (0x01)                 /**< NSC Frame Response communication type */
#define PTX_NSC_TYPE_FRAME_NTF                          (0x02)                 /**< NSC Frame NTF communication type */
#define PTX_NSC_TYPE_FRAME_TRANSP                       (0x03)                 /**< NSC Frame Transp communication type */
/** @} */

/**
 * \name PTX1K Communication parameters.
 * 
 * @{
 */
#define PTX_NSC_MAX_MTU_SIZE                            (253u)                 /**< NSC Maximum MTU size */
#define PTX_NSC_TRANSFER_TO                             (500u)                 /**< NSC Transfer */
#define PTX_NSC_MAX_RSP_LEN                             (256u)                 /**< NSC Maximum response length */
/** @} */

/**
 * \name NSC Standby command opcode and length definitions.
 * 
 * @{
 */

#define PTX_NSC_STANDBY_CMD_OPCODE                      (0x05u)                /**< NSC Standby command opcode */
#define PTX_NSC_STANDBY_RSP_OPCODE                      (0x45u)                /**< NSC Standby response opcode */
#define PTX_NSC_STANDBY_RESP_LEN                        (0x02u)                /**< NSC Standby response length */
#define PTX_NSC_STANDBY_PARAMETERS_LEN                  (0x00u)                /**< NSC Standby parameters length */
/** @} */

/**
 * \name NSC Wakeup command opcode and length definitions.
 * 
 * @{
 */
#define PTX_NSC_WAKEUP_CMD_OPCODE                       (0x06u)                /**< NSC Wakeup command opcode */
#define PTX_NSC_WAKEUP_RSP_OPCODE                       (0x46u)                /**< NSC Wakeup response opcode */
#define PTX_NSC_WAKEUP_RESP_LEN                         (0x02u)                /**< NSC Wakeup response length */
#define PTX_NSC_WAKEUP_PARAMETERS_LEN                   (0x00u)                /**< NSC Wakeup parameter length */
/** @} */

/**
 * \name NSC DATA.
 * 
 * @{
 */
#define PTX_NSC_DATA_MSG_MAX_PAYLOAD_LEN                PTX_NSC_MAX_MTU_SIZE   /**< NSC Message maximum payload length */
#define PTX_NSC_DATA_MSG_OPCODE_INDEX                   (0x00u)                /**< NSC Message opcode index */
#define PTX_NSC_DATA_MSG_OPCODE                         (0xC2u)                /**< NSC Message opcode */
#define PTX_NSC_DATA_MSG_OPCODE_CHAINING                (0xE2u)                /**< NSC Message opcode chaining */
#define PTX_NSC_DATA_MSG_OPCODE_LENGTH                  (0x01u)                /**< NSC Message opcode length */

#define PTX_NSC_DATA_MSG_CON_MSG_LEN_INDEX              (0x01u)                /**< NSC Message CON message length index */
#define PTX_NSC_DATA_MSG_CON_MSG_LENGTH                 (0x01u)                /**< NSC Message CON message length */
#define PTX_NSC_DATA_MSG_CON_MSG_LENGTH_VALUE_ACK       (0x00u)                /**< NSC Message CON acknowledge message length */

#define PTX_NSC_DATA_MSG_CON_MSG_INDEX                  (0x02u)                /**< NSC Message CON message index */
/** @} */

/**
 * \name NSC DATA CONTROL.
 * 
 * @{
 */
#define PTX_NSC_RFD_CTRL_OPCODE                         (0xD2u)                /**< NSC Data control opcode */
#define PTX_NSC_RFD_CTRL_ACK                            (0x00u)                /**< NSC Data control ACK */
#define PTX_NSC_RFD_CTRL_NACK                           (0x01u)                /**< NSC Data control NACK */
#define PTX_NSC_RFD_CTRL_EOF                            (0x02u)                /**< NSC Data control EOF */
#define PTX_NSC_RFD_CTRL_ATTENTION                      (0x03u)                /**< NSC Data control Attention */
#define PTX_NSC_RFD_CTRL_NR                             (0x04u)                /**< NSC Data control NR */
/** @} */

/**
 * \name Ensure that the DFY is reset after FW download.
 * 
 * @{
 */
#define PTX_NSC_FWDOWN_FORCED                           (1u)                   /**< NSC FW download forced */
#define PTX_NSC_FWDOWN_NOT_FORCED                       (0u)                   /**< NSC FW download not forced */
/** @} */

/**
 * \name NSC command opcode and length definitions.
 * 
 * @{
 */
#define PTX_NSC_INIT_CMD_OPCODE                         (0x02u)                /**< NSC Init command opcode */
#define PTX_NSC_INIT_RSP_OPCODE                         (0x42u)                /**< NSC Init response opcode */
#define PTX_NSC_INIT_RESP_LEN                           (0x02u)                /**< NSC Init response length */
/** @} */

/**
 * \name NSC Init command parameters values
 * 
 * @{
 */
#define PTX_NSC_INIT_CON_HOST_LENGTH                    (4U)                   /**< NSC Init CON host length */
#define PTX_NSC_INIT_CON_HOST_CE_LENGTH                 (4U)                   /**< NSC Init CON host CE length */
#define PTX_NSC_INIT_CON_UART_CONFIG                    (2U)                   /**< NSC Init CON UART config */
#define PTX_NSC_INIT_CON_PRNG_SEED                      (4U)                   /**< NSC Init CON PRNG seed */
#define PTX_NSC_INIT_CON_N_ALM_MAX                      (4U)                   /**< NSC Init maximum N_ALM value */
#define PTX_NSC_INIT_CON_N_ALM_MIN                      (4U)                   /**< NSC Init minimum N_ALM value */
/** @} */

/**
 * \name NSC RF Config command parameters values
 * 
 * @{
 */
#define PTX_NSC_RFCONF_CMD_OPCODE                       (0x11u)                /**< NSC RF Config command opcode */ 
#define PTX_NSC_RFCONF_RSP_OPCODE                       (0x51u)                /**< NSC RF Config response opcode */ 
#define PTX_NSC_RFCONF_RESP_LEN                         (0x02u)                /**< NSC RF Config response length */ 

#define PTX_NSC_RFCONF_TX_POWER_MODE                    (0x02u)                /**< NSC RF Config TX power mode */ 
#define PTX_NSC_RFCONF_WAVEBANK_0						(0x00u)                /**< NSC RF Config Wavebank 00 */
#define PTX_NSC_RFCONF_WAVEBANK_1						(0x01u)                /**< NSC RF Config Wavebank 01 */
#define PTX_NSC_RFCONF_WAVEBANK_2						(0x02u)                /**< NSC RF Config Wavebank 02 */
#define PTX_NSC_RFCONF_WAVEBANK_3						(0x03u)                /**< NSC RF Config Wavebank 03 */
#define PTX_NSC_RFCONF_WAVEBANK_4						(0x04u)                /**< NSC RF Config Wavebank 04 */
#define PTX_NSC_RFCONF_WAVEBANK_5						(0x05u)                /**< NSC RF Config Wavebank 05 */
#define PTX_NSC_RFCONF_WAVEBANK_6						(0x06u)                /**< NSC RF Config Wavebank 06 */
#define PTX_NSC_RFCONF_WAVEBANK_7						(0x07u)                /**< NSC RF Config Wavebank 07 */
#define PTX_NSC_RFCONF_WAVEBANK_8						(0x08u)                /**< NSC RF Config Wavebank 08 */
#define PTX_NSC_RFCONF_WAVEBANK_9						(0x09u)                /**< NSC RF Config Wavebank 09 */
#define PTX_NSC_RFCONF_WAVEBANK_10						(0x0Au)                /**< NSC RF Config Wavebank 10 */
#define PTX_NSC_RFCONF_WAVEBANK_11						(0x0Bu)                /**< NSC RF Config Wavebank 11 */
#define PTX_NSC_RFCONF_WAVEBANK_12						(0x0Cu)                /**< NSC RF Config Wavebank 12 */
#define PTX_NSC_RFCONF_WAVEBANK_13						(0x0Du)                /**< NSC RF Config Wavebank 13 */
#define PTX_NSC_RFCONF_WAVEBANK_14						(0x0Eu)                /**< NSC RF Config Wavebank 14 */
#define PTX_NSC_RFCONF_WAVEBANK_15						(0x0Fu)                /**< NSC RF Config Wavebank 15 */
#define PTX_NSC_RFCONF_WAVEBANK_16						(0x10u)                /**< NSC RF Config Wavebank 16 */
#define PTX_NSC_RFCONF_WAVEBANK_17						(0x11u)                /**< NSC RF Config Wavebank 17 */
#define PTX_NSC_RFCONF_WAVEBANK_18						(0x12u)                /**< NSC RF Config Wavebank 18 */
#define PTX_NSC_RFCONF_WAVEBANK_19						(0x13u)                /**< NSC RF Config Wavebank 19 */

#define PTX_NSC_RFCONF_MISC                             (0x04u)                /**< NSC RF Config miscellaneous */ 
#define PTX_NSC_RFCONF_POLL_A_106                       (0x11u)                /**< NSC RF Config Poll A 106 */ 
#define PTX_NSC_RFCONF_POLL_A_212                       (0x12u)                /**< NSC RF Config Poll A 212 */ 
#define PTX_NSC_RFCONF_POLL_A_424                       (0x13u)                /**< NSC RF Config Poll A 424 */ 
#define PTX_NSC_RFCONF_POLL_A_848                       (0x14u)                /**< NSC RF Config Poll A 848 */ 
#define PTX_NSC_RFCONF_POLL_B_106                       (0x15u)                /**< NSC RF Config Poll B 106 */ 
#define PTX_NSC_RFCONF_POLL_B_212                       (0x16u)                /**< NSC RF Config Poll B 212 */ 
#define PTX_NSC_RFCONF_POLL_B_424                       (0x17u)                /**< NSC RF Config Poll B 424 */ 
#define PTX_NSC_RFCONF_POLL_B_848                       (0x18u)                /**< NSC RF Config Poll B 848 */ 
#define PTX_NSC_RFCONF_POLL_F_212                       (0x19u)                /**< NSC RF Config Poll F 212 */ 
#define PTX_NSC_RFCONF_POLL_F_424                       (0x1Au)                /**< NSC RF Config Poll F 424 */ 
#define PTX_NSC_RFCONF_POLL_V                           (0x1Eu)                /**< NSC RF Config Poll V */ 
#define PTX_NSC_RFCONF_LISTEN                           (0x20u)                /**< NSC RF Config Listen */ 
#define PTX_NSC_RFCONF_LAST_PARAM                       (0x00u)                /**< NSC RF Config last parameter */ 
/** @} */

/**
 * \name NSC RF Discovery command parameters values
 * 
 * @{
 */
#define PTX_NSC_DISC_CMD_OPCODE                         (0x12u)                /**< NSC Discovery command opcode */ 
#define PTX_NSC_DISC_RESP_LEN                           (0x02u)                /**< NSC Discovery response length */ 
#define PTX_NSC_DISC_OPCODE                             (0x52u)                /**< NSC Discovery opcode */ 
#define PTX_NSC_DISC_CON_POLLB_CMD_LEN                  (3u)                   /**< NSC Discovery Poll B command length */ 
#define PTX_NSC_DISC_CON_POLLF_CMD_LEN                  (6u)                   /**< NSC Discovery Poll F command length */ 
#define PTX_NSC_DISC_CON_POLLV_CMD_LEN                  (11u)                  /**< NSC Discovery Poll V command length */ 
#define PTX_NSC_DISC_CON_POLLB_ATTRIB_INF_LEN           (15u)                  /**< NSC Discovery Poll B ATTRIB INF command length */ 
#define PTX_NSC_DISC_CON_POLL_NFCDEP_ATR_REQ_G_LEN      (20u)                  /**< NSC Discovery Poll NFCDEP ATR REQ length */ 
#define PTX_NSC_DISC_CON_LIS_SENSRESP_LEN               (2u)                   /**< NSC Discovery Listen SENSRESP length */ 
#define PTX_NSC_DISC_CON_LISA_NFCID1_LEN                (15u)                  /**< NSC Discovery Listen A NFCID1 length */ 
#define PTX_NSC_DISC_CON_LISB_SENSBRESP_LEN             (13u)                  /**< NSC Discovery Listen B SENSBRESP length */ 
#define PTX_NSC_DISC_CON_LISF_SENSFRESP_LEN             (19u)                  /**< NSC Discovery Listen F SENSBRESP length */ 
#define PTX_NSC_DISC_CON_LISV_INVRESP_LEN               (10u)                  /**< NSC Discovery Listen V INVRESP length */ 
#define PTX_NSC_DISC_CON_LIS_ISODEP_ATS_LEN             (21u)                  /**< NSC Discovery Listen ISODEP ATS length */ 
#define PTX_NSC_DISC_CON_LIS_ISODEP_ATTRIB_RES_LEN      (16u)                  /**< NSC Discovery Listen ISODEP ATTRIB RES length */ 
#define PTX_NSC_DISC_CON_LIS_NFCDEP_ATR_RES_G_LEN       (20u)                  /**< NSC Discovery Listen NFCDEP ATR RES length */ 
#define PTX_NSC_DISC_CON_IDLE_TIME_LEN                  (3u)                   /**< NSC Discovery IDLE time length */ 
#define PTX_NSC_TYPES_RFU_1_LEN                         (23U)                  /**< RFU */
/** @} */

/**
 * \name NSC RF Discovery command parameters values
 * 
 * @{
 */ 
#define PTX_NSC_SET_PARAMS_DISC_CMD_OPCODE              (0x16u)                /**< NSC RF Discovery set params command opcode */
#define PTX_NSC_SET_PARAMS_DISC_RSP_OPCODE              (0x56u)                /**< NSC RF Discovery set params response opcode */
#define PTX_NSC_SET_PARAMS_DISC_RSP_LEN                 (0x02u)                /**< NSC RF Discovery set params response length */
#define PTX_NSC_SET_PARAM_CMD_EOC                       (0x00u)                /**< NSC RF Discovery set params command EOC */
/** @} */

/**
 * \name NSC RF Deactivate command parameters values
 * 
 * @{
 */
#define PTX_NSC_DEACT_CMD_OPCODE                        (0x14u)                /**< NSC RF Deactivate command opcode */
#define PTX_NSC_DEACT_RESP_LEN                          (0x03u)                /**< NSC RF Deactivate response length */
#define PTX_NSC_DEACT_RSP_OPCODE                        (0x54u)                /**< NSC RF Deactivate response opcode */
/** @} */

/**
 * \name NSC RF Activate command parameters values
 * 
 * @{
 */
#define PTX_NSC_RF_ACTIVATE_CMD_OPCODE_INDEX            (0x00u)                /**< NSC RF Activate command opcode index */
#define PTX_NSC_RF_ACTIVATE_CMD_OPCODE                  (0x13u)                /**< NSC RF Activate command opcode */
#define PTX_NSC_RF_ACTIVATE_CMD_OPCODE_LENGTH           (0x01u)                /**< NSC RF Activate command opcode length */
#define PTX_NSC_RF_ACTIVATE_CMD_PARAMS_MAXLENGTH        (0x18u)                /**< NSC RF Activate command params maximum length */
#define PTX_NSC_RF_ACTIVATE_RSP_LENGTH                  (0x02u)                /**< NSC RF Activate response length */
#define PTX_NSC_RF_ACTIVATE_RSP_OPCODE                  (0x53u)                /**< NSC RF Activate response opcode */
/** @} */

/**
 * \name NSC RF Set Routing Table command parameters values
 * 
 * @{
 */
#define PTX_NSC_RF_SET_ROUTING_TABLE_CMD_OPCODE         (0x15u)                /**< NSC RF Set Routing Table command opcode */
#define PTX_NSC_RF_SET_ROUTING_TABLE_CMD_LENGTH         (0x0Cu)                /**< NSC RF Set Routing Table command lenght */
#define PTX_NSC_RF_SET_ROUTING_TABLE_RSP_LENGTH         (0x02u)                /**< NSC RF Set Routing Table response length */
#define PTX_NSC_RF_SET_ROUTING_TABLE_RSP_OPCODE         (0x55u)                /**< NSC RF Set Routing Table response opcode */
/** @} */


/**
 * \name Definitions for NSC RST RESPONSE.
 * 
 * @{
 */

#define PTX_NSC_RESET_RSP_LENGTH                        (0x02u)                /**< NSC Reset response length */
#define PTX_NSC_RESET_RSP_OPCODE                        (0x41u)                /**< NSC reset response opcode */
/** @} */

/**
 * \name Definitions for OpCodes NTFs.
 * 
 * @{
 */
#define PTX_NSC_OPCODE_RFFIELDNTF                       (0x91u)                /**< NSC RFFIELDNTF opcode */ 
#define PTX_NSC_OPCODE_RFDISCNTF                        (0x92u)                /**< NSC RFDISCNTF opcode */ 
#define PTX_NSC_OPCODE_RFACTNTF                         (0x93u)                /**< NSC RFACTNTF opcode */ 
#define PTX_NSC_OPCODE_RFDEACTNTF                       (0x94u)                /**< NSC RFDEACTNTF opcode */ 
#define PTX_NSC_OPCODE_RFERRORNTF                       (0x95u)                /**< NSC RFERRORNTF opcode */ 
#define PTX_NSC_OPCODE_LPCDNTF                          (0x97u)                /**< NSC LPCDNTF opcode */ 
#define PTX_NSC_RFERRORNTF_LENGTH                       (0x02u)                /**< NSC RFERRORNTF length */
/** @} */

/**
 * \name Definitions for NSC types of frames.
 * 
 * @{
 */
#define PTX_NSC_FRAMETYPE                               (0xC0u)                /**< NSC Frametype */
#define PTX_NSC_FRAMETYPE_CMD                           (0x00u)                /**< NSC Frametype command */
#define PTX_NSC_FRAMETYPE_RSP                           (0x40u)                /**< NSC Frametype response */
#define PTX_NSC_FRAMETYPE_NTF                           (0x80u)                /**< NSC Frametype NTF */
#define PTX_NSC_FRAMETYPE_TRANSPARENT                   (0xC0u)                /**< NSC Frametype transparent */
#define PTX_NSC_FRAMETYPE_TRANSP_HCP                    (0xC1u)                /**< NSC Frametype transparent HCP */
/** @} */

/**
 * \name Definitions for NSC Activate command parameters.
 * 
 * @{
 */
#define PTX_NSC_TYPES_LISBSENSRES_LEN                   (13U)                  /**< NSC Types LISBSENSRES length */ 
#define PTX_NSC_MAXIMUM_NFCID1_LEN                      (10u)                  /**< NSC maximum NFCID1 length */ 
#define PTX_NSC_NFCID0_LEN                              (4u)                   /**< NSC NFCID0 length */ 
#define PTX_NSC_NFCID2_LEN                              (8u)                   /**< NSC NFCID2 length */ 
#define PTX_NSC_TYPEV_UID_LEN                           (8u)                   /**< NSC Type V UID length */ 
/** @} */

/**
 * \name Common Type of Rf Tech for the Stack
 * 
 * @{
 */
#define PTX_NSC_TYPES_TECH_A                            (0x00u)                /**< NSC RF Tech type A */ 
#define PTX_NSC_TYPES_TECH_B                            (0x01u)                /**< NSC RF Tech type B */                
#define PTX_NSC_TYPES_TECH_F                            (0x02u)                /**< NSC RF Tech type F */                
#define PTX_NSC_TYPES_TECH_V                            (0x06u)                /**< NSC RF Tech type V */                
/** @} */

/**
 * \name RF_TECH parameter of NSC_RF_ACTIVATE_NTF.
 * 
 * @{
 */
#define PTX_NSC_TYPES_TECH_POLL_A                       (0x00u)                /**< NSC RF Activate NTF tech type Poll A */ 
#define PTX_NSC_TYPES_TECH_POLL_B                       (0x01u)                /**< NSC RF Activate NTF tech type Poll B */ 
#define PTX_NSC_TYPES_TECH_POLL_F                       (0x02u)                /**< NSC RF Activate NTF tech type Poll F */ 
#define PTX_NSC_TYPES_TECH_POLL_ACT                     (0x03u)                /**< NSC RF Activate NTF tech type Poll ACT */ 
#define PTX_NSC_TYPES_TECH_POLL_V                       (0x06u)                /**< NSC RF Activate NTF tech type Poll V */ 
#define PTX_NSC_TYPES_TECH_LISTEN_A                     (0x80u)                /**< NSC RF Activate NTF tech type Listen A */ 
/** @} */

/**
 * \name ERROR_CODE parameter of NSC_RF_ERROR_NTF.
 * 
 * @{
 */
#define PTX_NSC_RF_ERROR_NTF_CODE_OK                    (0x00u)                /**< NSC RF Error NTF OK */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_THERMAL           (0x06u)                /**< NSC RF Error NTF Thermal error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_OVERCURRENT       (0x07u)                /**< NSC RF Error NTF Overcurrent error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_CURRENT_LIMIT     (0x09u)                /**< NSC RF Error NTF Current limit error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_EXT_CURRENT_SENSOR (0x0Au)               /**< NSC RF Error NTF External current sensor */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_EMV_COLL          (0x11u)                /**< NSC RF Error NTF EMV Coll error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_TIMEOUT           (0x12u)                /**< NSC RF Error NTF Timeout error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_TRANSMISSION      (0x13u)                /**< NSC RF Error NTF Transmission error */
#define PTX_NSC_RF_ERROR_NTF_CODE_ERR_PROTOCOL          (0x14u)                /**< NSC RF Error NTF Protocol error */
/** @} */

/**
 * \name Maximum Number of Rf Parameters to set.
 * 
 * @{
 */
#define PTX_NSC_RF_SET_PARAMS_MAX                       (11u)                   /**< NSC RF Set Params maximum amount */
/** @} */

/**
 * \name Definitions for NSC Read Command.
 * 
 * @{
 */
#define PTX_NSC_TYPES_RD_OP_MAX                         (3u)                   /**< NSC Read RDRP maximum OP */
#define PTX_NSC_READ_CMD_HD_LENGTH                      (0x03u)                /**< NSC Read command HD length */
#define PTX_NSC_READ_CMD_OPCODE                         (0x03u)                /**< NSC Read command opcode */
#define PTX_NSC_READ_RSP_HD_LENGTH                      (0x03u)                /**< NSC Read response HD length */
#define PTX_NSC_READ_RSP_OPCODE                         (0x43u)                /**< NSC Read response opcode */
/** @} */


/**
 * \name Definitions for NSC Write Command.
 * @{
 */
#define PTX_NSC_TYPES_WR_OP_MAX                          (70u)                 /**< NSC Write WRRP maximum OP */
#define PTX_NSC_WRITE_CMD_HD_LENGTH                      (0x03u)               /**< NSC Write command HD length */
#define PTX_NSC_WRITE_CMD_OPCODE                         (0x04u)               /**< NSC Write command opcode */
#define PTX_NSC_WRITE_RSP_LENGTH                         (0x02u)               /**< NSC Write response length */
#define PTX_NSC_WRITE_RSP_OPCODE                         (0x44u)               /**< NSC Write response opcode */
/** @} */

/**
 * \name Generic / Misc. Defines
 * @{
 */
#define PTX_NSC_MISC_RF_CONFIG_BUFFER_SIZE            	(uint8_t)37            /**< NSC Miscellaneous RF Config Buffer size */
#define PTX_NSC_MISC_RF_CONFIG_FLAGS_SET                (uint8_t)0x01          /**< NSC Miscellaneous RF Config Flags set */
#define PTX_NSC_MISC_RF_CONFIG_FLAGS_LOADED             (uint8_t)0x02          /**< NSC Miscellaneous RF Config Flags loaded */
/** @} */

/**
 * \name Definitions for NSC RF RUN TEST CMD
 *
 * @{
 */
#define PTX_NSC_RF_RUN_TEST_CMD_OPCODE                  (0x17u)                 /**< NSC RF Run Test command opcode */
#define PTX_NSC_RF_RUN_TEST_CMD_RSP_OPCODE              (0x57u)                 /**< NSC RF Run Test Command response opcode */
#define PTX_NSC_RF_RUN_TEST_CMD_RSP_LENGTH              (0x02u)                 /**< NSC RF Run Test Command response length */
/** @} */

/**
 * \name Definitions for NSC RF STOP TEST CMD
 *
 * @{
 */
#define PTX_NSC_RF_STOP_TEST_CMD_OPCODE                 (0x18u)                 /**< NSC RF Stop Test command opcode */
#define PTX_NSC_RF_STOP_TEST_CMD_RSP_OPCODE             (0x58u)                 /**< NSC RF Run Test Command response opcode */
#define PTX_NSC_RF_STOP_TEST_CMD_RSP_LENGTH             (0x02u)                 /**< NSC RF Run Test Command response length */
/** @} */

/**
 * \name Definitions for NSC INIT CMD
 *
 * @{
 */
#define PTX_NSC_INIT_EXT_PROTECTION_EN_MASK             (0x03u)                 /**< NSC Ext. Current Protection Enable-Flag mask */
/** @} */

/**
 * \name NSC Extension Definitions
 *
 * @{
 */
#define PTX_NSC_MAX_EXTENSIONS                          (uint8_t)(1u)            /**< NSC Max. amount of Extensions supported in parallel */
/** @} */


/*
 * ####################################################################################################################
 * INTERNALS / SYSTEM PARAMETERS EXPOSED ON API OF NSC COMPONENT
 * ####################################################################################################################
 */

/**
 * \brief Callback function for Waiting For Events (asynchronously).
 *
 */
typedef void (*pptxNSC_WfeCallBack_t) (void *ctx, ptxNSC_Event_t *event);

 /**
  * \brief Processing function for Extension Notifications.
  *
  */
 typedef void (*pptxNSC_Process_Ext_NTF_t) (void *ctx, uint8_t *pld, size_t pldLen);


 /**
  * \brief Callback function used for processing extension NTF (i.e. WLC extensions).
  *
  */
 typedef void (*pptxNSC_ExtCallBack_t) (void *ctx, void *events, uint8_t *pld, size_t pldLen);

/*
 * ####################################################################################################################
 * INTERNAL TYPES
 * ####################################################################################################################
 */

/**
 * \brief NSC response Eror Codes
 */ 
typedef enum ptxNSC_Rsp_ErrorCodes
{
    ptxNscRsp_Sucessful             = (0x00u),                  /**< Command successfully executed without errors. */
    ptxNscRsp_UnknownError          = (0x01u),                  /**< Command failed due to an unknown error.  */
    ptxNscRsp_NotAllowedCmd         = (0x02u),                  /**< Command not allowed in the current state. */
    ptxNscRsp_UnknownErrorCmd       = (0x03u),                  /**< Command unknown. */
    ptxNscRsp_InvalidParameter      = (0x04u),                  /**< Invalid parameter. */
    ptxNscRsp_RouteNotFound         = (0x05u),                  /**< Route not found. */
    ptxNscRsp_PAThermalProtError    = (0x06u),                  /**< PA Thermal protection error. */
    ptxNscRsp_PAOvercurProtError    = (0x07u),                  /**< PA Overcurrent protection error. */
    ptxNscRsp_DeviceInStandby       = (0x08u),                  /**< Device is in Standby mode. */
    ptxNscRsp_RfCollisionError      = (0x11u),                  /**< RF Collision error (valid for EMVCo poll mode). */
    ptxNscRsp_RfTimeoutError        = (0x12u),                  /**< RF Timeout error. */
    ptxNscRsp_RfTransmissionError   = (0x13u),                  /**< RF Transmission error. */
    ptxNscRsp_RfProtocolError       = (0x14u),                  /**< RF Protocol error. */
    ptxNscRsp_RfPollModNotSupported = (0x15u),                  /**< RF Poll mode not supported. */
    ptxNscRsp_RfListModNotSupported = (0x16u),                  /**< RF Listen mode not supported. */
    ptxNscRsp_SwpUiccNotPresent     = (0x21u),                  /**< SWP error. UICC not present. */
    ptxNscRsp_SwpInterfaceError     = (0x22u),                  /**< SWP error. Interface error. */
    ptxNscRsp_SwpIdentityCheckFail  = (0x23u),                  /**< SWP identity check failed. */
    ptxNscRsp_SwpModeNotSupported   = (0x24u),                  /**< SWP mode not supported. */
} ptxNSC_Rsp_ErrorCodes_t;

/**
 * \brief NSC RF CONFIG Parameter Type List
 */
typedef enum ptxNSC_RfConfig_ParamList
{
    RfCfgParam_Undefined,

	RfCfgParam_Wavebank_0,
	RfCfgParam_Wavebank_1,
	RfCfgParam_Wavebank_2,
	RfCfgParam_Wavebank_3,
	RfCfgParam_Wavebank_4,
	RfCfgParam_Wavebank_5,
	RfCfgParam_Wavebank_6,
	RfCfgParam_Wavebank_7,
	RfCfgParam_Wavebank_8,
	RfCfgParam_Wavebank_9,
	RfCfgParam_Wavebank_10,
	RfCfgParam_Wavebank_11,
	RfCfgParam_Wavebank_12,
	RfCfgParam_Wavebank_13,
	RfCfgParam_Wavebank_14,
	RfCfgParam_Wavebank_15,
	RfCfgParam_Wavebank_16,
	RfCfgParam_Wavebank_17,
	RfCfgParam_Wavebank_18,
	RfCfgParam_Wavebank_19,
    RfCfgParam_RegsPollA106,
    RfCfgParam_RegsPollA212,
    RfCfgParam_RegsPollA424,
    RfCfgParam_RegsPollA848,
    RfCfgParam_RegsPollB106,
    RfCfgParam_RegsPollB212,
    RfCfgParam_RegsPollB424,
    RfCfgParam_RegsPollB848,
    RfCfgParam_RegsPollF212,
    RfCfgParam_RegsPollF424,
    RfCfgParam_RegsPollV,
    RfCfgParam_Listen,
    RfCfgParam_RegsMisc,

    RfCfgParam_MaxNum
} ptxNSC_RfConfig_ParamList_t;

/**
 * \brief Parameters for NSC_RF_CONFIG_CMD
 */
typedef struct ptxNSC_RfConfigTlv
{
    ptxNSC_RfConfig_ParamList_t     ID;                         /**< RF Config parameter list */
    uint8_t                         *Value;                     /**< Value */
    uint8_t                         Len;                        /**< Length */
} ptxNSC_RfConfigTlv_t;

/**
 * \brief Parameters for NSC_INIT_CMD
 */
typedef struct ptxNSC_InitPars
{
        uint8_t Con_Clk_Src;                                    /**< Defines the source clock of the clock synthesizer. */
        uint8_t Con_Var_Lbs;                                    /**< Reference division select parameter. */
        uint8_t Con_NHost[PTX_NSC_INIT_CON_HOST_LENGTH];        /**< Division factor for the clock synthesizer.. */
        uint8_t Con_NHost_Ce[PTX_NSC_INIT_CON_HOST_CE_LENGTH];  /**< Division factor for the clock synthesizer during the CE (Card Emulation) mode. */
        uint8_t Con_Uart_Config[PTX_NSC_INIT_CON_UART_CONFIG];  /**< UART configuration data. */
        uint8_t Con_Prng_Seed[PTX_NSC_INIT_CON_PRNG_SEED];      /**< PRNG seed value. */
        uint8_t Con_N_Alm_Max[PTX_NSC_INIT_CON_N_ALM_MAX];      /**< Maximum allowed N_ALM value for ALM frequency correction. */
        uint8_t Con_N_Alm_Min[PTX_NSC_INIT_CON_N_ALM_MIN];      /**< Minimum allowed N_ALM value for ALM frequency correction. */
        uint8_t Con_Paocp_Th;                                   /**< Power Amplifier over-current protection threshold. */
        uint8_t Con_Patp_Th;                                    /**< Power Amplifier thermal protection threshold. */
        uint8_t Con_Xcp_Ctrl;                                   /**< External current protection control parameter. */
        uint8_t Con_Xcp_Th_Gt;                                  /**< External current protection settings (threshold for ISensor & GuardTime for ILimiter). */
} ptxNSC_InitPars_t;

/**
 * \brief Parameters for NSC_DISCOVER_CMD
 */
typedef struct ptxNSC_RfDiscPars
{
        uint8_t Con_Poll;                                       /**< General poll mode configuration parameter. */
        uint8_t Con_Poll_Gt;                                    /**< Guard time (GT) in milliseconds before the GT of the 1st POLL command of the discovery loop. */
        uint8_t Con_Poll_Disc_Mode;                             /**< Determines the RF discovery mode: regular RF discovery, low power card detection or hybrid mode. */
        uint8_t Con_Poll_A;                                     /**< General NFC-A poll mode configuration parameter. */
        uint8_t Con_Poll_A_Cmd;                                 /**< NFC-A poll command. */
        uint8_t Con_Poll_A_Freq;                                /**< NFC-A poll frequency. */
        uint8_t Con_Poll_A_Dev_Limit;                           /**< The maximum number of devices to be resolved during the NFC-A poll resolution process. */
        uint8_t Con_Poll_B;                                     /**< General NFC-B poll mode configuration parameter. */
        uint8_t Con_Poll_B_Cmd[PTX_NSC_DISC_CON_POLLB_CMD_LEN]; /**< NFC-B poll command. */
        uint8_t Con_Poll_B_Freq;                                /**< NFC-B poll frequency. */
        uint8_t Con_Poll_B_Dev_Limit;                           /**< The maximum number of devices to be resolved during the NFC-B poll resolution process. */
        uint8_t Con_Poll_F;                                     /**< General NFC-F poll mode configuration parameter. */
        uint8_t Con_Poll_F_Cmd[PTX_NSC_DISC_CON_POLLF_CMD_LEN]; /**< NFC-F poll command (SENSF_REQ). */
        uint8_t Con_Poll_F_Freq;                                /**< NFC-F poll frequency. */
        uint8_t Con_Poll_F_Dev_Limit;                           /**< The maximum number of devices to be resolved during the NFC-F poll resolution process. */
        uint8_t Con_Poll_V;                                     /**< General NFC-V poll mode configuration parameter. */
        uint8_t Con_Poll_V_Cmd[PTX_NSC_DISC_CON_POLLV_CMD_LEN]; /**< NFC-V poll command (INVENTORY_REQ). */
        uint8_t Con_Poll_V_Freq;                                /**< NFC-V poll frequency. */
        uint8_t Con_Poll_V_Dev_Limit;                           /**< The maximum number of devices to be resolved during the NFC-V poll resolution process. */

        uint8_t Con_Poll_Iso_Dep;                               /**< ISO-DEP protocol configuration parameter. */
        uint8_t Con_Poll_Iso_Dep_Rats_Param;                    /**< Defines PARAM of RATS command. */
        uint8_t Con_Poll_Iso_Dep_Attrib_Param1;                 /**< Defines PARAM1 of ATTRIB command. */
        uint8_t Con_Poll_Iso_Dep_Attrib_Param2_Fsdi;            /**< Defines FSDI of PARAM2 of ATTRIB command. */
        uint8_t Con_Poll_Iso_Dep_Attrib_Param3;                 /**< Defines PARAM3 of ATTRIB command. */
        uint8_t Con_Poll_Iso_Dep_Attrib_Param4;                 /**< Defines PARAM4 of ATTRIB command. */
        uint8_t Con_Poll_Iso_Dep_Attrib_Inf_Len;                /**< The length of CON_POLL_B_ATTRIB_INF parameter. */
        uint8_t Con_Poll_B_Attrib_Inf[PTX_NSC_DISC_CON_POLLB_ATTRIB_INF_LEN];           /**< ATTRIB command Higher Layer INF parameter. */

        uint8_t Con_Poll_Nfc_Dep;                               /**< NFC-DEP protocol configuration parameter. */
        uint8_t Con_Poll_Nfc_Dep_Atr_Req_Pp;                    /**< Indicates the presence of optional parameters. */
        uint8_t Con_Poll_Nfc_Dep_Atr_Req_G_Len;                 /**< Defines the length of CON_POLL_NFC_DEP_ATR_REQ_G parameter. Valid values: 0 - 16. */
        uint8_t Con_Poll_Nfc_Dep_Atr_Req_G[PTX_NSC_DISC_CON_POLL_NFCDEP_ATR_REQ_G_LEN];    /**< General bytes of ATR_REQ command. */

        uint8_t Con_Listen;                                     /**< General listen mode configuration parameter. */
        uint8_t Con_Listen_A;                                   /**< NFC-A listen mode configuration parameter. */
        uint8_t Con_Listen_A_Sens_Res[PTX_NSC_DISC_CON_LIS_SENSRESP_LEN];   /**< Response to SENS_REQ/ALL_REQ command. */
        uint8_t Con_Listen_A_Sel_Res;                           /**< Response to SEL_REQ command. */
        uint8_t Con_Listen_A_Nfcid1[PTX_NSC_DISC_CON_LISA_NFCID1_LEN];         /**< NFC-A identifier of the NFC Forum Device in the Passive Communication mode. */

        uint8_t Con_Listen_B;                                   /**< NFC-B listen configuration parameter. */
        uint8_t Con_Listen_B_Afi;                               /**< AFI parameter. */
        uint8_t Con_Listen_B_Sensb_Res[PTX_NSC_DISC_CON_LISB_SENSBRESP_LEN];    /**< Response to SENSB_REQ/ALLB_REQ command. */

        uint8_t Con_Listen_F;                                   /**< NFC-F listen mode configuration parameter. */
        uint8_t Con_Listen_F_Sensf_Res[PTX_NSC_DISC_CON_LISF_SENSFRESP_LEN];   /**< Response to SENSF_REQ command. */

        uint8_t Con_Listen_V;                                   /**< NFC-V listen mode configuration parameter. */
        uint8_t Con_Listen_V_Inv_Res[PTX_NSC_DISC_CON_LISV_INVRESP_LEN];   /**< Response to INVENTORY_REQ command. */

        uint8_t Con_Listen_Iso_Dep;                             /**< ISO-DEP listen configuration parameter. */
        uint8_t Con_Listen_Iso_Dep_Ats[PTX_NSC_DISC_CON_LIS_ISODEP_ATS_LEN];   /**< Answer to select . */

        uint8_t Con_Listen_Iso_Dep_Attrib_Res_Len;              /**< The length of CON_LISTEN_ISO_DEP_ATTRIB_RES parameter. */
        uint8_t Con_Listen_Iso_Dep_Attrib_Res[PTX_NSC_DISC_CON_LIS_ISODEP_ATTRIB_RES_LEN];   /**< Response to ATTRIB command. */

        uint8_t Con_Listen_Nfc_Dep;                             /**< NFC-DEP listen configuration parameter. */
        uint8_t Con_Listen_Nfc_Dep_Atr_Res_To;                  /**< TO parameter of ATR_RES response. */
        uint8_t Con_Listen_Nfc_Dep_Atr_Res_Pp;                  /**< PP parameter of ATR_RES response. */
        uint8_t Con_Listen_Nfc_Dep_Atr_Res_G_Len;               /**< The length of CON_LISTEN_NFC_DEP_ATR_RES_G parameter. */
        uint8_t Con_Listen_Nfc_Dep_Atr_Res_G[PTX_NSC_DISC_CON_LIS_NFCDEP_ATR_RES_G_LEN];    /**< General bytes of ATR_RES response. */

        uint8_t Con_Idle;                                       /**< Configures the NSC functionality during the idle time. */
        uint8_t Con_Idle_Time[PTX_NSC_DISC_CON_IDLE_TIME_LEN];  /**< Defines the idle time between two consecutive poll/listen procedures in 32us time units. */

} ptxNSC_RfDiscPars_t;

/**
 * \brief Enum for Polling-Loop type
 */
typedef enum ptxNSC_PollType
{
    PollModeDisabled,                                           /**< Disables any polling activity */
    ConstantField,                                              /**< Turns on RF-field (but no modulation!). */
    NfcForumMode,                                               /**< NFC-Forum polling mode */
    EmvPollMode,                                                /**< EMVCo polling mode (standard) */
    EmvPollMode_TransacA,                                       /**< EMVCo polling mode (TRANSAC_A) */
    EmvPollMode_TransacB,                                       /**< EMVCo polling mode (TRANSAC_B) */
	IsoPollMode,												/**< ISO polling mode */
} ptxNSC_PollType_t;

/**
 * \brief Enum for initial technology in the discovery loop.
 */
typedef enum ptxNSC_InitTechPoll
{
    Init_Poll_ACM,                                              /**< In case of enabled, first technology to poll for NFC-ACM. */
    Init_Poll_A,                                                /**< In case of enabled, first technology to poll for NFC-A. */
    Init_Poll_B,                                                /**< In case of enabled, first technology to poll for NFC-B. */
    Init_Poll_F,                                                /**< In case of enabled, first technology to poll for NFC-F. */
    Init_Poll_V                                                 /**< In case of enabled, first technology to poll for NFC-V. */
} ptxNSC_InitTechPoll_t;

/**
 * \brief NSC Rf Discovery Con_Poll_Disc_Mode parameter settings.
 */
typedef enum ptxNSC_RfDiscovery_Mode
{
    DiscoveryMode_Regular           = 0x00u,                    /**< Regular RF discovery mode. */
    DiscoveryMode_Lpcd              = 0x01u,                    /**< Low power card detection (LPCD) mode. */
    DiscoveryMode_Hybrid_Start      = 0x02u,                    /**< Hybrid mode: regular/full-power RF poll phase is used in every n-th discovery period and
                                                                     LPCD is used in other periods. Acceptable values: 2 - 255. */
    DiscoveryMode_Hybrid_End        = 0xFFu
} ptxNSC_RfDiscovery_Mode_t;

/**
 * \brief NSC Rf Parameter Id
 */
typedef enum ptxNSC_RfParameter_Id
{
    RfParameter_EoC,                                            /**< End-of-Command parameter indicates the end of a command. */
    RfParameter_Rf_Tech,                                        /**< Set RF technology. */
    RfParameter_Fwt,                                            /**< Set Frame Waiting Time in 128/fc time units in big-endian format (most significant byte first). */
    RfParameter_Tx_Bit_Rate,                                    /**< Set transmission bitrate */
    RfParameter_Rx_Bit_Rate,                                    /**< Set reception bitrate */
    RfParameter_Tx_PAR,                                         /**< Enable or disable RF PARITY on tx automatically handled by PTX1K (enabled by default). */
    RfParameter_Rx_PAR,                                         /**< Enable or disable RF PARITY on rx automatically handled by PTX1K (enabled by default). */
    RfParameter_Tx_CRC,                                         /**< Enable or disable RF CRC on tx automatically handled by PTX1K (enabled by default). */
    RfParameter_Rx_CRC,                                         /**< Enable or disable RF CRC on rx automatically handled by PTX1K (enabled by default). */
    RfParameter_Res_Limit,                                      /**< Set max. response limit for NSC to receive on RF (0 == wait until Frame Waiting Time expires). */
    RfParameter_Tx_Residual_Bits,                               /**< Defines residual bits for the last transmitted byte. In case the parameter is 0 a full byte is transmitted. Valid values are 0 to 7 */
    RfParameter_Rf_Field,                                       /**< Enables or disables the RF Field (1 == enabled, 0 == disabled) */
} ptxNSC_RfParameter_Id_t;

/**
 * \brief NSC Rf Parameter Rf Tech
 */
typedef struct ptxNSC_RfPar_RfTech
{
    uint8_t     RfTech;                                         /**< RF technology. */
} ptxNSC_RfPar_RfTech_t;


/**
 * \brief NSC Rf Parameter Fwt
 */
typedef struct ptxNSC_RfPar_Fwt
{
    uint8_t     Fwt[3u];                                        /**< FWT. */
} ptxNSC_RfPar_Fwt_t;


/**
 * \brief NSC Rf Parameter Tx_Bit_Rate
 */
typedef struct ptxNSC_RfPar_TxBitRate
{
    uint8_t     TxBitRate;                                      /**< TX Bit Rate. */
} ptxNSC_RfPar_TxBitRate_t;


/**
 * \brief NSC Rf Parameter Rx_Bit_Rate
 */
typedef struct ptxNSC_RfPar_RxBitRate
{
    uint8_t     RxBitRate;                                      /**< RX Bit Rate. */
} ptxNSC_RfPar_RxBitRate_t;

/**
 * \brief NSC Rf Parameter Tx_Parity
 */
typedef struct ptxNSC_RfPar_TxParity
{
    uint8_t     TxParity;                                       /**< TX Parity. */
} ptxNSC_RfPar_TxParity_t;

/**
 * \brief NSC Rf Parameter Rx_Parity
 */
typedef struct ptxNSC_RfPar_RxParity
{
    uint8_t     RxParity;                                       /**< RX Parity. */
} ptxNSC_RfPar_RxParity_t;


/**
 * \brief NSC Rf Parameter Tx_CRC
 */
typedef struct ptxNSC_RfPar_TxCRC
{
    uint8_t     TxCRC;                                          /**< TX CRC. */
} ptxNSC_RfPar_TxCRC_t;


/**
 * \brief NSC Rf Parameter Rx_Parity
 */
typedef struct ptxNSC_RfPar_RxCRC
{
    uint8_t     RxCRC;                                          /**< RX CRC. */
} ptxNSC_RfPar_RxCRC_t;

/**
 * \brief NSC Rf Parameter Res(ponse)_Limit
 */
typedef struct ptxNsc_RfPar_ResLimit
{
    uint8_t     ResLimit;                                       /**< RES Limit. */
} ptxNsc_RfPar_ResLimit_t;

/**
 * \brief NSC Rf Parameter Tx_Residual_Bits
 */
typedef struct ptxNsc_RfPar_TxResidualBits
{
    uint8_t     TxResidualBits;                                 /**< Tx residual bits */
} ptxNsc_RfPar_TxResidualBits_t;

/**
 * \brief NSC Rf Parameter Rf_Field
 */
typedef struct ptxNsc_RfPar_RfField
{
    uint8_t     RfField;                                        /**< RF field */
} ptxNsc_RfPar_RfField_t;

/**
 * \brief Union Rf Parameters
 */
typedef union ptxNSC_RfPar_Params
{
    ptxNSC_RfPar_RfTech_t               RfTech;                 /**< RF technology. */
    ptxNSC_RfPar_Fwt_t                  Fwt;                    /**< FWT. */
    ptxNSC_RfPar_TxBitRate_t            TxBitRate;              /**< TX Bit Rate. */
    ptxNSC_RfPar_RxBitRate_t            RxBitRate;              /**< RX Bit Rate. */
    ptxNSC_RfPar_TxParity_t             TxParity;               /**< TX Parity. */
    ptxNSC_RfPar_RxParity_t             RxParity;               /**< RX Parity. */
    ptxNSC_RfPar_TxCRC_t                TxCRC;                  /**< TX CRC. */
    ptxNSC_RfPar_RxCRC_t                RxCRC;                  /**< RX CRC. */
    ptxNsc_RfPar_ResLimit_t             ResLimit;               /**< RES Limit. */
    ptxNsc_RfPar_TxResidualBits_t       TxResidualBits;         /**< TX Residual Bits */
    ptxNsc_RfPar_RfField_t              RfField;                /**< RF Field */

} ptxNSC_RfPar_Params_t;

/**
 * \brief Rf Param
 */
typedef struct ptxNSC_RfPar
{
    ptxNSC_RfParameter_Id_t             ParmId;                 /**< Param Type. */
    ptxNSC_RfPar_Params_t               Parm;                   /**< Param. */
} ptxNSC_RfPar_t;

/**
 * \brief NSC Rf Set Parameters CMD parameter structure.
 */
typedef struct ptxNSC_RfSetParams_Par
{
    ptxNSC_RfPar_t              RfParams[PTX_NSC_RF_SET_PARAMS_MAX];                /**< Buffer for Params. */
    size_t                      NumOfParams;                                        /**< Used Params. */
} ptxNSC_RfSetParams_Par_t;


/**
 * \brief NSC Rf Tech A Activation Parameters.
 */
typedef struct ptxNSC_RfActTech_A_Param
{
    uint8_t                     NfcId1[PTX_NSC_MAXIMUM_NFCID1_LEN];                 /**< Buffer containing NFCID1. */
    size_t                      NfcId1_len;                                         /**< Length of \ref NfcId1. */
    uint8_t                     DeviceRfState;                                      /**< Rf Device State. */
} ptxNSC_RfActTech_A_Param_t;

/**
 * \brief NSC Rf Tech B Activation Parameters.
 */
typedef struct ptxNSC_RfActTech_B_Param
{
    uint8_t                     SensBRes[PTX_NSC_TYPES_LISBSENSRES_LEN];            /**< Buffer containing SensB Response. */
    uint8_t                     DeviceRfState;                                      /**< Rf Device State. */
} ptxNSC_RfActTech_B_Param_t;

/**
 * \brief NSC Rf Tech F Activation Parameters.
 */
typedef struct ptxNSC_RfActTech_F_Param
{
    uint8_t                     NfcId2[PTX_NSC_NFCID2_LEN];                         /**< Buffer containing NfcId2. */
    uint8_t                     DeviceRfState;                                      /**< Rf Device State. */
} ptxNSC_RfActTech_F_Param_t;

/**
 * \brief NSC Rf Tech V Activation Parameters.
 */
typedef struct ptxNSC_RfActTech_V_Param
{
    uint8_t                     Uid[PTX_NSC_TYPEV_UID_LEN];                         /**< Buffer containing UID. */
    uint8_t                     DeviceRfState;                                      /**< Rf Device State. */
} ptxNSC_RfActTech_V_Param_t;

/**
 * \brief NSC Rf Tech Activation Parameters.
 */
typedef union ptxNSC_RfActTech_Param
{
    ptxNSC_RfActTech_A_Param_t      RfAct_A_Params;                                 /**< NSC Rf Tech A Activation Parameters. */
    ptxNSC_RfActTech_B_Param_t      RfAct_B_Params;                                 /**< NSC Rf Tech B Activation Parameters. */
    ptxNSC_RfActTech_F_Param_t      RfAct_F_Params;                                 /**< NSC Rf Tech F Activation Parameters. */
    ptxNSC_RfActTech_V_Param_t      RfAct_V_Params;                                 /**< NSC Rf Tech V Activation Parameters. */
} ptxNSC_RfActTech_Param_t;

/**
 * \brief NSC Rf Prot ISO-DEP (Type-A) Activation Parameters.
 */
typedef union ptxNSC_RfActProt_ISODEP_Param
{
    uint8_t         IsPpsRequired;                                      /**< Is pps required ? (According to NFC Forum not required but optionally sent if application decides). */
    uint8_t         Did;                                                /**< Frame Size for proximity coupling Device Integer. */
    uint8_t         Fsdi;                                               /**< Defines the logical number of the addressed NFC card. */
    uint8_t         Pps1;                                               /**< PPS1.*/
} ptxNSC_RfActProt_ISODEP_Param_t;

/**
 * \brief NSC Rf Prot Activation Parameters.
 */
typedef union ptxNSC_RfActProt_Param_t
{
    ptxNSC_RfActProt_ISODEP_Param_t IsoDepParams;                       /**< IDO-DEP Parameters. */
} ptxNSC_RfActProt_Param_t;

/**
 * \brief NSC Rf Activate structure.
 */
typedef struct ptxNSC_RfActiv_Param
{
    uint8_t                     RfTech;                                 /**< Rf Technology. */
    ptxNSC_RfActTech_Param_t    RfTechActParams;                        /**< Rf Technology specific activation parameters. */
    uint8_t                     RfProt;                                 /**< Rf Protocol. */
    uint8_t                     UseShortActivation;                     /**< Short Activation (if Device is in Ready-state, otherwise Long Activation (Sleep-state). */
} ptxNSC_RfActiv_Param_t;


/**
 * \brief NSC Rf Deactivate procedure type.
 */
typedef enum ptxNSC_RfDeact_Type
{
    DeactType_Generic               = 0x01u,                            /**< Generic Deactivation - no specific deactivation */
    DeactType_Protocol_Specific     = 0x02u,                            /**< Protocol Specific Deactivation - managed by the uCode itself */
} ptxNSC_RfDeact_Type_t;

/**
 * \brief NSC RF state-machine state after command execution.
 */
typedef enum ptxNSC_RfDeact_State
{
    RfIdle                = 0x01u,                                      /**< Rf Idle State. */
    RfDiscovery           = 0x02u,                                      /**< Rf Discovery State. */
    RfPollSleep           = 0x03u,                                      /**< Rf Poll State. */
    RfDiscoverNoFieldOff  = 0x05u,                                      /**< Rf Discovery No Field Off. */
} ptxNSC_RfDeact_State_t;

/**
 * \brief NSC RF deactivate parameters.
 */
typedef struct ptxNSC_RfDeactPars
{
    ptxNSC_RfDeact_State_t      Rf_State;                               /**< RF state-machine state after command execution. */
    ptxNSC_RfDeact_Type_t       Rf_Deactivate_Type;                     /**< RF deactivation type. */
} ptxNSC_RfDeactPars_t;

/**
 * \brief Types of NSC Read CMD.
 */
typedef enum ptxNSC_ReadCmd_Type
{
    ReadRandomAdd = 1u,                /**< Read Random Addres. */
} ptxNSC_ReadCmd_Type_t;

/**
 * \brief NSC Read CMD parameter structure.
 */
typedef struct ptxNSC_ReadCmd_Par
{
    ptxNSC_ReadCmd_Type_t       Type;                                           /**< Type of NSC Read CMD. */
    size_t                      NumOfRead;                                      /**< Number of reads. */
    uint16_t                    Addresses[PTX_NSC_TYPES_RD_OP_MAX];           /**< Buffer of addresses to read from. */
} ptxNSC_ReadCmd_Par_t;


/**
 * Types of NSC Read CMD.
 */
typedef enum ptxNSC_WriteCmd_Type
{
    WriteRandomAdd = 1u,                /**< Write Random Address. */
} ptxNSC_WriteCmd_Type_t;

/**
 * NSC Read CMD parameter structure.
 */
typedef struct ptxNSC_WriteCmd_Par
{
    ptxNSC_WriteCmd_Type_t      Type;                                           /**< Type of NSC Read CMD. */
    size_t                      NumOfWrite;                                     /**< Number of writes. */
    uint16_t                    Addresses[PTX_NSC_TYPES_WR_OP_MAX];             /**< Buffer of addresses to write to. */
    uint8_t                     Values[PTX_NSC_TYPES_WR_OP_MAX];                /**< Buffer of values to write. */
} ptxNSC_WriteCmd_Par_t;

/**
 * \brief NSC Revision-Info Types.
 */
typedef enum ptxNSC_RevisionType
{
    ptxNSC_Info_C_Stack,
    ptxNSC_Info_Local_Changes,
    ptxNSC_Info_DFY_Code,
    ptxNSC_Info_DFY_Toolchain,
    ptxNSC_Info_ChipID,
    ptxNSC_Info_ProductID,
} ptxNSC_RevisionType_t;

/**
 * \brief NSC chip power modes.
 */
typedef enum ptxNSC_Power_Mode
{
    PowerMode_Active           = 0x00u,                     /**< Active mode. Default mode of operation. */
    PowerMode_StandBy          = 0x01u,                     /**< StandBy mode. Low power mode, RF communication turned off. */
} ptxNSC_Power_Mode_t;

/**
 * \brief System States in PTX100x
 */
typedef enum ptxNSC_System_State
{
    SystemState_OK,
    SystemState_ERR_Overcurrent,
    SystemState_ERR_Temperature,
    SystemState_ERR_CurrentLimit
} ptxNSC_System_State_t;

/**
 * \brief NSC Mode. (Just used for UART Interface).
 */
typedef enum ptxNSC_Mode
{
    NscMode_HW,                                             /**< HW Mode. HW operations allowed and "uart rx thread" disabled. */
    NscMode_SYS                                             /**< HW Mode. HW operations not allowed and "uart rx thread" enabled.  */
} ptxNSC_Mode_t;

/**
 * \brief Test-IDs for RF-Test command.
 */
typedef enum ptxNSC_RfTest_ID
{
    RfTest_TRANSAC_A = 1,                                       /**< RF-Test Identifier TRANSAC-A. */
    RfTest_TRANSAC_B = 2,                                       /**< RF-Test Identifier TRANSAC-B. */
    RfTest_Carrier   = 3,                                       /**< RF-Test Identifier CARRIER. */
    RfTest_PRBS_9    = 4,                                       /**< RF-Test Identifier PRBS-9. */
    RfTest_PRBS_15   = 5,                                       /**< RF-Test Identifier PRBS-15. */
} ptxNSC_RfTest_ID_t;

/**
 * \brief NSC RSP Handler
 */
typedef struct ptxNSC_Rsp
{
    uint8_t         NewRspReceived;                         /**< Boolean used to report a new RSP message received. It is set to 0 before any CMD/RSP operation. */
    uint8_t         RspBuff[PTX_NSC_MAX_RSP_LEN];           /**< Buffer used for Responses */
    size_t          RspLen;                                 /**< Length of RSP. */
} ptxNSC_Rsp_t;

/**
 * \brief NSC Rf Config Registers Structures.
 */
typedef struct ptxNSC_RfConfig_Regs
{
    uint8_t    *RegsPolla106;           /**< Array of Registers for Poll A Tech at 106 Kbps. */
    size_t      RegsPolla106_Len;       /**< Length of \ref RegsPolla106 */
    uint8_t    *RegsPolla212;           /**< Array of Registers for Poll A Tech at 212 Kbps. */
    size_t      RegsPolla212_Len;       /**< Length of \ref RegsPolla212 */
    uint8_t    *RegsPolla424;           /**< Array of Registers for Poll A Tech at 424 Kbps. */
    size_t      RegsPolla424_Len;       /**< Length of \ref RegsPolla424 */
    uint8_t    *RegsPolla848;           /**< Array of Registers for Poll A Tech at 848 Kbps. */
    size_t      RegsPolla848_Len;       /**< Length of \ref RegsPolla424 */

    uint8_t    *RegsPollb106;           /**< Array of Registers for Poll B Tech at 106 Kbps. */
    size_t      RegsPollb106_Len;       /**< Length of \ref RegsPollb106 */
    uint8_t    *RegsPollb212;           /**< Array of Registers for Poll B Tech at 212 Kbps. */
    size_t      RegsPollb212_Len;       /**< Length of \ref RegsPollb212 */
    uint8_t    *RegsPollb424;           /**< Array of Registers for Poll B Tech at 424 Kbps. */
    size_t      RegsPollb424_Len;       /**< Length of \ref RegsPollb424 */
    uint8_t    *RegsPollb848;           /**< Array of Registers for Poll A Tech at 848 Kbps. */
    size_t      RegsPollb848_Len;       /**< Length of \ref RegsPollb848 */

    uint8_t    *RegsPollf212;           /**< Array of Registers for Poll F Tech at 212 Kbps. */
    size_t      RegsPollf212_Len;       /**< Length of \ref RegsPollf212 */
    uint8_t    *RegsPollf424;           /**< Array of Registers for Poll F Tech at 424 Kbps. */
    size_t      RegsPollf424_Len;       /**< Length of \ref RegsPollf424 */

    uint8_t    *RegsPollV;              /**< Array of Registers for Poll V Tech. */
    size_t      RegsPollV_Len;          /**< Length of \ref RegsPollV */

    uint8_t    *RegsListen;             /**< Array of Registers for Listen Mode. */
    size_t      RegsListen_Len;         /**< length of \ref RegsListen. */
} ptxNSC_RfConfig_Regs_t;

/**
 * \brief NSC Rf Config Parameters.
 */
typedef struct ptxNSC_RfConfig_Param
{
    uint8_t                 VersionNvm;                         /**< Version Nvm File */
    uint8_t                 *conTxPollModeHighPowerMod100;      /**< POLL MODE HIGH POWER MOD100 */
    size_t                  conTxPollModeHighPowerMod100Len;    /**< POLL MODE HIGH POWER MOD100 Length */
    uint8_t                 *conTxPollModeLowPowerMod100;       /**< POLL MODE LOW POWER MOD100 */
    size_t                  conTxPollModeLowPowerMod100Len;     /**< POLL MODE LOW POWER MOD100 Length */
    uint8_t                 *conTxPollModeHighPowerMod10;       /**< POLL MODE HIGH POWER MOD10 */
    size_t                  conTxPollModeHighPowerMod10Len;     /**< POLL MODE LOW POWER MOD10 Length */
    uint8_t                 *conTxPollModeLowPowerMod10;        /**< POLL MODE LOW POWER MOD10 */
    size_t                  conTxPollModeLowPowerMod10Len;      /**< POLL MODE LOW POWER MOD10 Length */
    uint8_t                 *conTxListenModeHighPower;          /**< LISTEN MODE HIGH POWER */
    size_t                  conTxListenModeHighPowerLen;        /**< LISTEN MODE HIGH POWER Length*/
    uint8_t                 *conTxListenModeLowPower;           /**< LISTEN MODE LOW POWER */
    size_t                  conTxListenModeLowPowerLen;         /**< LISTEN MODE LOW POWER Length*/
    uint8_t                 *MiscSettings;                      /**< MISCELLANEOUS */
    size_t                  MiscSettings_Len;                   /**< MISCELLANEOUS Length*/
    ptxNSC_RfConfig_Regs_t  Regs;                               /**< REGISTERS */
} ptxNSC_RfConfig_Param_t;

/**
 * \brief NSC Misc. Rf Config Parameters (Shadow-Copy)
 */
typedef struct ptxNSC_RFMiscConfig_Param
{
    uint8_t                 MiscSettings[PTX_NSC_MISC_RF_CONFIG_BUFFER_SIZE];   /**< MISCELLANEOUS Settings */
    uint8_t                 MiscSettings_Len;                                   /**< MISCELLANEOUS Length*/
    uint8_t                 MiscSettingsFlags;                                  /**< MISCELLANEOUS Flags */
} ptxNSC_RFMiscConfig_Param_t;

/**
 * \brief Configuration parameters for the NSC Component
 */
typedef struct ptxNSC_ConfigPars
{
    void                        *Plat;                      /**< Pointer to platform component. */
    pptxNSC_WfeCallBack_t        WfeCb;                     /**< Callback function to report upper layer Events received. */
    void                        *Ctx;                       /**< Context used by /ref WfeCb . */
} ptxNSC_ConfigPars_t;

/**
 * \brief Extension Prototype (RFU): Handler for NSC RF-Discover Command.
 */
typedef ptxStatus_t (*pExtNSCDiscoverCmd_t)(void *extCtx, uint8_t *cmdBuffer);

/**
 * \brief Extension Prototype (RFU): Handler for NSC RF-Discover Notification.
 */
typedef ptxStatus_t (*pExtNSCDiscoverNtf_t)(void *extCtx, struct ptxNSC *nscCtx, uint8_t *payload, size_t lengthOfPayload, ptxNSC_Event_t *event);

/**
 * \brief Extension Prototype (RFU): Handler for NSC RF-Activate Notification.
 */
typedef ptxStatus_t (*pExtNSCActivateNtf_t)(void *extCtx, struct ptxNSC *nscCtx, uint8_t *payload, size_t lengthOfPayload, ptxNSC_Event_t *event);

/**
 * \brief Extension Prototype (RFU): Extension Component Structure.
 */
typedef struct ptxNSC_Custom_Extension
{
    uint8_t                     ExtensionID;            /**< Unique Register ID */
    pExtNSCDiscoverCmd_t        CBFnExtDiscoverCmd;     /**< Callback Function to handle NSC Discover Command */
    pExtNSCDiscoverNtf_t        CBFnExtDiscoverNtf;     /**< Callback Function to handle NSC Discover Notification */
    pExtNSCActivateNtf_t        CBFnExtActivateNtf;     /**< Callback Function to handle NSC Activate Notification */
    void                        *ExtensionCtx;          /**< Extension-specific context. */

} ptxNSC_Custom_Extension_t;

/**
 * \brief Main NSC component structure.
 */
typedef struct ptxNSC
{
    ptxStatus_Comps_t           CompId;                                 /**< Component Id. */
    void                        *Plat;                                  /**< Pointer to platform component. */
    pptxNSC_WfeCallBack_t       WfeCb;                                  /**< Wait for Event CallBack function provided from other component. */
    void                        *Ctx;                                   /**< Context called by /ref WfeCb. */
    uint32_t                    MaxTransferUnit;                        /**< Maximum amount of bytes that can be transfered over NFC Rf link. */
    uint8_t                     RxCltMode;                              /**< Rx Clt Mode. */
    ptxNSC_Rsp_t                NscRsp;                                 /**< Nsc Rsp container.*/
    struct ptxNSC_System        *SysParams;                             /**< System parameters */
    ptxNSC_System_State_t       SysState;                               /**< System state */
    uint8_t                     DeactivationNTFPending;                 /**< Card-Deactivated Status-flag if Deactivate (Sleep) is used */
    uint32_t                    DeactiveTimeoutMS;                      /**< NSC-specific Timeout for RF-Deactivate Command */
    ptxNSC_Mode_t               NscMode;                                /**< Nsc Mode.*/
    pptxNSC_ExtCallBack_t       ExtensionCb;                            /**< Optional WLC callback for processing of extension NTF. */
    void                        *ExtensionCtx;                          /**< Optional WLC pointer for Context for processing of extension NTF. */
    pptxNSC_Process_Ext_NTF_t   ExtensionNtfProcess;                    /**< Optional WLC function pointer for processing of extension NTF. */
    ptxNSC_RFMiscConfig_Param_t RFConfigMiscParams;                     /**< Shadow-Copy of RF Config Misc. Parameters. */
    uint8_t                     ProductID;                              /**< Chip-specific product (family) Identifier */
    uint8_t                     TypeATransparentModeActive;             /**< Internal flag indicating whether Transparent-mode is active in NFC Type-A mode or not. */
    uint8_t                     TransparentModeNrResidualTxBits;        /**< Defines the number of residual bits to transmit in NFC Type A Transparent mode. */
    uint8_t                     NrCustomExtensions;                     /**< Current number of registered custom Extensions (RFU). */
    ptxNSC_Custom_Extension_t   CustomExtension[PTX_NSC_MAX_EXTENSIONS];/**< Custom Extensions Handlers (RFU). */
} ptxNSC_t;


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */


/**
 * \brief Initialize the NSC Component.
 *
 * This function has to be called before any other API functions at this Component.
 *
 * \param[in]   nscCtx           Pointer to a pointer where the NSC Content is going to be provided.
 * \param[in]   configPars       Nsc configuration parameters.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_Init(ptxNSC_t **nscCtx, ptxNSC_ConfigPars_t *configPars);


/**
 * \brief De-Initialize the NSC Component.
 *
 * This function has to be called last one at this Component.
 * This function cleans up the resources used by NSC.
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_Deinit(ptxNSC_t *nscCtx);

/**
 * \brief NSC Soft Reset on the PTX1K
 *
 * This function has to be called last one at this Component.
 * This function cleans up the resources used by NSC.
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_SoftReset(ptxNSC_t *nscCtx);


/**
 * \brief NSC Hard Reset on the PTX1K via the SEN-pin
 *
 * This function cleans up the resources used by NSC.
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_HardReset(ptxNSC_t *nscCtx);


/**
 * \brief Performs either an caller-agnostic NSC Hard (if SEN-pin is connected) or Soft Reset.
 *
 * This function cleans up the resources used by NSC.
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_Reset(ptxNSC_t *nscCtx);


/**
 * \brief NSC FW Downloading on the PTX1K
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_FwDownloader(ptxNSC_t *nscCtx);


/**
 * \brief NSC DFY Activation
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_DFY_Activation(ptxNSC_t *nscCtx);


/**
 * \brief NSC_INIT_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   nscInitPars      Pointer to parameters for NSC_INIT_CMD.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_InitCmd(ptxNSC_t *nscCtx, ptxNSC_InitPars_t *nscInitPars);


/**
 * \brief NSC_STANDY_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_StandbyCmd(ptxNSC_t *nscCtx);


/**
 * \brief NSC_WAKEUP_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_WakeupCmd(ptxNSC_t *nscCtx);


/**
 * \brief NSC_RF_CONFIG_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   nscRfCfgParams   Pointer to one (or more) RF-/SYS-Configuration parameter instance(s).
 * \param[in]   rfConfigTlvCount Number of RF-/SYS-Configuration parameter instance(s).
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfConfig(ptxNSC_t *nscCtx, ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t rfConfigTlvCount);


/**
 * \brief NSC_RF_DISCOVER_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   nscRfDiscPars    Pointer to parameters for NSC_DISCOVER_CMD.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfDiscovery(ptxNSC_t *nscCtx, ptxNSC_RfDiscPars_t *nscRfDiscPars);


/**
 * \brief NSC_RF_DISCOVER_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   nscRfParams      Pointer to parameters for NSC_RF_SET_PARAMETER_CMD.
 * \param[in]   nscRfParamsLen   Lenght of /ref nscRfParamsLen.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfSetParams(ptxNSC_t *nscCtx, ptxNSC_RfPar_t *nscRfParams, size_t nscRfParamsLen);


/**
 * \brief NSC_RF_ACTIVATE_CMD
 *
 * \param[in]   nscCtx             Pointer to an initialized instance of the NSC.
 * \param[in]   nscRfActPars       Pointer to parameters for NSC_RF_ACTIVATE_CMD.
 * \param[out]  activationData     Pointer to the buffer which will hold received card data.
 * \param[out]  activationDataLen  Pointer to the variable holding received activation data length.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfActivate(ptxNSC_t *nscCtx, ptxNSC_RfActiv_Param_t *nscRfActPars, uint8_t *activationData, size_t *activationDataLen);


/**
 * \brief NSC_RF_DEACTIVATE_CMD
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   nscRfDeactPars   Pointer to parameters for NSC_RF_DEACTIVATE_CMD.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfDeactivate(ptxNSC_t *nscCtx, ptxNSC_RfDeactPars_t *nscRfDeactPars);


/**
 * \brief Transmission of a NSC_RF_MSG to a remote RF device such as a reader.
 *
 * \param[in]   nscCtx           Pointer to an initialized instance of the NSC.
 * \param[in]   msgData          Buffer for the data to transmit
 * \param[in]   msgDataLen       Length of buffer for the data is going to transmit. Length of /ref msgData
 * \param[in]   isChained        Boolean for data chaining. 1 = chained or 0 = not-chained.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_RfDataMsgTx(ptxNSC_t *nscCtx, uint8_t *msgData, size_t msgDataLen, uint8_t isChained);


/**
 * \brief Get Maximum Transfer Unit
 *
 * \param[in]       nscCtx           Pointer to an initialized instance of the NSC.
 * \param[out]      maxTransferUnit  Pointer where the maximum payload of NSC Component is provided.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_Get_Mtu(ptxNSC_t *nscCtx, uint32_t *maxTransferUnit);


/**
 * \brief Get various revisions of system (C-Stack, DFY-Code/-Toochain, Chip-ID, Local changes etc.
 *
 * \param[in]       nscCtx              Pointer to the component structure.
 * \param[in]       revisionType        Type of Revision.
 * \param[out]      revisionInfo        Revision information.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_GetRevisionInfo(ptxNSC_t *nscCtx, ptxNSC_RevisionType_t revisionType, uint32_t *revisionInfo);


/**
 * \brief Checks System-State during initialization phase of IC
 *
 * \param[in]       nscCtx              Pointer to the component structure.
 * \param[in]       currentStatus       Status of currently executed function.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_CheckSystemState(ptxNSC_t *nscCtx, ptxStatus_t currentStatus);


/**
 * \brief Read out IC temperature sensor.
 *
 * \param[in]       nscCtx              Pointer to the component structure.
 * \param[out]      sensVal             Temperature sensor read-out value.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_ReadTempSensor(ptxNSC_t *nscCtx, uint8_t *sensVal);


/**
 * \brief Get uart configuration parameters for NSC init command.
 *
 * Parameters are calculated based on expected baudrate and flow control. The result is stored in provided uartConfig array.
 * UartConfig parameters are part of NSC_INIT_CMD and have to be provided always, no matter of the actual interface used.
 *
 * \param[in]       nscCtx              Pointer to the component structure.
 * \param[in]       baudRate            Expected baudrate for uart interface.
 * \param[out]      uartConfig          Array holding the resulting parameters, which have tobe provided in NSC init cmd.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_GetInitConfigParams(ptxNSC_t *nscCtx, uint32_t baudRate, uint8_t *uartConfig);

/**
 * \brief NSC Get last loaded Misc. RF-Configuration.
 *
 * This function is used to retrieve a copy of the last loaded miscellaneous RF-Configuration settings.
 *
 * \param[in]     nscCtx          Pointer to the component structure.
 * \param[in]     configBuffer    Pointer to buffer where Misc. settings should be stored.
 * \param[in,out] configBufferLen Size of configBuffer (in), Length of stored Misc. settings (out).
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_GetMiscRFConfig(ptxNSC_t *nscCtx, uint8_t *configBuffer, uint8_t *configBufferLen);

/**
 * \brief Reads a (register) value from the chip
 *
 * This function is used to read a (register) value from the chip.
 *
 * \param[in]     nscCtx        Pointer to the component structure.
 * \param[in]     address       Address to read
 * \param[out]    value         Read value
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Read(ptxNSC_t *nscCtx, uint16_t address, uint8_t *value);

/**
 * \brief Writes a single (register/memory) value to the chip
 *
 * This function is used to write a (register) value to the chip.
 *
 * \param[in]     nscCtx        Pointer to the component structure.
 * \param[in]     address       Address to write
 * \param[in]     value         Write value
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Write(ptxNSC_t *nscCtx, uint16_t address, uint8_t value);

/**
 * \brief Writes multiple (register/memory) values to the chip
 *
 * This function is used to write multiple register/memory values to to the chip.
 *
 * \param[in]     nscCtx        Pointer to the component structure.
 * \param[in]     addresses     Addresses to write
 * \param[in]     values        Write values
 * \param[in]     nrWrites      Number of Write addresses/values
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_WriteN(ptxNSC_t *nscCtx, uint16_t * addresses, uint8_t *values, size_t nrWrites);

/**
 * \brief Function for NSC_RF_RUN_CMD.
 *
 * This function builds the NSC_RF_RUN_CMD accordingly to inputParams and send it to the chip.
 *
 * \param[in]   nscCtx              Pointer to the component structure.
 * \param[in]   rfTestId            Test Identifier.
 * \param[in]   rfTestParams        Test Parameters.
 * \param[in]   rfTestParamsLen     Number of Test Parameters.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_RfTestRun(ptxNSC_t *nscCtx, ptxNSC_RfTest_ID_t rfTestId, uint8_t *rfTestParams, size_t rfTestParamsLen);

/**
 * \brief Function for NSC_RF_STOP_CMD.
 *
 * This function builds the NSC_RF_STOP_CMD accordingly to inputParams and send it to the chip.
 *
 * \param[in]   nscCtx              Pointer to the component structure.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_RfTestStop(ptxNSC_t *nscCtx);

/**
 * \brief Function to retrieve the current timeout value in [ms] to wait for a NSC.RF_DEACTIVATE_RSP.
 *
 * \param[in]   nscCtx              Pointer to the component structure.
 * \param[out]  currentTimeout      Pointer to store the current timeout value.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_GetDeactivateTimeout(ptxNSC_t *nscCtx, uint32_t *currentTimeout);

/**
 * \brief Function to set the current timeout value in [ms] to wait for a NSC.RF_DEACTIVATE_RSP.
 *
 * \param[in]   nscCtx              Pointer to the component structure.
 * \param[in]   currentTimeout      Pointer to variable holding new timeout value. If set to NULL, default value will be used.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_SetDeactivateTimeout(ptxNSC_t *nscCtx, uint32_t *currentTimeout);

/**
 * \brief Enables or disables the NFC Type-A Transparent Mode and allows to set the number of residual Tx-bits if enabled.
 *
 * \param[in]   nscCtx                  Pointer to the component structure.
 * \param[in]   transparentModeEnabled  Enables/Disables Type-A Transparent-Mode..
 * \param[in]   nrTxBits                Set number of residual Tx-bits if Type-A Transparent-Mode is enabled..
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_SetNrResidualTxBits(ptxNSC_t *nscCtx, uint8_t transparentModeEnabled, uint8_t nrTxBits);

/**
 * \brief Register an NSC-Extension (Prototype/RFU).
 *
 * \param[in]   nscCtx                  Pointer to the component structure.
 * \param[in]   extension               Pointer to Extension parameter structure.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_RegisterExtension(ptxNSC_t *nscCtx, ptxNSC_Custom_Extension_t *extension);

/**
 * \brief De-Register an NSC-Extension (Prototype/RFU).
 *
 * \param[in]   nscCtx                  Pointer to the component structure.
 * \param[in]   extensionID             ID of Extension to be de-registered within component.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_DeRegisterExtension(ptxNSC_t *nscCtx, uint8_t extensionID);

/*
 * ####################################################################################################################
 * COMMON FUNCTIONS
 * ####################################################################################################################
 */
/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_ProcessRspErrorCode (ptxNSC_Rsp_ErrorCodes_t NscRspErrorCode);


/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_Send(ptxNSC_t *nscCtx, ptxNscHal_BufferId_t bufferId, uint8_t *txBuf[], size_t txLen[], size_t numBuffers);


/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_ReceiveRsp(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen, uint32_t timeOut);


/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_Process (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen);


/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_SetMode (ptxNSC_t *nscCtx, ptxNSC_Mode_t newMode);

/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_GetMode (ptxNSC_t *nscCtx, ptxNSC_Mode_t *currentMode);

/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_Stop_WaitForRx(ptxNSC_t *nscCtx);


/**
 * \note Internal function
 */
ptxStatus_t ptxNSC_Start_WaitForRx(ptxNSC_t *nscCtx);

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

