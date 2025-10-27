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
    File        : ptxNativeTag_T3T.h

    Description : Native Tag API for NFC Forum Tag Type 3 (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_native_tag_T3T Native Tag T3T API
 *
 * @{
 */

#ifndef APIS_PTX_NATIVE_TAG_T3T_H_
#define APIS_PTX_NATIVE_TAG_T3T_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxStatus.h"
#include "ptx_IOT_READER.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \name T3T Tag specific definitions
 * @{
 */
#define PTX_T3T_MIN_TX_BUFFER_SIZE   (uint32_t)256          /**< T3T minimum Tx buffer size */
#define PTX_T3T_NFCID2_SIZE          (uint8_t)8             /**< T3T NFCID2 size */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */
/**
 * \brief MRTI parameters.
 */
typedef struct ptxNativeTag_T3T_MRTI
{
    uint8_t     MRTICheck;          /**< MRTI for Check  operations. */
    uint8_t     MRTIUpdate;         /**< MRTI for Update operations. */
} ptxNativeTag_T3T_MRTI_t;

/**
 * \brief Service Code List parameters.
 */
typedef struct ptxNativeTag_T3T_Services
{
    uint8_t     NOS;                /**< Number Of Services. */
    uint8_t     ServiceCodeListLen; /**< Service Code List length. */
    uint8_t     *ServiceCodeList;   /**< Service Code List. */
} ptxNativeTag_T3T_Services_t;

/**
 * \brief Block List parameters.
 */
typedef struct ptxNativeTag_T3T_Blocks
{
    uint8_t     NOB;                /**< Number Of Blocks. */
    uint8_t     BlockListLen;       /**< Block List length. */
    uint8_t     *BlockList;         /**< Block List. */
} ptxNativeTag_T3T_Blocks_t;

/**
 * \brief T3T Native Tag Initialization Parameters
 */
typedef struct ptxNativeTag_T3T_InitParams
{
    ptxIoTRd_t          *IotRd;                 /**< IOT Reader provided by upper layer */
    uint8_t             *TxBuffer;              /**< Internal Tx-Buffer provided by upper layer (may be shared with other components) */
    uint32_t            TxBufferSize;           /**< Size of TX Buffer */
    uint8_t             *NFCID2;                /**< ID of the tag */
    uint8_t             NFCID2Len;              /**< Length of the ID */
    uint8_t             MRTI_Check;             /**< Value stored for calculation of timeout value */
    uint32_t            MRTI_Update;            /**< Value stored for calculation of timeout value */

} ptxNativeTag_T3T_InitParams_t;

/**
 * \brief T3T Native Tag Component
 */
typedef struct ptxNativeTag_T3T
{
    /* Components */
    ptxStatus_Comps_t    CompId;                /**< Component Id */

    ptxIoTRd_t          *IotRd;                 /**< IOT Reader provided by upper layer */
    uint8_t             *TxBuffer;              /**< Internal Tx-Buffer provided by upper layer (may be shared with other components) */
    uint8_t             *NFCID2;                /**< ID of the tag */
    uint8_t             NFCID2Len;              /**< Length of the ID */
    uint8_t             MRTI_Check;             /**< Value stored for calculation of timeout value */
    uint32_t            MRTI_Update;            /**< Value stored for calculation of timeout value */
    uint32_t            TagTimeoutCheck;        /**< Timeout value of the tag for check operation */
    uint32_t            TagTimeoutUpdate;       /**< Timeout value of the tag for update operation*/
} ptxNativeTag_T3T_t;
/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */


/**
 * \brief Component Initialization.
 *
 * \param[in]       t3tComp         Pointer to an allocated instance of the T3T component.
 * \param[in]       initParams      Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T3TOpen (ptxNativeTag_T3T_t *t3tComp, ptxNativeTag_T3T_InitParams_t *initParams);

/**
 * \brief Component De-Initialization.
 *
 * \param[in]       t3tComp         Pointer to an initialized instance of the T3T component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T3TClose (ptxNativeTag_T3T_t *t3tComp);


/**
 * \brief T3T SENSF_REQ Command, Detect Tag and NDEF Support.
 *
 * \param[in]       t3tComp             Pointer to an initialized instance of the T3T component.
 * \param[in]       sc                  System code of the NFC forum device to be polled for.
 * \param[in]       rc                  Request code for additional information in the SENSF_RES.
 * \param[in]       tsn                 Time slot number, used for collision resolution.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T3TSENSF_REQ (ptxNativeTag_T3T_t *t3tComp,
                                       uint16_t sc,
                                       uint8_t rc,
                                       uint8_t tsn,
                                       uint8_t *rx,
                                       size_t *rxLen,
                                       uint32_t msTimeout);

/**
 * \brief T3T Check Command.
 *
 * \param[in]       t3tComp             Pointer to an initialized instance of the T3T component.
 * \param[in]       NFCID2              ID to address the T3T tag.
 * \param[in]       NFCID2Len           Length of ID.
 * \param[in]       serviceInfo         Information about Services to be updated.
 * \param[in]       blockInfo           information about Blocks to be updated.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNativeTag_T3TCheck (ptxNativeTag_T3T_t *t3tComp,
                                   uint8_t *NFCID2,
                                   size_t NFCID2Len,
                                   ptxNativeTag_T3T_Services_t serviceInfo,
                                   ptxNativeTag_T3T_Blocks_t blockInfo,
                                   uint8_t *rx,
                                   size_t *rxLen,
                                   uint32_t msTimeout);

/**
 * \brief T3T Update Command.
 *
 * \param[in]       t3tComp             Pointer to an initialized instance of the T3T component.
 * \param[in]       NFCID2              ID to address the T3T tag.
 * \param[in]       NFCID2Len           Length of ID.
 * \param[in]       serviceInfo         Information about Services to be updated.
 * \param[in]       blockInfo           Information about Blocks to be updated.
 * \param[in]       blockData           Data to be written to the blocks.
 * \param[in]       blockDataLen        Length of the data to be written to the blocks.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 *
 */
ptxStatus_t ptxNativeTag_T3TUpdate (ptxNativeTag_T3T_t *t3tComp,
                                    uint8_t *NFCID2,
                                    size_t NFCID2Len,
                                    ptxNativeTag_T3T_Services_t serviceInfo,
                                    ptxNativeTag_T3T_Blocks_t blockInfo,
                                    uint8_t *blockData,
                                    uint8_t blockDataLen,
                                    uint8_t *rx,
                                    size_t *rxLen,
                                    uint32_t msTimeout);

/**
 * \brief Sets / Updates the NFCID2 to be used.
 *
 * \param[in]       t3tComp             Pointer to an initialized instance of the T3T component.
 * \param[in]       NFCID2              Pointer to NFCID2 of the given Tag.
 * \param[in]       NFCID2Len           Length of NFCID2.
 * \param[in]       mrtiInfo            Information about MRTI Times of the T3T.
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T3TSetTagParams (ptxNativeTag_T3T_t *t3tComp,
                                          uint8_t *NFCID2,
                                          uint8_t NFCID2Len,
                                          ptxNativeTag_T3T_MRTI_t mrtiInfo);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

