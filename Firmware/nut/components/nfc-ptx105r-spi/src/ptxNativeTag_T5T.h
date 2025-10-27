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
    File        : ptxNativeTag_T5T.h

    Description : Native Tag API for NFC Forum Tag Type 5 (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_native_tag_T5T Native Tag T5T API
 *
 * @{
 */

#ifndef APIS_PTX_NATIVE_TAG_T5T_H_
#define APIS_PTX_NATIVE_TAG_T5T_H_

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
 * \name T5T Tag specific definitions.
 * @{
 */
#define PTX_T5T_UID_SIZE             (uint8_t)8         /**< T5T specific UID Size */
#define PTX_T5T_MIN_TX_BUFFER_SIZE   (uint32_t)64       /**< T5T specific Minimum Tx Buffer size */
#define PTX_T5T_MAX_NR_RETRIES       (uint8_t)5         /**< T5T specific Retry Count upon timeout errors for read and write commands. */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief T5T Native Tag Initialization Parameters
 */
typedef struct ptxNativeTag_T5T_InitParams
{
    void                *IotRd;                 /**< IOT Reader provided by upper layer */
    uint8_t             *TxBuffer;              /**< Internal Tx-Buffer provided by upper layer */
    uint32_t            TxBufferSize;           /**< Size of Internal TX-Buffer */
    uint8_t             *UID;                   /**< UID */
    uint8_t             UIDLen;                 /**< Length of UID */
    uint8_t             isSelected;             /**< Flag indicating if commands are to be sent in SELECTED mode (0 unselected, 1 selected) */

} ptxNativeTag_T5T_InitParams_t;

/**
 * \brief T5T Native Tag Component
 */
typedef struct ptxNativeTag_T5T
{
    /* Components */
    ptxStatus_Comps_t    CompId;                /**< Component Id */

    void                *IotRd;                 /**< IOT Reader provided by upper layer */
    uint8_t             *TxBuffer;              /**< Internal Tx-Buffer provided by upper layer (may be shared with other components) */

    uint8_t             *UID;                   /**< UID */
    size_t              UIDLen;                 /**< UID Length */

    uint8_t             isSelected;             /**< Flag indicating if commands are to be sent in SELECTED mode (0 unselected, 1 selected) */

} ptxNativeTag_T5T_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T5T Native Tag Component.
 *
 * \param[in]   t5tComp             Pointer to an allocated instance of the T5T component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNativeTag_T5TOpen (ptxNativeTag_T5T_t *t5tComp, ptxNativeTag_T5T_InitParams_t *initParams);

/**
 * \brief Performs a READ_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       blockNr             Number of block to read.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TReadSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                             uint8_t optionFlag,
                                                             uint8_t blockNr,
                                                             uint8_t *rx,
                                                             size_t *rxLen,
                                                             uint32_t msTimeout);

/**
 * \brief Performs a WRITE_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Use Special-Framing"-format for Write-alike commands).
 * \param[in]       blockNr             Number of block to write.
 * \param[in]       blockData           Data to write.
 * \param[in]       blockDataLen        Length of data to write.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TWriteSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                             uint8_t optionFlag,
                                                             uint8_t blockNr,
                                                             uint8_t *blockData,
                                                             uint8_t blockDataLen,
                                                             uint8_t *rx,
                                                             size_t *rxLen,
                                                             uint32_t msTimeout);

/**
 * \brief Performs a LOCK_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Use Special-Framing"-format for Write-alike commands).
 * \param[in]       blockNr             Number of block to lock.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           pplication-timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TLockSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                             uint8_t optionFlag,
                                                             uint8_t blockNr,
                                                             uint8_t *rx,
                                                             size_t *rxLen,
                                                             uint32_t msTimeout);

/**
 * \brief Performs a READ_MUTLIPLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       blockNr             Number of start-block.
 * \param[in]       nrBlocks            Number of blocks to read.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TReadMultipleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                             uint8_t optionFlag,
                                                             uint8_t blockNr,
                                                             uint8_t nrBlocks,
                                                             uint8_t *rx,
                                                             size_t *rxLen,
                                                             uint32_t msTimeout);

/**
 * \brief Performs a EXTENDED_READ_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       blockNr             Number of block to read.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TExtReadSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                                uint8_t optionFlag,
                                                                uint16_t blockNr,
                                                                uint8_t *rx,
                                                                size_t *rxLen,
                                                                uint32_t msTimeout);

/**
 * \brief Performs a EXTENDED_WRITE_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Use Special-Framing"-format for Write-alike commands).
 * \param[in]       blockNr             Number of block to write.
 * \param[in]       blockData           Data to write.
 * \param[in]       blockDataLen        Length of data to write.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TExtWriteSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                                 uint8_t optionFlag,
                                                                 uint16_t blockNr,
                                                                 uint8_t *blockData,
                                                                 uint8_t blockDataLen,
                                                                 uint8_t *rx,
                                                                 size_t *rxLen,
                                                                 uint32_t msTimeout);

/**
 * \brief Performs a EXTENDED_LOCK_SINGLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Use Special-Framing"-format for Write-alike commands).
 * \param[in]       blockNr             Number of block to lock.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TExtLockSingleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                                uint8_t optionFlag,
                                                                uint16_t blockNr,
                                                                uint8_t *rx,
                                                                size_t *rxLen,
                                                                uint32_t msTimeout);

/**
 * \brief Performs a EXTENDED_READ_MUTLIPLE_BLOCK_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       blockNr             Number of start-block.
 * \param[in]       nrBlocks            Number of blocks to read.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TExtReadMultipleBlock (ptxNativeTag_T5T_t *t5tComp,
                                                                  uint8_t optionFlag,
                                                                  uint16_t blockNr,
                                                                  uint16_t nrBlocks,
                                                                  uint8_t *rx,
                                                                  size_t *rxLen,
                                                                  uint32_t msTimeout);

/**
 * \brief Performs a SELECT_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       uid                 Pointer to UID of the given Tag.
 * \param[in]       uidLen              Length of UID.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TSelect (ptxNativeTag_T5T_t *t5tComp,
                                                    uint8_t optionFlag,
                                                    uint8_t *uid,
                                                    uint8_t uidLen,
                                                    uint8_t *rx,
                                                    size_t *rxLen,
                                                    uint32_t msTimeout);


/**
 * \brief Performs a SELECT_REQ-command.
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       optionFlag          Option-flag provided by upper layer ("Request Block Security Status" for Read-alike commands).
 * \param[in]       uid                 Pointer to UID of the given Tag.
 * \param[in]       uidLen              Length of UID.
 * \param[out]      rx                  Pointer to Rx-buffer where the received data from the card shall be stored.
 * \param[in,out]   rxLen               Size of Rx-buffer (in), Length of the received data (out).
 * \param[in]       msTimeout           Timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TSleep (ptxNativeTag_T5T_t *t5tComp,
                                                                uint8_t optionFlag,
                                                                uint8_t *uid,
                                                                uint8_t uidLen,
                                                                uint8_t *rx,
                                                                size_t *rxLen,
                                                                uint32_t msTimeout);

/**
 * \brief Sets / Updates the UID to be used (needs to be called after "ptxNativeTag_T5TSelect() is called and addressed-mode shall be used").
 *
 * \param[in]       t5tComp             Pointer to an initialized instance of the T5T component.
 * \param[in]       uid                 Pointer to UID of the given Tag.
 * \param[in]       uidLen              Length of UID.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TSetUID (ptxNativeTag_T5T_t *t5tComp,
                                                                uint8_t *uid,
                                                                uint8_t uidLen);

/**
 * \brief Unitialize / Close the T5T Native Tag Component.
 *
 * \param[in]   t5tComp             Pointer to an initialized instance of the T5T component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNativeTag_T5TClose (ptxNativeTag_T5T_t *t5tComp);


#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

