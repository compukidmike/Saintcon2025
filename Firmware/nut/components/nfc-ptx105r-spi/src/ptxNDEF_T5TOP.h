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
    Module      : NDEF T5T OPERATION API
    File        : ptxNDEF_T5TOP.h

    Description : Tag Type 5 NDEF Operation API (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_T5T_op Tag Type T5T Operation API
 *
 * @{
 */

#ifndef APIS_PTX_NDEF_OP_T5T_H_
#define APIS_PTX_NDEF_OP_T5T_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxNativeTag_T5T.h"
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
 * \name T5T Tag specific definitions
 * @{
 */
#define PTX_T5T_DEFAULT_TIMEOUT_MS   (uint32_t)202 /**< T5T default Timeout in ms */
#define PTX_T5T_SUPPORTED_VERSION    (uint8_t)0x10 /**< T5T supported Version */
/** @} */


/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief T5T NDEF OP Initialization Parameters
 */
typedef struct ptxNDEF_T5TOP_InitParams
{
    ptxNativeTag_T5T_InitParams_t   T5TInitParams;              /**< Native Tag layer initialization parameters. */

    uint8_t                         *WorkBuffer;                /**< Internal Workbuffer. */
    uint32_t                        WorkBufferSize;             /**< Internal Workbuffer size. */
    uint8_t                         *RxBuffer;                  /**< Internal Rx-Buffer */
    uint32_t                        RxBufferSize;               /**< Size of internal Rx-Buffer */

} ptxNDEF_T5TOP_InitParams_t;

/**
 * \brief T5T NDEF OP Capability Container Parameters
 */
typedef struct ptxNDEF_T5TOP_CC
{
    uint8_t                         MagicNumber;                /**< Magic Number */
    uint8_t                         Version;                    /**< Version */
    uint8_t                         ReadAccess;                 /**< Read permission */
    uint8_t                         WriteAccess;                /**< Write permission */
    uint16_t                        MLEN;                       /**< Length */
    uint16_t                        ExtCommandTypeRequired;     /**< external command type required */
    uint8_t                         SpecialFrameRequired;       /**< special frame required */
    uint8_t                         MultiBlockReadSupported;    /**< Multi block read supported */
    uint8_t                         LockBlockSupported;         /**< Lock Block supported */
    uint8_t                         Size;                       /**< Size */

} ptxNDEF_T5TOP_CC_t;

/**
 * \brief T5T NDEF OP Component
 */
typedef struct ptxNDEF_T5TOP
{
    /* Components */
    ptxStatus_Comps_t               CompId;                 /**< Component Id */

    ptxNativeTag_T5T_t              NativeTagT5T;           /**< T5T Native Tag Component */
    ptxNDEF_TagLifeCycle_t          LifeCycle;              /**< Tag Life-Cycle */
    uint8_t                         *RxBuffer;              /**< Internal Rx-Buffer provided by upper layer (may be shared with other components) */
    uint32_t                        RxBufferSize;           /**< Internal Rx-Buffer size */
    uint8_t                         *WorkBuffer;            /**< Internal Work-Buffer */
    uint32_t                        WorkBufferSize;         /**< Internal Work-Buffer Size */

    /* Tag Type specific members */
    ptxNDEF_T5TOP_CC_t              CCParams;               /**< T5T specific CC parameters */

    /* General Info and / or Greedy Collectin related Parameters */
    uint8_t                         BlockSize;              /**< Block size */
    uint16_t                        NDEF_TLV_POS_BN;        /**< NDEF TLV POS BN */
    uint8_t                         NDEF_TLV_POS_BY;        /**< NDEF TLV POS BY */
    uint16_t                        NDEF_TLV_LENGTH;        /**< NDEF TLV length */

} ptxNDEF_T5TOP_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T5T OP Component.
 *
 * \param[in]   t5tOpComp           Pointer to an allocated instance of the T5T-OP component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpOpen (ptxNDEF_T5TOP_t *t5tOpComp, ptxNDEF_T5TOP_InitParams_t *initParams);

/**
 * \brief Formats a Type 5 Tag to INITIALIZED state.
 *
 * \param[in]   t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpFormatTag (ptxNDEF_T5TOP_t *t5tOpComp);

/**
 * \brief Checks if a NDEF-message is present on the given Tag (or not).
 *
 * \param[in]   t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpCheckMessage (ptxNDEF_T5TOP_t *t5tOpComp);

/**
 * \brief Reads a NDEF-message from a given Tag.
 *
 * \param[in]     t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 * \param[in]     msgBuffer           Pointer to buffer holding the read NDEF-message.
 * \param[in,out] msgLen              Size of the buffer (in), Length of the read NDEF-message (out).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpReadMessage (ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Writes a NDEF-message onto a given Tag.
 *
 * \param[in] t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 * \param[in] msgBuffer           Pointer to buffer holding the NDEF-message to write (NULL -> empty NDEF-message is written).
 * \param[in] msgLen              Size of NDEF-message (0 -> empty NDEF-message is written).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpWriteMessage (ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, uint32_t msgLen);

/**
 * \brief Puts a Tag into READ-ONLY state (Attention: This is a irreversible Operation!).
 *
 * \param[in]   t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T5TOpLockTag (ptxNDEF_T5TOP_t *t5tOpComp);

/**
 * \brief Unitialize / Close the T5T OP Component
 *
 * \param[in]   t5tOpComp           Pointer to an initialized instance of the T5T-OP component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNDEF_T5TOpClose (ptxNDEF_T5TOP_t *t5tOpComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

