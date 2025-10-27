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
    Module      : NSC
    File        : ptxNSC_Intf_SPI.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxNSC_Intf.h"
#include "ptxNSC.h"
#include "ptxNSC_Hal.h"
#include "ptxNSC_Registers.h"
#include "ptxPLAT.h"
#include "ptxStatus.h"

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxNSC_HAL_Read(struct ptxNSC *nscCtx, uint8_t *rxBuf[], size_t *rxLen[], size_t numBuffers);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

void ptxNSC_GetRx (ptxNSC_t *nscCtx)
{
    const size_t RX_BUFFER_MAX_SIZE = 256;
    const size_t NUMBER_MAX_BUFFERS = 1;

    uint8_t rx_buff[RX_BUFFER_MAX_SIZE];

    if (NULL != nscCtx)
    {
        ptxNSC_t *pNscCtx = (ptxNSC_t *)nscCtx;

        if (ptxStatus_Comp_NSC == pNscCtx->CompId)
        {
            /* IRQ has been triggered by the PTX1K */

            ptxStatus_t status = ptxStatus_Success;
            uint8_t rx_buff_len = 0u;

            status = ptxNSC_HAL_Rra(pNscCtx, HIF_RBF_LEN_REG, &rx_buff_len);

            if ((ptxStatus_Success == status) && (rx_buff_len > 0))
            {
                size_t payloadLen = rx_buff_len;

                uint8_t *rxBuf[NUMBER_MAX_BUFFERS];
                size_t *rxLen[NUMBER_MAX_BUFFERS];

                rxBuf[0] = &rx_buff[0];
                rxLen[0] = &payloadLen;

                /* Let's read PTX1K Buffer. */
                status = ptxNSC_HAL_Read(pNscCtx, rxBuf, rxLen, NUMBER_MAX_BUFFERS);

                if (ptxStatus_Success == status)
                {
                	/* Let's dispatch the NSC received message. */
                	status = ptxNSC_Process (pNscCtx, &rx_buff[0], payloadLen);
                }
            }
        }
    }
}

