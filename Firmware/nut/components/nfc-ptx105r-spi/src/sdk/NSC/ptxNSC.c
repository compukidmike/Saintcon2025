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
    File        : ptxNSC.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxStatus.h"
#include "ptxNSC.h"
#include "ptxNSC_System.h"
#include "ptxPLAT.h"
#include "ptxNSC_Hal.h"
#include "ptxNSC_Registers.h"
#include "ptxNSC_Notifications.h"
#include "ptxNSC_uCODE.h"
#include "ptxNSC_Intf.h"
#include "ptxPlatform_Revision.h"
#include <string.h>


/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */


#define PTX_NSC_INIT_PARAMETERS_LEN     (sizeof(ptxNSC_InitPars_t))
#define PTX_NSC_DEACT_PARAMETERS_LEN    (sizeof(ptxNSC_RfDeactPars_t))
#define PTX_NSC_DISCOVER_PARMETERS_LEN  (254)

#define NSC_ACT_LEN1                    4u
#define NSC_ACT_LEN2                    7u
#define NSC_ACT_LEN3                    10u

#define NSC_EXTENSION_ID_AVAILABLE      (uint8_t)0xFF

/**
 * \brief Callback function for Waiting For Rx Events (asynchronously).
 *
 */
typedef void (*pptxNsc_WfrxCallBack_t) (void *ctx, ptxNSC_Event_t *event);


/*
 * ####################################################################################################################
 * NSC COMPONENT GLOBAL DATA DECLARATIONS
 * ####################################################################################################################
 */


ptxNSC_t nsc_comp;

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static ptxStatus_t ptxNSC_CheckHWVersion (ptxNSC_t *nscCtx);
static ptxStatus_t ptxNSC_IsRfErrorNtf (ptxNSC_t *nscCtx, uint8_t first_byte, size_t len, uint8_t *isRfErrorNtf);
static ptxStatus_t ptxNSC_IsRfDataAck (ptxNSC_t *nscCtx, uint8_t first_byte, uint8_t second_byte, size_t len, uint8_t *isRfDataAck);

static ptxStatus_t ptxNSC_Hal_WriteInstruction_Framing (ptxNSC_t *nscCtx, uint16_t address, uint8_t* pPayload, size_t txLen );
static ptxStatus_t ptxNSC_DataMsg_Ack(ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen);

static void ptxNSC_ProcessRsp (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen);
static ptxStatus_t ptxNSC_WaitForRsp(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen, uint32_t timeOut);

static ptxStatus_t ptxNSC_ClearRspRcvd(ptxNSC_t *nscCtx);
static ptxStatus_t ptxNSC_SetNewRspRcvd(ptxNSC_t *nscCtx, uint8_t *rsp, size_t rspLen);
static ptxStatus_t ptxNSC_GetNewRspRcvd(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen);
static uint8_t ptxNSC_IsNewRspRcvd(ptxNSC_t *nscCtx);
static ptxStatus_t ptxNSC_ReadCmd(ptxNSC_t *nscCtx, ptxNSC_ReadCmd_Par_t *nscReadPar, uint8_t *valueRead);
static ptxStatus_t ptxNSC_WriteCmd(ptxNSC_t *nscCtx, ptxNSC_WriteCmd_Par_t *nscwritePar);

/*
 * Allocate system parameters object.
 * Need to initialize after boot up to hold default values.
 * If UseExtCfg is set, the structure will not be initialized with default values, but will use current values.
 * Further on, the values are used for NscInitCmd.
 */
ptxNSC_System_t     NscSystemParams;


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxNSC_Init(ptxNSC_t **nscCtx, ptxNSC_ConfigPars_t *configPars)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != nscCtx) && (NULL != configPars) && (NULL != configPars->Plat)
            && (configPars->WfeCb != NULL) && (configPars->Ctx != NULL))
    {
        (void)memset(&nsc_comp, 0, sizeof(ptxNSC_t));

        nsc_comp.CompId = ptxStatus_Comp_NSC;
        nsc_comp.Plat = configPars->Plat;
        nsc_comp.MaxTransferUnit = PTX_NSC_MAX_MTU_SIZE;
        nsc_comp.WfeCb = configPars->WfeCb;
        nsc_comp.Ctx = configPars->Ctx;

        (void)memset(&NscSystemParams, 0, sizeof(ptxNSC_System_t));
        nsc_comp.SysParams = &NscSystemParams;
        nsc_comp.SysParams->Version = SYSTEM_DEFAULT_VERSION;
        nsc_comp.SysParams->PowerAmpOverCurrThreshold = SYSTEM_DEFAULT_PA_OVERCURRENT_TH;
        nsc_comp.SysParams->PowerAmpTempThreshold     = SYSTEM_DEFAULT_PA_TEMPERATURE_TH;
        nsc_comp.SysParams->ConClkSource              = SYSTEM_DEFAULT_CON_CLK_SOURCE;
        nsc_comp.SysParams->ConVarLBS                 = SYSTEM_DEFAULT_CON_VAR_LBS;
        (void)memcpy(&nsc_comp.SysParams->conHost[0],   &SYSTEM_DEFAULT_CON_NHOST[0],    sizeof(SYSTEM_DEFAULT_CON_NHOST));
        (void)memcpy(&nsc_comp.SysParams->conHostCE[0], &SYSTEM_DEFAULT_CON_NHOST_CE[0], sizeof(SYSTEM_DEFAULT_CON_NHOST_CE));

        nsc_comp.SysState = SystemState_OK;

        nsc_comp.DeactiveTimeoutMS = PTX_NSC_TRANSFER_TO;

        nsc_comp.ProductID = (uint8_t)0xFF;

        for (uint8_t i = 0; i < (uint8_t)PTX_NSC_MAX_EXTENSIONS; i++)
        {
            nsc_comp.CustomExtension[i].ExtensionID = NSC_EXTENSION_ID_AVAILABLE;
        }

#if defined (PTX_INTF_UART)
        // Not start "thread" waiting for incoming events from PTX1K
        // Init the system in HW Mode
        nsc_comp.NscMode = NscMode_HW;

        st = ptxNSC_Reset(&nsc_comp);

#elif defined(PTX_INTF_SPI) || defined(PTX_INTF_I2C)
        (void)ptxNSC_Reset(&nsc_comp);

        // Start "thread" waiting for incoming events from PTX1K
        st = ptxNSC_Start_WaitForRx(&nsc_comp);
#else
    #error Error - Missing or unsupported Host-Interface implementation used
#endif

        if (ptxStatus_Success == st)
        {
            *nscCtx = &nsc_comp;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }
    return st;
}

