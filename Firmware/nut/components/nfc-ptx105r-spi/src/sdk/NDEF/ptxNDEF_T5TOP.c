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
    File        : ptxNativeTag_T5T.c

    Description : Native Tag API for NFC Forum Tag Type 5 (IOT READER - Extension)
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T5TOP.h"
#include "ptxNativeTag_T5T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \name CC_Parameter Positions and Offsets
 * @{
 */
#define PTX_T5TOP_CC_BLOCK_NUMBER                           (uint16_t)0x0000  /**< CC block number. */
#define PTX_T5TOP_CC_OFFSET_MAGIC_NUMBER                    (uint8_t)0x00     /**< CC block offset for Magic Number. */
#define PTX_T5TOP_CC_OFFSET_VERSION_ACCESS                  (uint8_t)0x01     /**< CC block offset for version and access conditions. */
#define PTX_T5TOP_CC_OFFSET_MLEN_BASIC                      (uint8_t)0x02     /**< CC blocm offset for MLEN. */
#define PTX_T5TOP_CC_OFFSET_MLEN_EXTENDED                   (uint8_t)0x06     /**< CC block offset for extended MLEN. */
#define PTX_T5TOP_CC_OFFSET_ADDITIONAL_FEATURE              (uint8_t)0x03     /**< CC blocm offset for additional feature information. */
/** @} */

/**
 * \name CC-Parameters and Masks
 * @{
 */
#define PTX_T5TOP_CC_ADD_INFO_SPECIAL_FRAME_MASK            (uint8_t)0x10     /**< Mask for additional information byte containing Special Frame features. */
#define PTX_T5TOP_CC_ADD_INFO_LOCK_BLOCK_MASK               (uint8_t)0x08     /**< Mask for additional information byte containing block locking features. */
#define PTX_T5TOP_CC_ADD_INFO_MBREAD_MASK                   (uint8_t)0x01     /**< Mask for additional information byte containing multiple block read features. */
#define PTX_T5TOP_CC_MAGIC_NUMBER_E1                        (uint8_t)0xE1     /**< Magic Number E1h. */
#define PTX_T5TOP_CC_MAGIC_NUMBER_E2                        (uint8_t)0xE2     /**< Magic number E2h. */
#define PTX_T5TOP_CC_VERSION_MASK                           (uint8_t)0xF0     /**< Mask for version information. */
#define PTX_T5TOP_CC_READ_ACCESS_MASK                       (uint8_t)0x0C     /**< Mask for read access condition. */
#define PTX_T5TOP_CC_WRITE_ACCESS_MASK                      (uint8_t)0x03     /**< Mask for write access condition. */
/** @} */

/**
 * \name Response Frame Offsets and Masks
 * @{
 */
#define PTX_T5TOP_OFFSET_RES_FLAG                           (uint8_t)0x00     /**< Result flag offset. */
#define PTX_T5TOP_OFFSET_RES_DATA                           (uint8_t)0x01     /**< Result data offset fixed as 1 since "Block Security Status"-byte is never requested */
#define PTX_T5TOP_RES_FLAG_ERROR_MASK                       (uint8_t)0x01     /**< Result flag error mask. */
/** @} */

/**
 * \name TLV Parameters
 * @{
 */
#define PTX_T5TOP_NDEF_MESSAGE_TLV_T                        (uint8_t)0x03     /**< NDEF TLV T-value. */
#define PTX_T5TOP_TERMINATOR_TLV_T                          (uint8_t)0xFE     /**< Terminator TLV T-value. */
#define PTX_T5TOP_TLVHEADER_SHORT                           (uint8_t)2u       /**< NDEF TLV header length short. */
#define PTX_T5TOP_TLVHEADER_LONG                            (uint8_t)4u       /**< NDEF TLV header length long. */
#define PTX_T5TOP_TLV_LENGTHTHRESHOLD                       (uint8_t)0xFF     /**< Length threshold for longer NDEF TLV header. */
/** @} */

/**
 * \name Read Operation Flags
 * @{
 */
#define PTX_T5TOP_2ND_READ_OP_NOT_REQUIRED                  (uint8_t)0x00     /**< Internal flag for further reads in NDEF length detection, no extra read. */
#define PTX_T5TOP_2ND_READ_OP_GET_FULL_LENGTH               (uint8_t)0x01     /**< Internal flag for further reads in NDEF length detection, extra read to get 1 byte length. */
#define PTX_T5TOP_2ND_READ_OP_3BL_GET_PART_LENGTH           (uint8_t)0x02     /**< Internal flag for further reads in NDEF length detection, extra read to get part of 3 byte length. */
#define PTX_T5TOP_2ND_READ_OP_3BL_GET_FULL_LENGTH           (uint8_t)0x03     /**< Internal flag for further reads in NDEF length detection, extra read to get full 3 byte length. */
/** @} */

/**
 * \name Operation Specific Lengths
 * @{
 */
#define PTX_T5TOP_INTERNAL_BUFLENGTH                        (uint8_t)64u      /**< Internal work buffer length. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Get CC Information.
 *
 * \param[in] t5t           Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TGetCCInfo (ptxNDEF_T5TOP_t *t5tOpComp);

/**
 * \brief Handle CC Information.
 *
 * \param[in]  t5t           Pointer to component.
 * \param[out] ndefTLVFound  Flag whether an NDEF TLV has been found.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TOpHandleCCInfo(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *ndefTLVFound);

/**
 * \brief Read a single Block of Data.
 *
 * \param[in] t5t               Pointer to component.
 * \param[in] blockNumber       Block Number to read from.
 * \param[in,out] rx            Rx Data Storage.
 * \param[in,out] rxLen         Maximum Rx Length on input, Rx Data Length on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TOpReadBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *rx, size_t *rxLen);

/**
 * \brief Write a single block of Data.
 *
 * \param[in] t5t               Pointer to component.
 * \param[in] blockNumber       Block Number to read from.
 * \param[in] blockData         Data to be written.
 * \param[in] blockDataLen      Length of Data to be written.
 * \param[in,out] rx            Rx Data Storage.
 * \param[in,out] rxLen         Maximum Rx Length on input, Rx Data Length on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TOpWriteBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *blockData, uint8_t blockDataLen, uint8_t *rx, size_t *rxLen);
/**
 * \brief Lock a single Block.
 *
 * \param[in] t5t               Pointer to component.
 * \param[in] blockNumber       Block Number to lock.
 * \param[in,out] rx            Rx Data Storage.
 * \param[in,out] rxLen         Maximum Rx Length on input, Rx Data Length on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TOpLockBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *rx, size_t *rxLen);

/**
 * \brief Update L-Field of NDEF TLV.
 *
 * \param[in] t5t               Pointer to component.
 * \param[in] messageLen        Message Length.
 * \param[in] lengthSize        Size of L-Field.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TOpUpdateLength (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t messageLen, uint8_t lengthSize);

/**
 * \brief Read NDEF Message via Single Bock Read Operations.
 *
 * \param[in] t5tOpComp         Pointer to component.
 * \param[in,out] msgBuffer     Buffer to store Message.
 * \param[in,out] msgLen        Max Length of Message on input, Length of Message on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TReadNDEFSingleBlocks(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, size_t *msgLen);

/**
 * \brief Read NDEF Message via Multiple Bock Read Operations.
 *
 * \param[in] t5tOpComp         Pointer to component.
 * \param[in,out] msgBuffer     Buffer to store Message.
 * \param[in,out] msgLen        Max Length of Message on input, Length of Message on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5TReadNDEFMultipleBlocks(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, size_t *msgLen);

/**
 * \brief Write NDEF Message Loop.
 *
 * \param[in] t5tOpComp         Pointer to component.
 * \param[in] msgLen            Length of Message.
 * \param[in] msgBuffer         Message Buffer.
 * \param[in] offset            Offset within first block.
 * \param[in] currentBlock      Position on tag to start at.
 * \param[in] tlvHeaderLen      Size of NDEF TLV Header.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5T_WriteMessageLoop(ptxNDEF_T5TOP_t *t5tOpComp, uint32_t msgLen, uint8_t *msgBuffer, uint8_t offset, uint16_t currentBlock, uint8_t tlvHeaderLen);

/**
 * \brief Lock Blocks Loop.
 *
 * \param[in] t5tOpComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5T_LockLoop(ptxNDEF_T5TOP_t *t5tOpComp);

/**
 * \brief Look for TLVs in current data.
 *
 * \param[in] t5tOpComp             Pointer to component.
 * \param[in, out] currentByteNr    Current Block Position on Tag.
 * \param[in] data                  Data read from Tag, one Block.
 * \param[in] dataLen               Length of Data read from Tag.
 *
 * \param[out] nefTlvFound          Flag whether NDEF TLV has been found.
 * \param[out] terminatorFound      Flag whether Terminator TLV has been found.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T5T_ParseTLV(ptxNDEF_T5TOP_t *t5tOpComp, uint16_t *currentByteNr, uint8_t *data, size_t dataLen, uint8_t *ndefTlvFound, uint8_t *terminatorFound);

/**
 * \brief MTU finder (in units of Blocks)
 *
 * \param[in] t5tOpComp             Pointer to component.
 * \param[in] nbrBlocks             Number of blocks you want to handle
 *
 * \return Maximum number of blocks to be handled.
 */
static uint16_t ptxNDEF_T5T_SetMTU (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t nbrBlocks);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNDEF_T5TOpOpen (ptxNDEF_T5TOP_t *t5tOpComp, ptxNDEF_T5TOP_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != t5tOpComp) && (NULL != initParams))
    {
        if ((NULL != initParams->RxBuffer) &&
                (0 != initParams->RxBufferSize) &&
                (NULL != initParams->WorkBuffer) &&
                (0 != initParams->WorkBufferSize))
        {
            /* clear component */
            (void)memset(t5tOpComp, 0, sizeof(ptxNDEF_T5TOP_t));

            t5tOpComp->RxBuffer = initParams->RxBuffer;
            t5tOpComp->RxBufferSize = initParams->RxBufferSize;
            t5tOpComp->WorkBuffer = initParams->WorkBuffer;
            t5tOpComp->WorkBufferSize = initParams->WorkBufferSize;
            t5tOpComp->LifeCycle = TagLC_NoNDEFTag;

            status = ptxNativeTag_T5TOpen(&t5tOpComp->NativeTagT5T, &initParams->T5TInitParams);

            /* set Component-ID at the end to prevent futher calls in case of an error */
            if (ptxStatus_Success == status)
            {
                t5tOpComp->CompId = ptxStatus_Comp_T5TOP;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpFormatTag (ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        /* set UID */
        /*
        status = ptxNativeTag_T5TSetUID(&t5tOpComp->NativeTagT5T,
                                        &t5tOpComp->CardRegistry->ActiveCard->TechParams.CardVParams.UID[0],
                                        PTX_T5T_UID_SIZE);
         */

        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_NotImplemented);

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpCheckMessage (ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t ndef_tlv_found = 0;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        /* set UID */
        status = ptxNativeTag_T5TSetUID(&t5tOpComp->NativeTagT5T,
                                        &t5tOpComp->NativeTagT5T.UID[0],
                                        (uint8_t)t5tOpComp->NativeTagT5T.UIDLen);

        if (ptxStatus_Success == status)
        {
            /* read CC-parameters */
            status = ptxNDEF_T5TGetCCInfo (t5tOpComp);
        }

        if (ptxStatus_Success == status)
        {
            status = ptxNDEF_T5TOpHandleCCInfo(t5tOpComp, &ndef_tlv_found);
        }

        /* update Life-Cycle of Tag */
        if (ptxStatus_Success == status)
        {
            if (0 != ndef_tlv_found)
            {
                if (0 != t5tOpComp->CCParams.WriteAccess)
                {
                    t5tOpComp->LifeCycle = (0 != t5tOpComp->NDEF_TLV_LENGTH) ? TagLC_ReadWrite : TagLC_Initialized;
                }
                else
                {
                    t5tOpComp->LifeCycle = TagLC_ReadOnly;
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ResourceNotFound);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpReadMessage (ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, uint32_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t msg_buffer_size;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_buffer_size = *msgLen;

        if (0 != msg_buffer_size)
        {
            switch (t5tOpComp->LifeCycle)
            {
                case TagLC_Initialized:
                case TagLC_ReadWrite:
                case TagLC_ReadOnly:
                    /* OK */
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidState);
                    break;
            }

            if (ptxStatus_Success == status)
            {
                /* set UID */
                status = ptxNativeTag_T5TSetUID(&t5tOpComp->NativeTagT5T,
                                                &t5tOpComp->NativeTagT5T.UID[0],
                                                (uint8_t)t5tOpComp->NativeTagT5T.UIDLen);
            }

            if (ptxStatus_Success == status)
            {
                if (0 != t5tOpComp->CCParams.MultiBlockReadSupported)
                {
                    status = ptxNDEF_T5TReadNDEFMultipleBlocks(t5tOpComp, msgBuffer, &msg_buffer_size);
                }
                else
                {
                    status = ptxNDEF_T5TReadNDEFSingleBlocks(t5tOpComp, msgBuffer, &msg_buffer_size);
                }
            }

            /* everything correctly processed ? */
            if ((ptxStatus_Success == status) && (msg_buffer_size == t5tOpComp->NDEF_TLV_LENGTH))
            {
                *msgLen = (uint32_t)(t5tOpComp->NDEF_TLV_LENGTH);
            }
            else
            {
                *msgLen = 0;
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpWriteMessage (ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, uint32_t msgLen)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t length_size;
    uint16_t current_block_nr = 0;
    size_t rx_len;
    uint16_t mlen_loss = 0;
    uint8_t ndef_tlv_header_size;
    uint8_t init_block_write_offset = 0;
    uint16_t msg_len;
    uint8_t *msg_buffer;
    uint8_t EMPTY_NDEF_MESSAGE[] = {0xD0, 0x00, 0x00};

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        switch (t5tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            case TagLC_ReadWrite:
                /* OK */
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidState);
                break;
        }

        if (ptxStatus_Success == status)
        {
            /* set UID */
            status = ptxNativeTag_T5TSetUID(&t5tOpComp->NativeTagT5T,
                                            &t5tOpComp->NativeTagT5T.UID[0],
                                            (uint8_t)t5tOpComp->NativeTagT5T.UIDLen);
        }

        if (ptxStatus_Success == status)
        {
            /* determine NDEF-message to write */
            if ((NULL != msgBuffer) && (0 != msgLen))
            {
                msg_buffer = msgBuffer;
                msg_len = (uint16_t)msgLen;

            }
            else
            {
                msg_buffer = &EMPTY_NDEF_MESSAGE[0];
                msg_len = (uint16_t)sizeof(EMPTY_NDEF_MESSAGE);
            }

            length_size = (msg_len <= ((uint32_t)0xFE)) ? (uint8_t)0x01 : (uint8_t)0x03;
            ndef_tlv_header_size = (uint8_t)(length_size + 1);

            mlen_loss = (uint16_t)(ndef_tlv_header_size + t5tOpComp->NDEF_TLV_POS_BY);

            /* does NDEF-message fit at all onto the Tag ? */
            if (msg_len <= (uint32_t)(t5tOpComp->CCParams.MLEN - mlen_loss - ((t5tOpComp->BlockSize * t5tOpComp->NDEF_TLV_POS_BN) - t5tOpComp->CCParams.Size)))
            {
                /* reset L-field */
                status = ptxNDEF_T5TOpUpdateLength (t5tOpComp, 0, length_size);

                if (ptxStatus_Success == status)
                {
                    /*
                     * determine block-adress to start writing (NDEF_TLV_POS_BN defines the block where the NDEF-TLV starts,
                     * but not the actual NDEF-message, which could be stored in the next block;
                     */
                    current_block_nr = t5tOpComp->NDEF_TLV_POS_BN;
                    if ((uint8_t)((t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size)) < t5tOpComp->BlockSize)
                    {
                        init_block_write_offset = (uint8_t)(t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size);

                    }
                    else
                    {
                        init_block_write_offset = (uint8_t)((t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size) % t5tOpComp->BlockSize);
                        current_block_nr++;
                    }

                    /* if the offset != 0, we have to read the current block first */
                    if (0 != init_block_write_offset)
                    {
                        /* read block */
                        status = ptxNDEF_T5TOpReadBlock(t5tOpComp, current_block_nr, &t5tOpComp->RxBuffer[0], &rx_len);

                        if ((ptxStatus_Success == status) && (rx_len > 1) && ((rx_len - 1) <= t5tOpComp->WorkBufferSize))
                        {
                            (void)memcpy(&t5tOpComp->WorkBuffer[0], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], (uint32_t)(init_block_write_offset));
                        }
                        else
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                        }
                    }
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T5T_WriteMessageLoop(t5tOpComp, msg_len, msg_buffer, init_block_write_offset, current_block_nr, ndef_tlv_header_size);
            }

            if (ptxStatus_Success == status)
            {
                /* set intended size of L-field */
                status = ptxNDEF_T5TOpUpdateLength (t5tOpComp, msg_len, length_size);
            }
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpLockTag (ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t rx_len;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        switch (t5tOpComp->LifeCycle)
        {
            case TagLC_ReadWrite:
                /* transition to READ-ONLY i.e. locked-state can only be done if the Tag supports the Lock-commands */
                if (0 == t5tOpComp->CCParams.LockBlockSupported)
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_NotPermitted);
                }
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidState);
                break;
        }

        if (ptxStatus_Success == status)
        {
            /* set UID */
            status = ptxNativeTag_T5TSetUID(&t5tOpComp->NativeTagT5T,
                                            &t5tOpComp->NativeTagT5T.UID[0],
                                            (uint8_t)t5tOpComp->NativeTagT5T.UIDLen);
        }

        if (ptxStatus_Success == status)
        {
            /* clear write-access by reading block 0 (CC) and update the "Version & Access-Condition"-field at offset 2  */
            status = ptxNDEF_T5TOpReadBlock(t5tOpComp, 0, &t5tOpComp->RxBuffer[0], &rx_len);

            if (ptxStatus_Success == status)
            {
                /* skip status-flag at offset 0 */
                rx_len--;

                if (rx_len <=  t5tOpComp->WorkBufferSize)
                {
                    (void)memcpy(&t5tOpComp->WorkBuffer[0], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], (uint32_t)(rx_len));
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                }
                if (ptxStatus_Success == status)
                {
                    /* clear write-access bits */
                    t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_VERSION_ACCESS] = (uint8_t)(t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_VERSION_ACCESS] | PTX_T5TOP_CC_WRITE_ACCESS_MASK);

                    /* update block 0 */
                    status = ptxNDEF_T5TOpWriteBlock(t5tOpComp, 0, &t5tOpComp->WorkBuffer[0], t5tOpComp->BlockSize, &t5tOpComp->RxBuffer[0], &rx_len);
                }
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T5T_LockLoop(t5tOpComp);
            }
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T5TOpClose (ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        status = ptxNativeTag_T5TClose(&t5tOpComp->NativeTagT5T);

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNDEF_T5TGetCCInfo (ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t rx_len;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        /* reset current CC-parameters */
        (void)memset(&t5tOpComp->CCParams, 0, sizeof(ptxNDEF_T5TOP_CC_t));

        /* read CC-block */
        status = ptxNDEF_T5TOpReadBlock(t5tOpComp, PTX_T5TOP_CC_BLOCK_NUMBER, &t5tOpComp->RxBuffer[0], &rx_len);

        if (ptxStatus_Success == status)
        {
            t5tOpComp->BlockSize = (uint8_t)(rx_len - 1);

            switch (t5tOpComp->BlockSize)
            {
                case (uint8_t)4:
                case (uint8_t)8:
                case (uint8_t)16:
                case (uint8_t)32:
                    /* everything OK */
                    break;

                default:
                    t5tOpComp->LifeCycle = TagLC_NoNDEFTag;
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                    break;
            }

            if (ptxStatus_Success == status)
            {
                /*
                 * Note: The CC consists either of
                 *       - 4 bytes (MLEN-parameter at offset 3 != 0) or
                 *       - 8 bytes (MLEN-parameter at offset 3 == 0).
                 *
                 *       If the block-size is set to 4 but the CC consists of 8 byte, a 2nd read-operation is required.
                 */

                /* copy first BlockSize * bytes of CC as they're valid in any case */
                if (t5tOpComp->BlockSize <= t5tOpComp->WorkBufferSize)
                {
                    (void)memcpy(&t5tOpComp->WorkBuffer[0], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], (uint32_t)t5tOpComp->BlockSize);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                }
                if (ptxStatus_Success == status)
                {
                    t5tOpComp->CCParams.Size = (uint8_t)4u;

                    /* 2nd read-operation required ? */
                    if (0 == t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MLEN_BASIC])
                    {
                        t5tOpComp->CCParams.Size = (uint8_t)8u;

                        if ((uint8_t)4 == t5tOpComp->BlockSize)
                        {
                            status = ptxNDEF_T5TOpReadBlock(t5tOpComp, (uint16_t)(PTX_T5TOP_CC_BLOCK_NUMBER + 1), &t5tOpComp->RxBuffer[0], &rx_len);

                            if (ptxStatus_Success == status)
                            {
                                /* copy 2nd half of CC from already available data */
                                (void)memcpy(&t5tOpComp->WorkBuffer[4], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], (uint32_t)4);
                            }
                        }
                    }
                }
            }

            /* parse CC-data */
            if (ptxStatus_Success == status)
            {
                t5tOpComp->CCParams.MagicNumber = t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MAGIC_NUMBER];

                /* NDEF-Tag at all ?*/
                switch (t5tOpComp->CCParams.MagicNumber)
                {
                    case PTX_T5TOP_CC_MAGIC_NUMBER_E1:
                        /* OK - nothing to do */
                        break;

                    case PTX_T5TOP_CC_MAGIC_NUMBER_E2:
                        /* OK - EXT_*-commands are required for block-addresses above 0xFF */
                        t5tOpComp->CCParams.ExtCommandTypeRequired = (uint8_t)0x01;
                        break;

                    default:
                        /* not ready for NDEF - update Life-Cycle and cancel operation */
                        t5tOpComp->LifeCycle = TagLC_NoNDEFTag;
                        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                        break;
                }

                /* check supported mapping version and set Read-/Write-Access information */
                if (ptxStatus_Success == status)
                {
                    t5tOpComp->CCParams.Version = (uint8_t)((t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_VERSION_ACCESS] & PTX_T5TOP_CC_VERSION_MASK) >> 2);

                    if (((t5tOpComp->CCParams.Version & 0xF0) == (PTX_T5T_SUPPORTED_VERSION & 0xF0)) && ((t5tOpComp->CCParams.Version & 0x0F) >= (PTX_T5T_SUPPORTED_VERSION & 0x0F)))
                    {
                        if (0 == (t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_VERSION_ACCESS] & PTX_T5TOP_CC_READ_ACCESS_MASK))
                        {
                            t5tOpComp->CCParams.ReadAccess = (uint8_t)0x01;
                        }

                        if (0 == (t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_VERSION_ACCESS] & PTX_T5TOP_CC_WRITE_ACCESS_MASK))
                        {
                            t5tOpComp->CCParams.WriteAccess = (uint8_t)0x01;
                        }

                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                    }
                }

                /* set memory-length (MLEN) and feature flags */
                if (ptxStatus_Success == status)
                {
                    if (0 != t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MLEN_BASIC])
                    {
                        t5tOpComp->CCParams.MLEN = (uint16_t)(t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MLEN_BASIC] * 8);

                    }
                    else
                    {
                        t5tOpComp->CCParams.MLEN = (uint16_t)(((t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MLEN_EXTENDED + 0] << 8) |
                                                               t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_MLEN_EXTENDED + 1]) * 8);
                    }

                    if (0 != (t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_ADDITIONAL_FEATURE] & PTX_T5TOP_CC_ADD_INFO_SPECIAL_FRAME_MASK))
                    {
                        t5tOpComp->CCParams.SpecialFrameRequired = (uint8_t)0x01;
                    }

                    if (0 != (t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_ADDITIONAL_FEATURE] & PTX_T5TOP_CC_ADD_INFO_LOCK_BLOCK_MASK))
                    {
                        t5tOpComp->CCParams.LockBlockSupported = (uint8_t)0x01;
                    }

                    if (0 != (t5tOpComp->WorkBuffer[PTX_T5TOP_CC_OFFSET_ADDITIONAL_FEATURE] & PTX_T5TOP_CC_ADD_INFO_MBREAD_MASK))
                    {
                        t5tOpComp->CCParams.MultiBlockReadSupported = (uint8_t)0x01;
                    }
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TOpHandleCCInfo(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *ndefTLVFound)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t current_block_nr = 0;
    uint16_t current_byte_nr = 0;
    uint8_t quit_loop = 0;
    uint8_t ndef_tlv_found = 0;
    uint8_t terminator_tlv_found = 0;
    uint8_t init_block_read_offset = 0;
    size_t rx_len;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != ndefTLVFound))
    {
        /* reset Greedy-collection */
        t5tOpComp->NDEF_TLV_POS_BN = 0;
        t5tOpComp->NDEF_TLV_POS_BY = 0;
        t5tOpComp->NDEF_TLV_LENGTH = 0;

        /* determine starting block number and overall read index */
        switch (t5tOpComp->BlockSize)
        {
            case (uint8_t)4:
                current_block_nr = ((uint8_t)4 == t5tOpComp->CCParams.Size) ? (uint16_t)1 : (uint16_t)2;
                init_block_read_offset = 0;
                break;

            case (uint8_t)8:
                current_block_nr = ((uint8_t)4 == t5tOpComp->CCParams.Size) ? (uint16_t)0 : (uint16_t)1;
                init_block_read_offset = (0 == current_block_nr) ? (uint8_t)(t5tOpComp->BlockSize - t5tOpComp->CCParams.Size) : 0;
                break;

            default:
                current_block_nr = 0;
                init_block_read_offset = (uint8_t)(t5tOpComp->CCParams.Size);
                break;
        }

        /* Current Byte Number starts off as first byte of first block. */
        current_byte_nr = (uint16_t)(current_block_nr * t5tOpComp->BlockSize);

        /* search for the NDEF-TLV */
        while ((current_block_nr < ((t5tOpComp->CCParams.MLEN + t5tOpComp->CCParams.Size) / t5tOpComp->BlockSize)) && (ptxStatus_Success == status) && (0 == quit_loop))
        {
            /* read block */
            status = ptxNDEF_T5TOpReadBlock(t5tOpComp, current_block_nr, &t5tOpComp->RxBuffer[0], &rx_len);

            if ((ptxStatus_Success == status) && (rx_len >= (uint32_t)((1 + t5tOpComp->BlockSize))))
            {
                /* skip status-flag at offset 0 */
                rx_len--;

                current_byte_nr = (uint16_t)(current_byte_nr + init_block_read_offset);

                status = ptxNDEF_T5T_ParseTLV(t5tOpComp, &current_byte_nr, &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], rx_len, &ndef_tlv_found, &terminator_tlv_found);
                quit_loop = (uint8_t)(quit_loop | terminator_tlv_found | ndef_tlv_found | status);

                /* reset initial block offset because after 1st read, every further block gets read starting from offset 0 */
                init_block_read_offset = 0;
                current_block_nr = (uint16_t)(current_byte_nr / t5tOpComp->BlockSize);
            }
        }

        *ndefTLVFound = ndef_tlv_found;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TOpReadBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *rx, size_t *rxLen)
{
    ptxStatus_t status = ptxStatus_Success;

    /* Aside from actual Tag-component, remaining parameters are checked at lower-layers */
    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t5tOpComp->RxBufferSize;

        /* send standard- or Extended-Command depending on Block-Number */
        if (0 == t5tOpComp->CCParams.ExtCommandTypeRequired)
        {
            status = ptxNativeTag_T5TReadSingleBlock (&t5tOpComp->NativeTagT5T,
                     0,
                     (uint8_t)blockNumber,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);

        }
        else
        {
            status = ptxNativeTag_T5TExtReadSingleBlock (&t5tOpComp->NativeTagT5T,
                     0,
                     blockNumber,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);
        }

        /* check response (if applicable); complete packet incl. RES_FLAG return to calling function */
        if (ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                if (0 != (rx[PTX_T5TOP_OFFSET_RES_FLAG] & PTX_T5TOP_RES_FLAG_ERROR_MASK))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_NscRfError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TOpWriteBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *blockData, uint8_t blockDataLen, uint8_t *rx, size_t *rxLen)
{
    ptxStatus_t status = ptxStatus_Success;

    /* Aside from actual Tag-component, remaining parameters are checked at lower-layers */
    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        *rxLen = (uint32_t)t5tOpComp->RxBufferSize;

        /* send standard- or Extended-Command depending on Block-Number */
        if (0 == t5tOpComp->CCParams.ExtCommandTypeRequired)
        {
            status = ptxNativeTag_T5TWriteSingleBlock (&t5tOpComp->NativeTagT5T,
                     t5tOpComp->CCParams.SpecialFrameRequired,
                     (uint8_t)blockNumber,
                     blockData,
                     blockDataLen,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);

        }
        else
        {
            status = ptxNativeTag_T5TExtWriteSingleBlock (&t5tOpComp->NativeTagT5T,
                     t5tOpComp->CCParams.SpecialFrameRequired,
                     blockNumber,
                     blockData,
                     blockDataLen,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);
        }

        /* check response (if applicable); complete packet incl. RES_FLAG return to calling function */
        if (ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                if (0 != (rx[PTX_T5TOP_OFFSET_RES_FLAG] & PTX_T5TOP_RES_FLAG_ERROR_MASK))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_NscRfError);
            }
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TOpLockBlock (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t blockNumber, uint8_t *rx, size_t *rxLen)
{
    ptxStatus_t status = ptxStatus_Success;

    /* Aside from actual Tag-component, remaining parameters are checked at lower-layers */
    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        *rxLen = (uint32_t)t5tOpComp->RxBufferSize;

        /* send standard- or Extended-Command depending on Block-Number */
        if (0 == t5tOpComp->CCParams.ExtCommandTypeRequired)
        {
            status = ptxNativeTag_T5TLockSingleBlock (&t5tOpComp->NativeTagT5T,
                     t5tOpComp->CCParams.SpecialFrameRequired,
                     (uint8_t)blockNumber,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);

        }
        else
        {
            status = ptxNativeTag_T5TExtLockSingleBlock (&t5tOpComp->NativeTagT5T,
                     t5tOpComp->CCParams.SpecialFrameRequired,
                     blockNumber,
                     rx,
                     rxLen,
                     PTX_T5T_DEFAULT_TIMEOUT_MS);
        }

        /* check response (if applicable); complete packet incl. RES_FLAG return to calling function */
        if (ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                if (0 != (rx[PTX_T5TOP_OFFSET_RES_FLAG] & PTX_T5TOP_RES_FLAG_ERROR_MASK))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_ProtocolError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_NscRfError);
            }
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TOpUpdateLength (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t messageLen, uint8_t lengthSize)
{
    ptxStatus_t status = ptxStatus_Success;
    uint16_t current_block_nr;
    size_t rx_len;
    uint8_t i;
    uint8_t nr_blocks_to_process;
    uint8_t bytes_processed = 0;

    /* Aside from actual Tag-component, remaining parameters are checked at lower-layers */
    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (((uint8_t)1 == lengthSize) || ((uint8_t)3 == lengthSize)))
    {
        switch (t5tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            case TagLC_ReadWrite:
                /* OK */
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidState);
                break;
        }

        if (ptxStatus_Success == status)
        {
            /* does L-field completely fit into current block ? */
            if ((uint8_t)(t5tOpComp->NDEF_TLV_POS_BY + lengthSize) < t5tOpComp->BlockSize)
            {
                current_block_nr = t5tOpComp->NDEF_TLV_POS_BN;
                nr_blocks_to_process = (uint8_t)1;

            }
            else
            {
                /* does the L-field start at the next block i.e. is the T-field byte the last one of the current block ? */
                if (t5tOpComp->NDEF_TLV_POS_BY == (uint8_t)(t5tOpComp->BlockSize - 1))
                {
                    current_block_nr = (uint8_t)(t5tOpComp->NDEF_TLV_POS_BN + 1);
                    nr_blocks_to_process = (uint8_t)1;

                }
                else
                {
                    /* L-field is split between current and next block */
                    current_block_nr = t5tOpComp->NDEF_TLV_POS_BN;
                    nr_blocks_to_process = (uint8_t)2;
                }
            }

            /* read n-blocks to update the length field at once */
            bytes_processed = 0;
            for (i = 0; i < nr_blocks_to_process; i++)
            {
                if (ptxStatus_Success == status)
                {
                    status = ptxNDEF_T5TOpReadBlock(t5tOpComp, current_block_nr, &t5tOpComp->RxBuffer[0], &rx_len);
                }
                if (ptxStatus_Success == status)
                {
                    /* skip status-flag at offset 0 */
                    rx_len--;

                    if(rx_len <= t5tOpComp->WorkBufferSize)
                    {
                        (void)memcpy(&t5tOpComp->WorkBuffer[bytes_processed], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA], rx_len);
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                    }
                    bytes_processed = (uint8_t)(bytes_processed + rx_len);
                }
            }

            /* update L-field */
            if  ((uint8_t)1 == lengthSize)
            {
                t5tOpComp->WorkBuffer[(t5tOpComp->NDEF_TLV_POS_BY + 1) % t5tOpComp->BlockSize] = (uint8_t)messageLen;
            }
            else
            {
                t5tOpComp->WorkBuffer[(t5tOpComp->NDEF_TLV_POS_BY + 1) % t5tOpComp->BlockSize] = ((0 != messageLen) ? ((uint8_t)0xFF) : ((uint8_t)0x00));
                t5tOpComp->WorkBuffer[(t5tOpComp->NDEF_TLV_POS_BY + 2) % t5tOpComp->BlockSize] = (uint8_t)((messageLen & 0xFF00) >> 8);
                t5tOpComp->WorkBuffer[(t5tOpComp->NDEF_TLV_POS_BY + 3) % t5tOpComp->BlockSize] = (uint8_t)((messageLen & 0x00FF) >> 0);
            }

            if (ptxStatus_Success == status)
            {
                /* write L-field */
                bytes_processed = 0;
                for (i = 0; i < nr_blocks_to_process; i++)
                {
                    status = ptxNDEF_T5TOpWriteBlock(t5tOpComp, current_block_nr, &t5tOpComp->WorkBuffer[bytes_processed], t5tOpComp->BlockSize, &t5tOpComp->RxBuffer[0], &rx_len);

                    if (ptxStatus_Success == status)
                    {
                        bytes_processed = (uint8_t)(bytes_processed + t5tOpComp->BlockSize);
                        current_block_nr++;
                    }
                    else
                    {
                        /* error - skip further write-operations */
                        break;
                    }
                }
            }
        }

        /* if everything went OK, update the TLV-length of the message */
        if (ptxStatus_Success == status)
        {
            t5tOpComp->NDEF_TLV_LENGTH = messageLen;
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TReadNDEFSingleBlocks(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, size_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t msg_buffer_size;
    uint16_t current_block_nr;
    uint8_t ndef_tlv_header_size;
    uint8_t init_block_read_offset;
    size_t rx_len;
    size_t bytes_read = 0;
    uint16_t bytes_received;
    uint16_t bytes_to_copy;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_buffer_size = *msgLen;

        /*
         * determine block-adress to start reading (NDEF_TLV_POS_BN defines the block where the NDEF-TLV starts,
         * but not the actual NDEF-message, which could be stored in the next block;
         */
        current_block_nr = t5tOpComp->NDEF_TLV_POS_BN;
        ndef_tlv_header_size = (t5tOpComp->NDEF_TLV_LENGTH < (uint16_t)PTX_T5TOP_TLV_LENGTHTHRESHOLD) ? (uint8_t)PTX_T5TOP_TLVHEADER_SHORT : (uint8_t)PTX_T5TOP_TLVHEADER_LONG;

        if ((uint8_t)((t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size)) < t5tOpComp->BlockSize)
        {
            init_block_read_offset = (uint8_t)(t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size);

        }
        else
        {
            init_block_read_offset = (uint8_t)((t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size) % t5tOpComp->BlockSize);
            current_block_nr++;
        }

        while ((bytes_read < t5tOpComp->NDEF_TLV_LENGTH) && (bytes_read < (uint16_t)(t5tOpComp->CCParams.MLEN - ndef_tlv_header_size)) && (ptxStatus_Success == status))
        {
            /* read block */
            status = ptxNDEF_T5TOpReadBlock(t5tOpComp, current_block_nr, &t5tOpComp->RxBuffer[0], &rx_len);

            if ((ptxStatus_Success == status) && (rx_len >= init_block_read_offset))
            {
                /* skip status-flag at offset 0 */
                rx_len--;

                bytes_received = (uint16_t)(rx_len - init_block_read_offset);

                if ((t5tOpComp->NDEF_TLV_LENGTH - bytes_read) > bytes_received)
                {
                    bytes_to_copy = bytes_received;
                }
                else
                {
                    bytes_to_copy = (uint16_t)(t5tOpComp->NDEF_TLV_LENGTH - bytes_read);
                }

                /* does received data still fit into the output-buffer ? */
                if ((msg_buffer_size) >= (bytes_to_copy + bytes_read))
                {
                    /* copy NDEF-message content to output buffer */
                    (void)memcpy(&msgBuffer[bytes_read], &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + init_block_read_offset], bytes_to_copy);
                    bytes_read = (uint16_t)(bytes_read + bytes_to_copy);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                }
            }

            init_block_read_offset = 0;
            current_block_nr++;
        }

        if (ptxStatus_Success == status)
        {
            *msgLen = bytes_read;
        }
        else
        {
            *msgLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5TReadNDEFMultipleBlocks(ptxNDEF_T5TOP_t *t5tOpComp, uint8_t *msgBuffer, size_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t msg_buffer_size;
    uint16_t nbr_blocks;
    uint16_t nbr_blocks_total;
    uint16_t nbr_blocks_read = 0;
    uint8_t ndef_tlv_header_size;
    size_t rx_len = 0;
    uint8_t offset = 0;
    uint8_t first_read = 1;
    uint32_t bytes_read = 0;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_buffer_size = *msgLen;

        ndef_tlv_header_size = (t5tOpComp->NDEF_TLV_LENGTH < (uint16_t)PTX_T5TOP_TLV_LENGTHTHRESHOLD) ? (uint8_t)PTX_T5TOP_TLVHEADER_SHORT : (uint8_t)PTX_T5TOP_TLVHEADER_LONG;

        /* t5t->NDEF_TLV_LENGTH/PTX_T5TOP_BLOCKSIZE + 1 OR 0 dependent on divisibility */
        nbr_blocks_total = (uint16_t)(((t5tOpComp->NDEF_TLV_LENGTH + t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size) / t5tOpComp->BlockSize) + (((t5tOpComp->NDEF_TLV_LENGTH + t5tOpComp->NDEF_TLV_POS_BY + ndef_tlv_header_size) % t5tOpComp->BlockSize) ? (1) : (0)));

        while ((nbr_blocks_total > nbr_blocks_read) && (ptxStatus_Success == status))
        {
            nbr_blocks = ptxNDEF_T5T_SetMTU(t5tOpComp, (uint16_t)(nbr_blocks_total - nbr_blocks_read));

            rx_len = (size_t)t5tOpComp->RxBufferSize;

            if (0 == nbr_blocks)
            {
                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
            }
            else
            {
                if (1 != t5tOpComp->CCParams.ExtCommandTypeRequired)
                {
                    /* Reading multiple blocks returns NB+1 blocks, so we call the functions with one less than what we calculated. */
                    status = ptxNativeTag_T5TReadMultipleBlock(&t5tOpComp->NativeTagT5T,0,(uint8_t)(t5tOpComp->NDEF_TLV_POS_BN + nbr_blocks_read), (uint8_t)(nbr_blocks - 1u), &t5tOpComp->RxBuffer[0], &rx_len, PTX_T5T_DEFAULT_TIMEOUT_MS);
                }
                else
                {
                    /* Reading multiple blocks returns NB+1 blocks, so we call the functions with one less than what we calculated. */
                    status = ptxNativeTag_T5TExtReadMultipleBlock(&t5tOpComp->NativeTagT5T,0,(uint16_t)(t5tOpComp->NDEF_TLV_POS_BN + nbr_blocks_read), (uint16_t)(nbr_blocks - 1u), &t5tOpComp->RxBuffer[0], &rx_len, PTX_T5T_DEFAULT_TIMEOUT_MS);
                }
            }

            if (ptxStatus_Success == status)
            {
                offset = (first_read) ? ((uint8_t)(ndef_tlv_header_size + PTX_T5TOP_OFFSET_RES_DATA + t5tOpComp->NDEF_TLV_POS_BY)) : (PTX_T5TOP_OFFSET_RES_DATA);
                (msg_buffer_size >= (rx_len - offset)) ?
                        ((void)memcpy(&msgBuffer[bytes_read], &t5tOpComp->RxBuffer[offset], rx_len - offset)) :
                        (status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources));

                nbr_blocks_read = (uint16_t)(nbr_blocks_read + nbr_blocks);
                bytes_read = (uint32_t)(bytes_read + rx_len - offset);

                /* Set flag. */
                first_read = 0;
            }
        }

        if (ptxStatus_Success == status)
        {
            /* MsgLen is all bytes read without TLV-Header, beginning offsets, and end offsets. */
            *msgLen = bytes_read;
        }
        else
        {
            *msgLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5T_WriteMessageLoop(ptxNDEF_T5TOP_t *t5tOpComp, uint32_t msgLen, uint8_t *msgBuffer, uint8_t offset, uint16_t currentBlock, uint8_t tlvHeaderLen)
{
    ptxStatus_t status = ptxStatus_Success;

    uint32_t bytes_written = 0;
    uint16_t current_block_nr = currentBlock;
    uint8_t begin_offset = offset;
    uint8_t bytes_to_copy;
    size_t rx_len = 0;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (0 != msgLen) && (NULL != msgBuffer))
    {
        while ((bytes_written != msgLen) && (bytes_written < (uint16_t)(t5tOpComp->CCParams.MLEN - tlvHeaderLen)) && (ptxStatus_Success == status))
        {
            /* prepare NDEF-data for next write-cycle */
            if (msgLen - bytes_written + begin_offset >= t5tOpComp->BlockSize)
            {
                /* Need this full block */
                bytes_to_copy = (uint8_t)(t5tOpComp->BlockSize - (begin_offset));
            }
            else
            {
                /* Fits into this block */
                bytes_to_copy = (uint8_t)(msgLen - bytes_written);

                /* last (or first and only) block to be written completely or only partly ? */
                if ((((uint32_t)(bytes_to_copy + bytes_written) == msgLen)) && (bytes_to_copy != t5tOpComp->BlockSize))
                {
                    if ((ptxStatus_Success == status) && (t5tOpComp->BlockSize <= t5tOpComp->WorkBufferSize))
                    {
                        (void)memcpy(&t5tOpComp->WorkBuffer[begin_offset],
                                     &t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + begin_offset],
                                     (uint32_t)(t5tOpComp->BlockSize - begin_offset));
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InsufficientResources);
                    }
                }
            }

            if (ptxStatus_Success == status)
            {
                uint16_t temp_var = (uint16_t)(bytes_to_copy + begin_offset);
                if(temp_var <= (uint16_t)t5tOpComp->WorkBufferSize)
                {
                    (void)memcpy(&t5tOpComp->WorkBuffer[begin_offset], &msgBuffer[bytes_written], bytes_to_copy);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InternalError);
                }
            }

            if (ptxStatus_Success == status)
            {
                /* enough space left to store the (optional) TERMINATOR-TLV ? */
                if ((((uint32_t)(bytes_to_copy + bytes_written) == msgLen)) && (bytes_to_copy < t5tOpComp->BlockSize))
                {
                    t5tOpComp->WorkBuffer[bytes_to_copy + begin_offset] = PTX_T5TOP_TERMINATOR_TLV_T;
                }

                /* write data to Tag */
                status = ptxNDEF_T5TOpWriteBlock(t5tOpComp, current_block_nr, &t5tOpComp->WorkBuffer[0], t5tOpComp->BlockSize, &t5tOpComp->RxBuffer[0], &rx_len);

                if (ptxStatus_Success == status)
                {
                    bytes_written = (uint16_t)(bytes_written + bytes_to_copy);
                    begin_offset = 0;
                    current_block_nr++;
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5T_LockLoop(ptxNDEF_T5TOP_t *t5tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    uint16_t nr_blocks_to_lock;
    uint16_t current_block_nr = 0;
    size_t rx_len;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        /* Physically lock every available block (Attention: THIS IS NOT REVERSIBLE!) */
        nr_blocks_to_lock = (uint16_t)((t5tOpComp->CCParams.MLEN / t5tOpComp->BlockSize) +
                                       (uint16_t)(t5tOpComp->CCParams.Size / t5tOpComp->BlockSize) +
                                       (uint16_t)(((0 != t5tOpComp->CCParams.MLEN % t5tOpComp->BlockSize) ? (1u) : (0u)) +
                                               ((0 != t5tOpComp->CCParams.Size % t5tOpComp->BlockSize) ? (1u) : (0u))));

        for (current_block_nr = 0; current_block_nr < nr_blocks_to_lock; current_block_nr++)
        {
            /* lock block-by-block */
            status = ptxNDEF_T5TOpLockBlock (t5tOpComp, current_block_nr, &t5tOpComp->RxBuffer[0], &rx_len);

            if (ptxStatus_Success != status)
            {
                /* Exit loop. */
                current_block_nr = nr_blocks_to_lock;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T5T_ParseTLV(ptxNDEF_T5TOP_t *t5tOpComp, uint16_t *currentByteNr, uint8_t *data, size_t dataLen, uint8_t *ndefTlvFound, uint8_t *terminatorFound)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t current_byte = 0;
    uint16_t current_block = 0;
    uint8_t length_field_len = 0;
    uint8_t second_read_operation = 0;
    size_t rx_len = 0;
    uint16_t tlv_len = 0;
    uint8_t tlv_type = 0;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP) && (NULL != currentByteNr) && (NULL != data) && (0 != dataLen) && (NULL != ndefTlvFound) && (NULL != terminatorFound))
    {
        current_byte = *currentByteNr;
        current_block = (uint16_t)(current_byte / t5tOpComp->BlockSize);

        for (uint32_t idx = (uint32_t)(current_byte % t5tOpComp->BlockSize); idx < dataLen; idx++)
        {
            tlv_type = data[idx];

            /* T-field of NDEF-TLV found? Limit to 1 TLV. */
            if ((0 != tlv_type) && (PTX_T5TOP_TERMINATOR_TLV_T != tlv_type))
            {
                /* try to read L-field (Attention: Might consist of 1 or 3 bytes; can be split over next block ...) */
                if ((uint8_t)(idx + 1) < (uint8_t)dataLen)
                {
                    /* 1- or 3-byte L-field ?*/
                    if ((uint8_t)0xFE >= data[idx + 1])
                    {
                        /* 1-byte L-field */
                        tlv_len = (uint16_t)data[idx + 1];
                        length_field_len = 1;
                    }
                    else
                    {
                        length_field_len = 3;

                        /* can complete L-field be read (2 additional bytes) ? */
                        if ((uint8_t)(idx + 3) < (uint8_t)dataLen)
                        {
                            tlv_len = (uint16_t)((data[idx + 2] << 8) | (data[idx + 3]));

                            /* can at least 1st part of L-field be read (1 additional byte) ? */
                        }
                        else if ((uint8_t)(idx + 2) < (uint8_t)dataLen)
                        {
                            tlv_len = (uint16_t)((data[idx + 2] << 8));
                            second_read_operation = (uint8_t)PTX_T5TOP_2ND_READ_OP_3BL_GET_PART_LENGTH;

                        }
                        else
                        {
                            second_read_operation = (uint8_t)PTX_T5TOP_2ND_READ_OP_3BL_GET_FULL_LENGTH;
                        }
                    }
                }
                else
                {
                    second_read_operation = (uint8_t)PTX_T5TOP_2ND_READ_OP_GET_FULL_LENGTH;
                }

                if (PTX_T5TOP_NDEF_MESSAGE_TLV_T == tlv_type)
                {
                    t5tOpComp->NDEF_TLV_POS_BN = current_block;
                    t5tOpComp->NDEF_TLV_POS_BY = (uint8_t)(idx + (t5tOpComp->BlockSize - dataLen));
                }

                /* get L-field from next block (if necessary) */
                if (PTX_T5TOP_2ND_READ_OP_NOT_REQUIRED != second_read_operation)
                {
                    current_block++;

                    /* read block */
                    status = ptxNDEF_T5TOpReadBlock(t5tOpComp, current_block, &t5tOpComp->RxBuffer[0], &rx_len);

                    if (ptxStatus_Success == status)
                    {
                        switch (second_read_operation)
                        {
                            case PTX_T5TOP_2ND_READ_OP_GET_FULL_LENGTH:
                                /* 1- or 3-byte L-field ?*/
                                if ((uint8_t)0xFE >= t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 0])
                                {
                                    /* 1-byte L-field */
                                    tlv_len = (uint16_t)t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 0];

                                }
                                else
                                {
                                    tlv_len = (uint16_t)((t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 0] << 8) | \
                                                         (t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 1]));
                                }
                                break;

                            case PTX_T5TOP_2ND_READ_OP_3BL_GET_PART_LENGTH:
                                tlv_len = (uint16_t)(t5tOpComp->NDEF_TLV_LENGTH |
                                                     (uint16_t)((t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 0])));
                                break;

                            case PTX_T5TOP_2ND_READ_OP_3BL_GET_FULL_LENGTH:
                                tlv_len = (uint16_t)((t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 0] << 8) | \
                                                     (t5tOpComp->RxBuffer[PTX_T5TOP_OFFSET_RES_DATA + 1]));
                                break;

                            default:
                                status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InternalError);
                                break;
                        }
                    }
                }
                if (ptxStatus_Success == status)
                {
                    if (PTX_T5TOP_NDEF_MESSAGE_TLV_T == tlv_type)
                    {
                        t5tOpComp->NDEF_TLV_LENGTH = tlv_len;
                        *ndefTlvFound = (uint8_t)1;
                    }

                    /* Increment current byte number overall and within data block. */
                    current_byte = (uint16_t)(current_byte + (1u + length_field_len + tlv_len));
                    idx = idx + (1u + length_field_len + tlv_len);
                }
            }
            else
            {
                /* T-field of TERMINATOR-TLV found ? */
                if (PTX_T5TOP_TERMINATOR_TLV_T == tlv_type)
                {
                    /* Terminator-TLV determines the end of the reading-procedure */
                    *terminatorFound = (uint8_t)1;
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            *currentByteNr = current_byte;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T5TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static uint16_t ptxNDEF_T5T_SetMTU (ptxNDEF_T5TOP_t *t5tOpComp, uint16_t nbrBlocks)
{
    uint8_t transfer_len;
    uint16_t max_nbr_blocks = 0;

    if (PTX_COMP_CHECK(t5tOpComp, ptxStatus_Comp_T5TOP))
    {
        transfer_len = 253u - 1u; /* maxPTXbytes - RES_FLAG */

        if (transfer_len < (nbrBlocks * t5tOpComp->BlockSize))
        {
            max_nbr_blocks = (transfer_len / t5tOpComp->BlockSize);
        }
        else
        {
            max_nbr_blocks = nbrBlocks;
        }
    }

    return max_nbr_blocks;
}

