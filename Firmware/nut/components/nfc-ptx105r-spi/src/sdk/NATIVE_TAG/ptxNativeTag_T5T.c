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
#include "ptxPLAT.h"
#include "ptxNativeTag_T5T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \brief T5T Native Tag Component
 */
typedef enum ptxNativeTag_T5TFrameType
{
    T5T_FrameType_Standard,         /**< Standard frame type. */
    T5T_FrameType_Special,          /**< Special frame type. */
} ptxNativeTag_T5TFrameType;

/**
 * \name T5T Request Codes
 * @{
 */
#define PTX_T5T_READ_SINGLE_BLOCK_REQ_CODE              (uint8_t)0x20   /**< Request Code Read Single Block. */
#define PTX_T5T_WRITE_SINGLE_BLOCK_REQ_CODE             (uint8_t)0x21   /**< Request Code Write Single Block. */
#define PTX_T5T_LOCK_SINGLE_BLOCK_REQ_CODE              (uint8_t)0x22   /**< Request Code Lock Single Block. */
#define PTX_T5T_READ_MULTIPLE_BLOCK_REQ_CODE            (uint8_t)0x23   /**< Request Code Read Multiple Blocks. */
#define PTX_T5T_EXT_READ_SINGLE_BLOCK_REQ_CODE          (uint8_t)0x30   /**< Request Code Read Single Block for extended memory layout. */
#define PTX_T5T_EXT_WRITE_SINGLE_BLOCK_REQ_CODE         (uint8_t)0x31   /**< Request Code Write Single Block for extended memory layout. */
#define PTX_T5T_EXT_LOCK_SINGLE_BLOCK_REQ_CODE          (uint8_t)0x32   /**< Request Code Lock Single Block for extended memory layout. */
#define PTX_T5T_EXT_READ_MULTIPLE_BLOCK_REQ_CODE        (uint8_t)0x33   /**< Request Code Read Multiple Blocks for extended memory layout. */
#define PTX_T5T_SELECT_REQ_CODE                         (uint8_t)0x25   /**< Request Code for tag Select. */
#define PTX_T5T_SLPV_REQ_CODE                           (uint8_t)0x02   /**< Request Code for tag Sleep. */
/** @} */

/**
 * \name Request Flag Masks
 * @{
 */
#define PTX_T5T_COMMANDCODE_FRAME_OFFSET                (uint8_t)0x01   /**< Command frame offset. */
#define PTX_T5T_REQ_FLAG_OPTION_BIT_SET_MASK            (uint8_t)0x40   /**< Mask for setting the Option Bit in the Request Flag. */
#define PTX_T5T_REQ_FLAG_AMS_SET_MASK                   (uint8_t)0x20   /**< Mask for setting Addressed Mode. */
#define PTX_T5T_REQ_FLAG_HIGHT_DATA_RATE_SET_MASK       (uint8_t)0x02   /**< High data rate mask. */
#define PTX_T5T_REQ_FLAG_SELECT_FLAG_SET_MASK           (uint8_t)0x10   /**< Selection Flag set mask. */
#define PTX_T5T_CRC_ERROR_INTERNAL                      (uint8_t)0x80   /**< Internal error code for retries upon miscommunication. */
#define PTX_T5T_REQ_FLAG_TEMPLATE_MASK                  PTX_T5T_REQ_FLAG_HIGHT_DATA_RATE_SET_MASK /**< Template mask. */
/** @} */

/**
 * \name Selection Parameters
 * @{
 */
#define PTX_T5T_SELECTED_TRUE                            (uint8_t)0x01   /**< Internal Select flag true. */
#define PTX_T5T_SELECTED_FALSE                           (uint8_t)0x00   /**< Internal Select flag false. */
/** @} */

/**
 * \name Isolated EoF Parameters
 * @{
 */
#define PTX_T5T_ISOLATED_EOF_LEN                        (uint8_t)2u      /**< Isolated EoF length. */
#define PTX_T5T_FDT_V_EOF                               (uint32_t)20     /**< FDT waiting time between isolated EoFs. */
/** @} */
/**
 * \name Internal Masks.
 * @{
 */
