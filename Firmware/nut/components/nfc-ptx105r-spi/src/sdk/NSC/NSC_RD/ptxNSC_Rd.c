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
    File        : ptxNSC_Rd.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxNSC_Rd.h"
#include "ptxNSC.h"
#include <string.h>


/*
 * ####################################################################################################################
 * INTERNAL TYPES
 * ####################################################################################################################
 */
static ptxStatus_t ptxNSC_NscCltMsg_NumBytes (ptxNSC_t *nscCtx, size_t length, uint8_t *numBitsLastByte, size_t *numBytesTotal);
static ptxStatus_t ptxNSC_NscCltMsg_BuildRawBits (ptxNSC_t *nscCtx, uint8_t *pldBytes, uint8_t *pldParityBits, size_t lengthBytes, uint8_t *pldRawBits, size_t lengthRawBits);


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNSC_Rd_RfPressCheck_Nack (ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        const size_t nsc_RfPressCheckNack_len = 2u;
        uint8_t nsc_RfPressCheckNack[nsc_RfPressCheckNack_len];
        nsc_RfPressCheckNack[0] = PTX_NSC_RFD_CTRL_OPCODE;
        nsc_RfPressCheckNack[1] = PTX_NSC_RFD_CTRL_NACK;

        txBuf[0] = &nsc_RfPressCheckNack[0];
        txLen[0] = nsc_RfPressCheckNack_len;

        /* Let's send the message. */
        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_Rd_RfPressCheck_EmptyFrame (ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        const size_t nsc_RfPressCheckEmptyFrame_len = 2u;
        uint8_t nsc_RfPressCheckEmptyFrame[nsc_RfPressCheckEmptyFrame_len];
        nsc_RfPressCheckEmptyFrame[0] = PTX_NSC_DATA_MSG_OPCODE;
        nsc_RfPressCheckEmptyFrame[1] = 0x00;

        txBuf[0] = &nsc_RfPressCheckEmptyFrame[0];
        txLen[0] = nsc_RfPressCheckEmptyFrame_len;

        /* Let's send the message. */
        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_Rd_RfPressCheck_AttentionCmd (ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        const size_t nsc_RfPressCheckAttCmd_len = 2u;
        uint8_t nsc_RfPressCheckAttCmd[nsc_RfPressCheckAttCmd_len];
        nsc_RfPressCheckAttCmd[0] = PTX_NSC_RFD_CTRL_OPCODE;
        nsc_RfPressCheckAttCmd[1] = PTX_NSC_RFD_CTRL_ATTENTION;

        txBuf[0] = &nsc_RfPressCheckAttCmd[0];
        txLen[0] = nsc_RfPressCheckAttCmd_len;

        /* Let's send the message. */
        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_T5T_IsolatedEoF(struct ptxNSC *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        const size_t nsc_RfT5T_IsolatedEoF_len = 2u;
        uint8_t nsc_RfT5T_IsolatedEoF[nsc_RfT5T_IsolatedEoF_len];
        nsc_RfT5T_IsolatedEoF[0] = PTX_NSC_RFD_CTRL_OPCODE;
        nsc_RfT5T_IsolatedEoF[1] = PTX_NSC_RFD_CTRL_EOF;

        txBuf[0] = &nsc_RfT5T_IsolatedEoF[0];
        txLen[0] = nsc_RfT5T_IsolatedEoF_len;

        /* Let's send the message. */
        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_Rd_RfCltMsg (ptxNSC_t *nscCtx, uint8_t *pldBytes, uint8_t *pldParityBits, size_t length)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != pldBytes) && (NULL != pldParityBits) && (length > 0))
    {
        uint8_t num_bits_last_byte = 0;
        size_t num_bytes_bitstream = 0;

        status = ptxNSC_NscCltMsg_NumBytes (nscCtx, length, &num_bits_last_byte, &num_bytes_bitstream);

        if (status == ptxStatus_Success)
        {
            /* Buffer for NSC_CLT_MSG. */
            size_t len_nsc_clt_msg = 1u /* OpCode*/ + 1u /*length payload*/ + num_bytes_bitstream /*bitstream */ + 1u /*residual bits*/;
            uint8_t nsc_clt_msg [len_nsc_clt_msg];

            status = ptxNSC_NscCltMsg_BuildRawBits (nscCtx, pldBytes, pldParityBits, length, &nsc_clt_msg[2u], num_bytes_bitstream);

            if (ptxStatus_Success == status)
            {
                ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
                uint8_t *txBuf[1u];
                size_t txLen[1u];

                /* Let's fill-in OpCode + length + residual bits. */
                nsc_clt_msg [0] = PTX_NSC_DATA_MSG_OPCODE;
                nsc_clt_msg [1] = (uint8_t)(num_bytes_bitstream + 1u);
                nsc_clt_msg [len_nsc_clt_msg - 1u] = num_bits_last_byte;

                txBuf[0] = &nsc_clt_msg[0];
                txLen[0] = len_nsc_clt_msg;

                /* Let's send the message. */
                status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}


/*
 * ####################################################################################################################
 * HELPER FUNCTIONS
 * ####################################################################################################################
 */
static ptxStatus_t ptxNSC_NscCltMsg_NumBytes (ptxNSC_t *nscCtx, size_t length, uint8_t *numBitsLastByte, size_t *numBytesTotal)
{
    ptxStatus_t ret = ptxStatus_Success;

    if ((NULL != numBitsLastByte) && (NULL != numBytesTotal))
    {
        size_t num_bits = (8 * length) + length;
        size_t is_last_byte = ((num_bits % 8u) == 0) ? 0 : 1u;
        *numBitsLastByte = num_bits % 8u;
        *numBytesTotal = ((num_bits - *numBitsLastByte) / 8u) + is_last_byte;
    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return ret;
}

static ptxStatus_t ptxNSC_NscCltMsg_BuildRawBits (ptxNSC_t *nscCtx, uint8_t *pldBytes, uint8_t *pldParityBits, size_t lengthBytes, uint8_t *pldRawBits, size_t lengthRawBits)
{
    ptxStatus_t ret = ptxStatus_Success;

    if ((NULL != pldBytes) && (NULL != pldParityBits) && (NULL != pldRawBits))
    {
        // Ensure that enough capacity for writing the bitstream
        size_t num_tot_bits = (lengthBytes * 8u) + 8u;
        size_t is_last_byte = ((num_tot_bits % 8u) == 0) ? 0 : 1u;
        size_t num_bytes_bitstream = ((num_tot_bits - (num_tot_bits % 8u)) / 8u) + is_last_byte;

        if (lengthRawBits >= num_bytes_bitstream)
        {
            size_t index_raw_bits = 0;
            size_t index_raw_bytes = 0;
            size_t index_payload = 0;
            uint8_t par_temp = 0;
            (void)memset(pldRawBits, 0, lengthRawBits);

            do
            {
                // Write payload
                if (0 == (index_raw_bits % 8u))
                {
                    // Full byte of payload in a full byte of bitstream
                    pldRawBits[index_raw_bytes] = pldBytes[index_payload];
                } else
                {
                    // Some bits in current byte of bitstream, some others in the next byte
                    uint16_t two_bytes_temp = 0x0000;
                    uint8_t num_shift_bits = index_raw_bits % 8u;
                    two_bytes_temp = (uint16_t)(pldBytes[index_payload] << num_shift_bits);

                    pldRawBits[index_raw_bytes] = (((uint8_t)(0x00FF & two_bytes_temp)) | (pldRawBits[index_raw_bytes]) );
                    pldRawBits[index_raw_bytes + 1u] = ((uint8_t)(0x00FF & ((0xFF00 & two_bytes_temp) >> 8u)));
                }
                index_raw_bits += 8u;
                index_raw_bytes ++;

                // Write parity
                par_temp = (uint8_t)((pldParityBits[index_payload] & 0x01) << (index_raw_bits % 8u));
                pldRawBits[index_raw_bytes] = pldRawBits[index_raw_bytes] | par_temp;
                index_raw_bits ++;
                index_raw_bytes = (0 == (index_raw_bits % 8u)) ? (index_raw_bytes + 1) : index_raw_bytes;

                index_payload ++;

            }while(index_payload < lengthBytes);
        } else
        {
            ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
        }
    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }
    (void)nscCtx;
    return ret;
}
