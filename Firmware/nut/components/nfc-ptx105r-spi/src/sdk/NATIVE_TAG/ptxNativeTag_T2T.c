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
    File        : ptxNativeTag_T2T.c

    Description : Native Tag API for NFC Forum Tag Type 2 (IOT READER - Extension)
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptx_IOT_READER.h"
#include "ptxNativeTag_T2T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \name T2T Operation codes.
 * @{
 */
#define PTX_T2TOP_READ_CODE               (uint8_t)0x30     /**< Read operation code. */
#define PTX_T2TOP_WRITE_CODE              (uint8_t)0xA2     /**< Write operation code. */
#define PTX_T2TOP_SECTOR_SELECT_CODE      (uint8_t)0xC2     /**< Sector Select operation code. */
#define PTX_T2TOP_ACK_CODE                (uint8_t)0x0A     /**< T2T Acknowledge response byte. */
/** @} */
/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Build and set T2T command header to internal Tx buffer.
 *
 * \param[in] t2tComp           Pointer to component.
 * \param[in] commandCode       Command code.
 * \param[in] blockNr           Block number.
 * \param[in,out] bytesWritten  Tx buffer offset.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T2TSetCommandHeader(ptxNativeTag_T2T_t *t2tComp, uint8_t commandCode, uint8_t blockNr, uint32_t *bytesWritten);

/**
 * \brief Data exchange.
 *
 * \param[in] t2tComp           Pointer to component.
 * \param[in] txLen             Length of data to be sent.
 * \param[out] rx               Response buffer.
 * \param[in,out] rxLen         Length of response buffer on input, length of response on output.
 * \param[in] msTimeout         Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful.
 */
static ptxStatus_t ptxNativeTag_T2TTransceive(ptxNativeTag_T2T_t *t2tComp, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNativeTag_T2TOpen (ptxNativeTag_T2T_t *t2tComp, ptxNativeTag_T2T_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != t2tComp) && (NULL != initParams))
    {
        if ((NULL != initParams->IotRd) && (NULL != initParams->TxBuffer) && (PTX_T2T_MIN_TX_BUFFER_SIZE <= initParams->TxBufferSize))
        {
            /* clear component */
            (void)memset(t2tComp, 0, sizeof(ptxNativeTag_T2T_t));

            /* set members */
            t2tComp->CompId = ptxStatus_Comp_NativeTag_T2T;
            t2tComp->IoTRdStackComp = initParams->IotRd;
            t2tComp->TxBuffer = initParams->TxBuffer;

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T2TRead (ptxNativeTag_T2T_t *t2tComp,
                                  uint8_t blockNr,
                                  uint8_t *rx,
                                  size_t *rxLen,
                                  uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T2T command header */
        status = ptxNativeTag_T2TSetCommandHeader(t2tComp, PTX_T2TOP_READ_CODE, blockNr, &tx_index);
        /* everything ok so far ? - send data*/
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T2TTransceive(t2tComp, tx_index, rx, rxLen, msTimeout);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T2TWrite (ptxNativeTag_T2T_t *t2tComp,
                                   uint8_t blockNr,
                                   uint8_t *blockData,
                                   uint8_t blockDataLen,
                                   uint8_t *rx,
                                   size_t *rxLen,
                                   uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;

    if (PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T) && (NULL != blockData) && (NULL != rx) && (NULL != rxLen))
    {
        /* set T2T command header */
        status = ptxNativeTag_T2TSetCommandHeader(t2tComp, PTX_T2TOP_WRITE_CODE, blockNr, &tx_index);
        /* everything ok so far ? - send data*/
        if (ptxStatus_Success == status)
        {
            (void)memcpy(&t2tComp->TxBuffer[tx_index], blockData, blockDataLen);
            tx_index = (uint32_t)(tx_index + blockDataLen);
        }

        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T2TTransceive(t2tComp, tx_index, rx, rxLen, msTimeout);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T2TSectorSelect (ptxNativeTag_T2T_t *t2tComp,
        uint8_t secNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t tx_index = 0;
    size_t toClear;

    if (PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T) && (NULL != rx) && (NULL != rxLen) && (0xFF > secNr))
    {
        /* sector select does not utilise command header since it does not use a block number */

        /* prepare first packet */
        t2tComp->TxBuffer[tx_index] = PTX_T2TOP_SECTOR_SELECT_CODE;
        tx_index++;
        t2tComp->TxBuffer[tx_index] = 0xFF;
        tx_index++;

        /* everything ok so far ? - send first packet*/
        status = ptxNativeTag_T2TTransceive(t2tComp, tx_index, rx, rxLen, msTimeout);

        if ((ptxStatus_Success == status) && (rx[0] == PTX_T2TOP_ACK_CODE))
        {
            tx_index = 0;

            /* clear the buffer for second packet */
            toClear = sizeof(t2tComp->TxBuffer);
            (void)memset(t2tComp->TxBuffer, 0, toClear);

            /* prepare second packet */
            t2tComp->TxBuffer[tx_index] = secNr;
            tx_index++;
            t2tComp->TxBuffer[tx_index] = 0x00; // 2nd byte is RFU
            tx_index++;
            t2tComp->TxBuffer[tx_index] = 0x00; // 3rd byte is RFU
            tx_index++;
            t2tComp->TxBuffer[tx_index] = 0x00; // 4th byte is RFU
            tx_index++;

            /* send second paket */
            status = ptxNativeTag_T2TTransceive(t2tComp, tx_index, rx, rxLen, msTimeout);

            /* check PAT */
            if (((ptxStatus_NscRfError == PTX_GET_STATUS(status)) || (ptxStatus_TimeOut == PTX_GET_STATUS(status))) && (*rxLen == 0))
            {
                status = ptxStatus_Success;
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_ProtocolError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxNativeTag_T2TClose (ptxNativeTag_T2T_t *t2tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T))
    {
        memset(t2tComp, 0, sizeof(ptxNativeTag_T2T_t));
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNativeTag_T2TSetCommandHeader(ptxNativeTag_T2T_t *t2tComp, uint8_t commandCode, uint8_t blockNr, uint32_t *bytesWritten)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t index = 0;

    if (PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T) && (NULL != bytesWritten))
    {
        /* set command code */
        t2tComp->TxBuffer[index] = commandCode;
        index++;

        /* set block number */
        t2tComp->TxBuffer[index] = blockNr;
        index++;

        *bytesWritten = index;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}
static ptxStatus_t ptxNativeTag_T2TTransceive(ptxNativeTag_T2T_t *t2tComp, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_value = msTimeout;
    size_t rx_len = 0;

    /* Aside from actual Tag-component, remaining parameters are checked within Data-Exchange function */
    if ((PTX_COMP_CHECK(t2tComp, ptxStatus_Comp_NativeTag_T2T)) && (NULL != rx) && (NULL != rxLen))
    {
        /* send command */
        rx_len = *rxLen;
        status = ptxIoTRd_Data_Exchange (t2tComp->IoTRdStackComp, &t2tComp->TxBuffer[0], txLen, rx, (uint32_t*)&rx_len, timeout_value);

        /* handle reception part */
        if (ptxStatus_Success == status)
        {
            if (0 != rx_len)
            {
                /*
                 * Last byte of received data from chip indicates additional status / RF error flags.
                 * If everything was OK (value == 0 or value == 4 (T2T only)), cut it from the received length,
                 * otherwise report an error to upper layer.
                 */
                if (0 == rx[rx_len - 1] || 4 == rx[rx_len - 1])
                {
                    *rxLen = (uint32_t)(rx_len - 1);
                }
                else
                {
                    *rxLen = 0;
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_NscRfError);
                }
            }
            else
            {
                *rxLen = 0;
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_NscRfError);
            }
        }
        else
        {
            *rxLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T2T, ptxStatus_InvalidParameter);
    }

    return status;
}

