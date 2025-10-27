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
    Module      : NATIVE TAG API
    File        : ptxNativeTag_T2T.h

    Description : Native Tag API for NFC Forum Tag Type 2 (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_native_tag_T2T Native Tag T2T API
 *
 * @{
 */

#ifndef APIS_PTX_NATIVE_TAG_T2T_H_
#define APIS_PTX_NATIVE_TAG_T2T_H_

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
 * \name T2T Tag specific definitions
 * @{
 */
#define PTX_T2T_MIN_TX_BUFFER_SIZE   (uint32_t)32       /**< Internal work-buffer size for transmissions. */
#define PTX_T2T_BLOCK_SIZE           (uint8_t)4         /**< T2T memory block size */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief T2T Native Tag Initialization Parameters
 */
typedef struct ptxNativeTag_T2T_InitParams
{
    void                *IotRd;                 /**< Allocated instance of IoT-Reader component (see ptxIoTRd_Allocate_Stack()) */
    uint8_t             *TxBuffer;              /**< TxBuffer */
    uint32_t             TxBufferSize;          /**< Tx Buffer size */

} ptxNativeTag_T2T_InitParams_t;

/**
 * \brief T2T Native Tag Component
 */
typedef struct ptxNativeTag_T2T
{
    /* Components */
    ptxStatus_Comps_t    CompId;                /**< Component Id */

    void                *IoTRdStackComp;        /**< Allocated instance of IoT-Reader component (see ptxIoTRd_Allocate_Stack()) */
    uint8_t             *TxBuffer;              /**< Internal Tx-Buffer provided by upper layer (may be shared with other components) */

} ptxNativeTag_T2T_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T2T Native Tag Component.
 *
 * \param[in]   t2tComp             Pointer to an allocated instance of the T2T component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T2TOpen (ptxNativeTag_T2T_t *t2tComp, ptxNativeTag_T2T_InitParams_t *initParams);

/**
 * \brief Read the T2T blocks.
 *
 * \param[in]       t2tComp             Pointer to an initialized instance of the T2T component.
 * \param[in]       blockNr             Block number to be read (within sector).
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T2TRead (	ptxNativeTag_T2T_t *t2tComp,
                                                    uint8_t blockNr,
                                                    uint8_t *rx,
                                                    size_t *rxLen,
                                                    uint32_t msTimeout);

/**
 * \brief Write to the T2T blocks.
 *
 * \param[in]       t2tComp             Pointer to an initialized instance of the T2T component.
 * \param[in]       blockNr             Block number to be written (within sector).
 * \param[in]       blockData           Data to write.
 * \param[in]       blockDataLen        Length of data to write.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T2TWrite ( ptxNativeTag_T2T_t *t2tComp,
                                                    uint8_t blockNr,
                                                    uint8_t *blockData,
                                                    uint8_t blockDataLen,
                                                    uint8_t *rx,
                                                    size_t *rxLen,
                                                    uint32_t msTimeout);

/**
 * \brief Select the T2T sector.
 *
 * \param[in]       t2tComp             Pointer to an initialized instance of the T2T component.
 * \param[in]       secNr               Sector number to be used (default sector is 0).
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T2TSectorSelect (	ptxNativeTag_T2T_t *t2tComp,
                                                            uint8_t secNr,
                                                            uint8_t *rx,
                                                            size_t *rxLen,
                                                            uint32_t msTimeout);

/**
 * \brief Uninitialize / Close the T2T Native Tag Component.
 *
 * \param[in]   t2tComp             Pointer to an initialized instance of the T2T component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T2TClose (ptxNativeTag_T2T_t *t2tComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

