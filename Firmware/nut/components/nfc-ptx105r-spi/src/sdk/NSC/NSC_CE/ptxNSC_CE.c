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
    File        : ptxNSC_CE.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxNSC_CE.h"
#include "ptxNSC.h"
#include <string.h>

/*
 * ####################################################################################################################
 * INTERNAL TYPES
 * ####################################################################################################################
 */


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNSC_Set_Listen_RoutingTable_HCE (ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        const uint8_t nsc_set_routing_table_cmd_len = (PTX_NSC_RF_SET_ROUTING_TABLE_CMD_LENGTH);
        const uint8_t nsc_set_routing_table_cmd_opCode = PTX_NSC_RF_SET_ROUTING_TABLE_CMD_OPCODE;
        const uint8_t nsc_set_routing_table_params[11] = {0x0A,             /* Table-Length */
                                                          0x00,             /* no RF_EVENT in HCE */
                                                          0x01, 0x80, 0x10, /* Route Tech-A to Host */
                                                          0x01, 0x81, 0x10, /* Route Tech-B to Host */
                                                          0x01, 0x82, 0x10, /* Route Tech-F to Host */
                                                       };

        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_set_routing_table_cmd_index = 0;
        uint8_t nsc_set_routing_table_cmd [nsc_set_routing_table_cmd_len];

        nsc_set_routing_table_cmd[nsc_set_routing_table_cmd_index] = nsc_set_routing_table_cmd_opCode;
        nsc_set_routing_table_cmd_index++;

        (void)memcpy(&nsc_set_routing_table_cmd[nsc_set_routing_table_cmd_index],
                     &nsc_set_routing_table_params[0],
                     sizeof(nsc_set_routing_table_params));
        nsc_set_routing_table_cmd_index = (uint8_t)(nsc_set_routing_table_cmd_index + sizeof(nsc_set_routing_table_params));

        txBuf[0] = &nsc_set_routing_table_cmd[0];
        txLen[0] = nsc_set_routing_table_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_setrt_resp_len = 0;
            uint8_t *nsc_setrt_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_setrt_resp, &nsc_setrt_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_setrt_resp) && (nsc_setrt_resp_len > 0))
            {
                if ((PTX_NSC_RF_SET_ROUTING_TABLE_RSP_LENGTH == nsc_setrt_resp_len) && (PTX_NSC_RF_SET_ROUTING_TABLE_RSP_OPCODE == nsc_setrt_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_setrt_resp[1u]);
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                }
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    status = ptxNSC_CheckSystemState(nscCtx, status);

    return status;
}