ptxStatus_t ptxNSC_Deinit(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        /** Stop "thread" waiting for incoming events from PTX1K */
        status = ptxNSC_Stop_WaitForRx(nscCtx);

        nscCtx->CompId = ptxStatus_Comp_None;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_FwDownloader(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        /* First access to the PTX1K to check the Register Version. */
        status = ptxNSC_CheckHWVersion (nscCtx);

        if (ptxStatus_Success == status)
        {
            uint8_t value = 0;

            /* Make sure DFY is reset.*/
            status = ptxNSC_HAL_Rra(nscCtx, SYS_CONTROL_REG, &value);
            if (ptxStatus_Success == status)
            {
                if (SYS_CONTROL_REG_DFY_ENABLE_MASK == (value & SYS_CONTROL_REG_DFY_ENABLE_MASK))
                {
                    status = ptxNSC_SoftReset(nscCtx);
                }

                if (ptxStatus_Success == status)
                {
                    for (size_t i=0; i<NUM_OF_SECTIONS; i++)
                    {
                        status = ptxNSC_Hal_WriteInstruction_Framing (nscCtx, (uint16_t)ptxNSC_uCODE_init_adds[i], (uint8_t *)ptxNSC_uCODE_sections[i], ptxNSC_uCODE_sections_size[i]);
                        if(ptxStatus_Success != status)
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
    return status;
}

ptxStatus_t ptxNSC_InitCmd(ptxNSC_t *nscCtx, ptxNSC_InitPars_t *nscInitPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscInitPars))
    {
        const uint8_t nsc_init_cmd_len = (PTX_NSC_INIT_PARAMETERS_LEN + 1);
        const uint8_t nsc_init_cmd_opCode = PTX_NSC_INIT_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_init_cmd_index = 0;
        uint8_t nsc_init_cmd [nsc_init_cmd_len];

        nsc_init_cmd[nsc_init_cmd_index] = nsc_init_cmd_opCode;
        nsc_init_cmd_index++;

        nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Clk_Src;
        nsc_init_cmd_index++;

        nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Var_Lbs;
        nsc_init_cmd_index++;

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_NHost, PTX_NSC_INIT_CON_HOST_LENGTH);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_HOST_LENGTH);

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_NHost_Ce, PTX_NSC_INIT_CON_HOST_CE_LENGTH);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_HOST_CE_LENGTH);

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_Uart_Config, PTX_NSC_INIT_CON_UART_CONFIG);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_UART_CONFIG);

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_Prng_Seed, PTX_NSC_INIT_CON_PRNG_SEED);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_PRNG_SEED);

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_N_Alm_Max, PTX_NSC_INIT_CON_N_ALM_MAX);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_N_ALM_MAX);

        (void)memcpy(&nsc_init_cmd[nsc_init_cmd_index], nscInitPars->Con_N_Alm_Min, PTX_NSC_INIT_CON_N_ALM_MIN);
        nsc_init_cmd_index = (uint8_t)(nsc_init_cmd_index + PTX_NSC_INIT_CON_N_ALM_MIN);

        nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Paocp_Th;
        nsc_init_cmd_index++;

        nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Patp_Th;
        nsc_init_cmd_index++;

        nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Xcp_Ctrl;
        nsc_init_cmd_index++;

        if(0u != (nscInitPars->Con_Xcp_Ctrl & PTX_NSC_INIT_EXT_PROTECTION_EN_MASK))
        {
            nsc_init_cmd[nsc_init_cmd_index] = nscInitPars->Con_Xcp_Th_Gt;
            nsc_init_cmd_index++;
        }

        txBuf[0] = &nsc_init_cmd[0];
        txLen[0] = nsc_init_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_init_resp_len = 0;
            uint8_t *nsc_init_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_init_resp, &nsc_init_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_init_resp) && (nsc_init_resp_len > 0))
            {
                if ((PTX_NSC_INIT_RESP_LEN == nsc_init_resp_len) && (PTX_NSC_INIT_RSP_OPCODE == nsc_init_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_init_resp[1u]);
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

#if defined (PTX_INTF_UART)
    if (ptxStatus_Success == status)
    {
        /*
         * Update interface speed if NSC_INIT_CMD/RESP was successful. This is relevant only for UART interface.
         * Up to now system speed should be the default value 115200 baud, but gets now changed to the targeted application speed.
         */

        uint32_t speed = ptxPLAT_GetIntfSpeed(nscCtx->Plat);

        status = ptxPLAT_SetIntfSpeed(nscCtx->Plat, speed);
    }
#endif

    status = ptxNSC_CheckSystemState(nscCtx, status);

    return status;
}

ptxStatus_t ptxNSC_RfDiscovery(ptxNSC_t *nscCtx, ptxNSC_RfDiscPars_t *nscRfDiscPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscRfDiscPars))
    {
        const size_t nsc_disc_cmd_len = PTX_NSC_DISCOVER_PARMETERS_LEN;
        const uint8_t nsc_disc_cmd_opCode = PTX_NSC_DISC_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];
        uint8_t nsc_disc_cmd [nsc_disc_cmd_len];
        uint8_t nsc_disc_cmd_index = 0;

        (void)memset(&nsc_disc_cmd[0], 0, nsc_disc_cmd_len);

        nsc_disc_cmd[nsc_disc_cmd_index] = nsc_disc_cmd_opCode;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Gt;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Disc_Mode;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_A;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_A_Cmd;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_A_Freq;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_A_Dev_Limit;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_B;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Poll_B_Cmd, PTX_NSC_DISC_CON_POLLB_CMD_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_POLLB_CMD_LEN);
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_B_Freq;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_B_Dev_Limit;
        nsc_disc_cmd_index++;

        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_TYPES_RFU_1_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_F;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Poll_F_Cmd, PTX_NSC_DISC_CON_POLLF_CMD_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_POLLF_CMD_LEN);
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_F_Freq;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_F_Dev_Limit;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_V;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Poll_V_Cmd, PTX_NSC_DISC_CON_POLLV_CMD_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_POLLV_CMD_LEN);
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_V_Freq;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_V_Dev_Limit;
        nsc_disc_cmd_index++;

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Rats_Param;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Attrib_Param1;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Attrib_Param2_Fsdi;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Attrib_Param3;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Attrib_Param4;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Iso_Dep_Attrib_Inf_Len;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Poll_B_Attrib_Inf, PTX_NSC_DISC_CON_POLLB_ATTRIB_INF_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_POLLB_ATTRIB_INF_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Nfc_Dep;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Nfc_Dep_Atr_Req_Pp;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Poll_Nfc_Dep_Atr_Req_G_Len;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Poll_Nfc_Dep_Atr_Req_G, PTX_NSC_DISC_CON_POLL_NFCDEP_ATR_REQ_G_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_POLL_NFCDEP_ATR_REQ_G_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_A;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_A_Sens_Res, PTX_NSC_DISC_CON_LIS_SENSRESP_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LIS_SENSRESP_LEN);
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_A_Sel_Res;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_A_Nfcid1, PTX_NSC_DISC_CON_LISA_NFCID1_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LISA_NFCID1_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_B;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_B_Afi;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_B_Sensb_Res, PTX_NSC_DISC_CON_LISB_SENSBRESP_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LISB_SENSBRESP_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_F;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_F_Sensf_Res, PTX_NSC_DISC_CON_LISF_SENSFRESP_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LISF_SENSFRESP_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_V;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_V_Inv_Res, PTX_NSC_DISC_CON_LISV_INVRESP_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LISV_INVRESP_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Iso_Dep;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_Iso_Dep_Ats, PTX_NSC_DISC_CON_LIS_ISODEP_ATS_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LIS_ISODEP_ATS_LEN);
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Iso_Dep_Attrib_Res_Len;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_Iso_Dep_Attrib_Res, PTX_NSC_DISC_CON_LIS_ISODEP_ATTRIB_RES_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LIS_ISODEP_ATTRIB_RES_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Nfc_Dep;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Nfc_Dep_Atr_Res_To;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Nfc_Dep_Atr_Res_Pp;
        nsc_disc_cmd_index++;
        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Listen_Nfc_Dep_Atr_Res_G_Len;
        nsc_disc_cmd_index++;
        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Listen_Nfc_Dep_Atr_Res_G, PTX_NSC_DISC_CON_LIS_NFCDEP_ATR_RES_G_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_LIS_NFCDEP_ATR_RES_G_LEN);

        nsc_disc_cmd[nsc_disc_cmd_index] = nscRfDiscPars->Con_Idle;
        nsc_disc_cmd_index++;

        (void)memcpy(&nsc_disc_cmd[nsc_disc_cmd_index], nscRfDiscPars->Con_Idle_Time, PTX_NSC_DISC_CON_IDLE_TIME_LEN);
        nsc_disc_cmd_index = (uint8_t)(nsc_disc_cmd_index + PTX_NSC_DISC_CON_IDLE_TIME_LEN);

        for (uint8_t i = 0; i < PTX_NSC_MAX_EXTENSIONS; i++)
        {
            if (NULL != nscCtx->CustomExtension[i].CBFnExtDiscoverCmd)
            {
                status = nscCtx->CustomExtension[i].CBFnExtDiscoverCmd(nscCtx->CustomExtension[i].ExtensionCtx, &nsc_disc_cmd[0]);

                if ((ptxStatus_NotImplemented != PTX_GET_STATUS(status)) && (ptxStatus_Success != status))
                {
                    break;
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            txBuf[0] = &nsc_disc_cmd[0];
            txLen[0] = nsc_disc_cmd_index;

            status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

            if (ptxStatus_Success == status)
            {
                size_t nsc_disc_resp_len = 0;
                uint8_t *nsc_disc_resp = NULL;

                status = ptxNSC_ReceiveRsp(nscCtx, &nsc_disc_resp, &nsc_disc_resp_len, PTX_NSC_TRANSFER_TO);

                if ((ptxStatus_Success == status) && (NULL != nsc_disc_resp) && (nsc_disc_resp_len > 0))
                {
                    if ((PTX_NSC_DISC_RESP_LEN == nsc_disc_resp_len) && (PTX_NSC_DISC_OPCODE == nsc_disc_resp[0]))
                    {
                        status = ptxNSC_ProcessRspErrorCode (nsc_disc_resp[1u]);
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                    }
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

ptxStatus_t ptxNSC_RfSetParams(ptxNSC_t *nscCtx, ptxNSC_RfPar_t *nscRfParams, size_t nscRfParamsLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscRfParams)  && (nscRfParamsLen > 0)  && (nscRfParamsLen <= PTX_NSC_RF_SET_PARAMS_MAX) )
    {
        uint8_t is_rx_parity_modified = 0;
        uint8_t rx_parity = 0;
        const size_t nsc_setPars_cmd_len = 3u + (nscRfParamsLen * 2u) + 6u;
        const uint8_t nsc_setPars_cmd_opCode = PTX_NSC_SET_PARAMS_DISC_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];
        uint8_t nsc_setPars_cmd [nsc_setPars_cmd_len];
        size_t nsc_setPars_cmd_index = 0;
        unsigned int i=0;

        nsc_setPars_cmd[nsc_setPars_cmd_index] = nsc_setPars_cmd_opCode;
        nsc_setPars_cmd_index++;

        while( (i < nscRfParamsLen) && (ptxStatus_Success == status))
        {
            /* Copy Id of the parameter. */
            nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].ParmId;
            nsc_setPars_cmd_index++;

            /* Copy content of the parameter. */
            switch(nscRfParams[i].ParmId)
            {
                case RfParameter_Fwt:
                    (void)memcpy(&nsc_setPars_cmd[nsc_setPars_cmd_index], &nscRfParams[i].Parm.Fwt.Fwt[0], 3u);
                    nsc_setPars_cmd_index += 3u;
                    break;

                case RfParameter_Rf_Tech:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.RfTech.RfTech;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Tx_Bit_Rate:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.TxBitRate.TxBitRate;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Rx_Bit_Rate:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.RxBitRate.RxBitRate;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Tx_PAR:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.TxParity.TxParity;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Rx_PAR:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.RxParity.RxParity;
                    nsc_setPars_cmd_index++;

                    /* Rx Parity Modified */
                    is_rx_parity_modified = 1u;
                    rx_parity = nscRfParams[i].Parm.RxParity.RxParity;
                    break;

                case RfParameter_Tx_CRC:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.TxCRC.TxCRC;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Rx_CRC:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.RxCRC.RxCRC;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Res_Limit:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.ResLimit.ResLimit;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Tx_Residual_Bits:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.TxResidualBits.TxResidualBits;
                    nsc_setPars_cmd_index++;
                    break;

                case RfParameter_Rf_Field:
                    nsc_setPars_cmd[nsc_setPars_cmd_index] = nscRfParams[i].Parm.RfField.RfField;
                    nsc_setPars_cmd_index++;
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                    break;
            }

            i++;
        }

        if (ptxStatus_Success == status)
        {
            nsc_setPars_cmd[nsc_setPars_cmd_index] = (uint8_t)PTX_NSC_SET_PARAM_CMD_EOC;
            nsc_setPars_cmd_index++;

            txBuf[0] = &nsc_setPars_cmd[0];
            txLen[0] = nsc_setPars_cmd_index;

            status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

            if (ptxStatus_Success == status)
            {
                size_t nsc_setPars_resp_len = 0;
                uint8_t *nsc_setPars_resp = NULL;

                status = ptxNSC_ReceiveRsp(nscCtx, &nsc_setPars_resp, &nsc_setPars_resp_len, PTX_NSC_TRANSFER_TO);

                if ((ptxStatus_Success == status) && (NULL != nsc_setPars_resp) && (nsc_setPars_resp_len > 0))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_setPars_resp[1u]);

                    /* Let's keep track of CLT Mode. */
                    if ((ptxStatus_Success == status) && (1u  == is_rx_parity_modified))
                    {
                        /* If RX Parity OFF => Enable CLT Mode. */
                        if (0 == rx_parity)
                        {
                            nscCtx->RxCltMode = 1u;
                        } else
                        {
                            nscCtx->RxCltMode = 0u;
                        }
                    }

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

    return status;
}

ptxStatus_t ptxNSC_RfDeactivate(ptxNSC_t *nscCtx, ptxNSC_RfDeactPars_t *nscRfDeactPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscRfDeactPars))
    {
        const uint8_t nsc_deact_cmd_len = (PTX_NSC_DEACT_PARAMETERS_LEN + 1);
        const uint8_t nsc_deact_cmd_opCode = PTX_NSC_DEACT_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];
        uint8_t nsc_deact_cmd [nsc_deact_cmd_len];
        uint8_t nsc_deact_cmd_index = 0;

        nsc_deact_cmd[nsc_deact_cmd_index] = nsc_deact_cmd_opCode;
        nsc_deact_cmd_index++;
        nsc_deact_cmd[nsc_deact_cmd_index] = (uint8_t)nscRfDeactPars->Rf_Deactivate_Type;
        nsc_deact_cmd_index++;
        nsc_deact_cmd[nsc_deact_cmd_index] = (uint8_t)nscRfDeactPars->Rf_State;
        nsc_deact_cmd_index++;

        txBuf[0] = &nsc_deact_cmd[0];
        txLen[0] = nsc_deact_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
                size_t nsc_deact_resp_len = 0;
                uint8_t *nsc_deact_resp = NULL;

                status = ptxNSC_ReceiveRsp(nscCtx, &nsc_deact_resp, &nsc_deact_resp_len, PTX_NSC_TRANSFER_TO);

                if ((ptxStatus_Success == status) && (NULL != nsc_deact_resp) && (nsc_deact_resp_len > 0))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_deact_resp[1u]);

                    if (ptxStatus_Success == status)
                    {
                        if (nsc_deact_resp_len >= (size_t)2)
                        {
                            if (0 == nsc_deact_resp[2u])
                            {
                                /* the uCode indicates if a NTF is following (or not) */
                                nscCtx->DeactivationNTFPending = 0;
                            }
                        } else
                        {
                            /*
                             * reset pending-NTF status
                             */
                            nscCtx->DeactivationNTFPending = 0;
                        }
                    } else
                    {
                        /*
                         * reset pending-NTF status
                         */
                        nscCtx->DeactivationNTFPending = 0;

                        /*
                         * If the system is in standby, we might get ERROR_RESP with ERROR_CODE = 0x08 (Device in Standby).
                         * This is actually OK, because we switched to RF_IDLE state, which was the intention.
                         */
                        if(ptxNscRsp_DeviceInStandby == nsc_deact_resp[1u])
                        {
                            if (RfIdle == nscRfDeactPars->Rf_State)
                            {
                                status = ptxStatus_Success;
                            } else
                            {
                                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                            }
                        }
                    }
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_RfDataMsgTx(ptxNSC_t *nscCtx, uint8_t *msgData, size_t msgLen, uint8_t isChained)
{
    ptxStatus_t status = ptxStatus_Success;
    const size_t max_nsc_mtu = PTX_NSC_MAX_MTU_SIZE;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && ((msgLen <= max_nsc_mtu)) && ((isChained == 0u) || (isChained == 1u)) )
    {
        size_t msg_len = msgLen;
        size_t nr_buffers_to_send = 0;

        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_2;
        uint8_t *txBuf[3u];
        size_t txLen[3u];

        /* First buffer for NSC_DATA header. */
        const size_t nsc_header_len = 2u;
        uint8_t nsc_header[nsc_header_len];
        nsc_header[PTX_NSC_DATA_MSG_OPCODE_INDEX] = (0 != isChained) ? PTX_NSC_DATA_MSG_OPCODE_CHAINING : PTX_NSC_DATA_MSG_OPCODE;
        nsc_header[PTX_NSC_DATA_MSG_CON_MSG_LEN_INDEX] = (uint8_t)msg_len;

        /* Third buffer for optional status byte to append residual bits (if Type-A Transparent-Mode enabled). */
        const size_t nsc_status_len = 1u;
        uint8_t nsc_status[nsc_status_len];

        /*
         * Note: It's allowed to have payload == NULL and length == 0 in order to send empty packets to NSC, but if length
         * is set to a value != 0, then the pointer to the payload must be valid as well.
         *
         */
        if ((0 != msg_len) && (NULL == msgData))
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);

        } else
        {
            nr_buffers_to_send = (0 != msg_len) ? 2u : 1u;

            if (0 != nscCtx->TypeATransparentModeActive)
            {
                nsc_status[0] = nscCtx->TransparentModeNrResidualTxBits;

                nr_buffers_to_send++;

                /* Increment Message-length in Header by 1 to send additional status byte */
                nsc_header[PTX_NSC_DATA_MSG_CON_MSG_LEN_INDEX]++;
            }

            /* First buffer for NSC_DATA Header*/
            txBuf[0] = &nsc_header[0];
            txLen[0] = nsc_header_len;

            /* Second buffer for NSC_DATA payload coming from upper layers. */
            txBuf[1u] = msgData;
            txLen[1u] = msg_len;

            /* Third buffer for optional Status byte to append residual bits (see above). */
            txBuf[2u] = &nsc_status[0];
            txLen[2u] = nsc_status_len;

            /* Send the two buffers to lower layers */
            status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, nr_buffers_to_send);

            if ( 1u == isChained)
            {
                /* Here, it is handled the ACK from PTX1K when there is chaining at NSC protocol level. */
                if (ptxStatus_Success == status)
                {
                    size_t nscData_Rsp_Len = 0;
                    uint8_t *nscData_Rsp = NULL;

                    status = ptxNSC_ReceiveRsp(nscCtx, &nscData_Rsp, &nscData_Rsp_Len, PTX_NSC_TRANSFER_TO);

                    if ((ptxStatus_Success == status) && (NULL != nscData_Rsp) && (nscData_Rsp_Len > 0))
                    {
                        status = ptxNSC_DataMsg_Ack(nscCtx, &nscData_Rsp[0], nscData_Rsp_Len);
                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                    }
                }
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_DFY_Activation(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        /* Do a SW reset before DFY activation. */
        status = ptxNSC_SoftReset(nscCtx);

        if (ptxStatus_Success == status)
        {
            /* Set SYSTEM Mode before doing DFY Activation. */
            status = ptxNSC_SetMode(nscCtx, NscMode_SYS);
        }

        if (ptxStatus_Success == status)
        {

            /*
             * System Control Register has to be written to activate DragonFly
             * WRA @0x1000 0x01 in hex format 0xB00001
             */

            uint16_t address = (uint16_t)SYS_CONTROL_REG;
            uint8_t value_to_write = (uint8_t)0x01;

#if defined (PTX_INTF_UART)
            status = ptxNSC_HAL_Wra_NoCheck(nscCtx, address, value_to_write);
#elif defined(PTX_INTF_SPI) || defined(PTX_INTF_I2C)
            status = ptxNSC_HAL_Wra(nscCtx, address, value_to_write);
#else
    #error Error - Missing or unsupported Host-Interface implementation used
#endif
            if (ptxStatus_Success == status)
            {
                size_t response_length = 0;
                uint8_t *response_buffer = NULL;

                status = ptxNSC_ReceiveRsp(nscCtx, &response_buffer, &response_length, PTX_NSC_TRANSFER_TO);

                if ((ptxStatus_Success == status) && ( NULL != response_buffer) && (response_length > 0))
                {
                    if ((PTX_NSC_RESET_RSP_LENGTH == response_length) && (PTX_NSC_RESET_RSP_OPCODE == response_buffer[0]))
                    {
                        /*
                         * Assign Product-ID
                         */
                        nscCtx->ProductID = response_buffer[1u];

                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                    }
                }
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_StandbyCmd(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        const uint8_t nsc_stdby_cmd_len = (PTX_NSC_STANDBY_PARAMETERS_LEN + 1);
        const uint8_t nsc_stdby_cmd_opCode = PTX_NSC_STANDBY_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_stdby_cmd_index = 0;
        uint8_t nsc_stdby_cmd [nsc_stdby_cmd_len];

        nsc_stdby_cmd[nsc_stdby_cmd_index] = nsc_stdby_cmd_opCode; //nsc_stdby_cmd_opCode;
        nsc_stdby_cmd_index++;

        txBuf[0] = &nsc_stdby_cmd[0];
        txLen[0] = nsc_stdby_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_stdby_resp_len = 0;
            uint8_t *nsc_stdby_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_stdby_resp, &nsc_stdby_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_stdby_resp) && (nsc_stdby_resp_len > 0))
            {
                if ((PTX_NSC_STANDBY_RESP_LEN == nsc_stdby_resp_len) && (PTX_NSC_STANDBY_RSP_OPCODE == nsc_stdby_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_stdby_resp[1u]);
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

    return status;
}

ptxStatus_t ptxNSC_WakeupCmd(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        const uint8_t nsc_wakeup_cmd_len = (PTX_NSC_WAKEUP_PARAMETERS_LEN + 1);
        const uint8_t nsc_wakeup_cmd_opCode = PTX_NSC_WAKEUP_CMD_OPCODE;
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_wakeup_cmd_index = 0;
        uint8_t nsc_wakeup_cmd [nsc_wakeup_cmd_len];

        nsc_wakeup_cmd[nsc_wakeup_cmd_index] = nsc_wakeup_cmd_opCode;
        nsc_wakeup_cmd_index++;

        txBuf[0] = &nsc_wakeup_cmd[0];
        txLen[0] = nsc_wakeup_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_wakeup_resp_len = 0;
            uint8_t *nsc_wakeup_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_wakeup_resp, &nsc_wakeup_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_wakeup_resp) && (nsc_wakeup_resp_len > 0))
            {
                if ((PTX_NSC_WAKEUP_RESP_LEN == nsc_wakeup_resp_len) && (PTX_NSC_WAKEUP_RSP_OPCODE == nsc_wakeup_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_wakeup_resp[1u]);
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

    return status;
}

ptxStatus_t ptxNSC_Get_Mtu(ptxNSC_t *nscCtx, uint32_t *maxTransferUnit)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != maxTransferUnit))
    {
        *maxTransferUnit = nscCtx->MaxTransferUnit;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_GetRevisionInfo(ptxNSC_t *nscCtx, ptxNSC_RevisionType_t revisionType, uint32_t *revisionInfo)
{
    ptxStatus_t  ret = ptxStatus_Success;
    uint8_t value;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != revisionInfo))
    {
        /* Default Initialization */
        *revisionInfo = 0;

        switch (revisionType)
        {
            case ptxNSC_Info_C_Stack:
                *revisionInfo = PTX_PLATFORM_REVISION;
                break;

            case ptxNSC_Info_Local_Changes:
                *revisionInfo = PTX_PLATFORM_LOCAL_CHANGES;
                break;

            case ptxNSC_Info_DFY_Code:
                *revisionInfo = ptxNSC_uCODE_SRC_REV;
                break;

            case ptxNSC_Info_DFY_Toolchain:
                *revisionInfo = ptxNSC_uCODE_ASM_REV;
                break;

            case ptxNSC_Info_ChipID:
                value = 0;
                ptxNSC_ReadCmd_Par_t nscReadPar;
                nscReadPar.Type = ReadRandomAdd;
                nscReadPar.NumOfRead = 1u;
                nscReadPar.Addresses[0] = VERSION_REG;

                ret = ptxNSC_ReadCmd(nscCtx, &nscReadPar, &value);

                if (ptxStatus_Success == ret)
                {
                    *revisionInfo = (uint32_t)value;
                }
                break;

            case ptxNSC_Info_ProductID:
                *revisionInfo = (uint32_t)nscCtx->ProductID;
                break;

            default:
                ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
                break;
        }
    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return ret;
}

ptxStatus_t ptxNSC_ReadTempSensor(ptxNSC_t *nscCtx, uint8_t *sensVal)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        uint8_t dac_value = 0;

        uint16_t address = (uint16_t)ANA_COMP_REG;
        uint8_t reg_value = 0x00u;

        //Read-Modify-Write ANA_COMP_REG to enable temperature sensor
        st = ptxNSC_HAL_Rra(nscCtx, address, &reg_value);

        if (ptxStatus_Success == st)
        {
            //Enable TSense and TSense comparator
            reg_value |= (ANA_COMP_REG_TSENSE_COMP_EN_MASK + ANA_COMP_REG_TSENSE_EN_MASK);
            st = ptxNSC_HAL_Wra (nscCtx, address, reg_value);
        }

        if (ptxStatus_Success == st)
        {
            //Sensor enabled. Iteration for all 8 bits: load DAC value and check comparator status bit.
            uint32_t sleep_time;
            uint8_t mask = 0x80;
            dac_value = 0;
            for(uint8_t i=0; (i<8) && (ptxStatus_Success == st); i++)
            {
                //Set one bit for SAR-algo
                reg_value = (uint8_t)((uint8_t)dac_value + (uint8_t)(mask >> i));
                st = ptxNSC_HAL_Wra (nscCtx, (uint16_t)DAC_TSENSE_WORD_REG, reg_value);
                if(ptxStatus_Success == st)
                {
                    //Wait to let temp-sensor and DAC settle for the first measurement. First measurement takes longer.
                    sleep_time = 3u;
                    if(0 == i)
                    {
                        sleep_time = 20u;
                    }

                    (void) ptxPLAT_Sleep(nscCtx->Plat, sleep_time);

                    //Read status value to check if the comparator bit is set
                    st = ptxNSC_HAL_Rra(nscCtx, (uint16_t)ANA_STATUS0_REG, &reg_value);
                }

                if(ptxStatus_Success == st)
                {
                    //If comparator bit is set, add this bit-mask value to the result
                    if(0 != (reg_value & ANA_STATUS0_REG_TEMPSENS_COMP_OUT_MASK))
                    {
                        dac_value = (uint8_t)((uint8_t)dac_value + (uint8_t)(mask >> i));
                    }
                }
            }
        }

        if (ptxStatus_Success == st)
        {
            //Operation successful, so the value is valid.
            *sensVal = dac_value;
        }

        //Disable sensor and comparator.
        reg_value &= (uint8_t)(~(ANA_COMP_REG_TSENSE_COMP_EN_MASK + ANA_COMP_REG_TSENSE_EN_MASK));
        (void) ptxNSC_HAL_Wra (nscCtx, (uint16_t)ANA_COMP_REG, reg_value);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_GetMiscRFConfig(ptxNSC_t *nscCtx, uint8_t *configBuffer, uint8_t *configBufferLen)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != configBuffer) && (NULL != configBufferLen))
    {
        if (*configBufferLen >= PTX_NSC_MISC_RF_CONFIG_BUFFER_SIZE)
        {
            if (0 != (nscCtx->RFConfigMiscParams.MiscSettingsFlags & PTX_NSC_MISC_RF_CONFIG_FLAGS_LOADED))
            {
                (void)memcpy(&configBuffer[0], &nscCtx->RFConfigMiscParams.MiscSettings[0], nscCtx->RFConfigMiscParams.MiscSettings_Len);
                *configBufferLen = nscCtx->RFConfigMiscParams.MiscSettings_Len;
            } else
            {
                st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidState);
            }
        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_Read(ptxNSC_t *nscCtx, uint16_t address, uint8_t *value)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != value))
    {
        ptxNSC_ReadCmd_Par_t nscReadPar;
        (void)memset(&nscReadPar, 0, sizeof(ptxNSC_ReadCmd_Par_t));

        nscReadPar.Type = ReadRandomAdd;
        nscReadPar.NumOfRead = 1u;
        nscReadPar.Addresses[0] = address;

        st = ptxNSC_ReadCmd(nscCtx, &nscReadPar, value);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_Write(ptxNSC_t *nscCtx, uint16_t address, uint8_t value)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNSC_WriteCmd_Par_t nscWritePar;
        (void)memset(&nscWritePar, 0, sizeof(ptxNSC_WriteCmd_Par_t));

        nscWritePar.Type = WriteRandomAdd;
        nscWritePar.NumOfWrite = 1u;
        nscWritePar.Addresses[0] = address;
        nscWritePar.Values[0] = value;

        st = ptxNSC_WriteCmd(nscCtx, &nscWritePar);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_WriteN(ptxNSC_t *nscCtx, uint16_t * addresses, uint8_t *values, size_t nrWrites)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC)) && (NULL != addresses) && (NULL != values) && (nrWrites <= PTX_NSC_TYPES_WR_OP_MAX))
    {
        ptxNSC_WriteCmd_Par_t nscWritePar;
        (void)memset(&nscWritePar, 0, sizeof(ptxNSC_WriteCmd_Par_t));

        nscWritePar.Type = WriteRandomAdd;
        nscWritePar.NumOfWrite = nrWrites;
        (void)memcpy(&nscWritePar.Addresses[0], addresses, nrWrites);
        (void)memcpy(&nscWritePar.Values[0], values, nrWrites);

        st = ptxNSC_WriteCmd(nscCtx, &nscWritePar);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_RfTestRun(ptxNSC_t *nscCtx, ptxNSC_RfTest_ID_t rfTestId, uint8_t *rfTestParams, size_t rfTestParamsLen)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != rfTestParams)  && (rfTestParamsLen > 0))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_rf_run_test_parameter_cmd[20];

        size_t nsc_write_cmd_index = 0;
        nsc_rf_run_test_parameter_cmd[nsc_write_cmd_index] = (uint8_t)PTX_NSC_RF_RUN_TEST_CMD_OPCODE;
        nsc_write_cmd_index++;
        nsc_rf_run_test_parameter_cmd[nsc_write_cmd_index] = (uint8_t)rfTestId;
        nsc_write_cmd_index++;
        (void)memcpy(&nsc_rf_run_test_parameter_cmd[nsc_write_cmd_index], &rfTestParams[0], rfTestParamsLen);
        nsc_write_cmd_index += rfTestParamsLen;

        txBuf[0] = &nsc_rf_run_test_parameter_cmd[0];
        txLen[0] = nsc_write_cmd_index;

        st = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == st)
        {
            size_t nsc_run_test_resp_len = 0;
            uint8_t *nsc_run_test_resp = NULL;

            st = ptxNSC_ReceiveRsp(nscCtx, &nsc_run_test_resp, &nsc_run_test_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == st) && (NULL != nsc_run_test_resp) && (nsc_run_test_resp_len > PTX_NSC_RF_RUN_TEST_CMD_RSP_LENGTH))
            {
                if ((PTX_NSC_RF_RUN_TEST_CMD_RSP_OPCODE == nsc_run_test_resp[0]))
                {
                    st = ptxNSC_ProcessRspErrorCode (nsc_run_test_resp[1u]);
                } else
                {
                    st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                }
            }
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_RfTestStop(ptxNSC_t *nscCtx)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_rf_run_test_parameter_cmd[20];

        size_t nsc_write_cmd_index = 0;
        nsc_rf_run_test_parameter_cmd[nsc_write_cmd_index] = (uint8_t)PTX_NSC_RF_STOP_TEST_CMD_OPCODE;
        nsc_write_cmd_index++;

        txBuf[0] = &nsc_rf_run_test_parameter_cmd[0];
        txLen[0] = nsc_write_cmd_index;

        st = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == st)
        {
            size_t nsc_run_test_resp_len = 0;
            uint8_t *nsc_run_test_resp = NULL;

            st = ptxNSC_ReceiveRsp(nscCtx, &nsc_run_test_resp, &nsc_run_test_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == st) && (NULL != nsc_run_test_resp) && (nsc_run_test_resp_len > PTX_NSC_RF_STOP_TEST_CMD_RSP_LENGTH))
            {
                if ((PTX_NSC_RF_STOP_TEST_CMD_RSP_OPCODE == nsc_run_test_resp[0]))
                {
                    st = ptxNSC_ProcessRspErrorCode (nsc_run_test_resp[1u]);
                } else
                {
                    st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
                }
            }
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_GetDeactivateTimeout(ptxNSC_t *nscCtx, uint32_t *currentTimeout)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC)) && (NULL != currentTimeout))
    {
        *currentTimeout = nscCtx->DeactiveTimeoutMS;

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_SetDeactivateTimeout(ptxNSC_t *nscCtx, uint32_t *currentTimeout)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        nscCtx->DeactiveTimeoutMS = (NULL != currentTimeout) ? *currentTimeout : PTX_NSC_TRANSFER_TO;

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_SetNrResidualTxBits(ptxNSC_t *nscCtx, uint8_t transparentModeEnabled, uint8_t nrTxBits)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        nscCtx->TypeATransparentModeActive = transparentModeEnabled;
        nscCtx->TransparentModeNrResidualTxBits = (0 != transparentModeEnabled) ? nrTxBits : 0;

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_RegisterExtension(ptxNSC_t *nscCtx, ptxNSC_Custom_Extension_t *extension)
{
    ptxStatus_t st = ptxStatus_InsufficientResources;

    if ((PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC)) && (NULL != extension))
    {
        if (extension->ExtensionID != NSC_EXTENSION_ID_AVAILABLE)
        {
            if (nscCtx->NrCustomExtensions < PTX_NSC_MAX_EXTENSIONS)
            {
                for (uint8_t i = 0; i < PTX_NSC_MAX_EXTENSIONS; i++)
                {
                    if (extension->ExtensionID == nscCtx->CustomExtension[i].ExtensionID)
                    {
                        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NotPermitted);
                        break;

                    } else
                    {
                        if (NSC_EXTENSION_ID_AVAILABLE == nscCtx->CustomExtension[i].ExtensionID)
                        {
                            nscCtx->CustomExtension[i].CBFnExtDiscoverCmd = extension->CBFnExtDiscoverCmd;
                            nscCtx->CustomExtension[i].CBFnExtDiscoverNtf = extension->CBFnExtDiscoverNtf;
                            nscCtx->CustomExtension[i].CBFnExtActivateNtf = extension->CBFnExtActivateNtf;
                            nscCtx->CustomExtension[i].ExtensionCtx       = extension->ExtensionCtx;
                            nscCtx->CustomExtension[i].ExtensionID        = extension->ExtensionID;
                            nscCtx->NrCustomExtensions++;

                            st = ptxStatus_Success;

                            break;
                        }
                    }
                }
            }
            else
            {
                st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InsufficientResources);
            }
        }
        else
        {
            st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_DeRegisterExtension(ptxNSC_t *nscCtx, uint8_t extensionID)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        for (uint8_t i = 0; i < PTX_NSC_MAX_EXTENSIONS; i++)
        {
            if ((extensionID == nscCtx->CustomExtension[i].ExtensionID) && (0 != nscCtx->NrCustomExtensions))
            {
                (void)memset(&nscCtx->CustomExtension[i], 0, sizeof(ptxNSC_Custom_Extension_t));
                nscCtx->CustomExtension[i].ExtensionID = NSC_EXTENSION_ID_AVAILABLE;
                nscCtx->NrCustomExtensions--;
            }
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

/*
 * ####################################################################################################################
 * COMMON FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxNSC_ProcessRspErrorCode (ptxNSC_Rsp_ErrorCodes_t NscRspErrorCode)
{
    ptxStatus_t st = ptxStatus_Success;

    switch (NscRspErrorCode)
    {
        case ptxNscRsp_Sucessful:
            break;

        /*
         * All the errors at NSC protocol level are handled as NSC_PROTOCOL_ERROR.
         */
        case ptxNscRsp_UnknownError:
        case ptxNscRsp_NotAllowedCmd:
        case ptxNscRsp_UnknownErrorCmd:
        default:
            st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
            break;
    }

    return st;
}

