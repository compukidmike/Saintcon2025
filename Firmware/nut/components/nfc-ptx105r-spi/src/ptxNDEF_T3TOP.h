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
    Module      : NDEF T3T OPERATION API
    File        : ptxNDEF_T3TOP.h

    Description : Tag Type 3 NDEF Operation API (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_T3T_op Tag Type T3T Operation API
 *
 * @{
 */

#ifndef APIS_PTX_NDEF_OP_T3T_H_
#define APIS_PTX_NDEF_OP_T3T_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxNativeTag_T3T.h"
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
 * \name T3T Tag specific size definitions and limits.
 * @{
 */
#define PTX_T3T_BLOCK_SIZE           (uint8_t)16u   /**< T3T block size. */
#define PTX_T3T_RFU_SIZE             (uint8_t)4u    /**< Number of RFU bytes in Attribute Information Block. */
#define PTX_T3T_DEFAULT_TIMEOUT_MS   (uint32_t)500  /**< T3T Tag specific default timeout. */
#define PTX_T3T_SUPPORTED_VERSION    (uint8_t)0x10  /**< T3T Tag specific supported version. */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief T3T NDEF OP Initialization Parameters
 */
typedef struct ptxNDEF_T3TOP_InitParams
{
    uint8_t                         *RxBuffer;                      /**< Internal Rx-Buffer provided by upper layer */
    uint32_t                        RxBufferSize;                   /**< Size of Internal Rx-Buffer */
    ptxNativeTag_T3T_InitParams_t   T3TInitParams;                  /**< Native Tag Initialization Parameters. */

} ptxNDEF_T3TOP_InitParams_t;

/**
 * \brief T3T NDEF OP Capability Container Parameters
 */
typedef struct ptxNDEF_T3TOP_CC
{
    uint8_t                         AttributeInformationBlock[PTX_T3T_BLOCK_SIZE];  /**< contains full attribute information block */
    uint8_t                         Version;                        /**< version information, containing 4 bit major and 4 bit minor */
    uint8_t                         MajorVersion;                   /**< Version Information, major */
    uint8_t                         MinorVersion;                   /**< Version Information, minor */
    uint8_t                         Nbr;                            /**< maximum number of blocks that can be read with one check command */
    uint8_t                         Nbw;                            /**< maximum number of blocks that can be written with one update command */
    uint8_t                         NbrInt;                         /**< internal maximum number of blocks that can be read with one check command */
    uint8_t                         NbwInt;                         /**< internal maximum number of blocks that can be written with one update command */
    uint16_t                        NmaxB;                          /**< maximum number of blocks available for the NDEF message */
    uint8_t                         RFU[PTX_T3T_RFU_SIZE];          /**< to be ignored and not changed */
    uint8_t                         WriteFlag;                      /**< indicator whether a previous NDEF operation was finished */
    uint8_t                         RWFlag;                         /**< access condition whether the tag can be updated or not */
    uint32_t                        Ln;                             /**< size of actual stored NDEF data bytes */
    uint16_t                        Nbc;                            /**< number of NDEF data blocks */
    uint16_t                        Checksum;                       /**< helps check whether the attributes are correct */

} ptxNDEF_T3TOP_CC_t;

/**
 * \brief T3T NDEF OP Component
 */
typedef struct ptxNDEF_T3TOP
{
    /* Components */
    ptxStatus_Comps_t               CompId;                /**< Component Id */

    ptxNativeTag_T3T_t              NativeTagT3T;          /**< T3T Native Tag Component */
    ptxNDEF_TagLifeCycle_t          LifeCycle;             /**< Tag Life-Cycle */
    uint8_t                         *RxBuffer;             /**< Internal Rx-Buffer provided by upper layer (may be shared with other components) */
    uint32_t                        RxBufferSize;          /**< Internal Rx-Buffer size */
    uint8_t                         WorkBuffer[64];        /**< Internal Work-Buffer */
    ptxNDEF_T3TOP_CC_t              CCParams;              /**< T3T specific CC parameters */
    uint8_t                         *NFCID2;               /**< ID of the tag */
    uint8_t                         NFCID2Len;             /**< Length of the ID */
    uint8_t                         MRTI_Check;            /**< Value stored for calculation of timeout value */
    uint32_t                        MRTI_Update;           /**< Value stored for calculation of timeout value */
    ptxIoTRd_t                      *IotRd;                /**< IOT Reader provided by upper layer */
} ptxNDEF_T3TOP_t;


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T3T OP Component.
 *
 * \param[in]   t3tOpComp           Pointer to an allocated instance of the T3T-OP component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpOpen (ptxNDEF_T3TOP_t *t3tOpComp, ptxNDEF_T3TOP_InitParams_t *initParams);

/**
 * \brief Formats a Type 3 Tag to INITIALIZED state.
 *
 * \param[in]   t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpFormatTag (ptxNDEF_T3TOP_t *t3tOpComp);

/**
 * \brief Checks if a NDEF-message is present on the given Tag (or not).
 *
 * \param[in]   t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpCheckMessage (ptxNDEF_T3TOP_t *t3tOpComp);

/**
 * \brief Reads a NDEF-message from a given Tag.
 *
 * \param[in]     t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 * \param[in]     msgBuffer           Pointer to buffer holding the read NDEF-message.
 * \param[in,out] msgLen              Size of the buffer (in), Length of the read NDEF-message (out).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpReadMessage (ptxNDEF_T3TOP_t *t3tOpComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Writes a NDEF-message onto a given Tag.
 *
 * \param[in] t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 * \param[in] msgBuffer           Pointer to buffer holding the NDEF-message to write (NULL -> empty NDEF-message is written).
 * \param[in] msgLen              Size of NDEF-message (0 -> empty NDEF-message is written).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpWriteMessage (ptxNDEF_T3TOP_t *t3tOpComp, uint8_t *msgBuffer, uint32_t msgLen);

/**
 * \brief Puts a Tag into READ-ONLY state (Attention: This is a irreversible Operation!).
 *
 * \param[in]   t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T3TOpLockTag (ptxNDEF_T3TOP_t *t3tOpComp);

/**
 * \brief Unitialize / Close the T3T OP Component
 *
 * \param[in]   t3tOpComp           Pointer to an initialized instance of the T3T-OP component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNDEF_T3TOpClose (ptxNDEF_T3TOP_t *t3tOpComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

