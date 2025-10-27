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
#include "ptxNativeTag_T3T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \name T3T Specific Defines.
 * @{
 */
#define PTX_T3T_SENSF_REQ_CODE              (uint8_t)0x00       /**< SENSF-REQ operation code. */
#define PTX_T3T_SENSF_REQ_SC_NDEF           (uint16_t)0x12FC    /**< SENSF-REQ Service Code. */
#define PTX_T3T_CHECK_CODE                  (uint8_t)0x06       /**< Check operation code. */
#define PTX_T3T_UPDATE_CODE                 (uint8_t)0x08       /**< Update operation code. */
#define PTX_T3T_NFCID2_LEN                  (uint8_t)0x08       /**< NFCID2 length. */
#define PTX_T3T_NDEF_BLOCKS_PER_OP          (uint8_t)0x01       /**< number of blocks per read for specific operations. */
/** @} */
/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Data Exchange with a T3T.
 *
 * \param[in] t3t                   Pointer to component.
 * \param[in] tx                    Tx Buffer containing Data to be sent.
 * \param[in] txLen                 Length of Component-Internal Tx Buffer Data.
 * \param[in,out] rx                Rx Buffer to store response.
 * \param[in,out] rxLen             Maximum Rx Buffer length on input, Response length on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T3TTransceive (ptxNativeTag_T3T_t *t3tComp,
        uint8_t *tx,
        uint32_t txLen,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout);

/**
 * \brief Create the T3T Command Header.
 *
 * \param[in] t3t                   Pointer to an existing instance of the T2T component.
 * \param[in] commandCode           Command Code to be set.
 * \param[in] NFCID2                ID to address the T3T tag.
 * \param[in] NFCID2Len             Length of ID.
 * \param[in] serviceInfo           Information about Services to be updated.
 * \param[in] blockInfo             Information about Blocks to be updated.
 * \param[in,out] bytesWritten      Current Tx Buffer Index on input, Tx Buffer index after adding Command header on output.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T3TSetCommandHeader (ptxNativeTag_T3T_t *t3tComp,
        uint8_t commandCode,
        uint8_t *NFCID2,
        size_t NFCID2Len,
        ptxNativeTag_T3T_Services_t serviceInfo,
        ptxNativeTag_T3T_Blocks_t blockInfo,
        uint8_t *bytesWritten);
/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNativeTag_T3TOpen (ptxNativeTag_T3T_t *t3tComp, ptxNativeTag_T3T_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != t3tComp) && (NULL != initParams))
    {
        if ((NULL != initParams->IotRd) && (NULL != initParams->TxBuffer) && (PTX_T3T_MIN_TX_BUFFER_SIZE <= initParams->TxBufferSize))
        {
            /* clear component */
            (void)memset(t3tComp, 0, sizeof(ptxNativeTag_T3T_t));

            /* set members */
            t3tComp->CompId = ptxStatus_Comp_NativeTag_T3T;
            t3tComp->IotRd = initParams->IotRd;
            t3tComp->TxBuffer = initParams->TxBuffer;
            t3tComp->MRTI_Check = initParams->MRTI_Check;
            t3tComp->MRTI_Update = initParams->MRTI_Update;
            if (0 != initParams->NFCID2Len)
            {
                if ((PTX_T3T_NFCID2_LEN == initParams->NFCID2Len) && (NULL != initParams->NFCID2))
                {
                    t3tComp->NFCID2 = initParams->NFCID2;
                }
                else
                {
                    /* ignore, set NFCID2 later */
                }
            }
            else
            {
                t3tComp->NFCID2 = NULL;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T3TSetTagParams (ptxNativeTag_T3T_t *t3tComp,
        uint8_t *NFCID2,
        uint8_t NFCID2Len,
        ptxNativeTag_T3T_MRTI_t mrtiInfo)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t mask_a = 0x07;
    uint8_t mask_b = 0x38;
    uint8_t mask_e = 0xC0;
    uint8_t nbr_check_blocks = PTX_T3T_NDEF_BLOCKS_PER_OP;
    uint8_t nbr_update_blocks = PTX_T3T_NDEF_BLOCKS_PER_OP;
    uint32_t timeout_check;
    uint32_t timeout_update;
    uint16_t t_t3t = 1; /* assumed as 1 since it is usually below that */
    uint16_t last_comp_check = 1;
    uint16_t last_comp_update = 1;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T))
    {
        /* set NFCID2 */
        if (0 != NFCID2Len)
        {
            if ((PTX_T3T_NFCID2_LEN == NFCID2Len) && (NULL != NFCID2))
            {
                t3tComp->NFCID2 = NFCID2;
                t3tComp->NFCID2Len = NFCID2Len;
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
            }
        }
        else
        {
            t3tComp->NFCID2 = NULL;
        }

        /* 4^n */
        for (uint8_t i = 0; i < ((mrtiInfo.MRTICheck & mask_e)>>6u); i++)
        {
            last_comp_check = (uint16_t)(last_comp_check * 4u);
        }
        for (uint8_t i = 0; i < ((mrtiInfo.MRTIUpdate & mask_e)>>6u); i++)
        {
            last_comp_update = (uint16_t)(last_comp_update * 4u);
        }

        /* set timeout value */
        timeout_check = t_t3t * ((((mrtiInfo.MRTICheck & mask_a)+1u) + nbr_check_blocks*(((mrtiInfo.MRTICheck & mask_b)>>3u)+1u)) * last_comp_check);
        timeout_update = t_t3t * ((((mrtiInfo.MRTIUpdate & mask_a)+1u) + nbr_update_blocks*(((mrtiInfo.MRTIUpdate & mask_b)>>3u)+1u)) * last_comp_update);

        timeout_check += 20;        /* added to avoid floating point */
        timeout_update += 20;

        t3tComp->TagTimeoutCheck = timeout_check;
        t3tComp->TagTimeoutUpdate = timeout_update;

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;

}

