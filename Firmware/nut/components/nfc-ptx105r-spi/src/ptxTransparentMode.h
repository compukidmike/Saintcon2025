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
    Module      : Transparent-Mode API
    File        : ptxTransparentMode.h

    Description : Dedicated API to enable low-level NFC-commands to implement custom/proprietary RF-protocols.
*/

/**
 * \addtogroup grp_ptx_api_transparent_mode Transparent Mode API
 *
 * @{
 */

#ifndef APIS_PTX_TRANSPARENT_MODE_H_
#define APIS_PTX_TRANSPARENT_MODE_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \name Transparent-Mode RF-Flags
 *
 * @{
 */
#define PTX_TRANSPARENT_MODE_FLAGS_TX_PARITY            (uint8_t)0x01            /**< Enable Tx-Parity (Type-A only) */
#define PTX_TRANSPARENT_MODE_FLAGS_RX_PARITY            (uint8_t)0x02            /**< Enable Rx-Parity (Type-A only) */
#define PTX_TRANSPARENT_MODE_FLAGS_TX_CRC               (uint8_t)0x04            /**< Enable Tx-CRC (all Technology Types) */
#define PTX_TRANSPARENT_MODE_FLAGS_RX_CRC               (uint8_t)0x08            /**< Enable Rx-CRC (all Technology Types) */
/** @} */

/**
 * \name Optional Module Configuration
 *
 * @{
 */
#define PTX_TRANSPARENT_MODE_RESET_DEFAULT      /**< If active, default RF-parameters will be applied when RF-field gets turned off (default) */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief Transparent-Mode supported RF-Technologies.
 */
typedef enum ptxTransparentMode_RF_Tech
{
    TM_RF_Tech_A      = 0,
    TM_RF_Tech_B      = 1,
    TM_RF_Tech_F      = 2,
    TM_RF_Tech_V      = 6,
    TM_RF_Tech_BPrime = 7,

} ptxTransparentMode_RF_Tech_t;

/**
 * \brief Transparent-Mode supported RF-Bitrates for Type A, B and F (V fixed).
 */
typedef enum ptxTransparentMode_RF_Bitrate
{
    TM_RF_Bitrate_106 = 0,
    TM_RF_Bitrate_212 = 1,
    TM_RF_Bitrate_424 = 2,
    TM_RF_Bitrate_848 = 3,
    TM_RF_Bitrate_26  = 6,

} ptxTransparentMode_RF_Bitrate_t;

/**
 * \brief Transparent-Mode Initialization Parameters
 */
typedef struct ptxTransparentMode_RFParams
{
    ptxTransparentMode_RF_Tech_t       Tech;                    /**< RF-Technology */
    ptxTransparentMode_RF_Bitrate_t    TxRate;                  /**< Tx-Bitrate */
    ptxTransparentMode_RF_Bitrate_t    RxRate;                  /**< Rx-Bitrate */
    uint8_t                            Flags;                   /**< CRC- and Parity-Settings */
    uint8_t                            NrTxBits;                /**< Number of (residual) Bits to be sent for last Byte */
    uint8_t                            ResLimit;                /**< Max. Number of responses (if set to 0, Receiver automatically enabled again until TimeoutMS expires (needed for T3T) */

} ptxTransparentMode_RFParams_t;

/**
 * \brief Transparent-Mode Initialization Parameters
 */
typedef struct ptxTransparentMode_InitParams
{
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd     *IoTRdComp;         /**< Main Stack component */
#else
    struct ptxPOS       *POSComp;           /**< Main Stack component */
#endif

} ptxTransparentMode_InitParams_t;

/**
 * \brief Transparent-Mode Component
 */
typedef struct ptxTransparentMode
{
    /* Components */
    ptxStatus_Comps_t   CompId;             /**< Component Id */

#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd     *IoTRdComp;         /**< Main Stack component */
#else
    struct ptxPOS       *POSComp;           /**< Main Stack component */
#endif

    struct ptxNSC       *NscComp;           /**< Reference to NSC Component.*/
    uint8_t             BPrimeActive;       /**< Internal Flag indicating whether B-Prime Mode is active or not */

} ptxTransparentMode_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the Transparent-Mode Component.
 *
 * \param[in]   tmComp                  Pointer to an allocated instance of the Transparent-Mode component.
 * \param[in]   initParams              Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxTransparentMode_Init (ptxTransparentMode_t *tmComp, ptxTransparentMode_InitParams_t *initParams);


/**
 * \brief Configures the HW using the provided RF-Parameters (used for all following RF-Exchanges, can be overwritten).
 *
 * \param[in]   tmComp                  Pointer to an initialized instance of the Transparent-Mode component.
 * \param[in]   rfParams                Pointer to RF-parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxTransparentMode_SetRFParameters (ptxTransparentMode_t *tmComp, ptxTransparentMode_RFParams_t *rfParams);

/**
 * \brief Turns the RF-field on or off.
 *
 * \param[in]       tmComp              Pointer to an initialized instance of the Transparent-Mode component.
 * \param[in]       state               State of RF-field (0 = off, != 0 = on).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxTransparentMode_SetField (ptxTransparentMode_t *tmComp, uint8_t state);

/**
 * \brief Performs a RF data exchange.
 *
 * Attention: The last received byte is the contactless status byte which is defined as follows:
 *            Bit 7:     If set to 1, a contactless error occured (e.g. CRC- or Parity-error).
 *            Bit 6 - 0: Number of valid bits in last received byte.
 *
 * \param[in]       tmComp              Pointer to an initialized instance of the Transparent-Mode component.
 * \param[in]       rfParams            Pointer to RF-parameters (optional, otherwise parameters from \ref ptxTransparentMode_SetRFParameters are used).
 * \param[in]       tx                  Buffer containing the data to send.
 * \param[in]       txLength            Length of "tx".
 * \param[out]      rx                  Pointer to buffer where the data will be received.
 * \param[in,out]   rxLength            As input, capacity of "rx". As output, actual number of bytes written on "rx".
 * \param[in]       timeoutMS           Timeout given in [ms].
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxTransparentMode_Exchange (ptxTransparentMode_t *tmComp,
                                         ptxTransparentMode_RFParams_t *rfParams,
                                         uint8_t *tx,
                                         uint8_t txLength,
                                         uint8_t *rx,
                                         uint32_t *rxLength,
                                         uint32_t timeoutMS);
/**
 * \brief Deinitializes the Transparent-Mode Component.
 *
 * \param[in]   tmComp                  Pointer to an initialized instance of the Transparent-Mode component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTransparentMode_Deinit (ptxTransparentMode_t *tmComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

