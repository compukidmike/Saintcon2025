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
    File        : ptxNSC_Ext.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxNSC.h"
#include "ptxPLAT.h"
#include "ptxNSC_Hal.h"
#include <string.h>


/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define NSC_ACT_LEN1    4u
#define NSC_ACT_LEN2    7u
#define NSC_ACT_LEN3    10u


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static void ptxNSC_ProcessRfData (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessRfDataChained (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessRfCltMsg (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static ptxStatus_t ptxNSC_NscCltMsg_DecodeRawBits (ptxNSC_t *nscCtx, uint8_t *pldRawBits, size_t lengthRawBits, uint8_t residualBits,
                                                                        uint8_t *pldBytes, uint8_t *pldParityBits, size_t *lengthBytes, size_t *numTotalBits);

static void ptxNSC_ProcessNtfRfAct (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessNtfRfDisc (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessNtfRf_ActDisc (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);

static void ptxNSC_ProcessRfCtrl (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);

static void ptxNSC_ProcessNtfRfDeact (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessNtfRfError (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessNtfRfTimeoutError (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);
static void ptxNSC_ProcessNtfLpcd (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);

static void ptxNSC_ProcessNtfRfField (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event);

void ptxNSC_ProcessNtf (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen);

/*
 * Build Rf Activation Tech params for Type A.
 */
static size_t ptxNSC_RfAct_TechParams_A (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, ptxNSC_RfActTech_A_Param_t* actTech_A);
/*
 * Build Rf Activation Tech params for Type B.
 */
static size_t ptxNSC_RfAct_TechParams_B (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, ptxNSC_RfActTech_B_Param_t* actTech_B);
/*
 * Build Rf Activation Tech params for Type F.
 */
static size_t ptxNSC_RfAct_TechParams_F (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, ptxNSC_RfActTech_F_Param_t* actTech_F);
/*
 * Build Rf Activation Tech params for Type V.
 */
static size_t ptxNSC_RfAct_TechParams_V (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, ptxNSC_RfActTech_V_Param_t* actTech_V);
/*
 * Calculates exclusive OR of an array.
 */
static ptxStatus_t ptxNSC_RfActivate_Xor (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, uint8_t *xorResult);


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxNSC_RfActivate(ptxNSC_t *nscCtx, ptxNSC_RfActiv_Param_t *nscRfActPars, uint8_t *activationData, size_t *activationDataLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscRfActPars) && (NULL != activationData) &&
            (NULL != activationDataLen))
    {
        const size_t nsc_act_cmd_len = (PTX_NSC_RF_ACTIVATE_CMD_PARAMS_MAXLENGTH + 1);
        const uint8_t nsc_act_cmd_opCode = PTX_NSC_RF_ACTIVATE_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];
        uint8_t nsc_act_cmd [nsc_act_cmd_len];
        uint8_t nsc_act_cmd_index = 0;

        size_t  bytes_w = 0;
        size_t  bytes_not_w;

        nsc_act_cmd[nsc_act_cmd_index++] = nsc_act_cmd_opCode;

        /*
         * Write Rf Technology.
         */
        nsc_act_cmd[nsc_act_cmd_index++] = nscRfActPars->RfTech;

        bytes_not_w = nsc_act_cmd_len - nsc_act_cmd_index;

        /*
         * Write Rf Act Technology parameters.
         */
        switch (nscRfActPars->RfTech)
        {
            case PTX_NSC_TYPES_TECH_A:
                nscRfActPars->RfTechActParams.RfAct_A_Params.DeviceRfState = (0 != nscRfActPars->UseShortActivation) ? (uint8_t)0 : (uint8_t)1;
                bytes_w = ptxNSC_RfAct_TechParams_A (nscCtx, &nsc_act_cmd[nsc_act_cmd_index], bytes_not_w,
                                                        &nscRfActPars->RfTechActParams.RfAct_A_Params);
                break;

            case PTX_NSC_TYPES_TECH_B:
                bytes_w = ptxNSC_RfAct_TechParams_B (nscCtx, &nsc_act_cmd[nsc_act_cmd_index], bytes_not_w,
                                                        &nscRfActPars->RfTechActParams.RfAct_B_Params);
                break;

            case PTX_NSC_TYPES_TECH_F:
                bytes_w = ptxNSC_RfAct_TechParams_F (nscCtx, &nsc_act_cmd[nsc_act_cmd_index], bytes_not_w,
                                                        &nscRfActPars->RfTechActParams.RfAct_F_Params);
                break;

            case PTX_NSC_TYPES_TECH_V:
                bytes_w = ptxNSC_RfAct_TechParams_V (nscCtx, &nsc_act_cmd[nsc_act_cmd_index], bytes_not_w,
                                                        &nscRfActPars->RfTechActParams.RfAct_V_Params);
                break;

            default:
                bytes_w = 0;
                break;
        }

        nsc_act_cmd_index = (uint8_t)(nsc_act_cmd_index + bytes_w);

        /*
         * Write Rf Prot.
         */
        if (nsc_act_cmd_index < nsc_act_cmd_len)
        {
            nsc_act_cmd[nsc_act_cmd_index++] = nscRfActPars->RfProt;
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
        }

        txBuf[0] = &nsc_act_cmd[0];
        txLen[0] = nsc_act_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_act_resp_len = 0;
            uint8_t *nsc_act_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_act_resp, &nsc_act_resp_len, PTX_NSC_TRANSFER_TO);

            if (ptxStatus_Success == status)
            {
                if ((PTX_NSC_RF_ACTIVATE_RSP_LENGTH <= nsc_act_resp_len) && (PTX_NSC_RF_ACTIVATE_RSP_OPCODE == nsc_act_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_act_resp[1u]);
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                }

                if (ptxStatus_Success == status)
                {
                    /* Get card activation data. */
                    *activationDataLen = nsc_act_resp_len - PTX_NSC_RF_ACTIVATE_RSP_LENGTH;
                    (void) memcpy(activationData, &nsc_act_resp[PTX_NSC_RF_ACTIVATE_RSP_LENGTH], *activationDataLen);
                }
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

void ptxNSC_ProcessNtf (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen)
{
    const size_t ntf_min_len = 2u;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != buff) )
    {
        if ((buffLen >= ntf_min_len)                                       ||
                ((PTX_NSC_OPCODE_RFDISCNTF == buff[0]) && (1u == buffLen)) ||
                ((PTX_NSC_OPCODE_LPCDNTF == buff[0]) && (1u == buffLen))      )
        {
            ptxNSC_Event_t event;
            (void)memset(&event, 0, sizeof(ptxNSC_Event_t));

            switch(buff[0])
            {
                case PTX_NSC_DATA_MSG_OPCODE:
                    if (1u == nscCtx->RxCltMode)
                    {
                        ptxNSC_ProcessRfCltMsg (nscCtx, &buff[2u], buff[1u], &event);
                    } else
                    {
                        ptxNSC_ProcessRfData (nscCtx, &buff[2u], buff[1u], &event);
                    }
                    break;

                case PTX_NSC_DATA_MSG_OPCODE_CHAINING:
                    ptxNSC_ProcessRfDataChained (nscCtx, &buff[2u], buff[1u], &event);
                    break;

                case PTX_NSC_RFD_CTRL_OPCODE:
                    ptxNSC_ProcessRfCtrl (nscCtx, &buff[1], buffLen, &event);
                    break;

                case PTX_NSC_OPCODE_RFDISCNTF:
                    ptxNSC_ProcessNtfRfDisc (nscCtx, &buff[1], buffLen, &event);
                    break;

                case PTX_NSC_OPCODE_RFACTNTF:
                    ptxNSC_ProcessNtfRfAct (nscCtx, &buff[1], buffLen, &event);
                    break;

                case PTX_NSC_OPCODE_RFDEACTNTF:
                    ptxNSC_ProcessNtfRfDeact (nscCtx, &buff[1], (buffLen-1), &event);
                    break;

                case PTX_NSC_OPCODE_RFERRORNTF:
                    ptxNSC_ProcessNtfRfError (nscCtx, &buff[1], buffLen, &event);
                    break;

                case PTX_NSC_OPCODE_LPCDNTF:
                    ptxNSC_ProcessNtfLpcd (nscCtx, &buff[1], buffLen, &event);
                    break;

                case PTX_NSC_OPCODE_RFFIELDNTF:
                    ptxNSC_ProcessNtfRfField (nscCtx, &buff[1], buffLen, &event);
                    break;

                default:
                    /* If Extensions Processing set, Let's call it. */
                    if (NULL != nscCtx->ExtensionNtfProcess)
                    {
                        nscCtx->ExtensionNtfProcess(nscCtx, &buff[0], buffLen);
                    }

                    break;
            }
        }
    }
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static void ptxNSC_ProcessRfData (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = NSC_Event_NfcDataMsg;
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }
}

static void ptxNSC_ProcessRfDataChained (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = NSC_Event_NfcDataMsg_Chained;
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }
}

static void ptxNSC_ProcessRfCltMsg (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    ptxStatus_t st = ptxStatus_Success;
    const size_t max_len_bytes = 256u;
    size_t lengthBytes = max_len_bytes;
    size_t num_tot_bits = 0;

    uint8_t pld_bytes[lengthBytes];
    uint8_t pld_parity_bits[lengthBytes];

    st = ptxNSC_NscCltMsg_DecodeRawBits (nscCtx, &buff[0], buffLen, buff[buffLen - 1u], &pld_bytes[0], &pld_parity_bits[0], &lengthBytes, &num_tot_bits);

    if (ptxStatus_Success == st)
    {

        event->EventId = NSC_Event_NfcCltMsg;

        /* Assignment of payload bytes to first buffer of event. */
        event->Buff = &pld_bytes[0];
        event->BuffLen = lengthBytes;

        /* Assignment of buffer of parity bits to second buffer of event. */
        event->BuffSecondary = &pld_parity_bits[0];
        event->BuffSecondaryLen = lengthBytes;

        /* Assignment of total number of bits. */
        event->NumTotalBitsSecondary = num_tot_bits;

        if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
        {
            (void)nscCtx->WfeCb(nscCtx->Ctx, event);
        }
    }
}

static void ptxNSC_ProcessRfCtrl (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    if (PTX_NSC_RFD_CTRL_ACK == buff[0])
    {
        /* Forward up RF CONTROL ACK. */
        event->EventId = NSC_Event_RfCtr_ACK;
        if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
        {
            (void)nscCtx->WfeCb(nscCtx->Ctx, event);
        }
    } else if (PTX_NSC_RFD_CTRL_ATTENTION == buff[0])
    {
        /* Forward up RF CONTROL ATTENTION COMMAND. */
        event->EventId = NSC_Event_RfCtr_AttCmd;
        if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
        {
            (void)nscCtx->WfeCb(nscCtx->Ctx, event);
        }
    } else
    {
        /* Not supported option. */
    }
    (void)buffLen;
}

static void ptxNSC_ProcessNtfRfDisc (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    ptxStatus_t st;

    if (1u == buffLen)
    {
        event->EventId = NSC_EventRfDisc_LastOne;
        (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, buff, (buffLen - 1u), event);

    } else
    {
        event->RfTechMode = buff[0];

        switch (event->RfTechMode)
        {
            case PTX_NSC_TYPES_TECH_POLL_A:
                event->EventId = NSC_EventRfDisc_PassPoll_A;
                (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
                break;

            case PTX_NSC_TYPES_TECH_POLL_B:
                event->EventId = NSC_EventRfDisc_PassPoll_B;
                (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
                break;

            case PTX_NSC_TYPES_TECH_POLL_F:
                event->EventId = NSC_EventRfDisc_PassPoll_F;
                (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
                break;

            case PTX_NSC_TYPES_TECH_POLL_V:
                event->EventId = NSC_EventRfDisc_PassPoll_V;
                (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
                break;

            case PTX_NSC_TYPES_TECH_POLL_ACT:
                /* Any other technology will not be reported. */
                break;

            default:
                for (uint8_t i = 0; i < PTX_NSC_MAX_EXTENSIONS; i++)
                {
                    if (NULL != nscCtx->CustomExtension[i].CBFnExtDiscoverNtf)
                    {
                        st = nscCtx->CustomExtension[i].CBFnExtDiscoverNtf(nscCtx->CustomExtension[i].ExtensionCtx, nscCtx, &buff[1], (buffLen - 1u), event);

                        if (ptxStatus_Success == st)
                        {
                            break;
                        }
                    }
                }
                break;
        }
    }
}

static void ptxNSC_ProcessNtfRfAct (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    ptxStatus_t st;

    event->RfTechMode = buff[0];

    switch (event->RfTechMode)
    {
        case PTX_NSC_TYPES_TECH_POLL_A:
            event->EventId = NSC_EventRfAct_PassPoll_A;
            (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
            break;

        case PTX_NSC_TYPES_TECH_POLL_B:
            event->EventId = NSC_EventRfAct_PassPoll_B;
            (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
            break;

        case PTX_NSC_TYPES_TECH_POLL_F:
            event->EventId = NSC_EventRfAct_PassPoll_F;
            (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
            break;

        case PTX_NSC_TYPES_TECH_POLL_V:
            event->EventId = NSC_EventRfAct_PassPoll_V;
            (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
            break;

        case PTX_NSC_TYPES_TECH_LISTEN_A:
            event->EventId = NSC_EventRfAct_PassListen_A;
            (void)ptxNSC_ProcessNtfRf_ActDisc (nscCtx, &buff[1], (buffLen - 1u), event);
            break;

        default:
            for (uint8_t i = 0; i < PTX_NSC_MAX_EXTENSIONS; i++)
            {
                if (NULL != nscCtx->CustomExtension[i].CBFnExtActivateNtf)
                {
                    st = nscCtx->CustomExtension[i].CBFnExtActivateNtf(nscCtx->CustomExtension[i].ExtensionCtx, nscCtx, &buff[1], (buffLen - 1u), event);

                    if (ptxStatus_Success == st)
                    {
                        break;
                    }
                }
            }
            break;
    }
}

static void ptxNSC_ProcessNtfRf_ActDisc (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }

    (void)buff;
    (void)buffLen;
}

static void ptxNSC_ProcessNtfRfDeact (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = NSC_EventRfDeact;
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }

    (void)buff;
    (void)buffLen;
}

static void ptxNSC_ProcessNtfRfError (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    if (PTX_NSC_RF_ERROR_NTF_CODE_ERR_TIMEOUT == buff[0])
    {
        (void) ptxNSC_ProcessNtfRfTimeoutError (nscCtx, buff, buffLen, event);
    } else
    {
        event->EventId = NSC_EventError;
        event->Buff = buff;
        event->BuffLen = buffLen;

        if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
        {
            (void)nscCtx->WfeCb(nscCtx->Ctx, event);
        }
    }
    (void)buff;
    (void)buffLen;
}

static void ptxNSC_ProcessNtfLpcd (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = NSC_LPCDTrigered;
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }
}

static void ptxNSC_ProcessNtfRfTimeoutError (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = NSC_RfTimeOutError;
    event->Buff = buff;
    event->BuffLen = buffLen;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }

    (void)buff;
    (void)buffLen;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS - RF ACTIVATE HELPERS
 * ####################################################################################################################
 */

static size_t ptxNSC_RfAct_TechParams_A (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, ptxNSC_RfActTech_A_Param_t *actTech_A)
{
    ptxStatus_t st = ptxStatus_Success;

    size_t num_b_written = 0;
    const size_t min_b_buffer = 1;
    const uint8_t device_state_sleep = 1u;

    const uint8_t cascade_tag = 0x88;

    const size_t cl_len = 7u;
    const size_t cl_header_len = 2u;
    const size_t cl_payload_len_noBCC = 4u;
    const size_t cl_payload_len = 5u;

    const uint8_t cl_SEL_PAR = 0x70;

    uint8_t cl_1 [cl_len];
    const uint8_t cl_1_SEL_CMD = 0x93;
    uint8_t cl_2 [cl_len];
    const uint8_t cl_2_SEL_CMD = 0x95;
    uint8_t cl_3 [cl_len];
    const uint8_t cl_3_SEL_CMD = 0x97;

    const uint8_t end_Of_Params = 0x00;

    /*
     * CL 1
     */
    cl_1[0] = cl_1_SEL_CMD;
    cl_1[1] = cl_SEL_PAR;
    (void)memset(&cl_1 [cl_header_len], 0 , cl_payload_len);

    /*
     * CL 2
     */
    cl_2[0] = cl_2_SEL_CMD;
    cl_2[1] = cl_SEL_PAR;
    (void)memset(&cl_2 [cl_header_len], 0 , cl_payload_len);

    /*
     * CL 3
     */
    cl_3[0] = cl_3_SEL_CMD;
    cl_3[1] = cl_SEL_PAR;
    (void)memset(&cl_3 [cl_header_len], 0 , cl_payload_len);

    if ((NULL != buffer) && (bufferLen > min_b_buffer))
    {
        if (device_state_sleep == actTech_A->DeviceRfState)
        {
            /*
             * Adding NFC-A activation parameters in case that the card is sleeping.
             */
            switch (actTech_A->NfcId1_len)
            {
                case NSC_ACT_LEN1:
                    /*
                     * Build SEL_REQ_CL1.
                     */
                    (void)memcpy(&cl_1[cl_header_len], &actTech_A->NfcId1[0], cl_payload_len_noBCC);
                    st = ptxNSC_RfActivate_Xor(nscCtx, &cl_1[cl_header_len], cl_payload_len_noBCC, &cl_1[cl_header_len + cl_payload_len_noBCC]);

                    if ((ptxStatus_Success == st) && (bufferLen >= (cl_len + 1u)))
                    {
                        (void)memcpy(buffer, &cl_1[0], cl_len);
                        buffer[cl_len] = end_Of_Params;
                        num_b_written = cl_len + 1u;
                    }
                    break;

                case NSC_ACT_LEN2:
                    /*
                     * Build SEL_REQ_CL1 + SEL_REQ_CL2.
                     */
                    cl_1[cl_header_len] = cascade_tag;
                    (void)memcpy(&cl_1[cl_header_len + 1], &actTech_A->NfcId1[0], 3u);
                    st = ptxNSC_RfActivate_Xor(nscCtx, &cl_1[cl_header_len], cl_payload_len_noBCC, &cl_1[cl_header_len + cl_payload_len_noBCC]);

                    if (ptxStatus_Success == st)
                    {
                        (void)memcpy(&cl_2[cl_header_len], &actTech_A->NfcId1[3], cl_payload_len_noBCC);
                        st = ptxNSC_RfActivate_Xor(nscCtx, &cl_2[cl_header_len], cl_payload_len_noBCC, &cl_2[cl_header_len + cl_payload_len_noBCC]);
                    }

                    if ( (ptxStatus_Success == st) && (bufferLen >= ((2u * cl_len) + 1)) )
                    {
                        (void)memcpy(&buffer[0], &cl_1[0], cl_len);
                        (void)memcpy(&buffer[cl_len], &cl_2[0], cl_len);
                        buffer[2 * cl_len] = end_Of_Params;
                        num_b_written = (2u * cl_len) + 1u;
                    }

                    break;

                case NSC_ACT_LEN3:
                    /*
                     * Build SEL_REQ_CL1 + SEL_REQ_CL2 + SEL_REQ_CL3.
                     */
                    cl_1[cl_header_len] = cascade_tag;
                    (void)memcpy(&cl_1[cl_header_len + 1], &actTech_A->NfcId1[0], 3u);
                    st = ptxNSC_RfActivate_Xor(nscCtx, &cl_1[cl_header_len], cl_payload_len_noBCC, &cl_1[cl_header_len + cl_payload_len_noBCC]);

                    if (ptxStatus_Success == st)
                    {
                        cl_2[cl_header_len] = cascade_tag;
                        (void)memcpy(&cl_2[cl_header_len + 1], &actTech_A->NfcId1[3], 3u);
                        st = ptxNSC_RfActivate_Xor(nscCtx, &cl_2[cl_header_len], cl_payload_len_noBCC, &cl_2[cl_header_len + cl_payload_len_noBCC]);

                        if (ptxStatus_Success == st)
                        {
                            (void)memcpy(&cl_3[cl_header_len], &actTech_A->NfcId1[6], cl_payload_len_noBCC);
                            st = ptxNSC_RfActivate_Xor(nscCtx, &cl_3[cl_header_len], cl_payload_len_noBCC, &cl_3[cl_header_len + cl_payload_len_noBCC]);
                        }

                    }

                    if ( (ptxStatus_Success == st) && (bufferLen >= ((3u * cl_len) + 1u)) )
                    {
                        (void)memcpy(&buffer[0], &cl_1[0], cl_len);
                        (void)memcpy(&buffer[cl_len], &cl_2[0], cl_len);
                        (void)memcpy(&buffer[2 * cl_len], &cl_3[0], cl_len);
                        buffer[3 * cl_len] = end_Of_Params;
                        num_b_written = (3u * cl_len) + 1u;
                    }

                    break;

                default:
                    break;
            }

        } else
        {
            /*
             * In case that the card is not-sleeping (active) the type A Activation Parameters are not needed.
             */
            buffer[0] = end_Of_Params;
            num_b_written = 1u;
        }
    }

    (void) nscCtx;
    return num_b_written;
}

static size_t ptxNSC_RfAct_TechParams_B (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen,
                                                            ptxNSC_RfActTech_B_Param_t *actTech_B)
{
    size_t num_b_written = 0;

    if ((NULL != buffer) && (bufferLen > 0) && (NULL != actTech_B))
    {
        size_t index = 0;

        buffer[index] = actTech_B->DeviceRfState;
        index++;

        if ((index + PTX_NSC_NFCID0_LEN) <= bufferLen)
        {
            (void)memcpy(&buffer[index], &actTech_B->SensBRes[0], PTX_NSC_TYPES_LISBSENSRES_LEN);
            index += PTX_NSC_TYPES_LISBSENSRES_LEN;
            num_b_written = index;
        }
    }

    (void) nscCtx;
    return num_b_written;
}

static size_t ptxNSC_RfAct_TechParams_F (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen,
                                                            ptxNSC_RfActTech_F_Param_t *actTech_F)
{
    size_t num_b_written = 0;

    if ((NULL != buffer) && (bufferLen > 0) && (NULL != actTech_F))
    {
        size_t index = 0;

        buffer[index] = actTech_F->DeviceRfState;
        index++;

        if ((index + PTX_NSC_NFCID2_LEN) <= bufferLen)
        {
            (void)memcpy(&buffer[index], &actTech_F->NfcId2[0], PTX_NSC_NFCID2_LEN);
            index += PTX_NSC_NFCID2_LEN;
            num_b_written = index;
        }
    }

    (void) nscCtx;
    return num_b_written;
}

static size_t ptxNSC_RfAct_TechParams_V (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen,
                                                            ptxNSC_RfActTech_V_Param_t *actTech_V)
{
    size_t num_b_written = 0;

    if ((NULL != buffer) && (bufferLen > 0) && (NULL != actTech_V))
    {
        /* nothing to do */
    }

    (void) nscCtx;
    return num_b_written;
}

static ptxStatus_t ptxNSC_RfActivate_Xor (ptxNSC_t *nscCtx, uint8_t *buffer, size_t bufferLen, uint8_t *xorResult)
{
    ptxStatus_t st = ptxStatus_Success;
    const size_t max_len_input = 4u;

    if ( (NULL != buffer) && (NULL != xorResult) &&
         (bufferLen > 0) && (bufferLen <= max_len_input) )
    {
        size_t index = 0;

        for (index=0; index < bufferLen; index++)
        {
            *xorResult ^= buffer[index];
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return st;
}

static void ptxNSC_ProcessNtfRfField (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen, ptxNSC_Event_t *event)
{
    event->EventId = (buff[0] != 0) ? NSC_EventRfField_on : NSC_EventRfField_off;
    event->Buff = NULL;
    event->BuffLen = 0;

    if ((nscCtx->WfeCb != NULL) && (nscCtx->Ctx != NULL))
    {
        (void)nscCtx->WfeCb(nscCtx->Ctx, event);
    }

    (void)buff;
    (void)buffLen;
}


static ptxStatus_t ptxNSC_NscCltMsg_DecodeRawBits (ptxNSC_t *nscCtx, uint8_t *pldRawBits, size_t lengthRawBits, uint8_t residualBits,
                                                                        uint8_t *pldBytes, uint8_t *pldParityBits, size_t *lengthBytes, size_t *numTotalBits)
{
    ptxStatus_t ret = ptxStatus_Success;

    if ((NULL != pldRawBits) && (NULL != pldBytes) && (lengthRawBits > 0) && (NULL != pldParityBits) && (NULL != lengthBytes) && (NULL != numTotalBits))
    {
        // Ensure that enough capacity for writing the bitstream
        size_t num_pld_bytes = 0;
        size_t num_pld_bits = 0;

        // Let's calculate the number of bits in the bitstream
        if (0 == residualBits)
        {
            num_pld_bits = (lengthRawBits - 1u) * 8u;

            if (0 == (num_pld_bits % 9u))
            {
                num_pld_bytes = lengthRawBits / 9u;
            } else
            {
                ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
            }
        } else
        {
            num_pld_bits = ((lengthRawBits - 2u) * 8u) + residualBits;

            if (0 == (num_pld_bits % 9u))
            {
                num_pld_bytes = num_pld_bits / 9u;
            } else
            {
                num_pld_bytes = (num_pld_bits / 9u) + 1u;
            }
        }

        // There are two options for processing bitstream, either smaller than 9 bits or
        // if larger than 9 bits this shall be multiple of 9
        if (num_pld_bits >= 9u)
        {
            if (0 == (num_pld_bits % 9u))
            {
                /* If larger than 9 bits, It shall be multiple of 9u. */
            } else
            {
                ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
            }
        }


        if (ptxStatus_Success == ret)
        {
            size_t length_bytes_input = *lengthBytes;

            if (length_bytes_input >= num_pld_bytes)
            {
                size_t index_pld = 0;
                size_t index_bits_bitstream = 0;
                size_t index_bytes_bitstream = 0;
                size_t index_bits_shift = 0;

                /* Enough capacity of output buffers, Let's write them. */
                (void)memset(pldBytes, 0, length_bytes_input);
                (void)memset(pldParityBits, 0, length_bytes_input);

                if (num_pld_bits < 9u)
                {
                    /* Less than 9 bits, */
                    pldBytes[0] = pldRawBits[0];
                    pldParityBits[0] = 0x00;
                    *lengthBytes = 1u;
                    *numTotalBits = num_pld_bits;

                } else
                {
                    do
                    {
                        index_bits_shift = 0;

                        // Decode payload byte
                        if (0 == (index_bits_bitstream % 8u))
                        {
                            pldBytes[index_pld] = pldRawBits[index_bytes_bitstream];
                        } else
                        {
                            // Some bits of the bitstream to current byte, some bits to the next one.
                            index_bits_shift = index_bits_bitstream % 8u;
                            uint16_t two_bytes_temp = (uint16_t)(((((uint16_t)pldRawBits[index_bytes_bitstream + 1u]) << 8u) & 0xFF00)+ (uint16_t)pldRawBits[index_bytes_bitstream]);
                            pldBytes[index_pld] = (uint8_t)(two_bytes_temp >> index_bits_shift);
                        }

                        index_bits_bitstream += 8u;
                        index_bytes_bitstream ++;

                        // Decode parity bit
                        index_bits_shift = index_bits_bitstream % 8u;
                        pldParityBits[index_pld] = (0x01 & (pldRawBits[index_bytes_bitstream] >> index_bits_shift));

                        index_bits_bitstream ++;
                        index_bytes_bitstream = (index_bits_bitstream >= ((index_bytes_bitstream + 1u) * 8u)) ? (index_bytes_bitstream + 1) : index_bytes_bitstream;

                        index_pld ++;

                    }while(index_bits_bitstream < num_pld_bits);

                    /* Successful operation, let's update the number of bytes written, and the number of total bits */
                    *lengthBytes = index_pld;
                    *numTotalBits = index_pld * 9u;
                }
            } else
            {
                ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
            }
        }
    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }
    (void)nscCtx;
    return ret;
}