ptxStatus_t ptxNativeTag_T3TSENSF_REQ (ptxNativeTag_T3T_t *t3tComp,
                                       uint16_t sc,
                                       uint8_t rc,
                                       uint8_t tsn,
                                       uint8_t *rx,
                                       size_t *rxLen,
                                       uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T) && (0 != t3tComp->NFCID2Len) && (NULL != rx) && (NULL != rxLen))
    {
        status = ptxIoTRd_T3T_SENSFRequest(t3tComp->IotRd, sc, rc, tsn, rx, (uint32_t*)rxLen, msTimeout);

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T3TCheck (ptxNativeTag_T3T_t *t3tComp,
                                   uint8_t *NFCID2,
                                   size_t NFCID2Len,
                                   ptxNativeTag_T3T_Services_t serviceInfo,
                                   ptxNativeTag_T3T_Blocks_t blockInfo,
                                   uint8_t *rx,
                                   size_t *rxLen,
                                   uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t tx_index = 0;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T) && (0 != t3tComp->NFCID2Len) && (NULL != NFCID2) && (NULL != serviceInfo.ServiceCodeList) && (NULL != blockInfo.BlockList) && (NULL != rx) && (NULL != rxLen))
    {
        status = ptxNativeTag_T3TSetCommandHeader(t3tComp,PTX_T3T_CHECK_CODE,NFCID2,NFCID2Len,serviceInfo,blockInfo,&tx_index);

        if (ptxStatus_Success == status)
        {
            /* no need to add the block data field, send data */
            status = ptxNativeTag_T3TTransceive(t3tComp,&t3tComp->TxBuffer[0],tx_index,rx,rxLen,msTimeout);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T3TUpdate (ptxNativeTag_T3T_t *t3tComp,
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
    uint8_t tx_index = 0;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T) && (0 != t3tComp->NFCID2Len) && (NULL != NFCID2) && (NULL != serviceInfo.ServiceCodeList) && (NULL != blockInfo.BlockList) && (NULL != rx) && (NULL != rxLen) && (NULL != blockData))
    {
        status = ptxNativeTag_T3TSetCommandHeader(t3tComp,PTX_T3T_UPDATE_CODE,NFCID2,NFCID2Len,serviceInfo,blockInfo,&tx_index);

        if (ptxStatus_Success == status)
        {
            /* add the block data to the tx buffer */
            (void)memcpy(&t3tComp->TxBuffer[tx_index],&blockData[0],(uint32_t)blockDataLen);
            tx_index = (uint8_t)(tx_index + blockDataLen);

            /* send data */
            status = ptxNativeTag_T3TTransceive(t3tComp,&t3tComp->TxBuffer[0],tx_index,rx,rxLen,msTimeout);
        }
    }

    return status;
}

ptxStatus_t ptxNativeTag_T3TClose (ptxNativeTag_T3T_t *t3tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T))
    {
        memset(t3tComp, 0, sizeof(ptxNativeTag_T3T_t));
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNativeTag_T3TTransceive (ptxNativeTag_T3T_t *t3tComp,
        uint8_t *tx,
        uint32_t txLen,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_value;
    size_t rx_len;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T) && (NULL != tx) && (0 != t3tComp->NFCID2Len) && (NULL != rx) && (NULL != rxLen))
    {
        timeout_value = msTimeout;
        rx_len = *rxLen;

        status = ptxIoTRd_Data_Exchange (t3tComp->IotRd, tx, txLen, rx, (uint32_t*)&rx_len, timeout_value);

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
                    *rxLen = (uint32_t)(rx_len - 1);
                }
                else
                {
                    *rxLen = 0;
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_NscRfError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_NscRfError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNativeTag_T3TSetCommandHeader (ptxNativeTag_T3T_t *t3tComp,
        uint8_t commandCode,
        uint8_t *NFCID2,
        size_t NFCID2Len,
        ptxNativeTag_T3T_Services_t serviceInfo,
        ptxNativeTag_T3T_Blocks_t blockInfo,
        uint8_t *bytesWritten)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t tx_index = 0;

    if (PTX_COMP_CHECK(t3tComp, ptxStatus_Comp_NativeTag_T3T) && (0 != NFCID2Len) && (NULL != NFCID2) && (NULL != serviceInfo.ServiceCodeList) && (NULL != blockInfo.BlockList))
    {
        /* length byte prepended later */
        t3tComp->TxBuffer[tx_index] = commandCode;
        tx_index++;
        (void)memcpy(&t3tComp->TxBuffer[tx_index],&NFCID2[0],(uint32_t)NFCID2Len);
        tx_index = (uint8_t)(tx_index + PTX_T3T_NFCID2_LEN);
        t3tComp->TxBuffer[tx_index] = serviceInfo.NOS;
        tx_index++;
        (void)memcpy(&t3tComp->TxBuffer[tx_index],&serviceInfo.ServiceCodeList[0],(uint32_t)serviceInfo.ServiceCodeListLen);
        tx_index = (uint8_t)(tx_index + serviceInfo.ServiceCodeListLen);
        t3tComp->TxBuffer[tx_index] = blockInfo.NOB;
        tx_index++;
        (void)memcpy(&t3tComp->TxBuffer[tx_index],&blockInfo.BlockList[0],(uint32_t)blockInfo.BlockListLen);
        tx_index = (uint8_t)(tx_index + blockInfo.BlockListLen);

        *bytesWritten = tx_index;

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T3T, ptxStatus_InvalidParameter);
    }

    return status;
}