ptxStatus_t ptxNSC_Send(ptxNSC_t *nscCtx, ptxNscHal_BufferId_t bufferId, uint8_t *txBuf[], size_t txLen[], size_t numBuffers)
{
    ptxStatus_t status = ptxStatus_Success;

    (void)ptxNSC_ClearRspRcvd(nscCtx);
    status = ptxNSC_HAL_WriteBuffer(nscCtx, bufferId, txBuf, txLen, numBuffers);

    return status;
}

ptxStatus_t ptxNSC_ReceiveRsp(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen, uint32_t timeOut)
{
    ptxStatus_t status = ptxStatus_Success;

    status = ptxNSC_WaitForRsp(nscCtx, rsp, rspLen, timeOut);

    return status;
}

ptxStatus_t ptxNSC_CheckSystemState(ptxNSC_t *nscCtx, ptxStatus_t currentStatus)
{
    ptxStatus_t st = currentStatus;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        switch (nscCtx->SysState)
        {
            /* Overwrite current status if critical system error occurred. */
            case SystemState_ERR_Temperature:
                st = ptxStatus_SysErrOvertemperature;
                break;

            case SystemState_ERR_Overcurrent:
                st = ptxStatus_SysErrOvercurrent;
                break;

            default:
                /* Keep current status. */
                break;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxNSC_Process (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != buff) && (buffLen >= 1u))
    {
        /* Let's check what kind of message has been received.  */
        uint8_t msg_type = (uint8_t)((buff[0] & PTX_NSC_TYPE_FRAME_MASK) >> 6u);

        if ((PTX_NSC_TYPE_FRAME_RSP == msg_type) && (buffLen >= 2u))
        {
            /* RSPs. */
            ptxNSC_ProcessRsp (nscCtx, &buff[0], buffLen);
        } else if ((PTX_NSC_TYPE_FRAME_NTF == msg_type) || (PTX_NSC_TYPE_FRAME_TRANSP == msg_type))
        {

            /* There is one special case when a Transparent message has to be treated as RSP*/
            /* This is the ACK of RF Frame*/
            uint8_t is_RfDataAck;
            status = ptxNSC_IsRfDataAck (nscCtx, buff[0], buff[1u], buffLen, &is_RfDataAck);

            if ((ptxStatus_Success == status) && (1u == is_RfDataAck))
            {
                ptxNSC_ProcessRsp (nscCtx, &buff[0], buffLen);
            } else
            {
                /* NTFs and NSC Transparent messages. */
                ptxNSC_ProcessNtf (nscCtx, &buff[0], buffLen);
            }
        } else
        {
            /* Error in OpCode of Msg Received, nothing to receive. */
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}


ptxStatus_t ptxNSC_HardReset(ptxNSC_t *nscCtx)
{
    ptxStatus_t status;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        status = ptxPLAT_ResetChip(nscCtx->Plat);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}


ptxStatus_t ptxNSC_Reset(ptxNSC_t *nscCtx)
{
    ptxStatus_t status;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        /* Try the platform-dependent HW-reset */
        status = ptxNSC_HardReset(nscCtx);

        if (ptxStatus_NotImplemented == PTX_GET_STATUS(status))
        {
            /* If platform-dependent HW-reset not implemented, perform the standard SW-reset via register access */

#if defined (PTX_INTF_UART)
            /* If UART interface, try to synchronize first. */
            (void) ptxNSC_UARTComSync(nscCtx);
#endif

            status = ptxNSC_SoftReset(nscCtx);
        }

#if defined (PTX_INTF_UART)
        /* After reset, UART host interface operates at default speed. */
        status = ptxNSC_UARTSetDefaultSpeed(nscCtx);
#endif

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static ptxStatus_t ptxNSC_ReadCmd(ptxNSC_t *nscCtx, ptxNSC_ReadCmd_Par_t *nscReadPar, uint8_t *valueRead)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscReadPar) &&
            (nscReadPar->NumOfRead <= PTX_NSC_TYPES_RD_OP_MAX) && (NULL != valueRead))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_read_cmd[PTX_NSC_READ_CMD_HD_LENGTH + (2u * nscReadPar->NumOfRead)];
        size_t expected_read_length = PTX_NSC_READ_RSP_HD_LENGTH + nscReadPar->NumOfRead;        //RESP_OPCODE + ErrorByte + Num data bytes + DataBytes

        size_t nsc_read_cmd_index = 0;
        nsc_read_cmd[nsc_read_cmd_index] = (uint8_t)PTX_NSC_READ_CMD_OPCODE;
        nsc_read_cmd_index++;
        nsc_read_cmd[nsc_read_cmd_index] = (uint8_t)nscReadPar->Type;
        nsc_read_cmd_index++;
        nsc_read_cmd[nsc_read_cmd_index] = (uint8_t)nscReadPar->NumOfRead;
        nsc_read_cmd_index++;

        for (uint8_t i=0; i < (uint8_t)nscReadPar->NumOfRead; i++)
        {
            nsc_read_cmd[nsc_read_cmd_index] = (0xFFu & (uint8_t)(nscReadPar->Addresses[i] >> 8u));
            nsc_read_cmd_index++;
            nsc_read_cmd[nsc_read_cmd_index] = (0xFFu & (uint8_t)nscReadPar->Addresses[i]);
            nsc_read_cmd_index++;
        }

        txBuf[0] = &nsc_read_cmd[0];
        txLen[0] = nsc_read_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_read_cmd_resp_len = 0;
            uint8_t *nsc_read_cmd_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_read_cmd_resp, &nsc_read_cmd_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_read_cmd_resp) && (nsc_read_cmd_resp_len > PTX_NSC_READ_RSP_HD_LENGTH))
            {
                if ((expected_read_length == nsc_read_cmd_resp_len) && ((uint8_t)nscReadPar->NumOfRead == nsc_read_cmd_resp[2]) && (PTX_NSC_READ_RSP_OPCODE == nsc_read_cmd_resp[0]))
                {
                    (void)memcpy(valueRead, &nsc_read_cmd_resp[3], (nsc_read_cmd_resp_len-PTX_NSC_READ_RSP_HD_LENGTH));
                    status = ptxNSC_ProcessRspErrorCode (nsc_read_cmd_resp[1u]);
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

    return status;
}

static ptxStatus_t ptxNSC_WriteCmd(ptxNSC_t *nscCtx, ptxNSC_WriteCmd_Par_t *nscwritePar)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != nscwritePar) &&
            (nscwritePar->NumOfWrite <= PTX_NSC_TYPES_WR_OP_MAX))
    {
        ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;
        uint8_t *txBuf[1u];
        size_t txLen[1u];

        uint8_t nsc_write_cmd[PTX_NSC_WRITE_CMD_HD_LENGTH + (3u * nscwritePar->NumOfWrite)];
        size_t expected_write_length = PTX_NSC_WRITE_RSP_LENGTH;

        size_t nsc_write_cmd_index = 0;
        nsc_write_cmd[nsc_write_cmd_index] = (uint8_t)PTX_NSC_WRITE_CMD_OPCODE;
        nsc_write_cmd_index++;
        nsc_write_cmd[nsc_write_cmd_index] = (uint8_t)nscwritePar->Type;
        nsc_write_cmd_index++;
        nsc_write_cmd[nsc_write_cmd_index] = (uint8_t)nscwritePar->NumOfWrite;
        nsc_write_cmd_index++;

        for (uint8_t i=0; i < (uint8_t)nscwritePar->NumOfWrite; i++)
        {
            nsc_write_cmd[nsc_write_cmd_index] = (0xFFu & (uint8_t)(nscwritePar->Addresses[i] >> 8u));
            nsc_write_cmd_index++;
            nsc_write_cmd[nsc_write_cmd_index] = (0xFFu & (uint8_t)nscwritePar->Addresses[i]);
            nsc_write_cmd_index++;
            nsc_write_cmd[nsc_write_cmd_index] = nscwritePar->Values[i];
            nsc_write_cmd_index++;
        }

        txBuf[0] = &nsc_write_cmd[0];
        txLen[0] = nsc_write_cmd_index;

        status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

        if (ptxStatus_Success == status)
        {
            size_t nsc_write_cmd_resp_len = 0;
            uint8_t *nsc_write_cmd_resp = NULL;

            status = ptxNSC_ReceiveRsp(nscCtx, &nsc_write_cmd_resp, &nsc_write_cmd_resp_len, PTX_NSC_TRANSFER_TO);

            if ((ptxStatus_Success == status) && (NULL != nsc_write_cmd_resp) && (nsc_write_cmd_resp_len > PTX_NSC_WRITE_RSP_LENGTH))
            {
                if ((expected_write_length == nsc_write_cmd_resp_len) && (PTX_NSC_WRITE_RSP_OPCODE == nsc_write_cmd_resp[0]))
                {
                    status = ptxNSC_ProcessRspErrorCode (nsc_write_cmd_resp[1u]);
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

    return status;
}

static ptxStatus_t ptxNSC_WaitForRsp(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen, uint32_t timeOut)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != nscCtx) && (NULL != rsp) && (NULL != rspLen))
    {
        // Clear new rsp, as new response it is expected.
        ptxNSC_ClearRspRcvd(nscCtx);

        // Let's check if there is already new response
        uint8_t is_new_rsp = ptxNSC_IsNewRspRcvd(nscCtx);

        if (1u == is_new_rsp)
        {
            status = ptxNSC_GetNewRspRcvd(nscCtx, rsp, rspLen);
        } else
        {
            struct ptxPlatTimer *timer = NULL;

            // Let's get a Timer
            status = ptxPLAT_GetInitializedTimer(nscCtx->Plat, &timer);

            if (ptxStatus_Success == status)
            {
                // Let's start a Timer
                status = ptxPLAT_TimerStart(nscCtx->Plat, timer, timeOut, 0u, NULL, NULL);

                if (ptxStatus_Success == status)
                {
                    uint8_t is_timer_elapsed = 0;
                    uint8_t rx_is_ongoing = 0;
                    uint8_t is_rx_pending = 0;

                    do
                    {
                        if((0 == rx_is_ongoing))
                        {
                            (void) ptxPLAT_DisableInterrupts (nscCtx->Plat);

                            // Check if the timer has elapsed
                            (void) ptxPLAT_TimerIsElapsed(nscCtx->Plat, timer, &is_timer_elapsed);

                            // Check if there is an rx pending operation
                            (void) ptxPLAT_IsRxPending (nscCtx->Plat, &is_rx_pending);

                            // Check that nor IRQ pin neither Timer has already triggered
                            if ((0 == is_timer_elapsed) && (0 == is_rx_pending))
                            {
                                // Wait for any interrupt on the CPU
                                (void) ptxPLAT_WaitForInterrupt(nscCtx->Plat);
                            }

                            (void) ptxPLAT_EnableInterrupts (nscCtx->Plat);
                        }

                        rx_is_ongoing = ptxPLAT_CheckRxActive(nscCtx->Plat);

                        // Check if a new response has been received
                        is_new_rsp = ptxNSC_IsNewRspRcvd(nscCtx);

                        if (0 == is_timer_elapsed)
                        {
                            // Check if the timer has elapsed
                            (void) ptxPLAT_TimerIsElapsed(nscCtx->Plat, timer, &is_timer_elapsed);
                        }

                    } while((0 == is_timer_elapsed) && (0 == is_new_rsp));

                    if (0 == is_new_rsp)
                    {
                        // Let's check again if there has not been any Rsp received, just in case
                        is_new_rsp = ptxNSC_IsNewRspRcvd(nscCtx);
                    }

                    // Get Response in case of received
                    if (0 == is_new_rsp)
                    {
                        /**
                         * Clean up.
                         */
                        (void)ptxNSC_ClearRspRcvd(nscCtx);

                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_TimeOut);
                    } else if (1u == is_new_rsp)
                    {
                        status = ptxNSC_GetNewRspRcvd(nscCtx, rsp, rspLen);
                    } else
                    {
                        /**
                         * Clean up.
                         */
                        (void)ptxNSC_ClearRspRcvd(nscCtx);

                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                    }
                }

                // Release the timer
                (void) ptxPLAT_TimerDeinit(nscCtx->Plat, timer);
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_ClearRspRcvd(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;
    nscCtx->NscRsp.NewRspReceived = 0;

#if defined (PTX_INTF_UART)
    /** Prepare NSC Rx control structure. */
    (void) ptxNSC_UARTSetCleanStateRx (nscCtx);
#endif

    return status;
}

static uint8_t ptxNSC_IsNewRspRcvd(ptxNSC_t *nscCtx)
{
    /* Let's check if there has been any thing received. */
    ptxStatus_t status = ptxStatus_Success;
    uint8_t is_new_rsp = 0;

    status = ptxPLAT_TriggerRx(nscCtx->Plat);

    if (ptxStatus_Success == status)
    {
        is_new_rsp = nscCtx->NscRsp.NewRspReceived;
    }

    return is_new_rsp;
}

static ptxStatus_t ptxNSC_SetNewRspRcvd(ptxNSC_t *nscCtx, uint8_t *rsp, size_t rspLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != rsp) && (rspLen > 0) && (rspLen <= PTX_NSC_MAX_RSP_LEN))
    {
        /* Set flag; new response received. */
        nscCtx->NscRsp.NewRspReceived = 1u;
        (void)memcpy(&nscCtx->NscRsp.RspBuff[0], rsp, rspLen);
        nscCtx->NscRsp.RspLen = rspLen;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return status;
}

static ptxStatus_t ptxNSC_GetNewRspRcvd(ptxNSC_t *nscCtx, uint8_t **rsp, size_t *rspLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != rsp) && (NULL != rspLen))
    {
        if (1u == nscCtx->NscRsp.NewRspReceived)
        {
            /* Clear flag; new response received. */
            nscCtx->NscRsp.NewRspReceived = 0u;
            *rsp = &nscCtx->NscRsp.RspBuff[0];
            *rspLen = nscCtx->NscRsp.RspLen;
        }else
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return status;
}

static void ptxNSC_ProcessRsp (ptxNSC_t *nscCtx, uint8_t *buff, size_t buffLen)
{
    if ((NULL != buff) && (buffLen >= 2u))
    {
        (void)ptxNSC_SetNewRspRcvd(nscCtx, buff, buffLen);
    }
}

ptxStatus_t ptxNSC_Start_WaitForRx(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        status = ptxPLAT_StartWaitForRx(nscCtx->Plat, (pptxPlat_RxCallBack_t) &ptxNSC_GetRx, nscCtx);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNSC_Stop_WaitForRx(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        status = ptxPLAT_StopWaitForRx(nscCtx->Plat);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_Hal_WriteInstruction_Framing (ptxNSC_t *nscCtx, uint16_t address, uint8_t *pPayload, size_t txLen )
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != pPayload) && (txLen > 0))
    {
        uint8_t *payload = pPayload;
        size_t tx_len_remain = txLen;
        uint16_t addr = address;

        /*
         * Times buffer size.
         */
        while (tx_len_remain > PTX_NSC_HAL_PAYLOAD_LENGTH_WI_MAX)
        {
            status = ptxNSC_HAL_WriteInstruction (nscCtx, addr, payload, PTX_NSC_HAL_PAYLOAD_LENGTH_WI_MAX);

            if (ptxStatus_Success != status)
            {
                break;
            }

            tx_len_remain -= PTX_NSC_HAL_PAYLOAD_LENGTH_WI_MAX;
            payload += PTX_NSC_HAL_PAYLOAD_LENGTH_WI_MAX;
            addr = (uint16_t)(addr + (PTX_NSC_HAL_PAYLOAD_LENGTH_WI_MAX / 2u));
        }

        /*
         * Send the remaining ones.
         */
        if (ptxStatus_Success == status)
        {
            status = ptxNSC_HAL_WriteInstruction (nscCtx, addr, payload, tx_len_remain);
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_DataMsg_Ack(ptxNSC_t *nscCtx,uint8_t *buff, size_t buffLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (buff != NULL) && (buffLen > 1u) )
    {
        uint8_t is_RfError_Ntf = 0;

        /* Has been received an Rf Error NTF ? */
        status = ptxNSC_IsRfErrorNtf (nscCtx, buff[0], buffLen, &is_RfError_Ntf);

        if (status == ptxStatus_Success)
        {
            if (1u == is_RfError_Ntf)
            {
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscRfError);
            } else
            {
                uint8_t is_RfData_Ack = 0;

                status = ptxNSC_IsRfDataAck (nscCtx,  buff[0],  buff[1u], buffLen, &is_RfData_Ack);

                if (status == ptxStatus_Success)
                {
                    if (1u == is_RfData_Ack)
                    {
                        /* Successful operation. */
                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                    }
                }
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_CheckHWVersion (ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t value = 0;

    /* Let's read the Version of PTX1K. */
    status = ptxNSC_HAL_Rra(nscCtx, VERSION_REG, &value);

    if (ptxStatus_Success == status)
    {
        if (VERSION_REG_RST == value)
        {
            /* Correct PTX1K version read out. */
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
        }
    }

    return status;
}

static ptxStatus_t ptxNSC_IsRfErrorNtf (ptxNSC_t *nscCtx, uint8_t first_byte, size_t len, uint8_t *isRfErrorNtf)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((len > 0) && (NULL != isRfErrorNtf))
    {
        *isRfErrorNtf = 0;
        if ((PTX_NSC_OPCODE_RFERRORNTF == first_byte) && (PTX_NSC_RFERRORNTF_LENGTH == len))
        {
            *isRfErrorNtf = 1u;
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return status;
}

static ptxStatus_t ptxNSC_IsRfDataAck (ptxNSC_t *nscCtx, uint8_t first_byte, uint8_t second_byte, size_t len, uint8_t *isRfDataAck)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((len > 1u) && (NULL != isRfDataAck))
    {
        *isRfDataAck = 0;

        if ((PTX_NSC_RFD_CTRL_OPCODE == first_byte)
                && (PTX_NSC_DATA_MSG_CON_MSG_LENGTH_VALUE_ACK == second_byte)
                                                            && (2u == len))
        {
            *isRfDataAck = 1u;
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    (void)nscCtx;
    return status;
}


