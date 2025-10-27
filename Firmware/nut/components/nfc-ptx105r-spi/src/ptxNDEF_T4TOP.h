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
    Module      : NDEF T4T OPERATION API
    File        : ptxNDEF_T4TOP.h

    Description : Tag Type 4 NDEF Operation API (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_T4T_op Tag Type T4T Operation API
 *
 * @{
 */

#ifndef APIS_PTX_NDEF_OP_T4T_H_
#define APIS_PTX_NDEF_OP_T4T_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxNativeTag_T4T.h"
#include "ptxNDEF_Defines.h"
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
 * \name T4T Tag specific data length definitions.
 * @{
 */
#define PTX_T4T_DEFAULT_TIMEOUT_MS      (uint32_t)50000 /**< Default timeout value in ms. */
#define PTX_T4T_MAXIMUM_NLEN_LENGTH     (uint8_t)4u     /**< Maximum NLEN length. */
#define PTX_T4T_CC_LEN                  (uint8_t)2u     /**< CC length. */
#define PTX_T4T_FILEIDENTIFIER_LEN      (uint8_t)2u     /**< File Identifier length. */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief T4T NDEF OP Initialization Parameters
 */
typedef struct ptxNDEF_T4TOP_InitParams
{
    uint8_t                         *RxBuffer;     /**< Internal Rx-Buffer */
    uint32_t                        RxBufferSize;  /**< Size of Internal Rx-Buffer */
    ptxNativeTag_T4T_InitParams_t   T4TInitParams; /**< Native Tag initialization Parameters */
} ptxNDEF_T4TOP_InitParams_t;

/**
 * \brief T4T NDEF OP Capability Container Parameters
 */
typedef struct ptxNDEF_T4TOP_CC
{
    uint8_t                         CCLen[PTX_T4T_CC_LEN];  /**< Length of the CC file, 2 bytes */
    uint8_t                         MappingVersion;         /**< Mapping version info */
    uint8_t                         MappingMajor;           /**< Mapping Major version. */
    uint8_t                         MappingMinor;           /**< Mapping Minor version. */
    uint16_t                        MLeDigit;               /**< Digit containing maximum RAPDU size */
    uint16_t                        MLcDigit;               /**< Digit containing maximum CAPDU size */
    uint8_t                         NDEFTLV[10];            /**< Stores NDEF TLV, 8 bytes for mapping version 20h, 10 bytes for 30h, checked accordingly */
    uint8_t                         NDEFFileIdentifier[PTX_T4T_FILEIDENTIFIER_LEN];  /**< NDEF file identifier, 2 bytes */
    uint32_t                        NDEFFileSize;           /**< Size of the NDEF file in bytes, 2 bytes */
    uint8_t                         NDEFAccessRead;         /**< Access condition for read */
    uint8_t                         NDEFAccessWrite;        /**< Access condition for write */

} ptxNDEF_T4TOP_CC_t;

/**
 * \brief T4T NDEF file contents
 */
typedef struct ptxNDEF_T4TOP_NLEN
{
    uint8_t                         NLEN[PTX_T4T_MAXIMUM_NLEN_LENGTH];  /**< NLEN or ENLEN field */
    uint8_t                         NbrNLENBytes;           /**< Number of relevant (E)NLEN bytes, either 2 or 4 */
    uint32_t                        DigitNLEN;              /**< Digit representation of (E)NLEN */

} ptxNDEF_T4TOP_NLEN_t;

/**
 * \brief T4T NDEF OP Component
 */
typedef struct ptxNDEF_T4TOP
{
    /* Components */
    ptxStatus_Comps_t               CompId;                /**< Component Id */

    ptxNativeTag_T4T_t              NativeTagT4T;          /**< T4T Native Tag Component */
    ptxNDEF_TagLifeCycle_t          LifeCycle;             /**< Tag Life-Cycle */
    uint8_t                         *RxBuffer;             /**< Internal Rx-Buffer provided by upper layer (may be shared with other components) */
    uint32_t                        RxBufferSize;          /**< Internal Rx-Buffer size */

    /* Tag Type specific members */
    ptxNDEF_T4TOP_CC_t              CCParams;              /**< T4T specific CC parameters */

    ptxNDEF_T4TOP_NLEN_t            NLEN;                  /**< T4T NDEF file contents */

    /* General Info and / or Greedy Collecting related Parameters */

} ptxNDEF_T4TOP_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T4T OP Component.
 *
 * \param[in]   t4tOpComp           Pointer to an allocated instance of the T4T-OP component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpOpen (ptxNDEF_T4TOP_t *t4tOpComp, ptxNDEF_T4TOP_InitParams_t *initParams);

/**
 * \brief Formats a Type 4 Tag to INITIALIZED state.
 *
 * \param[in]   t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpFormatTag (ptxNDEF_T4TOP_t *t4tOpComp);

/**
 * \brief Checks if a NDEF-message is present on the given Tag (or not).
 *
 * \param[in]   t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpCheckMessage (ptxNDEF_T4TOP_t *t4tOpComp);

/**
 * \brief Reads a NDEF-message from a given Tag.
 *
 * \param[in]     t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 * \param[in]     msgBuffer           Pointer to buffer holding the read NDEF-message.
 * \param[in,out] msgLen              Size of the buffer (in), Length of the read NDEF-message (out).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpReadMessage (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Writes a NDEF-message onto a given Tag.
 *
 * \param[in] t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 * \param[in] msgBuffer           Pointer to buffer holding the NDEF-message to write (NULL -> empty NDEF-message is written).
 * \param[in] msgLen              Size of NDEF-message (0 -> empty NDEF-message is written).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpWriteMessage (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t msgLen);

/**
 * \brief Puts a Tag into READ-ONLY state (Attention: This is a irreversible Operation!).
 *
 * \param[in]   t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T4TOpLockTag (ptxNDEF_T4TOP_t *t4tOpComp);

/**
 * \brief Unitialize / Close the T4T OP Component
 *
 * \param[in]   t4tOpComp           Pointer to an initialized instance of the T4T-OP component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNDEF_T4TOpClose (ptxNDEF_T4TOP_t *t4tOpComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

