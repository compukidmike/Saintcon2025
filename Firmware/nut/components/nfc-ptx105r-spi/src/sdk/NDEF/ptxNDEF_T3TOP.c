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
    File        : ptxNativeTag_T3T.c

    Description : Native Tag API for NFC Forum Tag Type 3 (IOT READER - Extension)
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T3TOP.h"
#include "ptxNativeTag_T3T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \name NDEF Specific Communication Defines
 * @{
 */
#define PTX_T3TOP_READ_ATTRIBUTE_INFORMATION_BLOCK_SC           (uint16_t)0x000B        /**< Service Code for reading Attribute Information Block. */
#define PTX_T3TOP_WRITE_ATTRIBUTE_INFORMATION_BLOCK_SC          (uint16_t)0x0009        /**< Service Code for writing Attribute Information Block. */
#define PTX_T3TOP_ATTRIBUTE_INFORMATION_BYTES                   (uint8_t)0x0E           /**< Number of Attribute Information Block data bytes. */
#define PTX_T3TOP_ATTRIBUTE_INFORMATION_BYTES_CHECKSUM          (uint8_t)0x10           /**< Number of Attribute Information Block data bytes, including checksum. */
#define PTX_T3TOP_SENSF_REQ_SC_NDEF                             (uint16_t)0x12FC        /**< Service Code for SENSF-REQ. */
#define PTX_T3TOP_READ_NDEF_SC                                  (uint16_t)0x000B        /**< Service Code for NDEF Read. */
#define PTX_T3TOP_WRITE_NDEF_SC                                 (uint16_t)0x0009        /**< Service Code for NDEF Write. */
#define PTX_T3TOP_NOS_NDEF                                      (uint8_t)0x01           /**< Number of Services for NDEF operations. */
#define PTX_T3TOP_NOB_NDEF                                      (uint8_t)0x01           /**< Number of Blocks for specific NDEF operations. */
#define PTX_T3TOP_BLOCK_SIZE                                    (uint8_t)0x10           /**< T3T block size. */
#define PTX_T3TOP_NBR_INTERNAL                                  (uint8_t)15u            /**< Internal maximum blocks for read operations. */
#define PTX_T3TOP_NBW_INTERNAL                                  (uint8_t)15u            /**< Internal maximum blocks for write operations. */
#define PTX_T3TOP_BLOCKLIST_2BYTES_BASE                         (uint16_t)0x8000        /**< Block number base for 2 byte block numbers. */
#define PTX_T3TOP_BLOCKLIST_3BYTES_BASE                         (uint32_t)0x00000000    /**< Block number base for 3 byte block numbers. */
/** @} */

/**
 * \name Access Flags
 * @{
 */
#define PTX_T3TOP_WRITE_FLAG_ON                                 (uint8_t)0x0F           /**< Attribute Information Block Write Flag ON. */
#define PTX_T3TOP_WRITE_FLAG_OFF                                (uint8_t)0x00           /**< Attribute Information Block Write Flag OFF. */
#define PTX_T3TOP_READ_ONLY_FLAG                                (uint8_t)0x00           /**< Attribute Information Block access condition Read Only. */
/** @} */

/**
 * \name Response Offsets and Status Flags
 * @{
 */
#define PTX_T3TOP_CHECK_RSP_OFFSET_DATA                         (uint8_t)0x0C           /**< Offset of data in a read response frame. */
#define PTX_T3TOP_CHECK_RSP_OFFSET_STATUSFLAG1                  (uint8_t)0x09           /**< Offset of status flag 1 in a read response frame. */
#define PTX_T3TOP_CHECK_RSP_OFFSET_STATUSFLAG2                  (uint8_t)0x0A           /**< Offset of status flag 2 in a read response frame. */
#define PTX_T3TOP_CHECK_RSP_OFFSET_NUMBER_BYTES                 (uint8_t)0x0B           /**< Offset of length information in a read response frame. */
#define PTX_T3TOP_STATUSFLAG1_OK                                (uint8_t)0x00           /**< Status Flag 1 OK. */
#define PTX_T3TOP_STATUSFLAG2_OK                                (uint8_t)0x00           /**< Status Flag 2 OK. */
#define PTX_T3TOP_UPDATE_RSP_OFFSET_STATUSFLAG1                 (uint8_t)0x09           /**< Offset of status flag 1 in a write response frame. */
#define PTX_T3TOP_UPDATE_RSP_OFFSET_STATUSFLAG2                 (uint8_t)0x0A           /**< Offset of status flag 2 in a write response frame. */
#define PTX_T3TOP_UPDATE_RSP_LENGTH                             (uint8_t)0x0B           /**< Update response length. */
#define PTX_T3TOP_UPDATE_RSP_STATUSFLAG2_ACCESS_DENIED          (uint8_t)0x70           /**< Status Flag 2 Access Denied. */
#define PTX_T3TOP_UPDATE_RSP_STATUSFLAG2_MAX_NUM_WRITES         (uint8_t)0x71           /**< Status Flag 2 Maximum number of writes to tag reached. */
/** @} */

/**
 * \name Attribute information Offsets
 * @{
 */
#define PTX_T3T_ATTRIB_OFFSET_VERSION                           (uint8_t)0u             /**< Attribute Information Block offset for version information. */
#define PTX_T3T_ATTRIB_OFFSET_MAX_READ_BLOCKS                   (uint8_t)1u             /**< Attribute Information Block offset for maximum number of blocks handled per read operation. */
#define PTX_T3T_ATTRIB_OFFSET_MAX_WRITE_BLOCKS                  (uint8_t)2u             /**< Attribute Information Block offset for maximum number of blocks handled per write operation. */
#define PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_HIGH         (uint8_t)3u             /**< Attribute Information Block offset for upper byte of number of available blocks. */
#define PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_LOW          (uint8_t)4u             /**< Attribute Information Block offset for lower byte of number of available blocks. */
#define PTX_T3T_ATTRIB_OFFSET_RFU                               (uint8_t)5u             /**< Attribute Information Block offset for RFU bytes. */
#define PTX_T3T_ATTRIB_OFFSET_WRITE_FLAG                        (uint8_t)9u             /**< Attribute Information Block offset for Write Flag. */
#define PTX_T3T_ATTRIB_OFFSET_RWFLAG                            (uint8_t)10u            /**< Attribute Information Block offset for access conditions. */
#define PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_HIGH              (uint8_t)11u            /**< Attribute Information Block offset for upper byte of available NDEF data length. */
#define PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_MID               (uint8_t)12u            /**< Attribute Information Block offset for middle byte of available NDEF data length. */
#define PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_LOW               (uint8_t)13u            /**< Attribute Information Block offset for lower byte of available NDEF data length. */
#define PTX_T3T_ATTRIB_OFFSET_CHECKSUM_HIGH                     (uint8_t)14u            /**< Attribute Information Block offset for upper byte of checksum. */
#define PTX_T3T_ATTRIB_OFFSET_CHECKSUM_LOW                      (uint8_t)15u            /**< Attribute Information Block offset for lower byte of checksum. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief T3T Read Operation.
 *
 * \param[in] t3topComp             Pointer to component.
 * \param[in] NFCID2                Tag ID.
 * \param[in] NFCID2Len             Length of Tag ID.
 * \param[in] serviceInfo           Information about Services to be updated.
 * \param[in] blockInfo             Information about Blocks to be updated.
 * \param[in,out] rx                Rx Buffer to store response.
 * \param[in,out] rxLen             Maximum Rx Buffer length on input, Response length on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpCheck  (ptxNDEF_T3TOP_t *t3tOpComp,
                                        uint8_t *NFCID2,
                                        size_t NFCID2Len,
                                        ptxNativeTag_T3T_Services_t serviceInfo,
                                        ptxNativeTag_T3T_Blocks_t blockInfo,
                                        uint8_t *rx,
                                        size_t *rxLen,
                                        uint32_t msTimeout);

/**
 * \brief T3T Write Operation.
 *
 * \param[in] t3topComp             Pointer to component.
 * \param[in] NFCID2                Tag ID.
 * \param[in] NFCID2Len             Length of Tag ID.
 * \param[in] serviceInfo           Information about Services to be updated.
 * \param[in] blockInfo             Information about Blocks to be updated.
 * \param[in] blockData             Data to be written.
 * \param[in] blockDataLen          Length of Data to be written.
 * \param[in,out] rx                Rx Buffer to store response.
 * \param[in,out] rxLen             Maximum Rx Buffer length on input, Response length on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpUpdate (ptxNDEF_T3TOP_t *t3tOpComp,
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
 * \brief Read Message loop.
 *
 * \param[in]     t3topComp             Pointer to component.
 * \param[out]    msgLen                Length of Message read.
 * \param[in,out] msgBuffer             Message data buffer.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpReadLoop(ptxNDEF_T3TOP_t *t3tOpComp, uint32_t *msgLen, uint8_t *msgBuffer);

/**
 * \brief Write Message Loop.
 *
 * \param[in] msgLen                Length of Message data.
 * \param[in] msgBuffer             Message data buffer.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpWriteLoop (ptxNDEF_T3TOP_t *t3tOpComp, uint32_t msgLen, uint8_t *msgBuffer);

/**
 * \brief Get Attribute Information.
 *
 * \param[in] t3topComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpGetAttributeInformationBlock (ptxNDEF_T3TOP_t *t3tOpComp);

/**
 * \brief Set Attribute Information.
 *
 * \param[in] t3topComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpSetAttributeInformationBlock (ptxNDEF_T3TOP_t *t3tOpComp);

/**
 * \brief Calculate Communication Checksum.
 *
 * \param[in] t3topComp             Pointer to component.
 * \param[out] checksum             Calculated Checksum.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpCalculateChecksum (ptxNDEF_T3TOP_t *t3tOpComp, uint16_t *checksum);

/**
 * \brief Generate a BlockList, assuming all blocks from startBlock onwards are to be used.
 *
 * \param[out] blockList            BlockList.
 * \param[in,out] blockListLen      Maximum BlockList Capacity on input, Block List Length on output.
 * \param[in] nob                   Number of Blocks.
 * \param[in] maxNob                Maximum Number of Blocks.
 * \param[in] startBlock            First Block in the List.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpGenerateBlockList(uint8_t* blockList, uint16_t *blockListLen, uint16_t nob, uint16_t maxNob, uint16_t startBlock);

/**
 * \brief Set T3T Tag parameters.
 *
 * \param[in] t3topComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNDEF_T3TOpSetTagParams (ptxNDEF_T3TOP_t *t3tOpComp);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNDEF_T3TOpOpen (ptxNDEF_T3TOP_t *t3tOpComp, ptxNDEF_T3TOP_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;
    ptxNativeTag_T3T_InitParams_t T3T_init_params;

    if ((NULL != t3tOpComp) && (NULL != initParams))
    {
        if ((NULL != initParams->RxBuffer) &&
                (0 != initParams->RxBufferSize))
        {
            /* clear component */
            (void)memset(t3tOpComp, 0, sizeof(ptxNDEF_T3TOP_t));

            t3tOpComp->RxBuffer = initParams->RxBuffer;
            t3tOpComp->RxBufferSize = initParams->RxBufferSize;
            t3tOpComp->LifeCycle = TagLC_NoNDEFTag;
            t3tOpComp->NFCID2 = initParams->T3TInitParams.NFCID2;
            t3tOpComp->NFCID2Len = initParams->T3TInitParams.NFCID2Len;
            t3tOpComp->MRTI_Check = initParams->T3TInitParams.MRTI_Check;
            t3tOpComp->MRTI_Update = initParams->T3TInitParams.MRTI_Update;
            t3tOpComp->IotRd = initParams->T3TInitParams.IotRd;

            /* initialize lower layer component */
            (void)memset(&T3T_init_params, 0, sizeof(ptxNativeTag_T3T_InitParams_t));
            T3T_init_params.IotRd = initParams->T3TInitParams.IotRd;
            T3T_init_params.TxBuffer = initParams->T3TInitParams.TxBuffer;
            T3T_init_params.TxBufferSize = initParams->T3TInitParams.TxBufferSize;
            T3T_init_params.NFCID2 = initParams->T3TInitParams.NFCID2;
            T3T_init_params.NFCID2Len = initParams->T3TInitParams.NFCID2Len;
            T3T_init_params.MRTI_Check = initParams->T3TInitParams.MRTI_Check;
            T3T_init_params.MRTI_Update = initParams->T3TInitParams.MRTI_Update;

            status = ptxNativeTag_T3TOpen(&t3tOpComp->NativeTagT3T, &T3T_init_params);

            /* set Component-ID at the end to prevent further calls in case of an error */
            if (ptxStatus_Success == status)
            {
                t3tOpComp->CompId = ptxStatus_Comp_T3TOP;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T3TOpFormatTag (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_NotImplemented);

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T3TOpCheckMessage (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t nfcid2[PTX_T3T_NFCID2_SIZE];
    ptxNativeTag_T3T_MRTI_t mrti_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        status = ptxNDEF_T3TOpSetTagParams(t3tOpComp);

        if (ptxStatus_Success == status)
        {
            /* set NFCID2 */
            (void)memcpy(&nfcid2[0],&t3tOpComp->NFCID2[0],(uint32_t)PTX_T3T_NFCID2_SIZE);
            mrti_info.MRTICheck = t3tOpComp->MRTI_Check;
            mrti_info.MRTIUpdate = (uint8_t)t3tOpComp->MRTI_Update;

            status =  ptxNativeTag_T3TSetTagParams(&t3tOpComp->NativeTagT3T,
                                                   &nfcid2[0],
                                                   PTX_T3T_NFCID2_SIZE,
                                                   mrti_info);


            if (ptxStatus_Success == status)
            {
                /* read attribute information block (equivalent to CC params) */
                status = ptxNDEF_T3TOpGetAttributeInformationBlock(t3tOpComp);
            }

            if (ptxStatus_Success == status)
            {
                if (PTX_T3TOP_READ_ONLY_FLAG != t3tOpComp->CCParams.RWFlag)
                {
                    t3tOpComp->LifeCycle = (0 != t3tOpComp->CCParams.Ln) ? TagLC_ReadWrite : TagLC_Initialized;

                }
                else
                {
                    t3tOpComp->LifeCycle = TagLC_ReadOnly;
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T3TOpReadMessage (ptxNDEF_T3TOP_t *t3tOpComp, uint8_t *msgBuffer, uint32_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    uint32_t msg_buffer_size;
    uint8_t nfcid2[PTX_T3T_NFCID2_SIZE];
    ptxNativeTag_T3T_MRTI_t mrti_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        status = ptxNDEF_T3TOpSetTagParams(t3tOpComp);

        if (ptxStatus_Success == status)
        {
            msg_buffer_size = *msgLen;

            if (0 != msg_buffer_size)
            {
                switch (t3tOpComp->LifeCycle)
                {
                    case TagLC_Initialized:
                    case TagLC_ReadWrite:
                    case TagLC_ReadOnly:
                        /* OK */
                        break;

                    default:
                        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidState);
                        break;
                }

                /* check if previous writing procedures have finished */
                if (PTX_T3TOP_WRITE_FLAG_ON == t3tOpComp->CCParams.WriteFlag)
                {
                    status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_T3T_WriteFlagSet);
                }

                if (ptxStatus_Success == status)
                {
                    /* set NFCID2 */
                    (void)memcpy(&nfcid2[0],&t3tOpComp->NFCID2[0],(uint32_t)PTX_T3T_NFCID2_SIZE);
                    mrti_info.MRTICheck = t3tOpComp->MRTI_Check;
                    mrti_info.MRTIUpdate = (uint8_t)t3tOpComp->MRTI_Update;


                    status =  ptxNativeTag_T3TSetTagParams(&t3tOpComp->NativeTagT3T,
                                                           &nfcid2[0],
                                                           PTX_T3T_NFCID2_SIZE,
                                                           mrti_info);


                    if (ptxStatus_Success == status)
                    {
                        /* read blocks */
                        status = ptxNDEF_T3TOpReadLoop(t3tOpComp, msgLen, msgBuffer);
                    }
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T3TOpWriteMessage (ptxNDEF_T3TOP_t *t3tOpComp, uint8_t *msgBuffer, uint32_t msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t nfcid2[PTX_T3T_NFCID2_SIZE];
    uint16_t msg_len;
    uint8_t EMPTY_NDEF_MESSAGE[] = {0x0D, 0x00, 0x00};
    uint8_t *msg_buffer;
    uint16_t nbr_data_blocks;

    ptxNativeTag_T3T_MRTI_t mrti_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        switch (t3tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            case TagLC_ReadWrite:
                /* OK */
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidState);
                break;
        }

        if (ptxStatus_Success == status)
        {
            status = ptxNDEF_T3TOpSetTagParams(t3tOpComp);

            if (ptxStatus_Success == status)
            {
                /* set NFCID2 */
                (void)memcpy(&nfcid2[0],&t3tOpComp->NFCID2[0],(uint32_t)PTX_T3T_NFCID2_SIZE);
                mrti_info.MRTICheck = t3tOpComp->MRTI_Check;
                mrti_info.MRTIUpdate = (uint8_t)t3tOpComp->MRTI_Update;

                status =  ptxNativeTag_T3TSetTagParams(&t3tOpComp->NativeTagT3T,
                                                       &nfcid2[0],
                                                       PTX_T3T_NFCID2_SIZE,
                                                       mrti_info);
            }
        }

        /* check for other write procedure in progress */
        if (PTX_T3TOP_WRITE_FLAG_ON == t3tOpComp->CCParams.WriteFlag)
        {
            status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidState);
        }

        if (ptxStatus_Success == status)
        {
            /* check for empty message */
            if ((0 != msgLen) && (NULL != msgBuffer))
            {
                msg_buffer = msgBuffer;
                msg_len = (uint16_t)msgLen;
            }
            else
            {
                msg_buffer = &EMPTY_NDEF_MESSAGE[0];
                msg_len = sizeof(EMPTY_NDEF_MESSAGE);
            }

            nbr_data_blocks = (uint16_t)(msg_len / PTX_T3TOP_BLOCK_SIZE);
            if (0 != (msg_len % PTX_T3TOP_BLOCK_SIZE))
            {
                nbr_data_blocks++;
            }

            if (t3tOpComp->CCParams.NmaxB >= nbr_data_blocks)
            {
                /* write flag needs to be set to ON */
                t3tOpComp->CCParams.WriteFlag = PTX_T3TOP_WRITE_FLAG_ON;
                status = ptxNDEF_T3TOpSetAttributeInformationBlock(t3tOpComp);

                if (ptxStatus_Success == status)
                {
                    status = ptxNDEF_T3TOpWriteLoop(t3tOpComp, msg_len, msg_buffer);
                }

                if (ptxStatus_Success == status)
                {
                    /* write is done, check if write flag needs to be set to OFF and write Ln field */

                    t3tOpComp->CCParams.WriteFlag = PTX_T3TOP_WRITE_FLAG_OFF;

                    /* update message length infos */
                    t3tOpComp->CCParams.Ln = msgLen;
                    t3tOpComp->CCParams.Nbc = (t3tOpComp->CCParams.Ln % 16);
                    if (0 != (t3tOpComp->CCParams.Ln % 16))
                    {
                        t3tOpComp->CCParams.Nbc++;
                    }
                    status = ptxNDEF_T3TOpSetAttributeInformationBlock(t3tOpComp);

                    if (ptxStatus_Success == status)
                    {
                        switch (msgLen)
                        {
                            case 0:
                                t3tOpComp->LifeCycle = TagLC_Initialized;
                                break;
                            default:
                                t3tOpComp->LifeCycle = TagLC_ReadWrite;
                                break;
                        }
                    }
                }
                else
                {
                    /* write might have failed, write flag needs to be set to OFF */
                    t3tOpComp->CCParams.WriteFlag = PTX_T3TOP_WRITE_FLAG_OFF;
                }
            }

        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    (void)msgBuffer;
    (void)msgLen;

    return status;
}

ptxStatus_t ptxNDEF_T3TOpLockTag (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t nfcid2[PTX_T3T_NFCID2_SIZE];
    ptxNativeTag_T3T_MRTI_t mrti_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        status = ptxNDEF_T3TOpSetTagParams(t3tOpComp);

        if (ptxStatus_Success == status)
        {
            if (0 == t3tOpComp->CCParams.Ln)
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidState);
            }

            if (ptxStatus_Success == status)
            {
                /* set NFCID2 */
                (void)memcpy(&nfcid2[0],&t3tOpComp->NFCID2[0],(uint32_t)PTX_T3T_NFCID2_SIZE);
                mrti_info.MRTICheck = t3tOpComp->MRTI_Check;
                mrti_info.MRTIUpdate = (uint8_t)t3tOpComp->MRTI_Update;

                status =  ptxNativeTag_T3TSetTagParams(&t3tOpComp->NativeTagT3T,
                                                       &nfcid2[0],
                                                       PTX_T3T_NFCID2_SIZE,
                                                       mrti_info);
            }
        }

        if (ptxStatus_Success == status)
        {
            t3tOpComp->CCParams.RWFlag = PTX_T3TOP_READ_ONLY_FLAG;

            status = ptxNDEF_T3TOpSetAttributeInformationBlock(t3tOpComp);

            if (ptxStatus_Success == status)
            {
                t3tOpComp->LifeCycle = TagLC_ReadOnly;
            }
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T3TOpClose (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        status = ptxNativeTag_T3TClose(&t3tOpComp->NativeTagT3T);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNDEF_T3TOpCheck (ptxNDEF_T3TOP_t *t3tOpComp,
                                       uint8_t *NFCID2,
                                       size_t NFCID2Len,
                                       ptxNativeTag_T3T_Services_t serviceInfo,
                                       ptxNativeTag_T3T_Blocks_t blockInfo,
                                       uint8_t *rx,
                                       size_t *rxLen,
                                       uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t num_bytes_res;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != NFCID2) &&
            (NULL != serviceInfo.ServiceCodeList) && (NULL != blockInfo.BlockList) &&
            (0 != serviceInfo.ServiceCodeListLen) && (0 != blockInfo.BlockListLen) &&
            (0 != serviceInfo.NOS) && (0 != blockInfo.NOB) &&
            (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t3tOpComp->RxBufferSize;

        /* send native tag command */
        status = ptxNativeTag_T3TCheck(&t3tOpComp->NativeTagT3T,NFCID2,NFCID2Len,serviceInfo,blockInfo,rx,rxLen,msTimeout);

        if (ptxStatus_Success == status)
        {
            /* handle response */
            if ((0 != *rxLen) && (PTX_T3TOP_CHECK_RSP_OFFSET_DATA <= *rxLen))
            {
                /* OK, check response */
                if ((PTX_T3TOP_STATUSFLAG1_OK != rx[PTX_T3TOP_CHECK_RSP_OFFSET_STATUSFLAG1]) || (PTX_T3TOP_STATUSFLAG2_OK != rx[PTX_T3TOP_CHECK_RSP_OFFSET_STATUSFLAG2]))
                {
                    /* error, no block data received */
                    status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_ACKError);
                }
                else
                {
                    /* OK, result stays in RxBuffer */
                    num_bytes_res = (uint8_t)(rx[PTX_T3TOP_CHECK_RSP_OFFSET_NUMBER_BYTES] * PTX_T3TOP_BLOCK_SIZE);

                    for (uint8_t i = 0; i<num_bytes_res; i++)
                    {
                        t3tOpComp->RxBuffer[i] = t3tOpComp->RxBuffer[PTX_T3TOP_CHECK_RSP_OFFSET_NUMBER_BYTES+1+i];
                    }
                    *rxLen = num_bytes_res;
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_NscRfError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpUpdate (ptxNDEF_T3TOP_t *t3tOpComp,
                                        uint8_t *NFCID2,
                                        size_t NFCID2Len,
                                        ptxNativeTag_T3T_Services_t serviceInfo,
                                        ptxNativeTag_T3T_Blocks_t blockInfo,
                                        uint8_t *blockData,
                                        uint8_t blockDataLen,
                                        uint8_t *rx,
                                        size_t *rxLen,
                                        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != NFCID2) &&
            (NULL != serviceInfo.ServiceCodeList) && (NULL != blockInfo.BlockList) &&
            (0 != serviceInfo.ServiceCodeListLen) && (0 != blockInfo.BlockListLen) &&
            (0 != serviceInfo.NOS) && (0 != blockInfo.NOB) &&
            (NULL != rx) && (NULL != rxLen) && (NULL != blockData))
    {
        *rxLen = (uint32_t)t3tOpComp->RxBufferSize;

        /* send native tag command */
        status = ptxNativeTag_T3TUpdate(&t3tOpComp->NativeTagT3T,NFCID2,NFCID2Len,serviceInfo,blockInfo,blockData,blockDataLen,rx,rxLen,msTimeout);

        if (ptxStatus_Success == status)
        {
            /* handle response */
            if (0 != *rxLen)
            {
                /* OK, check response */
                if ((PTX_T3TOP_UPDATE_RSP_LENGTH <= *rxLen) && (PTX_T3TOP_STATUSFLAG1_OK != rx[PTX_T3TOP_UPDATE_RSP_OFFSET_STATUSFLAG1]) && (PTX_T3TOP_STATUSFLAG2_OK != rx[PTX_T3TOP_UPDATE_RSP_OFFSET_STATUSFLAG2]))
                {
                    /* error */
                    switch (rx[PTX_T3TOP_UPDATE_RSP_OFFSET_STATUSFLAG2])
                    {
                        case PTX_T3TOP_UPDATE_RSP_STATUSFLAG2_ACCESS_DENIED:
                            /* cannot write to memory */
                            status = PTX_STATUS(ptxStatus_Comp_T3TOP,ptxStatus_AccessDenied);
                            break;
                        case PTX_T3TOP_UPDATE_RSP_STATUSFLAG2_MAX_NUM_WRITES:
                            /* operation successful, but exceeded the max number of writes to memory */
                            break;
                        default:
                            status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_NscRfError);
                            break;
                    }

                }
                else
                {
                    /* OK */
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_NscRfError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpReadLoop(ptxNDEF_T3TOP_t *t3tOpComp, uint32_t *msgLen, uint8_t *msgBuffer)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t current_block = 1;
    size_t rx_len;
    uint32_t bytes_to_copy;
    uint16_t nr_ndef_blocks = t3tOpComp->CCParams.Nbc;
    uint16_t blocks_to_read = 0;
    uint32_t bytes_read = 0;
    uint32_t msg_len = 0;

    /* One Entry is 2 bytes. */
    uint8_t service_code_list[PTX_T3TOP_NOS_NDEF * 2];
    service_code_list[0] = (PTX_T3TOP_READ_NDEF_SC & 0xFF);
    service_code_list[1] = (PTX_T3TOP_READ_NDEF_SC >> 8u);

    uint8_t block_list[PTX_T3TOP_NBR_INTERNAL * 3u];
    uint16_t block_list_len = PTX_T3TOP_NBR_INTERNAL * 3u;

    ptxNativeTag_T3T_Services_t service_info;
    ptxNativeTag_T3T_Blocks_t block_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != msgLen) && (NULL != msgBuffer))
    {
        msg_len = *msgLen;
        while ((t3tOpComp->CCParams.Nbc > (current_block-1)) && (ptxStatus_Success == status))
        {
            block_list_len = PTX_T3TOP_NBR_INTERNAL * 3u;

            if ((nr_ndef_blocks - (current_block - 1)) <= t3tOpComp->CCParams.NbrInt)
            {
                blocks_to_read = (uint16_t)(nr_ndef_blocks - (current_block - 1));
            }
            else
            {
                blocks_to_read = t3tOpComp->CCParams.NbrInt;
            }
            status = ptxNDEF_T3TOpGenerateBlockList(block_list, &block_list_len, blocks_to_read, t3tOpComp->CCParams.NbrInt, current_block);

            if (ptxStatus_Success == status)
            {
                block_info.NOB = (uint8_t)blocks_to_read;
                block_info.BlockList = block_list;
                block_info.BlockListLen = (uint8_t)block_list_len;

                service_info.NOS = PTX_T3TOP_NOS_NDEF;
                service_info.ServiceCodeList = service_code_list;
                service_info.ServiceCodeListLen = PTX_T3TOP_NOS_NDEF * 2u;

                status = ptxNDEF_T3TOpCheck(t3tOpComp,t3tOpComp->NativeTagT3T.NFCID2,t3tOpComp->NativeTagT3T.NFCID2Len,service_info,block_info,t3tOpComp->RxBuffer,&rx_len,t3tOpComp->NativeTagT3T.TagTimeoutCheck);
            }

            if (ptxStatus_Success == status)
            {
                /* in case of last read operation only copy NDEF bytes to message buffer */
                bytes_to_copy = (uint32_t)((t3tOpComp->CCParams.Ln < (uint32_t)(bytes_read + (uint32_t)(blocks_to_read * PTX_T3TOP_BLOCK_SIZE)))
                                           ? (uint32_t)(t3tOpComp->CCParams.Ln - bytes_read)
                                           : (uint32_t)(blocks_to_read * PTX_T3TOP_BLOCK_SIZE));

                (msg_len >= (bytes_read + bytes_to_copy)) ?
                        ((void)memcpy(&msgBuffer[bytes_read],&t3tOpComp->RxBuffer[0],bytes_to_copy)) :
                        (status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InsufficientResources));
                bytes_read = (uint32_t)(bytes_read + bytes_to_copy);

                current_block = (uint16_t)(current_block + blocks_to_read);
            }
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
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpWriteLoop (ptxNDEF_T3TOP_t *t3tOpComp, uint32_t msgLen, uint8_t *msgBuffer)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t nbr_padding_bytes = 0;
    uint32_t bytes_written = 0;
    uint16_t current_block = 1;
    uint16_t blocks_to_write;
    uint16_t nbr_data_blocks;
    size_t rx_len;

    /* One Entry is 2 bytes. */
    uint8_t service_code_list[PTX_T3TOP_NOS_NDEF * 2];
    service_code_list[0] = (PTX_T3TOP_WRITE_NDEF_SC & 0xFF);
    service_code_list[1] = (PTX_T3TOP_WRITE_NDEF_SC >> 8u);

    uint8_t block_data[PTX_T3TOP_BLOCK_SIZE * PTX_T3TOP_NBW_INTERNAL];
    uint8_t block_list[PTX_T3TOP_NBR_INTERNAL * 3u];
    uint16_t block_list_len = PTX_T3TOP_NBR_INTERNAL * 3u;

    ptxNativeTag_T3T_Services_t service_info;
    ptxNativeTag_T3T_Blocks_t block_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != msgBuffer) && (0 != msgLen))
    {
        nbr_data_blocks = (uint16_t)(msgLen / PTX_T3TOP_BLOCK_SIZE);
        if (0 != (msgLen % PTX_T3TOP_BLOCK_SIZE))
        {
            nbr_data_blocks++;
        }

        if (t3tOpComp->CCParams.NmaxB < nbr_data_blocks)
        {
            status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InsufficientResources);
        }

        while ((msgLen > bytes_written) && (ptxStatus_Success == status))
        {
            block_list_len = PTX_T3TOP_NBW_INTERNAL * 3u;

            if ((nbr_data_blocks - (current_block - 1)) <= t3tOpComp->CCParams.NbwInt)
            {
                blocks_to_write = (uint16_t)(nbr_data_blocks - (current_block - 1));
            }
            else
            {
                blocks_to_write = t3tOpComp->CCParams.NbwInt;
            }
            status = ptxNDEF_T3TOpGenerateBlockList(block_list, &block_list_len, blocks_to_write, t3tOpComp->CCParams.NbwInt, current_block);

            /* find out if padding is needed and fill data block */
            if ((msgLen < (bytes_written + (uint32_t)((blocks_to_write * PTX_T3TOP_BLOCK_SIZE)))) && (0 != ((msgLen - bytes_written) % PTX_T3TOP_BLOCK_SIZE)) && (ptxStatus_Success == status))
            {
                nbr_padding_bytes = (uint8_t)(PTX_T3TOP_BLOCK_SIZE - ((msgLen - bytes_written) % PTX_T3TOP_BLOCK_SIZE));
            }
            else
            {
                nbr_padding_bytes = 0;
            }

            (void)memset(&block_data[0], 0, PTX_T3TOP_BLOCK_SIZE * PTX_T3TOP_NBW_INTERNAL);
            
            if (((PTX_T3TOP_BLOCK_SIZE * PTX_T3TOP_NBW_INTERNAL) >= ((PTX_T3TOP_BLOCK_SIZE * blocks_to_write) - nbr_padding_bytes)) && (msgLen >= (uint32_t)(bytes_written + (uint32_t)((PTX_T3TOP_BLOCK_SIZE * blocks_to_write) - nbr_padding_bytes))))
            {
                (void)memcpy(&block_data[0],&msgBuffer[bytes_written],(uint32_t)(PTX_T3TOP_BLOCK_SIZE * blocks_to_write) - nbr_padding_bytes);
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InsufficientResources);
            }

            if (ptxStatus_Success == status)
            {
                block_info.NOB = (uint8_t)blocks_to_write;
                block_info.BlockList = block_list;
                block_info.BlockListLen = (uint8_t)block_list_len;

                service_info.NOS = PTX_T3TOP_NOS_NDEF;
                service_info.ServiceCodeList = service_code_list;
                service_info.ServiceCodeListLen = PTX_T3TOP_NOS_NDEF * 2u;

                status = ptxNDEF_T3TOpUpdate(t3tOpComp,t3tOpComp->NativeTagT3T.NFCID2,t3tOpComp->NativeTagT3T.NFCID2Len,service_info,block_info,block_data,(uint8_t)(PTX_T3TOP_BLOCK_SIZE * block_info.NOB),t3tOpComp->RxBuffer,&rx_len,t3tOpComp->NativeTagT3T.TagTimeoutUpdate);

                if (ptxStatus_Success == status)
                {
                    bytes_written = (uint32_t)(bytes_written + (uint32_t)((PTX_T3TOP_BLOCK_SIZE * blocks_to_write) - nbr_padding_bytes));
                    current_block = (uint16_t)(current_block + blocks_to_write);
                    (void)memset(&block_data[0],0x00,PTX_T3TOP_BLOCK_SIZE);
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpCalculateChecksum (ptxNDEF_T3TOP_t *t3tOpComp, uint16_t *checksum)
{
    ptxStatus_t status = ptxStatus_Success;
    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP) && (NULL != checksum))
    {
        *checksum = 0;
        t3tOpComp->CCParams.Checksum = (uint16_t)(((uint16_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_CHECKSUM_HIGH] << 8u) | (uint16_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_CHECKSUM_LOW]);

        for (uint8_t i = 0; i<PTX_T3TOP_ATTRIBUTE_INFORMATION_BYTES; i++)
        {
            *checksum = (uint16_t)(*checksum + t3tOpComp->CCParams.AttributeInformationBlock[i]);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpGetAttributeInformationBlock (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t service_code_list[PTX_T3TOP_NOS_NDEF * 2u];
    uint8_t block_list[PTX_T3TOP_NOB_NDEF * 2u];
    size_t rx_len;
    uint16_t checksum = 0;

    ptxNativeTag_T3T_Services_t service_info;
    ptxNativeTag_T3T_Blocks_t block_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        service_code_list[0] = (PTX_T3TOP_READ_ATTRIBUTE_INFORMATION_BLOCK_SC & 0xFF);
        service_code_list[1] = (PTX_T3TOP_READ_ATTRIBUTE_INFORMATION_BLOCK_SC >> 8u);
        block_list[0] = 0x80;
        block_list[1] = 0x00;

        service_info.NOS = PTX_T3TOP_NOS_NDEF;
        service_info.ServiceCodeList = service_code_list;
        service_info.ServiceCodeListLen = PTX_T3TOP_NOS_NDEF * 2u;

        block_info.NOB = PTX_T3TOP_NOB_NDEF;
        block_info.BlockList = block_list;
        block_info.BlockListLen = PTX_T3TOP_NOB_NDEF * 2u;

        /* read the attribute information block */
        status = ptxNDEF_T3TOpCheck(t3tOpComp,t3tOpComp->NativeTagT3T.NFCID2,t3tOpComp->NativeTagT3T.NFCID2Len,service_info,block_info,t3tOpComp->RxBuffer,&rx_len,t3tOpComp->NativeTagT3T.TagTimeoutCheck);
        if ((ptxStatus_Success == status) && (PTX_T3TOP_ATTRIBUTE_INFORMATION_BYTES_CHECKSUM == rx_len))
        {
            (void)memcpy(&t3tOpComp->CCParams.AttributeInformationBlock[0],&t3tOpComp->RxBuffer[0],(uint32_t)rx_len);

            status = ptxNDEF_T3TOpCalculateChecksum(t3tOpComp, &checksum);

            if (t3tOpComp->CCParams.Checksum == checksum)
            {
                /* fill the components */
                t3tOpComp->CCParams.Version = t3tOpComp->CCParams.AttributeInformationBlock[0];
                if ((PTX_T3T_SUPPORTED_VERSION >> 4u) != (t3tOpComp->CCParams.Version >> 4u))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
                }
                t3tOpComp->CCParams.MajorVersion = (t3tOpComp->CCParams.Version >> 4u);
                t3tOpComp->CCParams.MinorVersion = (t3tOpComp->CCParams.Version & 0xFF);

                t3tOpComp->CCParams.Nbr = t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_MAX_READ_BLOCKS];
                t3tOpComp->CCParams.Nbw = t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_MAX_WRITE_BLOCKS];

                /* Check if we can actually do the maximum */
                if (PTX_T3TOP_NBR_INTERNAL < t3tOpComp->CCParams.Nbr)
                {
                    t3tOpComp->CCParams.NbrInt = PTX_T3TOP_NBR_INTERNAL;
                }
                else
                {
                    t3tOpComp->CCParams.NbrInt = t3tOpComp->CCParams.Nbr;
                }
                if (PTX_T3TOP_NBW_INTERNAL < t3tOpComp->CCParams.Nbw)
                {
                    t3tOpComp->CCParams.NbwInt = PTX_T3TOP_NBW_INTERNAL;
                }
                else
                {
                    t3tOpComp->CCParams.NbwInt = t3tOpComp->CCParams.Nbw;
                }

                /* byte 3 upper, byte 4 lower */
                t3tOpComp->CCParams.NmaxB = (uint16_t)(((uint16_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_HIGH] << 8u) | (uint16_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_LOW]);

                (void)memcpy(&t3tOpComp->CCParams.RFU[0],&t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_RFU],(uint32_t)PTX_T3T_RFU_SIZE);

                t3tOpComp->CCParams.WriteFlag = t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_WRITE_FLAG];
                t3tOpComp->CCParams.RWFlag = t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_RWFLAG];

                t3tOpComp->CCParams.Ln  = (((uint32_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_HIGH] << 16u) | ((uint32_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_MID] << 8u) | (uint32_t)t3tOpComp->CCParams.AttributeInformationBlock[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_LOW]);
                if (0 == t3tOpComp->CCParams.Ln % PTX_T3T_BLOCK_SIZE)
                {
                    t3tOpComp->CCParams.Nbc = (uint16_t)(t3tOpComp->CCParams.Ln / PTX_T3T_BLOCK_SIZE);
                }
                else
                {
                    t3tOpComp->CCParams.Nbc = (uint16_t)((t3tOpComp->CCParams.Ln / PTX_T3T_BLOCK_SIZE) + 1);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_ProtocolError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpSetAttributeInformationBlock (ptxNDEF_T3TOP_t *t3tOpComp)
{
    /* translate the attributes back into bytes and write the block */
    ptxStatus_t status = ptxStatus_Success;
    uint8_t attribute_information[PTX_T3T_BLOCK_SIZE];
    uint8_t service_code_list[PTX_T3TOP_NOS_NDEF * 2u];
    uint8_t block_list[PTX_T3TOP_NOB_NDEF * 2u];
    size_t rx_len;
    uint16_t checksum = 0;

    ptxNativeTag_T3T_Services_t service_info;
    ptxNativeTag_T3T_Blocks_t block_info;

    if (PTX_COMP_CHECK(t3tOpComp, ptxStatus_Comp_T3TOP))
    {
        /* get the components into a data block (already updated outside the function) */
        attribute_information[PTX_T3T_ATTRIB_OFFSET_VERSION]  = t3tOpComp->CCParams.Version;
        attribute_information[PTX_T3T_ATTRIB_OFFSET_MAX_READ_BLOCKS]  = t3tOpComp->CCParams.Nbr;
        attribute_information[PTX_T3T_ATTRIB_OFFSET_MAX_WRITE_BLOCKS]  = t3tOpComp->CCParams.Nbw;

        attribute_information[PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_HIGH]  = (uint8_t)(t3tOpComp->CCParams.NmaxB >> 8u);
        attribute_information[PTX_T3T_ATTRIB_OFFSET_NBR_AVAILABLE_BLOCKS_LOW]  = (uint8_t)(t3tOpComp->CCParams.NmaxB & 0xFF);

        (void)memcpy(&attribute_information[PTX_T3T_ATTRIB_OFFSET_RFU],&t3tOpComp->CCParams.RFU[0],(uint32_t)PTX_T3T_RFU_SIZE);

        attribute_information[PTX_T3T_ATTRIB_OFFSET_WRITE_FLAG]  = t3tOpComp->CCParams.WriteFlag;
        attribute_information[PTX_T3T_ATTRIB_OFFSET_RWFLAG] = t3tOpComp->CCParams.RWFlag;

        attribute_information[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_HIGH] = (uint8_t)(t3tOpComp->CCParams.Ln >> 16u);
        attribute_information[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_MID] = (uint8_t)((t3tOpComp->CCParams.Ln >> 8u) & 0xFF);
        attribute_information[PTX_T3T_ATTRIB_OFFSET_NDEFDATA_LENGTH_LOW] = (uint8_t)(t3tOpComp->CCParams.Ln & 0xFF);

        memcpy(&t3tOpComp->CCParams.AttributeInformationBlock[0], &attribute_information[0], PTX_T3TOP_ATTRIBUTE_INFORMATION_BYTES);

        status = ptxNDEF_T3TOpCalculateChecksum(t3tOpComp, &checksum);
        if (ptxStatus_Success == status)
        {
            attribute_information[PTX_T3T_ATTRIB_OFFSET_CHECKSUM_HIGH] = (uint8_t)(checksum >> 8u);
            attribute_information[PTX_T3T_ATTRIB_OFFSET_CHECKSUM_LOW] = (uint8_t)(checksum & 0xFF);

            (void)memcpy(&t3tOpComp->CCParams.AttributeInformationBlock[0],&attribute_information[0],(size_t)PTX_T3T_BLOCK_SIZE);
            /* write attribute block */
            service_code_list[0] = (PTX_T3TOP_WRITE_ATTRIBUTE_INFORMATION_BLOCK_SC & 0xFF);
            service_code_list[1] = (PTX_T3TOP_WRITE_ATTRIBUTE_INFORMATION_BLOCK_SC >> 8u);
            block_list[0] = (PTX_T3TOP_BLOCKLIST_2BYTES_BASE >> 8u);
            block_list[1] = (PTX_T3TOP_BLOCKLIST_2BYTES_BASE & 0xFF);

            service_info.NOS = PTX_T3TOP_NOS_NDEF;
            service_info.ServiceCodeList = service_code_list;
            service_info.ServiceCodeListLen = PTX_T3TOP_NOS_NDEF * 2u;

            block_info.NOB = PTX_T3TOP_NOB_NDEF;
            block_info.BlockList = block_list;
            block_info.BlockListLen = PTX_T3TOP_NOB_NDEF * 2u;

            status = ptxNDEF_T3TOpUpdate(t3tOpComp,t3tOpComp->NativeTagT3T.NFCID2,t3tOpComp->NativeTagT3T.NFCID2Len,service_info,block_info,attribute_information,sizeof(attribute_information),t3tOpComp->RxBuffer,&rx_len,t3tOpComp->NativeTagT3T.TagTimeoutUpdate);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpGenerateBlockList(uint8_t* blockList, uint16_t *blockListLen, uint16_t nob, uint16_t maxNob, uint16_t startBlock)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t block_list_len = 0;
    uint8_t entry_len = 0;
    uint32_t entry = 0;
    uint16_t list_offset = 0;

    if ((NULL != blockList) && (NULL != blockListLen) && (0 != *blockListLen) && (maxNob >= nob))
    {
        for (uint8_t idx = 0; idx < nob; idx++)
        {
            entry = (uint32_t)(startBlock + idx);
            entry_len = (0xFF < entry) ? (3u) : (2u);

            if (*blockListLen >= block_list_len + entry_len) /* Will the BlockNumber still fit? */
            {
                switch (entry_len)
                {
                    case 2:
                        entry = entry | PTX_T3TOP_BLOCKLIST_2BYTES_BASE;

                        blockList[list_offset] = (uint8_t)(entry >> 8u);
                        list_offset++;
                        blockList[list_offset] = (uint8_t)(entry & 0xFF);
                        list_offset++;
                        break;

                    case 3:
                        entry = entry | PTX_T3TOP_BLOCKLIST_3BYTES_BASE;

                        blockList[list_offset] = (uint8_t)(0x00);
                        list_offset++;
                        blockList[list_offset] = (uint8_t)(entry & 0xFF);
                        list_offset++;
                        blockList[list_offset] = (uint8_t)(((entry >> 8u) & 0xFF));
                        list_offset++;
                        break;

                    default:
                        /* Cannot happen. */
                        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InternalError);
                        break;
                }

                block_list_len = (uint16_t)(block_list_len + entry_len);
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InsufficientResources);
            }
        }

        if (ptxStatus_Success == status)
        {
            *blockListLen = block_list_len;
        }
        else
        {
            *blockListLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T3TOpSetTagParams (ptxNDEF_T3TOP_t *t3tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    ptxIoTRd_CardRegistry_t *registry;

    if (NULL != t3tOpComp)
    {
        status = ptxIoTRd_Get_Card_Registry(t3tOpComp->IotRd, &registry);

        if ((ptxStatus_Success == status) && (NULL != registry))
        {
            if (registry->ActiveCardProtType == Prot_T3T)
            {
                t3tOpComp->NFCID2 = &registry->ActiveCard->TechParams.CardFParams.SENSF_RES[2];
                t3tOpComp->NFCID2Len = PTX_T3T_NFCID2_SIZE;
                t3tOpComp->MRTI_Check = registry->ActiveCard->TechParams.CardFParams.SENSF_RES[15];
                t3tOpComp->MRTI_Update = registry->ActiveCard->TechParams.CardFParams.SENSF_RES[16];
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T3TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