#define PTX_T5T_1_BYTE_SHIFT_MASK                       (uint8_t)8u      /**< Shift Mask 8 bit. */
#define PTX_T5T_UINT16_LOWER_BYTE_MASK                  (uint16_t)0x00FF /**< Mask lower 8 bits of an uint16. */
#define PTX_T5T_UINT16_UPPER_BYTE_MASK                  (uint16_t)0xFF00 /**< Mask upper 8 bits of an uint16. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Build and set command header to internal Tx buffer.
 *
 * \param[in] t5tComp               Pointer to component.
 * \param[in] commandCode           Command code.
 * \param[in] optionFlag            Option flag.
 * \param[in,out] bytesWritten      Offset within internal Tx buffer.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T5TSetCommandHeader(ptxNativeTag_T5T_t *t5tComp, uint8_t commandCode, uint8_t optionFlag, uint32_t *bytesWritten);

/**
 * \brief Data exchange.
 *
 * \param[in] t5tComp               Pointer to component.
 * \param[in] frameType             Frame Type.
 * \param[in] txLen                 Internal Tx data length.
 * \param[out] rx                   Response buffer.
 * \param[in,out] rxLen             Response buffer length on input, response length on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T5TTransceive(ptxNativeTag_T5T_t *t5tComp, ptxNativeTag_T5TFrameType frameType, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/**
 * \brief Handle RF Reset event.
 *
 * \param[in] t5tComp               Pointer to component.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T5THandleRfReset (ptxNativeTag_T5T_t *t5tComp);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNativeTag_T5TOpen (ptxNativeTag_T5T_t *t5tComp, ptxNativeTag_T5T_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != t5tComp) && (NULL != initParams))
    {
        if ((NULL != initParams->IotRd) && (NULL != initParams->TxBuffer) && (PTX_T5T_MIN_TX_BUFFER_SIZE <= initParams->TxBufferSize))
        {
            /* clear component */
            (void)memset(t5tComp, 0, sizeof(ptxNativeTag_T5T_t));

            /* set members */
            t5tComp->IotRd = initParams->IotRd;
            t5tComp->TxBuffer = initParams->TxBuffer;

            if (0 != initParams->UIDLen)
            {
                if ((PTX_T5T_UID_SIZE == initParams->UIDLen) && (NULL != initParams->UID))
                {
                    t5tComp->UID = initParams->UID;
                    t5tComp->UIDLen = initParams->UIDLen;
                    t5tComp->isSelected = initParams->isSelected;
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
                }
            }
            else
            {
                t5tComp->UID = NULL;
                t5tComp->UIDLen = 0;
            }

            /* set Component-ID at the end to prevent futher calls in case of an error */
            if (ptxStatus_Success == status)
            {
                t5tComp->CompId = ptxStatus_Comp_NativeTag_T5T;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TReadSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint8_t blockNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_READ_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = blockNr;
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TWriteSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint8_t blockNr,
        uint8_t *blockData,
        uint8_t blockDataLen,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;
    ptxNativeTag_T5TFrameType frame_type = (0 != optionFlag) ? T5T_FrameType_Special : T5T_FrameType_Standard;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen) && (NULL != blockData) && (PTX_T5T_MIN_TX_BUFFER_SIZE >= blockDataLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_WRITE_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = blockNr;
            tx_index++;

            (void)memcpy(&t5tComp->TxBuffer[tx_index], blockData, blockDataLen);
            tx_index = (uint32_t)(tx_index + blockDataLen);
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, frame_type, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TLockSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint8_t blockNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;
    ptxNativeTag_T5TFrameType frame_type = (0 != optionFlag) ? T5T_FrameType_Special : T5T_FrameType_Standard;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_LOCK_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = blockNr;
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, frame_type, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TReadMultipleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint8_t blockNr,
        uint8_t nrBlocks,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_READ_MULTIPLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = blockNr;
            tx_index++;

            t5tComp->TxBuffer[tx_index] = nrBlocks;
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TExtReadSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint16_t blockNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_EXT_READ_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_LOWER_BYTE_MASK) >> 0);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_UPPER_BYTE_MASK) >> PTX_T5T_1_BYTE_SHIFT_MASK);
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TExtWriteSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint16_t blockNr,
        uint8_t *blockData,
        uint8_t blockDataLen,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;
    ptxNativeTag_T5TFrameType frame_type = (0 != optionFlag) ? T5T_FrameType_Special : T5T_FrameType_Standard;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen) && (NULL != blockData) && (PTX_T5T_MIN_TX_BUFFER_SIZE >= blockDataLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_EXT_WRITE_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_LOWER_BYTE_MASK) >> 0);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_UPPER_BYTE_MASK) >> PTX_T5T_1_BYTE_SHIFT_MASK);
            tx_index++;

            (void)memcpy(&t5tComp->TxBuffer[tx_index], blockData, blockDataLen);
            tx_index = (uint32_t)(tx_index + blockDataLen);
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, frame_type, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TExtLockSingleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint16_t blockNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;
    ptxNativeTag_T5TFrameType frame_type = (0 != optionFlag) ? T5T_FrameType_Special : T5T_FrameType_Standard;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_EXT_LOCK_SINGLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_LOWER_BYTE_MASK) >> 0);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_UPPER_BYTE_MASK) >> PTX_T5T_1_BYTE_SHIFT_MASK);
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, frame_type, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TExtReadMultipleBlock (ptxNativeTag_T5T_t *t5tComp,
        uint8_t optionFlag,
        uint16_t blockNr,
        uint16_t nrBlocks,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_EXT_READ_MULTIPLE_BLOCK_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_LOWER_BYTE_MASK) >> 0);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((blockNr & PTX_T5T_UINT16_UPPER_BYTE_MASK) >> PTX_T5T_1_BYTE_SHIFT_MASK);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((nrBlocks & PTX_T5T_UINT16_LOWER_BYTE_MASK) >> 0);
            tx_index++;

            t5tComp->TxBuffer[tx_index] = (uint8_t)((nrBlocks & PTX_T5T_UINT16_UPPER_BYTE_MASK) >> PTX_T5T_1_BYTE_SHIFT_MASK);
            tx_index++;
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TSelect (ptxNativeTag_T5T_t *t5tComp,
                                    uint8_t optionFlag,
                                    uint8_t *uid,
                                    uint8_t uidLen,
                                    uint8_t *rx,
                                    size_t *rxLen,
                                    uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen)  && (NULL != uid) && (PTX_T5T_UID_SIZE == uidLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_SELECT_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            (void)memcpy(&t5tComp->TxBuffer[tx_index], uid, uidLen);
            tx_index = (uint32_t)(tx_index + uidLen);
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);
        }

        /* check RES_FLAG */
        if ((0x00 == rx[0]) && (ptxStatus_Success == status))
        {
            t5tComp->isSelected = PTX_T5T_SELECTED_TRUE;
        }
        else
        {
            t5tComp->isSelected = PTX_T5T_SELECTED_FALSE;
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TSleep (ptxNativeTag_T5T_t *t5tComp,
                                   uint8_t optionFlag,
                                   uint8_t *uid,
                                   uint8_t uidLen,
                                   uint8_t *rx,
                                   size_t *rxLen,
                                   uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != rx) && (NULL != rxLen) && (NULL != uid) && (PTX_T5T_UID_SIZE == uidLen))
    {
        /* set T5T command-header */
        status = ptxNativeTag_T5TSetCommandHeader(t5tComp, PTX_T5T_SLPV_REQ_CODE, optionFlag, &tx_index);

        /* set command specific parameters */
        if (ptxStatus_Success == status)
        {
            (void)memcpy(&t5tComp->TxBuffer[tx_index], uid, uidLen);
            tx_index = (uint32_t)(tx_index + uidLen);
        }

        /* everything OK so far ? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T5TTransceive(t5tComp, T5T_FrameType_Standard, tx_index, rx, rxLen, msTimeout);

            /* Attention: A successful SLEEP-command doesn't send a response - therefore overwrite the status for the caller */
            if ((((ptxStatus_TimeOut == PTX_GET_STATUS(status)) || (ptxStatus_NscRfError == PTX_GET_STATUS(status))) && (0 == *rxLen)))
            {
                status = ptxStatus_Success;
            }
        }

        if ((ptxStatus_Success == status) && (0 == memcmp(&t5tComp->UID[0], &uid[0], PTX_T5T_UID_SIZE)))
        {
            t5tComp->isSelected = PTX_T5T_SELECTED_FALSE;
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T5TSetUID (ptxNativeTag_T5T_t *t5tComp,
                                    uint8_t *uid,
                                    uint8_t uidLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T))
    {
        status = ptxNativeTag_T5THandleRfReset(t5tComp);
        if (ptxStatus_Success == status)
        {
            if (0 != uidLen)
            {
                /* check if new UID is being set if in SELECTED mode, if yes exit SELECTED mode */
                if ((NULL != t5tComp->UID) && (PTX_T5T_SELECTED_TRUE == t5tComp->isSelected))
                {
                    0 != memcmp(t5tComp->UID, uid, PTX_T5T_UID_SIZE) ? (t5tComp->isSelected = PTX_T5T_SELECTED_FALSE) : (t5tComp->isSelected = PTX_T5T_SELECTED_TRUE);
                }

                if ((PTX_T5T_UID_SIZE == uidLen) && (NULL != uid))
                {
                    t5tComp->UID = uid;
                    t5tComp->UIDLen = uidLen;
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
                }
            }
            else
            {
                t5tComp->UID = NULL;
                t5tComp->UIDLen = 0;
                t5tComp->isSelected = PTX_T5T_SELECTED_FALSE;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;

}

ptxStatus_t ptxNativeTag_T5TClose (ptxNativeTag_T5T_t *t5tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T))
    {
        memset(t5tComp, 0, sizeof(ptxNativeTag_T5T_t));
        t5tComp->CompId = ptxStatus_Comp_None;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNativeTag_T5TSetCommandHeader(ptxNativeTag_T5T_t *t5tComp, uint8_t commandCode, uint8_t optionFlag, uint32_t *bytesWritten)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t req_flag = PTX_T5T_REQ_FLAG_TEMPLATE_MASK;
    uint32_t index = 0;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T) && (NULL != bytesWritten))
    {
        status = ptxNativeTag_T5THandleRfReset(t5tComp);
        if (ptxStatus_Success == status)
        {
            /* set Option-Flag ? */
            if (0 != optionFlag)
            {
                req_flag = (uint8_t)(req_flag | PTX_T5T_REQ_FLAG_OPTION_BIT_SET_MASK);
            }

            /* use Adressed-Mode ? */
            if ((NULL != t5tComp->UID) || ((PTX_T5T_SELECT_REQ_CODE == commandCode) || (PTX_T5T_SLPV_REQ_CODE == commandCode)))
            {
                req_flag = (uint8_t)(req_flag | PTX_T5T_REQ_FLAG_AMS_SET_MASK);
            }

            /* SELECTED mode? */
            if ((PTX_T5T_SELECTED_FALSE != t5tComp->isSelected) && (PTX_T5T_SELECT_REQ_CODE != commandCode))
            {
                req_flag = (uint8_t)(req_flag | PTX_T5T_REQ_FLAG_SELECT_FLAG_SET_MASK);
                req_flag = (uint8_t)(req_flag & ~PTX_T5T_REQ_FLAG_AMS_SET_MASK);
            }

            /* set REQ_FLAG-byte */
            t5tComp->TxBuffer[index] = req_flag;
            index++;

            /* set command-code */
            t5tComp->TxBuffer[index] = commandCode;
            index++;

            switch (commandCode)
            {
                case PTX_T5T_SELECT_REQ_CODE:
                case PTX_T5T_SLPV_REQ_CODE:
                    /* don't insert UID here - done by caller because the UID is treated as payload in this case */
                    break;

                default:
                    /* set UID (optional) */
                    if ((0 != (req_flag & PTX_T5T_REQ_FLAG_AMS_SET_MASK)) && (NULL != t5tComp->UID))
                    {
                        (void)memcpy(&t5tComp->TxBuffer[index], &t5tComp->UID[0], t5tComp->UIDLen);
                        index = (uint32_t)(index + t5tComp->UIDLen);
                    }
                    break;
            }
            *bytesWritten = index;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNativeTag_T5TTransceive(ptxNativeTag_T5T_t *t5tComp, ptxNativeTag_T5TFrameType frameType, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_value;
    size_t rx_len;
    uint8_t retry_cnt = 0;

    /* Aside from actual Tag-component, remaining parameters are checked within Data-Exchange function */
    if ((PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T)) && (NULL != rxLen))
    {
        ptxIoTRd_t* stack_comp = (ptxIoTRd_t*)t5tComp->IotRd;

        timeout_value = (T5T_FrameType_Special == frameType) ? 1 : msTimeout;

        while (PTX_T5T_MAX_NR_RETRIES >= retry_cnt)
        {
            /* send command */
            rx_len = *rxLen;
            status = ptxIoTRd_Data_Exchange (stack_comp, &t5tComp->TxBuffer[0], txLen, rx, (uint32_t*)&rx_len, timeout_value);

            if ((T5T_FrameType_Special != frameType) &&
                    ((ptxStatus_TimeOut == PTX_GET_STATUS(status)) || (PTX_T5T_CRC_ERROR_INTERNAL == rx[rx_len-1])) &&
                    (PTX_T5T_SLPV_REQ_CODE != t5tComp->TxBuffer[PTX_T5T_COMMANDCODE_FRAME_OFFSET]) &&
                    (0 == t5tComp->isSelected))
            {
                /* Retry possibly needed. */
                retry_cnt++;
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_NscRfError);
            }
            else
            {
                /* No retry needed. */
                retry_cnt = PTX_T5T_MAX_NR_RETRIES + 1u;
            }

            /* Special-Frame format is supposed to return a timeout-error after the first RF-exchange -> reset the status */
            if ((T5T_FrameType_Special == frameType) && (ptxStatus_TimeOut == PTX_GET_STATUS(status)))
            {
                status = ptxStatus_Success;
            }

            if (ptxStatus_Success == status)
            {
                /* special-frame format i.e. isolated EoF required ? */
                if (T5T_FrameType_Special == frameType)
                {
                    (void)ptxPLAT_Sleep(stack_comp->Plat, PTX_T5T_FDT_V_EOF);

                    rx_len = *rxLen;
                    status = ptxIoTRd_T5T_IsolatedEoF (stack_comp, rx, (uint32_t*)&rx_len, msTimeout);
                }
            }

            /* handle reception part */
            if (ptxStatus_Success == status)
            {
                if (0 != rx_len)
                {
                    /*
                     * Last byte of received data from chip indicates additional status / RF error flags.
                     * If everything was OK (value == 0), cut it from the received length, otherwise report an error to upper layer.
                     */
                    if (0 == rx[rx_len - 1])
                    {
                        *rxLen = (size_t)(rx_len - 1);

                    }
                    else
                    {
                        *rxLen = 0;
                        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_NscRfError);
                    }
                }
                else
                {
                    *rxLen = 0;
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_NscRfError);
                }
            }
        }

        if (ptxStatus_Success != status)
        {
            *rxLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNativeTag_T5THandleRfReset (ptxNativeTag_T5T_t *t5tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t5tComp, ptxStatus_Comp_NativeTag_T5T))
    {
        ptxIoTRd_t *iotRd_internal = t5tComp->IotRd;

        if (NULL != iotRd_internal)
        {
            if (0 != iotRd_internal->rfResetFlag)
            {
                t5tComp->isSelected = PTX_T5T_SELECTED_FALSE;
                iotRd_internal->rfResetFlag = 0;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T5T, ptxStatus_InvalidParameter);
    }

    return status;
}