ptxStatus_t ptxNSC_HAL_WriteBuffer(ptxNSC_t *nscCtx, ptxNscHal_BufferId_t bufferId, uint8_t *txBuf[], size_t txLen[], size_t numBuffers)
{
    ptxStatus_t status = ptxStatus_Success;
    const size_t max_num_buffers = 3u;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (bufferId < NscWriteBuffer_Max)
            && (NULL != txBuf) && (NULL != txLen) && (numBuffers > 0) && (numBuffers <= max_num_buffers))
    {
        const uint8_t ptxNsc_Buffer_Add_Mask = 0x1F;
        const uint8_t ptxNsc_Buffer_Id_Mask = 0x07;
        const size_t num_buf = numBuffers + 1u;
        uint8_t address_write;
        uint8_t *tx_buf       [num_buf];
        size_t tx_len         [num_buf];

        address_write = (uint8_t)((((uint8_t)bufferId) & ptxNsc_Buffer_Add_Mask) | ((ptxNsc_Buffer_Id_Mask) << 5u));

        /* Send to HAL with the same concept of the array of buffers to prevent double allocation on the Stack of the uCode */
        tx_buf[0] = &address_write;
        tx_len[0] = 1u;

        for (size_t i=1; i < num_buf; i++)
        {
            if (txBuf[i-1u] != NULL)
            {
                tx_buf[i] = txBuf[i-1u];
                tx_len[i] = txLen[i-1u];
            } else
            {
                /* Wrong buffer provided */
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
                break;
            }
        }

        if (ptxStatus_Success == status)
        {
            status = ptxPLAT_TRx(nscCtx->Plat, tx_buf, tx_len, num_buf, NULL, NULL, 0, 0);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxNSC_HAL_Wra(ptxNSC_t *nscCtx, uint16_t address, uint8_t value)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        const size_t num_buf = 2u;
        const size_t addr_write_len = 2u;
        uint8_t address_write [addr_write_len];
        uint8_t *tx_buf       [num_buf];
        size_t tx_len         [num_buf];

        address_write[0] = (uint8_t)((address & 0xFF00u) >> 8) | (uint8_t)((uint8_t)PTX_NSC_HAL_WRITE_RANDOM_ADDRESS_MASK << 5);
        address_write[1] = (uint8_t) (address & 0x00FFu);

        /*
         * Two buffers are used
         */
        tx_buf[0] = &address_write[0];
        tx_len[0] = 2;

        tx_buf[1] = &value;
        tx_len[1] = 1;

        status = ptxPLAT_TRx(nscCtx->Plat, tx_buf, tx_len, num_buf, NULL, NULL, 0, 0);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_HAL_WriteInstruction(ptxNSC_t *nscCtx, uint16_t address, uint8_t *pPayload, size_t txLen )
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != pPayload) && (txLen > 0))
    {
        const size_t num_buf = 2u;
        const size_t addr_write_len = 2u;
        uint8_t address_write [addr_write_len];
        uint8_t *tx_buf       [num_buf];
        size_t tx_len         [num_buf];

        address_write[0] = (uint8_t)((address & 0xFF00u) >> 8) | (uint8_t)((uint8_t)PTX_NSC_HAL_WRITE_INSTRUCTION_MASK << 5);
        address_write[1] = (uint8_t) (address & 0x00FFu);

        /*
         * Two buffers are used
         */
        tx_buf[0] = &address_write[0];
        tx_len[0] = 2;

        tx_buf[1] = pPayload;
        tx_len[1] = txLen;

        status = ptxPLAT_TRx(nscCtx->Plat, tx_buf, tx_len, num_buf, NULL, NULL, 0, 0);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_HAL_Rra(ptxNSC_t *nscCtx, uint16_t address, uint8_t *value)
{
    ptxStatus_t status = ptxStatus_Success;

    /*
     * Each interface has its own peculiarities. What is common and required is the address to read from.
     * Also, valid pointer for the returned data has to be provided.
     * It is expected to read a single byte from the given address.
     *
     * Here we have to provide the local rx buffer big enough to handle all interface�s response data.
     * For SPI: it is required to read 4By to get 1By of data.
     */
    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != value))
    {
        const size_t num_buf_tx = 1u;
        uint8_t *tx_buf[num_buf_tx];
        size_t tx_len[num_buf_tx];

        const size_t num_buf_rx = 1u;
        uint8_t *rx_buf[num_buf_rx];
        size_t *rx_buff_len[num_buf_rx];

        const size_t addr_read_len = PTX_NSC_HAL_ADDRESS_LENGTH;
        uint8_t address_to_read[addr_read_len];

        size_t rx_len = 2u;
        uint8_t rx_buf_local[rx_len];

        address_to_read[0] = (uint8_t)((address & 0xFF00u) >> 8) | (uint8_t)((uint8_t)PTX_NSC_HAL_READ_RANDOM_ADDRESS_MASK << 5);
        address_to_read[1] = (uint8_t) (address & 0x00FFu);

        /*
         * One buffer is used for TX
         */
        tx_buf[0] = &address_to_read[0];
        tx_len[0] = PTX_NSC_HAL_ADDRESS_LENGTH;

        /*
         * One buffer is used for RX
         */
        rx_buf[0] = &rx_buf_local[0];
        rx_buff_len[0] = &rx_len;

        status = ptxPLAT_TRx(nscCtx->Plat, tx_buf, tx_len, num_buf_tx, rx_buf, rx_buff_len, num_buf_rx, 0);

        if(ptxStatus_Success == status)
        {
            if(rx_len > 0)
            {
                *value = rx_buf_local[1u];
            } else
            {
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_SetMode (ptxNSC_t *nscCtx, ptxNSC_Mode_t newMode)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) &&
                ((NscMode_HW == newMode) || (NscMode_SYS == newMode)))
    {
        nscCtx->NscMode = newMode;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;

}

ptxStatus_t ptxNSC_GetMode (ptxNSC_t *nscCtx, ptxNSC_Mode_t *currentMode)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC)) && (NULL != currentMode))
    {
        *currentMode = nscCtx->NscMode;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_SoftReset(ptxNSC_t *nscCtx)
{
    ptxStatus_t st = ptxStatus_Success;

    const uint32_t ms_to_reset = 10;

    /*
     * Soft Reset Done in HW Mode
     */
    (void) ptxNSC_SetMode(nscCtx, NscMode_HW);

    /*
     * The result of the WRA of the soft Reset is uncertain. It is possible that the NSC does
     * not acknowledge it properly as it is resetting itself.
     * However, here we expect only that Wra operation finishes successfully, nothing else.
     */
    (void) ptxNSC_HAL_Wra(nscCtx, SYS_CONTROL_REG, SYS_CONTROL_REG_SYS_SOFT_RESET_MASK);

    /*
     * Sleep time needed to ensure that the actual Reset has been performed.
     */
    st = ptxPLAT_Sleep(nscCtx->Plat, ms_to_reset);

    return st;
}

ptxStatus_t ptxNSC_GetInitConfigParams(ptxNSC_t *nscCtx, uint32_t baudRate, uint8_t *uartConfig)
{
    ptxStatus_t st = ptxStatus_Success;

    (void)nscCtx;
    (void)baudRate;
    (void)uartConfig;

    return st;
}

static ptxStatus_t ptxNSC_HAL_Read(ptxNSC_t *nscCtx, uint8_t *rxBuf[], size_t *rxLen[], size_t numBuffers)
{
    ptxStatus_t status = ptxStatus_Success;
    const size_t max_num_buffers = 4u;
    uint8_t read_buffer_command = ((uint8_t)PTX_NSC_HAL_READ_BUFFER_MASK) << 5;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != rxBuf) && (NULL != rxLen)
                                        && (numBuffers > 0) && ((numBuffers + 1u) <= max_num_buffers))
    {
        for (size_t i=0; i< numBuffers;i++)
        {
            if((rxBuf[i] != NULL) && (rxLen[i] != NULL))
            {
                if((*(rxLen[i]) > 0) && (*(rxLen[i])<= PTX_NSC_HAL_BUFFER_LENGTH_RX_MAX))
                {
                    /* Rx Buffer correct. */
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
                    break;
                }
            } else
            {
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
                break;
            }
        }

        if (ptxStatus_Success == status)
        {
            /* Num buffers at this level. */
            size_t num_buffers = 1u + numBuffers;

            uint8_t *tx_buf                [1u];
            size_t tx_len                  [1u];

            uint8_t *rx_buf                [max_num_buffers];
            size_t *rx_len                 [max_num_buffers];

            tx_buf[0] = &read_buffer_command;
            tx_len[0] = 1u;

            size_t nsc_len_len = 1u;
            uint8_t nsc_len = 0;
            rx_buf[0] = &nsc_len;
            rx_len[0] = &nsc_len_len;

            for (size_t i=1; i < num_buffers; i++)
            {
                rx_buf[i] = rxBuf[i-1u];
                rx_len[i] = rxLen[i-1u];
            }

            status = ptxPLAT_TRx(nscCtx->Plat, tx_buf, tx_len, 1u, rx_buf, rx_len, 1u + numBuffers, 0);

            if(ptxStatus_Success == status)
            {
                size_t total_rx_payload = 0;
                for (size_t i=1; i < num_buffers; i++)
                {
                    total_rx_payload += *(rx_len[i]);
                }

                /* Higher level protocol data size: nsc_len */
                if (nsc_len <= total_rx_payload)
                {
                    /* Successful read operation so update length variable */
                    *rxLen[0] = nsc_len;
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InsufficientResources);
                }
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}


