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
    Module      : IOT_READER
    File        : ptx_IOT_READER.c

    Description : API Implementation for IOT READER
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptx_IOT_READER.h"
#include "ptxNSC.h"
#include "ptxNSC_System.h"
#include "ptxNSC_Registers.h"
#include "ptxNSC_Rd.h"
#include "ptxPLAT.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define IOTRD_BUFFER_SIZE                               (1024u)
#define IOTRD_BUFFER_SIZE_LOGGING                       (512u)

/*
 * 1 Timer-tick in the NFC-HW is 32us
 * */
#define IOTRD_IDLE_TIME_TICK_MS                         (uint32_t)(1000/32)
#define IOTRD_IDLE_TIME_MAX_MASK                        (uint32_t)(0x000FFFFF)

#define IOTRD_DEACTIVATION_NTF_PENDING_TIMEOUT_MS       (uint32_t)3000

/**
 * Maximum general bytes length for LLCP protocol activation
 */
#define IOTRD_MAX_GEN_BYTES                             (uint8_t)20

#define IOTRD_EXTENSION_ID_AVAILABLE                    (uint8_t)0xFF

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/*
 * IoT Reader. Type of Rf Event Received.
 */
typedef enum ptxIoTRd_RfEventRcvd_Id
{
    RfEvent_NotRcvd,
    RfEvent_RfMsgRcvd,
    RfEvent_RfMsgChainedRcvd,
    RfEvent_RfErrorRcvd,
    RfEvent_RfErrorTimeOutRcvd,
    RfEvent_RfCltRcvd,
    RfEvent_RfCtrlAck,
    RfEvent_RfCtrlAttCmd
}ptxIoTRd_RfEventRcvd_Id_t;


static ptxStatus_t ptxIoTRd_GetRfDiscParams(ptxIoTRd_t *iotRd, ptxNSC_RfDiscPars_t *nscRfDiscPars);
static ptxStatus_t ptxIoTRd_ResetCardRegistry(ptxIoTRd_t *iotRd);
static void ptxIoTRd_CallBackEvents (void *iotRd, ptxNSC_Event_t *event);
static ptxStatus_t ptxIoTRd_RcvRfMsg (ptxIoTRd_t *iotRd, uint8_t *msgDataRx, size_t *msgLenRx, uint8_t *isChained, uint32_t msTimeOut);
static ptxStatus_t ptxIoTRd_RcvRfCltMsg (ptxIoTRd_t *iotRd, uint8_t *pld, uint8_t *pldPar, size_t *pldRx, size_t *numTotalBits, uint32_t msTimeOut);
static ptxIoTRd_RfEventRcvd_Id_t ptxIoTRd_IsRfRcvd(ptxIoTRd_t *iotRd, ptxIoTRd_RfMsg_t *RfMsg);
static ptxStatus_t ptxIoTRd_ClearRfMsgRcvd(ptxIoTRd_t *iotRd, ptxIoTRd_RfMsg_t *RfMsg);
static ptxStatus_t ptxIoTRd_RfPresenceCheck_ISODEP_Nack (ptxIoTRd_t *iotRd);
static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_ISODEP_Ack (ptxIoTRd_t *iotRd, uint32_t timeoutMs);
static ptxStatus_t ptxIoTRd_RfPresenceCheck_ISODEP_EmptyFrame (ptxIoTRd_t *iotRd);
static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_ISODEP_EmptyFrame (ptxIoTRd_t *iotRd, uint32_t timeoutMs);
static ptxStatus_t ptxIoTRd_RfPresenceCheck_NFCDEP_AttCmd (ptxIoTRd_t *iotRd);
static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_NFCDEP_AttCmd (ptxIoTRd_t *iotRd, uint32_t timeoutMs);
static ptxStatus_t ptxIoTRd_NSCRfSetCltMode (ptxIoTRd_t *iotRd);
static ptxStatus_t ptxIoTRd_NSCRfClearCltMode (ptxIoTRd_t *iotRd);
static ptxStatus_t ptxIoTRd_TempOffsetComp (ptxIoTRd_t *iotRd, int8_t *tempOffset, uint8_t *tempVal);
static ptxStatus_t ptxIoTRd_Set_T3T_MultiRxMode(ptxIoTRd_t *iotRd, uint8_t enableRxMode, uint32_t timeoutMS);
static ptxStatus_t ptxIoTRd_Manage_DDPC (ptxIoTRd_t *iotRd, uint8_t enableDDPC);

/*
 * IoT Reader. Add Card to registry from NTF.
 */
static ptxStatus_t ptxIoTRd_AddCardToRegistryFromNTF(ptxIoTRd_t *iotRd, ptxNSC_Event_t *event);

/*
 * IoT Reader. Add Card to registry from RSP.
 */
static ptxStatus_t ptxIoTRd_AddCardToRegistryFromRSP(ptxIoTRd_t *iotRd,
                                                  ptxIoTRd_CardParams_t *cardParams,
                                                  ptxIoTRd_CardProtocol_t protocol,
                                                  uint8_t *activationData,
                                                  size_t activationDataLen);

/*
 * IoT Reader. Convert input (RF-)configuration ID to internally used NSC-ID.
 */
static ptxNSC_RfConfig_ParamList_t ptxIoTRd_MapRFConfigID(ptxIoTRd_ChipConfigID_t configID);

/*
 * Card registry used for the storage of discovered cards´ data.
 * NOTE: beware of the maximum number of cards available to store.
 */
ptxIoTRd_CardRegistry_t discCardRegistry;


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxIoTRd_InitNSC(ptxIoTRd_t *iotRd, ptxIoTRd_ComInterface_Params_t *initParams)
{
	ptxStatus_t st = ptxStatus_Success;

	/* Initialize PLAT Component */

	ptxPLAT_ConfigPars_t plat_initializer;
	plat_initializer.Speed = 0;

	if (NULL != initParams)
	{
		plat_initializer.Speed = initParams->Speed;
		plat_initializer.DeviceAddress = initParams->DeviceAddress;

		if (NULL == iotRd->Plat)
		{
			st = ptxPLAT_AllocAndInit(&iotRd->Plat, &plat_initializer);
		}
	}

	if (ptxStatus_Success == st)
	{
		/* Initialize NSC Component */
		ptxNSC_ConfigPars_t config_Pars;
		config_Pars.Plat = iotRd->Plat;
		config_Pars.WfeCb = ptxIoTRd_CallBackEvents;
		config_Pars.Ctx = iotRd;

		st = ptxNSC_Init(&(iotRd->Nsc), &config_Pars);
	}

	return st;
}

ptxStatus_t ptxIoTRd_Init(ptxIoTRd_t *iotRd, ptxIoTRd_InitPars_t *initParams)
{
    ptxStatus_t st = ptxStatus_Success;

    if (iotRd != NULL)
    {
        /* Initialize IOT Rd Component. */

        iotRd->CompId = ptxStatus_Comp_IoTReader;

        /* Allocate memory for the card registry */
        iotRd->CardRegistry = &discCardRegistry;
        (void)memset(iotRd->CardRegistry, 0, sizeof(ptxIoTRd_CardRegistry_t));

        /* set Extension IDs */
        for (uint8_t i = 0; i < (uint8_t)PTX_IOTRD_MAX_EXTENSIONS; i++)
        {
            iotRd->Extension[i].ExtensionID = IOTRD_EXTENSION_ID_AVAILABLE;
        }

        /* Initialize PLAT Component */
		ptxPLAT_ConfigPars_t plat_initializer = {0};

		if (NULL != initParams->ComInterface)
		{
			plat_initializer.Speed = initParams->ComInterface->Speed;
			plat_initializer.DeviceAddress = initParams->ComInterface->DeviceAddress;
		}

        if (NULL == iotRd->Plat)
        {
			st = ptxPLAT_AllocAndInit(&iotRd->Plat, &plat_initializer);
        }

        if (ptxStatus_Success == st)
        {
        	if (NULL == iotRd->Nsc)
        	{
        		st = ptxIoTRd_InitNSC(iotRd, NULL);
        	}
        }

        if (ptxStatus_Success == st)
        {
            /* Initialize the PTX1K. */

            /* Update temperature sensor data with the data provided by the user, or calibrate sensor. */
            if (NULL != initParams->TemperatureSensor)
            {
                uint8_t tempsense_shutdown = initParams->TemperatureSensor->Tshutdown;
                if (1u == initParams->TemperatureSensor->Calibrate)
                {
                    st = ptxIoTRd_TempSensor_Calibration (iotRd, initParams->TemperatureSensor->Tambient, &tempsense_shutdown);
                }

                if (ptxStatus_Success == st)
                {
                    /* Return to the user compensated over-temperature treshold. */
                    initParams->TemperatureSensor->Tshutdown = tempsense_shutdown;

                    /* Update system configuration with fresh parameter. */
                    ptxNSC_t *temp_nsc = iotRd->Nsc;

                    /* GET */
                    ptxNSC_System_t sys_cfg_params;
                    st = ptxNSC_System_GetConfig (temp_nsc, &sys_cfg_params);
                    if (ptxStatus_Success == st)
                    {
                        /* UPDATE */
                        sys_cfg_params.PowerAmpTempThreshold = tempsense_shutdown;

                        /* SET */
                        sys_cfg_params.UseExtCfg = 1u;

                        st = ptxNSC_System_SetConfig (temp_nsc, &sys_cfg_params);
                    }
                }
            }
        }

        if (ptxStatus_Success == st)
        {
            /* Reset system-state */
            iotRd->Nsc->SysState = SystemState_OK;

            /* Download the FW for PTX1K. */
            st = ptxNSC_FwDownloader(iotRd->Nsc);

            /* DFY Activation. */
            if (ptxStatus_Success == st)
            {
                st = ptxNSC_DFY_Activation(iotRd->Nsc);
            }

            if (ptxStatus_Success == st)
            {
                /* NSC_INIT_CMD for PTX1K. */
                ptxNSC_InitPars_t nsc_Init_Pars;

                uint8_t con_clk_src = 0;                                                            /* Cristal-clock. */
                uint8_t con_var_lbs = (uint8_t)((uint8_t)1 << 5);                                   /* Reference division select parameter. */
                uint8_t con_NHost[PTX_NSC_INIT_CON_HOST_LENGTH] = {0x78, 0x00, 0x00, 0x00};         /* Division factor for the clock synthesizer. */
                uint8_t con_NHost_Ce[PTX_NSC_INIT_CON_HOST_LENGTH] = {0x78, 0x40, 0x00, 0x00};      /* Division factor for the clock synthesizer during the CE (Card Emulation) mode.*/
                uint8_t con_Uart_Config[PTX_NSC_INIT_CON_UART_CONFIG] = {0x3A, 0x03};               /* UART configuration data. 115200 and HW Flow Control deactivated. */
                (void)ptxNSC_GetInitConfigParams(iotRd->Nsc, plat_initializer.Speed, &con_Uart_Config[0]);

                uint8_t con_Prng_Seed[PTX_NSC_INIT_CON_PRNG_SEED] = {0x44, 0x44, 0x44, 0x44};       /* PRNG seed value. */
                uint8_t con_N_Alm_Max[PTX_NSC_INIT_CON_N_ALM_MAX] = {0x3C, 0x72, 0x7E, 0x12};       /* Maximum allowed N_ALM value for ALM frequency correction. */
                uint8_t con_N_Alm_Min[PTX_NSC_INIT_CON_N_ALM_MIN] = {0x3B, 0xCE, 0x54, 0x0F};       /* Minimum allowed N_ALM value for ALM frequency correction. */
                uint8_t con_paocp_th = 0x04u;                                                       /* Over-current protection default threshold. */
                uint8_t con_patp_th = 0xFFu;                                                        /* Thermal protection default threshold. */

                nsc_Init_Pars.Con_Clk_Src = con_clk_src;
                nsc_Init_Pars.Con_Var_Lbs = con_var_lbs;
                (void) memcpy(&nsc_Init_Pars.Con_NHost[0], &con_NHost[0], PTX_NSC_INIT_CON_HOST_LENGTH);
                (void) memcpy(&nsc_Init_Pars.Con_NHost_Ce[0], &con_NHost_Ce[0], PTX_NSC_INIT_CON_HOST_CE_LENGTH);
                (void) memcpy(&nsc_Init_Pars.Con_Uart_Config[0], &con_Uart_Config[0], PTX_NSC_INIT_CON_UART_CONFIG);
                (void) memcpy(&nsc_Init_Pars.Con_Prng_Seed[0], &con_Prng_Seed[0], PTX_NSC_INIT_CON_PRNG_SEED);
                (void) memcpy(&nsc_Init_Pars.Con_N_Alm_Max[0], &con_N_Alm_Max[0], PTX_NSC_INIT_CON_N_ALM_MAX);
                (void) memcpy(&nsc_Init_Pars.Con_N_Alm_Min[0], &con_N_Alm_Min[0], PTX_NSC_INIT_CON_N_ALM_MIN);

                nsc_Init_Pars.Con_Paocp_Th = con_paocp_th;
                nsc_Init_Pars.Con_Patp_Th = con_patp_th;

                nsc_Init_Pars.Con_Xcp_Ctrl = 0u;
                nsc_Init_Pars.Con_Xcp_Th_Gt = 0u;

                if ( (NULL != initParams->ExtProtection) && (initParams->ExtProtection->Type != ProtType_None) )
                {
                    nsc_Init_Pars.Con_Xcp_Ctrl |= initParams->ExtProtection->Type;

                    if (ProtType_CurrentSensor == initParams->ExtProtection->Type)
                    {
                        nsc_Init_Pars.Con_Xcp_Ctrl |= initParams->ExtProtection->Settings.Sensor.Supply;
                        nsc_Init_Pars.Con_Xcp_Ctrl |= initParams->ExtProtection->Settings.Sensor.Attenuation;
                        nsc_Init_Pars.Con_Xcp_Th_Gt = initParams->ExtProtection->Settings.Sensor.Threshold;
                    }
                    else
                    {
                        nsc_Init_Pars.Con_Xcp_Th_Gt = initParams->ExtProtection->Settings.Limiter.GuardTime;
                    }
                }


                ptxNSC_t *temp_nsc = iotRd->Nsc;
                if (1u == temp_nsc->SysParams->UseExtCfg)
                {
                    ptxNSC_System_t sys_cfg_params;
                    st = ptxNSC_System_GetConfig (iotRd->Nsc, &sys_cfg_params);
                    if (ptxStatus_Success == st)
                    {
                        nsc_Init_Pars.Con_Paocp_Th = sys_cfg_params.PowerAmpOverCurrThreshold;
                        nsc_Init_Pars.Con_Patp_Th = sys_cfg_params.PowerAmpTempThreshold;
                        nsc_Init_Pars.Con_Clk_Src = sys_cfg_params.ConClkSource;
                        nsc_Init_Pars.Con_Var_Lbs = sys_cfg_params.ConVarLBS;
                        (void) memcpy(&nsc_Init_Pars.Con_NHost[0], &sys_cfg_params.conHost[0], PTX_NSC_INIT_CON_HOST_LENGTH);
                        (void) memcpy(&nsc_Init_Pars.Con_NHost_Ce[0], &sys_cfg_params.conHostCE[0], PTX_NSC_INIT_CON_HOST_CE_LENGTH);
                    }
                }

                st = ptxNSC_InitCmd(iotRd->Nsc, &nsc_Init_Pars);
            }

            if (ptxStatus_Success == st)
            {
                /* InitCmd was successful. Device is in RfIdle state. */

                /* NSC_RFCONFIG_CMD for PTX1K. Use default settings. */
                st = ptxNSC_RfConfig(iotRd->Nsc, NULL, 0);
            }

            if (ptxStatus_Success == st)
            {
                st = ptxHce_Init(&iotRd->Hce, iotRd->Plat, iotRd->Nsc, iotRd->BuffNtf, PTX_IOTRD_RF_MSG_MAX_SIZE);
            }
        }

        /* check for pending system errors (e.g. termperature-errors during initialization) */
        st = ptxNSC_CheckSystemState(iotRd->Nsc, st);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Initiate_Discovery (ptxIoTRd_t *iotRd, ptxIoTRd_DiscConfig_t *discConfig)
{
    ptxStatus_t st = ptxStatus_Success;
    uint32_t idle_time;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader))
    {
        ptxNSC_RfDiscPars_t rf_disc_params;

        st = ptxIoTRd_GetRfDiscParams(iotRd, &rf_disc_params);

        if (ptxStatus_Success == st)
        {
            /* reset the internal card registry */
            (void)ptxIoTRd_ResetCardRegistry(iotRd);

            /* overwrite default values ? */
            if (NULL != discConfig)
            {
                iotRd-> StandbyActive = 0;

                /* Stand-by mode enabled and IDLE-time == 0 is not allowed */
                if ((0 != discConfig->EnableStandBy) && (0 == discConfig->IdleTime))
                {
                    st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NotPermitted);
                }

                if (ptxStatus_Success == st)
                {
                    if (0 != discConfig->EnableLPCDNotification)
                    {
                        rf_disc_params.Con_Poll = (uint8_t)(rf_disc_params.Con_Poll | (uint8_t)0x80);
                    }

                    rf_disc_params.Con_Poll_A   = ((uint8_t)0 == discConfig->PollTypeA) ? (uint8_t)(0x00) : (uint8_t)(0x05);
                    rf_disc_params.Con_Poll_B   = ((uint8_t)0 == discConfig->PollTypeB) ? (uint8_t)(0x00) : (uint8_t)(0x01);
                    rf_disc_params.Con_Poll_F   = 0;
                    rf_disc_params.Con_Poll_V   = ((uint8_t)0 == discConfig->PollTypeV) ? (uint8_t)(0x00) : (uint8_t)(0x01);

                    if (0 != discConfig->PollTypeF212)
                    {
                        rf_disc_params.Con_Poll_F = 0x01u;
                    }

                    if (0 != discConfig->PollTypeF424)
                    {
                        rf_disc_params.Con_Poll_F = 0x03u;
                    }

                    if (0 != discConfig->EnableIsoPollMode)
                    {
                        rf_disc_params.Con_Poll = IsoPollMode;
                    }

                    iotRd->PollMode = rf_disc_params.Con_Poll;

                    rf_disc_params.Con_Poll = (uint8_t)((rf_disc_params.Con_Poll & 0x8F));

                    switch (discConfig->PollStartTechnology)
                    {
                        case Tech_TypeB:
                            rf_disc_params.Con_Poll |= (uint8_t)(Init_Poll_B << 4u);
                            break;

                        case Tech_TypeF:
                            rf_disc_params.Con_Poll |= (uint8_t)(Init_Poll_F << 4u);
                            break;

                        case Tech_TypeV:
                            rf_disc_params.Con_Poll |= (uint8_t)(Init_Poll_V << 4u);
                            break;

                        case Tech_TypeA:
                        default:
                            rf_disc_params.Con_Poll |= (uint8_t)(Init_Poll_A << 4u);
                            break;
                    }

                    if (0 != (PTX_IOTRD_RF_BAILOUT_TECH_A & discConfig->PollBailOutFlags))
                    {
                        rf_disc_params.Con_Poll_A |= (uint8_t)0x02;
                    }

                    if (0 != (PTX_IOTRD_RF_BAILOUT_TECH_B & discConfig->PollBailOutFlags))
                    {
                        rf_disc_params.Con_Poll_B |= (uint8_t)0x02;
                    }

                    if (0 != (PTX_IOTRD_RF_BAILOUT_TECH_F & discConfig->PollBailOutFlags))
                    {
                        rf_disc_params.Con_Poll_F |= (uint8_t)0x04;
                    }

                    if (0 != discConfig->EnableExtdAtqB)
                    {
                        rf_disc_params.Con_Poll_B_Cmd[2] = rf_disc_params.Con_Poll_B_Cmd[2] | 0x10;
                    }

                    if (0 != discConfig->AfiValue)
                    {
                        rf_disc_params.Con_Poll_B_Cmd[1] = discConfig->AfiValue;
                    }


                    if (0 != discConfig->PollTypeADeviceLimit)
                    {
                        rf_disc_params.Con_Poll_A_Dev_Limit = discConfig->PollTypeADeviceLimit;
                    }

                    if (0 != discConfig->PollTypeBDeviceLimit)
                    {
                        rf_disc_params.Con_Poll_B_Dev_Limit = discConfig->PollTypeBDeviceLimit;
                    }

                    if (0 != discConfig->PollTypeFDeviceLimit)
                    {
                        rf_disc_params.Con_Poll_F_Dev_Limit = discConfig->PollTypeFDeviceLimit;
                    }

                    if (0 != discConfig->PollTypeVDeviceLimit)
                    {
                        rf_disc_params.Con_Poll_V_Dev_Limit = discConfig->PollTypeVDeviceLimit;
                    }

                    if (0 != discConfig->DisableIsoDepProtocol)
                    {
                        rf_disc_params.Con_Poll_Iso_Dep = 0;
                    }

                    if (0 != discConfig->DisableNfcDepProtocol)
                    {
                        rf_disc_params.Con_Poll_Nfc_Dep = 0;
                    }

                    if (NULL != discConfig->ConPollNfcDepAtrReqG
                        && 0 <  discConfig->ConPollNfcDepAtrReqGLen
                        && IOTRD_MAX_GEN_BYTES >= discConfig->ConPollNfcDepAtrReqGLen)
                    {
                        memcpy(&rf_disc_params.Con_Poll_Nfc_Dep_Atr_Req_G[0], &discConfig->ConPollNfcDepAtrReqG[0], IOTRD_MAX_GEN_BYTES);
                        rf_disc_params.Con_Poll_Nfc_Dep_Atr_Req_G_Len = discConfig->ConPollNfcDepAtrReqGLen;

                        rf_disc_params.Con_Poll_Nfc_Dep_Atr_Req_Pp |= 0x02;
                    } else
                    {
                        rf_disc_params.Con_Poll_Nfc_Dep_Atr_Req_Pp &= (uint8_t)~0x02;
                    }

                    rf_disc_params.Con_Poll_Disc_Mode = discConfig->Discover_Mode;

                    idle_time = ((discConfig->IdleTime * IOTRD_IDLE_TIME_TICK_MS) & IOTRD_IDLE_TIME_MAX_MASK);
                    rf_disc_params.Con_Idle_Time[0] = (uint8_t)((idle_time >> 16) & (uint8_t)0xFF);
                    rf_disc_params.Con_Idle_Time[1] = (uint8_t)((idle_time >>  8) & (uint8_t)0xFF);
                    rf_disc_params.Con_Idle_Time[2] = (uint8_t)((idle_time >>  0) & (uint8_t)0xFF);

                    rf_disc_params.Con_Poll_Gt = 0;
                    if (0 != discConfig->PollGuardTime)
                    {
                        rf_disc_params.Con_Poll_Gt = discConfig->PollGuardTime;
                    }

                    if (1u == discConfig->ContinuousField)
                    {
                        rf_disc_params.Con_Poll = (uint8_t)ConstantField;
                    }

                    if (1u == discConfig->ListenTypeA)
                    {
                        rf_disc_params.Con_Listen = 0x03;                   /* Enable general Listen Mode + Field-NTFs */

                        /* Listen-A Technology. */
                        rf_disc_params.Con_Listen_A = 0x01;                 /* Enable Type-A */
                        rf_disc_params.Con_Listen_A_Sens_Res[0] = 0x01;
                        rf_disc_params.Con_Listen_A_Sens_Res[1] = 0x00;     /* SENS_RES */
                        rf_disc_params.Con_Listen_A_Sel_Res = 0x20;         /* Support ISO-DEP Protocol */
                        rf_disc_params.Con_Listen_A_Nfcid1[0] = 0x08;
                        rf_disc_params.Con_Listen_A_Nfcid1[1] = 0x01;
                        rf_disc_params.Con_Listen_A_Nfcid1[2] = 0x02;
                        rf_disc_params.Con_Listen_A_Nfcid1[3] = 0x03;       /* 4-Byte NFCID-1 ID */
                        rf_disc_params.Con_Listen_A_Nfcid1[4] = 0x08;       /* BCC of NFCID-1 ID */

                        /*Listen-A ISO-DEP Protocol. */
                        rf_disc_params.Con_Listen_Iso_Dep = 0x01;           /* Let uCode to handle ISO-DEP protocol */
                        rf_disc_params.Con_Listen_Iso_Dep_Ats[0] = 0x05;    /* ATS-Length TL */
                        rf_disc_params.Con_Listen_Iso_Dep_Ats[1] = 0x78;    /* TA(1), TB(1), TC(1), following, FSCI = 8 (256 byte) */
                        rf_disc_params.Con_Listen_Iso_Dep_Ats[2] = 0x00;    /* only 106 kBit/s */
                        rf_disc_params.Con_Listen_Iso_Dep_Ats[3] = 0x91;    /* FWI = 9, SFGI = 1*/
                        rf_disc_params.Con_Listen_Iso_Dep_Ats[4] = 0x00;    /* DID not supported */
                    }

                    rf_disc_params.Con_Idle = 0;
                    if (0 != discConfig->EnableStandBy)
                    {
                        /* Use Stand-by, Wake-up via Host-Interface or internal Timer */
                        rf_disc_params.Con_Idle = 0x89u;

                        /* Set internal flag to indicate that the HW could potentially by in Stand-by mode for certain operations */
                        iotRd-> StandbyActive = 1;

                        /* Wake up also via by RF-field */
                        if (0 != rf_disc_params.Con_Listen)
                        {
                            rf_disc_params.Con_Idle |= (uint8_t)0x02;
                        }
                    }

                    if (1u == discConfig->EnableHbr)
                    {
                        rf_disc_params.Con_Poll_Iso_Dep |= 0x02u;
                    }

                    if ((1u == discConfig->ListenTypeA) && (0u == discConfig->PollTypeA) && (0u == discConfig->PollTypeB) && (0u == discConfig->PollTypeF212) && (0u == discConfig->PollTypeF424) && (0u == discConfig->PollTypeV))
                    {
                        rf_disc_params.Con_Poll = 0x00;
                    }
                }
            }

            if (ptxStatus_Success == st)
            {
                st = ptxNSC_RfDiscovery(iotRd->Nsc, &rf_disc_params);
            }
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Update_ChipConfig (ptxIoTRd_t *iotRd, uint8_t nrConfigs, ptxIoTRd_ChipConfig_t *configParams)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t i;
    uint8_t rfconfig_tlv_count = 0;
    uint8_t update_rf_cfg = 0;
    uint8_t update_sys_cfg = 0;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (NULL != configParams) && (0 != nrConfigs))
    {
        ptxNSC_System_t sys_cfg_params;
        uint8_t num_of_tlvs = nrConfigs;

        ptxNSC_RfConfigTlv_t nsc_rf_cfg_params[num_of_tlvs];
        ptxIoTRd_ChipConfig_t* current_cfg = configParams;

        (void)memset(&sys_cfg_params, 0, sizeof(ptxNSC_System_t));
        (void)memset(&nsc_rf_cfg_params[0], 0, sizeof(ptxNSC_RfConfigTlv_t) * num_of_tlvs);

        status = ptxNSC_System_GetConfig (iotRd->Nsc, &sys_cfg_params);
        if (ptxStatus_Success == status)
        {
            update_sys_cfg = 1u;
        }

        for (i = 0; i < nrConfigs; i++)
        {
            if ((NULL != current_cfg) && ((RF_LAST_RF_CONFIG_ENTRY > current_cfg->ID)))
            {
                ptxNSC_RfConfig_ParamList_t nsc_id = ptxIoTRd_MapRFConfigID(current_cfg->ID);

                if ((NULL != current_cfg->Value) && (0 != current_cfg->Len) && (nsc_id != RfCfgParam_Undefined))
                {
                    nsc_rf_cfg_params[rfconfig_tlv_count].ID = nsc_id;
                    nsc_rf_cfg_params[rfconfig_tlv_count].Value = current_cfg->Value;
                    nsc_rf_cfg_params[rfconfig_tlv_count].Len = current_cfg->Len;
                    rfconfig_tlv_count++;
                    update_rf_cfg = 1u;
                }
            } else if ((NULL != current_cfg) && (0 != update_sys_cfg))
            {
                switch (current_cfg->ID)
                {
                    case SYS_ThermalThreshold:
                        sys_cfg_params.PowerAmpTempThreshold = current_cfg->Value[0];
                        break;

                    case SYS_OvercurrentThreshold:
                        sys_cfg_params.PowerAmpOverCurrThreshold = current_cfg->Value[0];
                        break;

                    case SYS_ConClockSrc:
                        sys_cfg_params.ConClkSource = current_cfg->Value[0];
                        break;

                    case SYS_ConVarLBS:
                        sys_cfg_params.ConVarLBS = current_cfg->Value[0];
                        break;

                    case SYS_ConNHost:
                        (void)memcpy(&sys_cfg_params.conHost[0], &current_cfg->Value[0], 0x04u);
                        break;

                    case SYS_ConNHosCE:
                        (void)memcpy(&sys_cfg_params.conHostCE[0], &current_cfg->Value[0], 0x04u);
                        break;

                    default:
                        /* ignore */
                        break;
                }

                sys_cfg_params.UseExtCfg = 0x01u;
                update_sys_cfg = 2u;
            }

            current_cfg++;
        }

        /* update RF-Config ? */
        if ((ptxStatus_Success == status) && (0 != update_rf_cfg))
        {
            status = ptxNSC_RfConfig(iotRd->Nsc, &nsc_rf_cfg_params[0], rfconfig_tlv_count);
        }

        /* update System-Config ? */
        if ((ptxStatus_Success == status) && (2u == update_sys_cfg))
        {
            status = ptxNSC_System_SetConfig (iotRd->Nsc, &sys_cfg_params);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

uint16_t ptxIoTRd_ConfigHBR (ptxIoTRd_t *iotRd, ptxIoTRd_HBRConfig_t *configParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != iotRd) && (NULL != configParams))
    {
    	uint8_t misc_cfg_len = 255;
        uint8_t misc_cfg[misc_cfg_len];

        status = ptxNSC_GetMiscRFConfig(iotRd->Nsc, &misc_cfg[0], &misc_cfg_len);
        if (ptxStatus_Success == status)
        {
        	ptxNSC_RfConfigTlv_t rfConfParams;

            misc_cfg[15] = (uint8_t)(((configParams->PollA.Rx.BitRate848 & 0x01) << 7)
                         | ((configParams->PollA.Rx.BitRate424 & 0x01) << 6)
                         | ((configParams->PollA.Rx.BitRate212 & 0x01) << 5)
                         | (0x01 << 4)
                         | ((configParams->PollA.Tx.BitRate848 & 0x01) << 3)
                         | ((configParams->PollA.Tx.BitRate424 & 0x01) << 2)
                         | ((configParams->PollA.Tx.BitRate212 & 0x01) << 1)
                         | (0x01));

            misc_cfg[16] = (uint8_t)(((configParams->PollB.Rx.BitRate848 & 0x01) << 7)
                         | ((configParams->PollB.Rx.BitRate424 & 0x01) << 6)
                         | ((configParams->PollB.Rx.BitRate212 & 0x01) << 5)
                         | (0x01 << 4)
                         | ((configParams->PollB.Tx.BitRate848 & 0x01) << 3)
                         | ((configParams->PollB.Tx.BitRate424 & 0x01) << 2)
                         | ((configParams->PollB.Tx.BitRate212 & 0x01) << 1)
                         | (0x01));

            rfConfParams.ID = RfCfgParam_RegsMisc;
            rfConfParams.Value = misc_cfg;
            rfConfParams.Len = misc_cfg_len;

        	status = ptxNSC_RfConfig(iotRd->Nsc, &rfConfParams, 1);

        }

    }
    return status;
}


ptxStatus_t ptxIoTRd_Get_Revision_Info (ptxIoTRd_t *iotRd, ptxIoTRd_RevisionType_t revisionType, uint32_t *revisionInfo)
{
    ptxStatus_t status = ptxStatus_Success;
    ptxNSC_RevisionType_t nsc_revision_type;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (NULL != revisionInfo))
    {
        /* map revision type */
        switch (revisionType)
        {
            case RevInfo_C_Stack:
                nsc_revision_type = ptxNSC_Info_C_Stack;
                break;

            case RevInfo_Local_Changes:
                nsc_revision_type = ptxNSC_Info_Local_Changes;
                break;

            case RevInfo_DFY_Code:
                nsc_revision_type = ptxNSC_Info_DFY_Code;
                break;

            case RevInfo_DFY_Toolchain:
                nsc_revision_type = ptxNSC_Info_DFY_Toolchain;
                break;

            case RevInfo_ChipID:
                nsc_revision_type = ptxNSC_Info_ChipID;
                break;

            case RevInfo_ProductID:
                nsc_revision_type = ptxNSC_Info_ProductID;
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }

        /* retrieve selected revision info */
        if (ptxStatus_Success == status)
        {
            status = ptxNSC_GetRevisionInfo(iotRd->Nsc, nsc_revision_type, revisionInfo);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_Get_Card_Registry (ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t **cardRegistry)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (NULL != cardRegistry))
    {
        /* set pointer to internal card registry */
        *cardRegistry = iotRd->CardRegistry;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_Reader_Deactivation (ptxIoTRd_t *iotRd, uint8_t deactivationType)
{
    const uint32_t SLEEP_WAIT_TIME_STEPS = (uint32_t)10;

    const uint8_t STANDBY_MAX_REPETITOINS = (uint8_t)3;
    const uint32_t RESPONSE_TIMEOUT_MS_STEP_SIZE = (uint32_t)5;

    ptxStatus_t st = ptxStatus_Success;
    ptxStatus_t tmp_st = ptxStatus_Success;

    uint8_t is_sleep_request = 0;

    uint8_t standby_repetition_counter = 0;
    uint32_t deactive_rsp_timeout = 0;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader))
    {
        ptxNSC_RfDeactPars_t nsc_RfDeact_Pars;
        (void)memset(&nsc_RfDeact_Pars, 0, sizeof(ptxNSC_RfDeactPars_t));

        switch (deactivationType)
        {
            case PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE:
            case PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE_PROTOCOL:
                /* reset the internal card registry */
                (void)ptxIoTRd_ResetCardRegistry(iotRd);

                nsc_RfDeact_Pars.Rf_State = RfIdle;
                nsc_RfDeact_Pars.Rf_Deactivate_Type = (deactivationType == PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE) ? DeactType_Generic : DeactType_Protocol_Specific;
                iotRd->Nsc->DeactivationNTFPending = (nsc_RfDeact_Pars.Rf_Deactivate_Type == DeactType_Generic) ? (uint8_t)0 : (uint8_t)1;

                if (iotRd->StandbyActive)
                {
                    standby_repetition_counter = STANDBY_MAX_REPETITOINS;
                }
                break;

            case PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER:
            case PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER_PROTOCOL:
                /* reset the internal card registry */
                (void)ptxIoTRd_ResetCardRegistry(iotRd);

                nsc_RfDeact_Pars.Rf_State = RfDiscovery;
                nsc_RfDeact_Pars.Rf_Deactivate_Type = (deactivationType == PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER) ? DeactType_Generic : DeactType_Protocol_Specific;
                iotRd->Nsc->DeactivationNTFPending = (nsc_RfDeact_Pars.Rf_Deactivate_Type == DeactType_Generic) ? (uint8_t)0 : (uint8_t)1;

                if (iotRd->StandbyActive)
                {
                    standby_repetition_counter = STANDBY_MAX_REPETITOINS;
                }
                break;

            case PTX_IOTRD_RF_DEACTIVATION_TYPE_SLEEP:
            case PTX_IOTRD_RF_DEACTIVATION_TYPE_SLEEP_NON_BLOCKING:
                nsc_RfDeact_Pars.Rf_State = RfPollSleep;
                nsc_RfDeact_Pars.Rf_Deactivate_Type = DeactType_Protocol_Specific;
                iotRd->Nsc->DeactivationNTFPending = (uint8_t)1;
                is_sleep_request = (uint8_t)1;
                break;

            case PTX_IOTRD_RF_DEACTIVATION_TYPE_NO_RF_RESET:
                /* reset the internal card registry */
                (void)ptxIoTRd_ResetCardRegistry(iotRd);

                nsc_RfDeact_Pars.Rf_State = RfDiscoverNoFieldOff;
                nsc_RfDeact_Pars.Rf_Deactivate_Type = DeactType_Generic;
                iotRd->Nsc->DeactivationNTFPending = 0;
                break;

            default:
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }

        if (ptxStatus_Success == st)
        {
            do
            {
                st = ptxNSC_RfDeactivate(iotRd->Nsc, &nsc_RfDeact_Pars);

                if (ptxStatus_Success != st)
                {
                    tmp_st = ptxNSC_GetDeactivateTimeout(iotRd->Nsc, &deactive_rsp_timeout);

                    if (ptxStatus_Success == tmp_st)
                    {
                        deactive_rsp_timeout = (uint32_t)(deactive_rsp_timeout + RESPONSE_TIMEOUT_MS_STEP_SIZE);

                        tmp_st = ptxNSC_SetDeactivateTimeout(iotRd->Nsc, &deactive_rsp_timeout);
                    }
                }

                /* prevent overflow before decrementing counter */
                if (0 != standby_repetition_counter)
                {
                    standby_repetition_counter--;
                }

            } while ((ptxStatus_Success != st) && (0 != standby_repetition_counter) && (ptxStatus_Success == tmp_st));
        }

        if (ptxStatus_Success != tmp_st)
        {
            st = tmp_st;
        }

        if ((ptxStatus_Success == st) && (0 != iotRd->Nsc->DeactivationNTFPending))
        {
            uint32_t elapsed_time = 0;

            while ((0 != iotRd->Nsc->DeactivationNTFPending) && (ptxStatus_Success == st))
            {
                st = ptxPLAT_TriggerRx(iotRd->Plat);

                if (ptxStatus_Success == st)
                {
                    (void)ptxPLAT_Sleep(iotRd->Plat, SLEEP_WAIT_TIME_STEPS);

                    if (0 != iotRd->Nsc->DeactivationNTFPending)
                    {
                        elapsed_time += SLEEP_WAIT_TIME_STEPS;

                        if (elapsed_time > IOTRD_DEACTIVATION_NTF_PENDING_TIMEOUT_MS)
                        {
                            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                        }
                    }
                }
            }

            if ((ptxStatus_Success == st) && (0 != is_sleep_request))
            {
                ptxIoTRd_CardRegistry_t * card_registry = NULL;

                /* get reference to the internal card registry */
                (void)ptxIoTRd_Get_Card_Registry (iotRd, &card_registry);

                /* update Device-State */
                card_registry->ActiveCard->DeviceState = (uint8_t)0x01;
            }
        }

        if (ptxStatus_Success == st)
        {
            if ((PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE == deactivationType) || (PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE_PROTOCOL == deactivationType))
            {
                /* reset LPCD-counter again */
                iotRd->LPCDNtfCounter = 0;
            }

            if (iotRd->StandbyActive)
            {
                /* restore default timeout value - no need anymore to check the return value here */
                (void)ptxNSC_SetDeactivateTimeout(iotRd->Nsc, NULL);
            }
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Activate_Card (ptxIoTRd_t *iotRd, ptxIoTRd_CardParams_t *cardParams, ptxIoTRd_CardProtocol_t protocol)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t activate_handled_by_extension = 0;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (NULL != cardParams))
    {
        for (uint8_t i = 0; i < PTX_IOTRD_MAX_EXTENSIONS; i++)
        {
            if (NULL != iotRd->Extension[i].CBFnExtActivateCmd)
            {
                status = iotRd->Extension[i].CBFnExtActivateCmd(iotRd->Extension[i].ExtensionCtx, iotRd, cardParams, protocol);

                if (ptxStatus_Success == status)
                {
                    activate_handled_by_extension = 1u;
                }

                if (ptxStatus_NotImplemented != PTX_GET_STATUS(status))
                {
                    status = ptxStatus_Success;
                }

                if (0 != activate_handled_by_extension)
                {
                    break;
                }
            }
        }

        if (0 == activate_handled_by_extension)
        {
            ptxNSC_RfActiv_Param_t act_params;
            (void)memset(&act_params, 0, sizeof(ptxNSC_RfActiv_Param_t));

            /* assign RF-Technology and -Protocol => common for all settings */
            act_params.RfTech = (uint8_t)cardParams->TechType;
            act_params.RfProt = (uint8_t)protocol;

            /* Note: force Sleep-mode for RF-state to be able to select 1-out-of-n cards */
            switch (cardParams->TechType)
            {
                case Tech_TypeA:
                    act_params.RfTechActParams.RfAct_A_Params.DeviceRfState = cardParams->DeviceState;
                    act_params.RfTechActParams.RfAct_A_Params.NfcId1_len = cardParams->TechParams.CardAParams.NFCID1_LEN;
                    (void)memcpy(&act_params.RfTechActParams.RfAct_A_Params.NfcId1[0],
                                 &cardParams->TechParams.CardAParams.NFCID1[0],
                                 cardParams->TechParams.CardAParams.NFCID1_LEN);
                    break;

                case Tech_TypeB:
                    act_params.RfTechActParams.RfAct_B_Params.DeviceRfState = cardParams->DeviceState;
                    (void)memcpy(&act_params.RfTechActParams.RfAct_B_Params.SensBRes[0],
                                 &cardParams->TechParams.CardBParams.SENSB_RES[0],
                                 PTX_IOTRD_TECH_B_SENSB_MAX_SIZE);
                    break;

                case Tech_TypeF:
                    act_params.RfTechActParams.RfAct_F_Params.DeviceRfState = cardParams->DeviceState;
                    (void)memcpy(&act_params.RfTechActParams.RfAct_F_Params.NfcId2[0],
                                 &cardParams->TechParams.CardFParams.SENSF_RES[2],
                                 PTX_NSC_NFCID2_LEN);
                    break;

                case Tech_TypeV:
                    act_params.RfTechActParams.RfAct_V_Params.DeviceRfState = cardParams->DeviceState;
                    (void)memcpy(&act_params.RfTechActParams.RfAct_V_Params.Uid[0],
                                 &cardParams->TechParams.CardVParams.UID[0],
                                 PTX_NSC_TYPEV_UID_LEN);
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NotSupported);
                    break;
            }

            ptxIoTRd_CardRegistry_t *card_registry = iotRd->CardRegistry;

            uint8_t *activationData = &card_registry->ActiveCardProtInfo[0];
            size_t activationDataLen = PTX_IOTRD_HIGH_LEVEL_PROT_MAX_SIZE;

            act_params.UseShortActivation = (0 == cardParams->DeviceState) ? (uint8_t)0x01 : (uint8_t)0x00;

            status = ptxNSC_RfActivate(iotRd->Nsc, &act_params, activationData, &activationDataLen);

            /* the short activation may fail at this stage - try again using the long activation */
            if ((ptxStatus_Success != status) && (0 != act_params.UseShortActivation))
            {
                act_params.UseShortActivation = 0;

                status = ptxNSC_RfActivate(iotRd->Nsc, &act_params, activationData, &activationDataLen);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxIoTRd_AddCardToRegistryFromRSP(iotRd,
                                                       cardParams,
                                                       protocol,
                                                       activationData,
                                                       activationDataLen);

                if (ptxStatus_Success == status)
                {
                    iotRd->DiscoverState = RF_DISCOVER_STATUS_CARD_ACTIVE;

                    /* reset LPCD-counter again */
                    iotRd->LPCDNtfCounter = 0;
                }
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_Data_Exchange (ptxIoTRd_t *iotRd, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout)
{

    ptxStatus_t st = ptxStatus_Success;

    /* There can be only Tx operation without Rx. So, Rx buffer and length parameters can be NULL. */
    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (tx != NULL) && (txLength > 0))
    {
        /* Num of bytes pending to be transfered. */
        uint32_t txLength_pending = txLength;
        /* Num of bytes already sent. */
        uint32_t txLength_sent = 0;

        uint32_t nsc_max_transfer_unit = 0;
        (void) ptxNSC_Get_Mtu(iotRd->Nsc, &nsc_max_transfer_unit);

        /* Tx Rf Data. */
        do
        {
            if (txLength_pending > nsc_max_transfer_unit)
            {
                /* Chaining needed. */
                st = ptxNSC_RfDataMsgTx(iotRd->Nsc, &tx[txLength_sent], (size_t) nsc_max_transfer_unit, 1u);

                txLength_pending -= nsc_max_transfer_unit;
                txLength_sent += nsc_max_transfer_unit;
            } else
            {
                /* No Chaining needed. */
                st = ptxNSC_RfDataMsgTx(iotRd->Nsc, &tx[txLength_sent], (size_t) txLength_pending, 0);
                txLength_pending = 0;
            }

        } while((ptxStatus_Success == st) && (txLength_pending > 0));

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    /* Rx Rf Data. */
    if (ptxStatus_Success == st)
    {
        /* Rx parameters can be NULL. In that case, both of them have to be NULL. Otherwise, invalid parameter is returned. */
        if ((rx != NULL) && (rxLength != NULL))
        {
            uint8_t is_rxFrame_chained = 0;
            size_t num_bytes_read = 0;
            size_t num_bytes_pending_to_read = *rxLength;
            const size_t max_num_bytes_to_read = 253u;
            size_t max_num_bytes_to_read_temp;

            /* Clear Card Rx Event. */
            (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, &iotRd->RfMsg);

            do
            {
                if (num_bytes_pending_to_read > max_num_bytes_to_read)
                {
                    max_num_bytes_to_read_temp = max_num_bytes_to_read;
                    st = ptxIoTRd_RcvRfMsg(iotRd, &rx[num_bytes_read], &max_num_bytes_to_read_temp, &is_rxFrame_chained, msAppTimeout);
                    num_bytes_pending_to_read -= max_num_bytes_to_read_temp;
                    num_bytes_read += max_num_bytes_to_read_temp;
                } else
                {
                    st = ptxIoTRd_RcvRfMsg(iotRd, &rx[num_bytes_read], &num_bytes_pending_to_read, &is_rxFrame_chained, msAppTimeout);
                    num_bytes_read += num_bytes_pending_to_read;
                    num_bytes_pending_to_read = 0;
                }
            } while ((ptxStatus_Success == st) && (1u == is_rxFrame_chained) && (num_bytes_pending_to_read > 0));

            /* Read operation at APDU level has been correct. Let's forward up length*/
            if (ptxStatus_Success == st)
            {
                *rxLength = num_bytes_read;
            }
        } else
        {
            if ( ((rx == NULL) && (rxLength != NULL)) || ((rx != NULL) && (rxLength == NULL)))
            {
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
            }
        }
    }

    return st;
}

ptxStatus_t ptxIoTRd_Bits_Exchange_Mode (ptxIoTRd_t *iotRd, uint8_t enable)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && ((0 == enable) || (1u == enable)))
    {
        if (1u == enable)
        {
            st = ptxIoTRd_NSCRfSetCltMode (iotRd);
        } else
        {
            st = ptxIoTRd_NSCRfClearCltMode (iotRd);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Bits_Exchange (ptxIoTRd_t *iotRd, uint8_t *tx, uint8_t *txPar, size_t txLength,
                                                                    uint8_t *rx, uint8_t *rxPar, size_t *rxLength, size_t *numTotBits, uint32_t msTimeout)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (tx != NULL) && (txPar != NULL) && (txLength > 0) &&
                                                            (rx != NULL) && (rxPar != NULL) && (rxLength != NULL) && (numTotBits != NULL) )
    {
        st = ptxNSC_Rd_RfCltMsg (iotRd->Nsc, tx, txPar, txLength);

        if (ptxStatus_Success == st)
        {
            st = ptxIoTRd_RcvRfCltMsg (iotRd, rx, rxPar, rxLength, numTotBits, msTimeout);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_RF_PresenceCheck (ptxIoTRd_t *iotRd, ptxIoTRd_CheckPresType_t presCheckType)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && ((PresCheck_A == presCheckType) || (PresCheck_B == presCheckType) )))
    {
        if (RF_DISCOVER_STATUS_CARD_ACTIVE == iotRd->DiscoverState)
        {
            switch(iotRd->CardRegistry->ActiveCardProtType)
            {
                case Prot_ISODEP:
                    if (PresCheck_A == presCheckType)
                    {
                        /* Presence Check. ISO-DEP NACK */
                        st = ptxIoTRd_RfPresenceCheck_ISODEP_Nack (iotRd);
                    } else
                    {
                        /* Presence Check. ISO-DEP EMPTY FRAME */
                        st = ptxIoTRd_RfPresenceCheck_ISODEP_EmptyFrame (iotRd);
                    }
                    break;

                case Prot_NFCDEP:
                    st = ptxIoTRd_RfPresenceCheck_NFCDEP_AttCmd (iotRd);
                    break;

                default:
                    st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                    break;
            }
        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_T5T_IsolatedEoF ( ptxIoTRd_t *iotRd, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (rx != NULL) && (rxLength != NULL))
    {
        /* Send Isolated EoF T5T. */
        st = ptxNSC_T5T_IsolatedEoF(iotRd->Nsc);

        /* Rx Rf Data. */
        if (ptxStatus_Success == st)
        {
            uint8_t is_rxFrame_chained = 0;
            size_t num_bytes_read = 0;
            size_t num_bytes_pending_to_read = *rxLength;
            const size_t max_num_bytes_to_read = 253u;
            size_t max_num_bytes_to_read_temp;

            /* Clear Card Rx Event. */
            (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, &iotRd->RfMsg);

            do
            {
                if (num_bytes_pending_to_read > max_num_bytes_to_read)
                {
                    max_num_bytes_to_read_temp = max_num_bytes_to_read;
                    st = ptxIoTRd_RcvRfMsg(iotRd, &rx[num_bytes_read], &max_num_bytes_to_read_temp, &is_rxFrame_chained, msAppTimeout);
                    num_bytes_pending_to_read -= max_num_bytes_to_read_temp;
                    num_bytes_read += max_num_bytes_to_read_temp;
                } else
                {
                    st = ptxIoTRd_RcvRfMsg(iotRd, &rx[num_bytes_read], &num_bytes_pending_to_read, &is_rxFrame_chained, msAppTimeout);
                    num_bytes_read += num_bytes_pending_to_read;
                    num_bytes_pending_to_read = 0;
                }
            } while ((ptxStatus_Success == st) && (1u == is_rxFrame_chained) && (num_bytes_pending_to_read > 0));

            /* Read operation at APDU level has been correct. Let's forward up length*/
            if (ptxStatus_Success == st)
            {
                *rxLength = num_bytes_read;
            }
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_T3T_SENSFRequest (ptxIoTRd_t *iotRd, uint16_t systemCode, uint8_t requestCode, uint8_t tsn, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout)
{
    ptxStatus_t st = ptxStatus_Success;
    uint8_t trx_buffer[253];

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && (rx != NULL) && (rxLength != NULL))
    {
        /* This function is only available if activated card / device is using the T3T-protocol */
        if ( Prot_T3T == iotRd->CardRegistry->ActiveCardProtType)
        {
            /* enable Multi-Rx mode for T3T with a a timeout of 2,44 + 15 * 1,22 => ~21ms (15 == max. timeslots) */
            (void)ptxIoTRd_Set_T3T_MultiRxMode(iotRd, (uint8_t)1, (uint32_t)25);

            /*
             * Assemble T3T SENSF_REQ-command
             */
        	trx_buffer[0] = 0x00u;   // constant (command-code)
        	trx_buffer[1] = (uint8_t)((systemCode >> 8) & (uint8_t)0xFF);
        	trx_buffer[2] = (uint8_t)((systemCode >> 0) & (uint8_t)0xFF);
        	trx_buffer[3] = requestCode;
        	trx_buffer[4] = tsn;
        	 (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, &iotRd->RfMsg);

            st = ptxNSC_RfDataMsgTx(iotRd->Nsc, &trx_buffer[0], (size_t)5, 0);

            /* Rx Rf Data. */
            if (ptxStatus_Success == st)
            {
                uint8_t is_rxFrame_chained = 0;
                size_t num_bytes_read = 0;
                size_t num_bytes_pending_to_read = *rxLength;
                const size_t max_num_bytes_to_read = 253u;
                size_t max_num_bytes_to_read_temp;

                /* FeliCa-/T3T-response data is build up as follows
                 * Offset 0: Complete ATQC/SENSF_RES - 18 or 20 bytes, depending if RD requested) consisting of
                 *           - LEN-byte.....: 12h or 14h
                 *           - RESPONSE-code: 01h
                 *           - IDm..........: 8 bytes manufacturer ID
                 *           - PMm..........: 8 bytes manufacturer Parameters
                 *
                 * Offset 18 (or 20): Contactless Status byte appended by PTX-HW
                 *                    - 00h - No Error
                 *                    - 8xh - Contactless Error (e.g. collision in same timeslot)
                 *
                 * ... FeliCa-/T3T-response data of next card (if available)
                 *
                 */
                do
                {
                    if (num_bytes_pending_to_read > max_num_bytes_to_read)
                    {
                        max_num_bytes_to_read_temp = max_num_bytes_to_read;
                        st = ptxIoTRd_RcvRfMsg(iotRd, &trx_buffer[0], &max_num_bytes_to_read_temp, &is_rxFrame_chained, msAppTimeout);
                        if (0 != max_num_bytes_to_read_temp)
                        {
                            rx[num_bytes_read] = (uint8_t)max_num_bytes_to_read_temp;
                            num_bytes_read++;
                            (void)memcpy(&rx[num_bytes_read], &trx_buffer[0], max_num_bytes_to_read_temp);
                        }
                        num_bytes_pending_to_read -= max_num_bytes_to_read_temp;
                        num_bytes_read += max_num_bytes_to_read_temp;
                    } else
                    {
                        st = ptxIoTRd_RcvRfMsg(iotRd, &trx_buffer[0], &num_bytes_pending_to_read, &is_rxFrame_chained, msAppTimeout);
                        if (0 != num_bytes_pending_to_read)
                        {
                            rx[num_bytes_read] = (uint8_t)num_bytes_pending_to_read;
                            num_bytes_read++;
                            (void)memcpy(&rx[num_bytes_read], &trx_buffer[0], num_bytes_pending_to_read);
                        }
                        num_bytes_read += num_bytes_pending_to_read;
                        num_bytes_pending_to_read = 0;
                    }

                } while ((ptxStatus_Success == st) && (1u == is_rxFrame_chained) && (num_bytes_pending_to_read > 0));

                /* Read operation at APDU level has been correct. Let's forward up length*/
                if (ptxStatus_Success == st)
                {
                    *rxLength = num_bytes_read;
                }
            }

            /* disable Multi-Rx mode */
            (void)ptxIoTRd_Set_T3T_MultiRxMode(iotRd, 0, 0);

        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NotPermitted);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Set_Power_Mode (ptxIoTRd_t *iotRd, uint8_t newPowerMode)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader) && ((PowerMode_Active == newPowerMode) || (PowerMode_StandBy == newPowerMode)) )
    {
        if (PowerMode_StandBy == newPowerMode)
        {
            st = ptxNSC_StandbyCmd(iotRd->Nsc);
        } else
        {
            st = ptxNSC_WakeupCmd(iotRd->Nsc);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Get_System_Info (ptxIoTRd_t *iotRd, ptxIoTRd_SysInfoType_t infoType, uint8_t *infoBuffer, uint8_t *infoBufferLength)
{
    const uint8_t WORK_BUFFER_SIZE = 30;

    ptxStatus_t st = ptxStatus_Success;
    uint8_t work_buffer[WORK_BUFFER_SIZE];
    uint8_t work_buffer_len = WORK_BUFFER_SIZE;

    if ((PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader)) && (NULL != infoBuffer) && (NULL != infoBufferLength))
    {
        switch (infoType)
        {
            case SysInfo_VDPA_Calibration_Result:
                if (work_buffer_len >= (size_t)infoBuffer[0])
                {
                    uint32_t nbr_calibrations = (uint32_t)infoBuffer[0];

                    /* load system internally stored RF-Misc. Settings */

                    st = ptxNSC_GetMiscRFConfig(iotRd->Nsc, &work_buffer[0], &work_buffer_len);

                    if (ptxStatus_Success == st)
                    {
                        ptxIoTRd_ChipConfig_t chip_config;
                        (void)memset(&chip_config, 0, sizeof(ptxIoTRd_ChipConfig_t));
                        chip_config.ID = RF_Misc;
                        chip_config.Value = &work_buffer[0];
                        chip_config.Len = work_buffer_len;

                        uint8_t value = 0;;

                        /* load RF-Misc. Settings n-times and read out the calibrated value stored in register ANA_TX_CONTROL_REG*/
                        for (uint32_t i = 0; i < nbr_calibrations; i++)
                        {
                            st = ptxIoTRd_Update_ChipConfig (iotRd, (uint8_t)1, &chip_config);

                            if (ptxStatus_Success == st)
                            {
                                st = ptxNSC_Read(iotRd->Nsc, ANA_TX_CONTROL_REG, &value);

                                if (ptxStatus_Success == st)
                                {
                                    infoBuffer[i] = value;
                                }
                            }

                            if (ptxStatus_Success != st)
                            {
                                break;
                            }
                        }

                        if (ptxStatus_Success == st)
                        {
                            *infoBufferLength = (uint8_t)nbr_calibrations;
                        }
                    }
                } else
                {
                    st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                }
                break;

            default:
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_SWReset (ptxIoTRd_t *iotRd)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader))
    {
        st = ptxNSC_Reset(iotRd->Nsc);

        /* Reset system-state */
        iotRd->Nsc->SysState = SystemState_OK;

        /* Reset registry */
        (void)ptxIoTRd_ResetCardRegistry(iotRd);

        /* reset component to be able to call Init() again */
        (void)ptxIoTRd_Deinit(iotRd);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Get_Status_Info (ptxIoTRd_t *iotRd, ptxIoTRd_StatusType_t statusType, uint8_t *statusInfo)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader)) && (NULL != statusInfo))
    {
        /* Let's check first if there has been anything received. */
        st = ptxPLAT_TriggerRx(iotRd->Plat);

        switch (statusType)
        {
            case StatusType_System:
                switch (iotRd->Nsc->SysState)
                {
                    case SystemState_ERR_Overcurrent:
                        *statusInfo = PTX_SYSTEM_STATUS_ERR_OVERCURRENT;
                        break;

                    case SystemState_ERR_Temperature:
                        *statusInfo = PTX_SYSTEM_STATUS_ERR_TEMPERATURE;
                        break;

                    default:
                        *statusInfo = PTX_SYSTEM_STATUS_OK;
                        break;
                }
                break;

            case StatusType_Discover:
                *statusInfo = iotRd->DiscoverState;
                break;

            case StatusType_DeactivateSleep:
                *statusInfo = (uint8_t)((0 != iotRd->Nsc->DeactivationNTFPending) ? PTX_IOTRD_RF_DEACTIVATION_SLEEP_ONGOING : PTX_IOTRD_RF_DEACTIVATION_SLEEP_DONE);
                break;

            case StatusType_LastRFError:
                *statusInfo = iotRd->LastRFError;
                iotRd->LastRFError = PTX_RF_ERROR_NTF_CODE_NO_ERROR;
                break;

            case StatusType_Lpcd:
                *statusInfo = iotRd->LpcdState;
                break;

            case StatusType_LPCDNtfCounter:
                *statusInfo = iotRd->LPCDNtfCounter;
                break;

            default:
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Deinit(ptxIoTRd_t *iotRd)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(iotRd, ptxStatus_Comp_IoTReader))
    {
        (void) ptxNSC_Deinit(iotRd->Nsc);
        iotRd->Nsc = NULL;

        (void) ptxPLAT_Deinit(iotRd->Plat);
        iotRd->Plat = NULL;

        (void) ptxHce_Deinit(&iotRd->Hce);

        iotRd->CompId = ptxStatus_Comp_None;
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRd_Set_RSSI_Mode (ptxIoTRd_t *iotRd, ptxIoTRd_RSSI_Mode_t rssiMode, uint8_t *rssiRefreshPeriodInt)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t refresh_period_int = 0;

    if (NULL != iotRd)
    {
        switch (rssiMode)
        {
            case RSSI_Mode_Enabled:
                // default value of 1 ms
                refresh_period_int = 1U;

                if (NULL != rssiRefreshPeriodInt)
                {
                    if ((0 < *rssiRefreshPeriodInt) && (16U >= *rssiRefreshPeriodInt))
                    {
                        refresh_period_int = *rssiRefreshPeriodInt;
                    }
                }

                uint8_t cmd_params[1];
                cmd_params[0] = refresh_period_int;

                status = ptxIoTRd_Manage_DDPC(iotRd, 1U);

                if (ptxStatus_Success == status)
                {
                    status = ptxNSC_RfTestRun(iotRd->Nsc, RfTest_Carrier, &cmd_params[0], (size_t)1);
                }
                break;

            case RSSI_Mode_Disabled:
                status = ptxNSC_RfTestStop(iotRd->Nsc);

                if (ptxStatus_Success == status)
                {
                    status = ptxIoTRd_Manage_DDPC(iotRd, 0U);
                }
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_Get_RSSI_Value (ptxIoTRd_t *iotRd, uint16_t *rssiValue)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t RSSI_VALUE_MEM_ADDR_L = (uint16_t)0x06FCu;
    uint16_t RSSI_VALUE_MEM_ADDR_H = (uint16_t)0x06FDu;
    uint8_t rssi_val_h = 0;
    uint8_t rssi_val_l = 0;

    if ((NULL != iotRd) && (NULL != rssiValue))
    {
        status = ptxNSC_Read(iotRd->Nsc, RSSI_VALUE_MEM_ADDR_H, &rssi_val_h);

        if (ptxStatus_Success == status)
        {
            status = ptxNSC_Read(iotRd->Nsc, RSSI_VALUE_MEM_ADDR_L, &rssi_val_l);
        }

        *rssiValue = (ptxStatus_Success == status) ? ((uint16_t)((rssi_val_h << 8) | rssi_val_l)) : 0;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_Register_Extension(ptxIoTRd_t *iotRd, uint8_t extensionID, ptxIoTRd_Extension_t *extension)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != iotRd) && (NULL != extension) && (IOTRD_EXTENSION_ID_AVAILABLE != extensionID))
    {
        if (iotRd->NrExtensions < PTX_IOTRD_MAX_EXTENSIONS)
        {
            for (uint8_t i = 0; i < (uint8_t)PTX_IOTRD_MAX_EXTENSIONS; i++)
            {
                if (extensionID == iotRd->Extension[i].ExtensionID)
                {
                    status = PTX_STATUS(ptxStatus_Comp_POS, ptxStatus_NotPermitted);

                    break;
                } else
                {
                    if (IOTRD_EXTENSION_ID_AVAILABLE == iotRd->Extension[i].ExtensionID)
                    {
                        iotRd->Extension[i].CBFnExtProcessNtf = extension->CBFnExtProcessNtf;
                        iotRd->Extension[i].ExtensionCtx      = extension->ExtensionCtx;
                        iotRd->Extension[i].ExtensionID       = extensionID;
                        iotRd->NrExtensions++;

                        status = ptxStatus_Success;

                        break;
                    }
                }
            }
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_POS, ptxStatus_InsufficientResources);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_POS, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxIoTRd_DeRegister_Extension(ptxIoTRd_t *iotRd, uint8_t extensionID)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != iotRd)
    {
        for (uint8_t i = 0; i < iotRd->NrExtensions; i++)
        {
            if (extensionID == iotRd->Extension[i].ExtensionID)
            {
                (void)memset(&iotRd->Extension[i], 0, sizeof(ptxIoTRd_Extension_t));
                iotRd->Extension[i].ExtensionID = IOTRD_EXTENSION_ID_AVAILABLE;
                iotRd->NrExtensions--;
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_POS, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK
 * ####################################################################################################################
 */

static ptxStatus_t ptxIoTRd_ClearRfMsgRcvd(ptxIoTRd_t *iotRd, ptxIoTRd_RfMsg_t *RfMsg)
{
    ptxStatus_t st = ptxStatus_Success;

    if (RfMsg != NULL)
    {
        RfMsg->State = RfMsg_NotReceived;
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }
    (void)iotRd;
    return st;
}

static ptxIoTRd_RfEventRcvd_Id_t ptxIoTRd_IsRfRcvd(ptxIoTRd_t *iotRd, ptxIoTRd_RfMsg_t *RfMsg)
{
    ptxStatus_t st = ptxStatus_Success;
    ptxIoTRd_RfEventRcvd_Id_t RfMsg_State = RfEvent_NotRcvd;

    /* Let's check first if there has been any thing received. */
    st = ptxPLAT_TriggerRx(iotRd->Plat);

    if (ptxStatus_Success == st)
    {
        if (NULL != RfMsg)
        {
            switch (RfMsg->State)
            {
                case RfMsg_RfMsg_Rcv:
                    RfMsg_State = RfEvent_RfMsgRcvd;

                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_RfMsg_Chained_Rcv:
                    RfMsg_State = RfEvent_RfMsgChainedRcvd;

                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_RfError:
                    RfMsg_State = RfEvent_RfErrorRcvd;

                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_RfErrorTimeOut:
                    RfMsg_State = RfEvent_RfErrorTimeOutRcvd;

                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_RfClt:
                    RfMsg_State = RfEvent_RfCltRcvd;

                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_CtrlAck:
                    RfMsg_State = RfEvent_RfCtrlAck;
                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                case RfMsg_CtrlAttCmd:
                    RfMsg_State = RfEvent_RfCtrlAttCmd;
                    /* Clear Card Rx Event. */
                    (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, RfMsg);
                    break;

                default:
                    break;
            }
        }
    }

    return RfMsg_State;
}

static ptxStatus_t ptxIoTRd_RcvRfMsg (ptxIoTRd_t *iotRd, uint8_t *msgDataRx, size_t *msgLenRx, uint8_t *isChained, uint32_t msTimeOut)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != msgDataRx) && (NULL != msgLenRx) && (NULL != isChained))
    {
        size_t msgRx_Capacity = *msgLenRx;

        // First check if there is a RfMessage Received
        ptxIoTRd_RfEventRcvd_Id_t is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

        if (RfEvent_RfMsgRcvd == is_rf_msg)
        {
            // Rf message received
            *isChained = 0;

            // Copy data to input buffer
            if (msgRx_Capacity >= iotRd->RfMsg.BuffLen )
            {
                (void)memcpy(msgDataRx, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                *msgLenRx = iotRd->RfMsg.BuffLen;
            } else
            {
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
            }
        } else if (RfEvent_RfMsgChainedRcvd == is_rf_msg)
        {
            // Rf message chained received
            *isChained = 1u;

            // Copy data to input buffer
            if (msgRx_Capacity >= iotRd->RfMsg.BuffLen )
            {
                (void)memcpy(msgDataRx, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                *msgLenRx = iotRd->RfMsg.BuffLen;
            } else
            {
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
            }
        } else if (RfEvent_RfErrorRcvd == is_rf_msg)
        {
            // Rf error received
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
        } else if (RfEvent_RfErrorTimeOutRcvd == is_rf_msg)
        {
            // Rf error timeout received
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
        } else if (RfEvent_RfCltRcvd == is_rf_msg)
        {
            // Nsc Rf Clt Msg received, when not expected
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
        } else
        {
            // Rf message not received, let's wait for it
            struct ptxPlatTimer *timer = NULL;

            // Let's get a Timer
            st = ptxPLAT_GetInitializedTimer(iotRd->Plat, &timer);

            if (ptxStatus_Success == st)
            {
                // Let's start a Timer
                st = ptxPLAT_TimerStart(iotRd->Plat, timer, msTimeOut, 0u, NULL, NULL);

                if (ptxStatus_Success == st)
                {
                    uint8_t is_timer_elapsed = 0;

                    do
                    {
                        // Wait for any interrupt on the CPU
                        (void) ptxPLAT_WaitForInterrupt(iotRd->Plat);

                        // Check if a RfMessage has been received
                        is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);


                        // Check if the timer has elapsed
                        (void) ptxPLAT_TimerIsElapsed(iotRd->Plat, timer, &is_timer_elapsed);

                    } while((0 == is_timer_elapsed) && (RfEvent_NotRcvd == is_rf_msg));

                    if (1u == is_timer_elapsed)
                    {
                        // Internal error
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                    } else if (RfEvent_RfMsgRcvd == is_rf_msg)
                    {
                        // Rf message received
                        *isChained = 0;

                        // Copy data to input buffer
                        if (msgRx_Capacity >= iotRd->RfMsg.BuffLen )
                        {
                            (void)memcpy(msgDataRx, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                            *msgLenRx = iotRd->RfMsg.BuffLen;
                        } else
                        {
                            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
                        }
                    } else if (RfEvent_RfMsgChainedRcvd == is_rf_msg)
                    {
                        // Rf message chained received
                        *isChained = 1u;

                        // Copy data to input buffer
                        if (msgRx_Capacity >= iotRd->RfMsg.BuffLen )
                        {
                            (void)memcpy(msgDataRx, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                            *msgLenRx = iotRd->RfMsg.BuffLen;
                        } else
                        {
                            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
                        }
                    } else if (RfEvent_RfErrorRcvd == is_rf_msg)
                    {
                        // Rf error received
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                    } else if (RfEvent_RfErrorTimeOutRcvd == is_rf_msg)
                    {
                        // Rf error timeout received
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                    } else
                    {
                        // Internal error
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                    }
                }
            }

            (void) ptxPLAT_TimerDeinit(iotRd->Plat, timer);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

static ptxStatus_t ptxIoTRd_RcvRfCltMsg (ptxIoTRd_t *iotRd, uint8_t *pld, uint8_t *pldPar, size_t *pldRx, size_t *numTotalBits, uint32_t msTimeOut)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != pld) && (NULL != pldPar) && (NULL != pldRx) && (NULL != numTotalBits))
    {
        size_t msgRx_Capacity = *pldRx;

        // First check if there is a RfMessage Received
        ptxIoTRd_RfEventRcvd_Id_t is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

        if (RfEvent_RfCltRcvd == is_rf_msg)
        {
            // Rf Clt message received

            // Copy data to input buffer
            if ((msgRx_Capacity >= iotRd->RfMsg.BuffLen ) && (msgRx_Capacity >= iotRd->RfMsg.BuffSecondLen))
            {
                (void)memcpy(pld, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                (void)memcpy(pldPar, iotRd->RfMsg.BuffSecond, iotRd->RfMsg.BuffSecondLen);
                *pldRx = iotRd->RfMsg.BuffLen;
                *numTotalBits = iotRd->RfMsg.NumTotalBits;
            } else
            {
                st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
            }
        } else if ((RfEvent_RfMsgRcvd == is_rf_msg) || (RfEvent_RfMsgChainedRcvd == is_rf_msg))
        {
            // Rf message received, not expected at this time
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
        } else if ((RfEvent_RfErrorRcvd == is_rf_msg) || (RfEvent_RfErrorTimeOutRcvd == is_rf_msg))
        {
            // Rf error received
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
        } else
        {
            // Clt Rf message not received, let's wait for it
            struct ptxPlatTimer *timer = NULL;

            // Let's get a Timer
            st = ptxPLAT_GetInitializedTimer(iotRd->Plat, &timer);

            if (ptxStatus_Success == st)
            {
                // Let's start a Timer
                st = ptxPLAT_TimerStart(iotRd->Plat, timer, msTimeOut, 0u, NULL, NULL);

                if (ptxStatus_Success == st)
                {
                    uint8_t is_timer_elapsed = 0;

                    do
                    {
                        // Wait for any interrupt on the CPU
                        (void) ptxPLAT_WaitForInterrupt(iotRd->Plat);

                        // Check if a RfMessage has been received
                        is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

                        // Check if the timer has elapsed
                        (void) ptxPLAT_TimerIsElapsed(iotRd->Plat, timer, &is_timer_elapsed);

                    } while((0 == is_timer_elapsed) && (RfEvent_NotRcvd == is_rf_msg));

                    if (1u == is_timer_elapsed)
                    {
                        // TimeOut error
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                    } else if ( (RfEvent_RfMsgRcvd == is_rf_msg) || (RfEvent_RfMsgChainedRcvd == is_rf_msg) )
                    {
                        // Rf Msg received, not expected here.
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                    } else if (RfEvent_RfCltRcvd == is_rf_msg)
                    {
                        // Rf Clt message received

                        // Copy data to input buffer
                        if ((msgRx_Capacity >= iotRd->RfMsg.BuffLen ) && (msgRx_Capacity >= iotRd->RfMsg.BuffSecondLen))
                        {
                            (void)memcpy(pld, iotRd->RfMsg.Buff, iotRd->RfMsg.BuffLen);
                            (void)memcpy(pldPar, iotRd->RfMsg.BuffSecond, iotRd->RfMsg.BuffSecondLen);
                            *pldRx = iotRd->RfMsg.BuffLen;
                            *numTotalBits = iotRd->RfMsg.NumTotalBits;
                        } else
                        {
                            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InsufficientResources);
                        }

                    } else if ((RfEvent_RfErrorRcvd == is_rf_msg) || (RfEvent_RfErrorTimeOutRcvd == is_rf_msg))
                    {
                        // Rf error received
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                    } else
                    {
                        // Internal error
                        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                    }
                }
            }

            (void) ptxPLAT_TimerDeinit(iotRd->Plat, timer);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}


static ptxStatus_t ptxIoTRd_RfPresenceCheck_ISODEP_Nack (ptxIoTRd_t *iotRd)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeoutMs = 2000;

    status = ptxNSC_Rd_RfPressCheck_Nack (iotRd->Nsc);

    if (ptxStatus_Success == status)
    {
        status = ptxIoTRd_RcvRfPresenceCheck_ISODEP_Ack (iotRd, timeoutMs);
    }
    return status;
}

static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_ISODEP_Ack (ptxIoTRd_t *iotRd, uint32_t timeoutMs)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t received_rf_event = 0;

    // First check if there is a RfMessage Received
    ptxIoTRd_RfEventRcvd_Id_t is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

    switch (is_rf_msg)
    {
        case RfEvent_RfMsgRcvd:
        case RfEvent_RfMsgChainedRcvd:
        case RfEvent_RfCtrlAttCmd:
        case RfEvent_RfCltRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
            break;

        case RfEvent_RfErrorRcvd:
        case RfEvent_RfErrorTimeOutRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
            break;

        case RfEvent_RfCtrlAck:
            received_rf_event = 1;
            /* Successful ACK received. */
            break;

        default:
            /* Not Rf event received. */
            break;
    }

    if ( 0 == received_rf_event)
    {
        // Rf message not received, let's wait for it
        struct ptxPlatTimer *timer = NULL;

        // Let's get a Timer
        status = ptxPLAT_GetInitializedTimer(iotRd->Plat, &timer);

        if (ptxStatus_Success == status)
        {
            // Let's start a Timer
            status = ptxPLAT_TimerStart(iotRd->Plat, timer, timeoutMs, 0u, NULL, NULL);

            if (ptxStatus_Success == status)
            {
                uint8_t is_timer_elapsed = 0;

                do
                {
                    // Wait for any interrupt on the CPU
                    (void) ptxPLAT_WaitForInterrupt(iotRd->Plat);

                    // Check if a RfMessage has been received
                    is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

                    // Check if the timer has elapsed
                    (void) ptxPLAT_TimerIsElapsed(iotRd->Plat, timer, &is_timer_elapsed);

                } while((0 == is_timer_elapsed) && (RfEvent_NotRcvd == is_rf_msg));

                if (1u == is_timer_elapsed)
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                } else if (RfEvent_RfCtrlAck == is_rf_msg)
                {
                    /* Rf Control ACK received => Successful operation! */
                } else if ((RfEvent_RfErrorRcvd == is_rf_msg) || (RfEvent_RfErrorTimeOutRcvd == is_rf_msg))
                {
                    // Rf error received
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                } else
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                }
            }
        }

        (void) ptxPLAT_TimerDeinit(iotRd->Plat, timer);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_RfPresenceCheck_ISODEP_EmptyFrame (ptxIoTRd_t *iotRd)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_ms = 2000;

    status = ptxNSC_Rd_RfPressCheck_EmptyFrame(iotRd->Nsc);

    if (ptxStatus_Success == status)
    {
        status = ptxIoTRd_RcvRfPresenceCheck_ISODEP_EmptyFrame(iotRd, timeout_ms);
    }
    return status;
}

static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_ISODEP_EmptyFrame (ptxIoTRd_t *iotRd, uint32_t timeoutMs)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t received_rf_event = 0;

    // First check if there is a RfMessage Received
    ptxIoTRd_RfEventRcvd_Id_t is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

    switch (is_rf_msg)
    {
        case RfEvent_RfCtrlAck:
        case RfEvent_RfCtrlAttCmd:
        case RfEvent_RfMsgChainedRcvd:
        case RfEvent_RfCltRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
            break;

        case RfEvent_RfErrorRcvd:
        case RfEvent_RfErrorTimeOutRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
            break;

        case RfEvent_RfMsgRcvd:
            received_rf_event = 1;
            /* Successful RF DATA MSG received. */
            break;

        default:
            /* Not Rf event received. */
            break;
    }

    if ( 0 == received_rf_event)
    {
        // Rf message not received, let's wait for it
        struct ptxPlatTimer *timer = NULL;

        // Let's get a Timer
        status = ptxPLAT_GetInitializedTimer(iotRd->Plat, &timer);

        if (ptxStatus_Success == status)
        {
            // Let's start a Timer
            status = ptxPLAT_TimerStart(iotRd->Plat, timer, timeoutMs, 0u, NULL, NULL);

            if (ptxStatus_Success == status)
            {
                uint8_t is_timer_elapsed = 0;

                do
                {
                    // Wait for any interrupt on the CPU
                    (void) ptxPLAT_WaitForInterrupt(iotRd->Plat);

                    // Check if a RfMessage has been received
                    is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

                    // Check if the timer has elapsed
                    (void) ptxPLAT_TimerIsElapsed(iotRd->Plat, timer, &is_timer_elapsed);

                } while((0 == is_timer_elapsed) && (RfEvent_NotRcvd == is_rf_msg));

                if (1u == is_timer_elapsed)
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                } else if (RfEvent_RfMsgRcvd == is_rf_msg)
                {
                    /* Successful RF DATA MSG received. */
                } else if ((RfEvent_RfErrorRcvd == is_rf_msg) || (RfEvent_RfErrorTimeOutRcvd == is_rf_msg))
                {
                    // Rf error received
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                } else
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                }
            }
        }

        (void) ptxPLAT_TimerDeinit(iotRd->Plat, timer);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_RfPresenceCheck_NFCDEP_AttCmd (ptxIoTRd_t *iotRd)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_ms = 2000;

    status = ptxNSC_Rd_RfPressCheck_AttentionCmd(iotRd->Nsc);

    if (ptxStatus_Success == status)
    {
        status = ptxIoTRd_RcvRfPresenceCheck_NFCDEP_AttCmd(iotRd, timeout_ms);
    }
    return status;
}

static ptxStatus_t ptxIoTRd_RcvRfPresenceCheck_NFCDEP_AttCmd (ptxIoTRd_t *iotRd, uint32_t timeoutMs)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t received_rf_event = 0;

    // First check if there is a RfMessage Received
    ptxIoTRd_RfEventRcvd_Id_t is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

    switch (is_rf_msg)
    {
        case RfEvent_RfMsgRcvd:
        case RfEvent_RfMsgChainedRcvd:
        case RfEvent_RfCtrlAck:
        case RfEvent_RfCltRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
            break;

        case RfEvent_RfErrorRcvd:
        case RfEvent_RfErrorTimeOutRcvd:
            received_rf_event = 1;
            status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
            break;

        case RfEvent_RfCtrlAttCmd:
            received_rf_event = 1;
            /* Successful ATT Command received. */
            break;

        default:
            /* Not Rf event received. */
            break;
    }

    if ( 0 == received_rf_event)
    {
        // Rf message not received, let's wait for it
        struct ptxPlatTimer *timer = NULL;

        // Let's get a Timer
        status = ptxPLAT_GetInitializedTimer(iotRd->Plat, &timer);

        if (ptxStatus_Success == status)
        {
            // Let's start a Timer
            status = ptxPLAT_TimerStart(iotRd->Plat, timer, timeoutMs, 0u, NULL, NULL);

            if (ptxStatus_Success == status)
            {
                uint8_t is_timer_elapsed = 0;

                do
                {
                    // Wait for any interrupt on the CPU
                    (void) ptxPLAT_WaitForInterrupt(iotRd->Plat);

                    // Check if a RfMessage has been received
                    is_rf_msg = ptxIoTRd_IsRfRcvd(iotRd, &iotRd->RfMsg);

                    // Check if the timer has elapsed
                    (void) ptxPLAT_TimerIsElapsed(iotRd->Plat, timer, &is_timer_elapsed);

                } while((0 == is_timer_elapsed) && (RfEvent_NotRcvd == is_rf_msg));

                if (1u == is_timer_elapsed)
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_TimeOut);
                } else if (RfEvent_RfCtrlAttCmd == is_rf_msg)
                {
                    /* Successful ATT Command received. */
                } else if  ((RfEvent_RfErrorRcvd == is_rf_msg) || (RfEvent_RfErrorTimeOutRcvd == is_rf_msg))
                {
                    // Rf error received
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscRfError);
                } else
                {
                    // Internal error
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InternalError);
                }
            }
        }

        (void) ptxPLAT_TimerDeinit(iotRd->Plat, timer);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_AddCardToRegistryFromNTF(ptxIoTRd_t *iotRd, ptxNSC_Event_t *event)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != iotRd) && (NULL != event))
    {
        ptxIoTRd_CardParams_t *current_card = &iotRd->CardRegistry->Cards[iotRd->CardRegistry->NrCards];
        (void)memset(current_card, 0, sizeof(ptxIoTRd_CardParams_t));

        uint8_t param_index = 0;

        switch (event->EventId)
        {
            case NSC_EventRfAct_PassPoll_A:
            case NSC_EventRfDisc_PassPoll_A:
                /* RF-Technology Type */
                current_card->TechType = Tech_TypeA;

                /* SENS_RES */
                current_card->TechParams.CardAParams.SENS_RES[0] = ((uint8_t *)event->Buff)[param_index];
                param_index++;
                current_card->TechParams.CardAParams.SENS_RES[1] = ((uint8_t *)event->Buff)[param_index];
                param_index++;

                /* NFCID1_LEN */
                current_card->TechParams.CardAParams.NFCID1_LEN = ((uint8_t *)event->Buff)[param_index];
                param_index++;

                /* NFCID1 */
                (void)memcpy (&current_card->TechParams.CardAParams.NFCID1[0],
                              &((uint8_t *)event->Buff)[param_index],
                              current_card->TechParams.CardAParams.NFCID1_LEN);
                param_index = (uint8_t)(param_index + current_card->TechParams.CardAParams.NFCID1_LEN);

                /* SEL_RES */
                current_card->TechParams.CardAParams.SEL_RES_LEN = 0x01u;
                current_card->TechParams.CardAParams.SEL_RES = ((uint8_t *)event->Buff)[param_index];
                param_index++;

                if (event->EventId == NSC_EventRfAct_PassPoll_A)
                {
                    /* store Protocol-Type - unique per card registry*/
                    iotRd->CardRegistry->ActiveCardProtType = (ptxIoTRd_CardProtocol_t)((uint8_t*)event->Buff)[param_index];
                    param_index++;

                    switch (iotRd->CardRegistry->ActiveCardProtType)
                    {
                        case Prot_ISODEP:
                            /* ATS-Response includes the Length information at offet 0; store it also in the buffer */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = ((uint8_t *)event->Buff)[param_index];
                            (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                         &((uint8_t *)event->Buff)[param_index],
                                         iotRd->CardRegistry->ActiveCardProtInfoLen);
                            param_index = (uint8_t)(param_index + iotRd->CardRegistry->ActiveCardProtInfoLen);

                            iotRd->CardRegistry->ActiveCardProtSpeed = ((uint8_t*)event->Buff)[param_index];
                            break;

                        case Prot_NFCDEP:
                            /* ATR-Response includes the Length information at offet 0; store it also in the buffer */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = ((uint8_t *)event->Buff)[param_index];
                            (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                         &((uint8_t *)event->Buff)[param_index],
                                         iotRd->CardRegistry->ActiveCardProtInfoLen);
                            break;

                        default:
                            /* No High-level protocol info available */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = 0;
                            iotRd->CardRegistry->ActiveCardProtSpeed = 0;
                            break;
                    }

                    /* keep reference to active card and set number of cards */
                    iotRd->CardRegistry->ActiveCard = &iotRd->CardRegistry->Cards[iotRd->CardRegistry->NrCards];
                }
                else
                {
                    /* NSC_EventRfDisc_PassPoll_A */

                    /* store device state */
                    current_card->DeviceState = ((uint8_t *)event->Buff)[param_index];
                }
                iotRd->CardRegistry->NrCards++;
                break;

            case NSC_EventRfAct_PassPoll_B:
            case NSC_EventRfDisc_PassPoll_B:
                /* RF-Technology Type */
                current_card->TechType = Tech_TypeB;

                /* SENSB_RES */
                (void)memcpy (&current_card->TechParams.CardBParams.SENSB_RES[0],
                              &((uint8_t *)event->Buff)[param_index],
                              PTX_IOTRD_TECH_B_SENSB_MAX_SIZE);
                param_index = (uint8_t)(param_index + PTX_IOTRD_TECH_B_SENSB_MAX_SIZE);

                if (event->EventId == NSC_EventRfAct_PassPoll_B)
                {
                    /* store Protocol-Type - unique per card registry*/
                    iotRd->CardRegistry->ActiveCardProtType = (ptxIoTRd_CardProtocol_t)((uint8_t*)event->Buff)[param_index];
                    param_index++;

                    switch (iotRd->CardRegistry->ActiveCardProtType)
                    {
                        case Prot_ISODEP:
                            /* Attrib-Response starts at offset 1 */
                            param_index++;

                            iotRd->CardRegistry->ActiveCardProtInfoLen = ((uint8_t *)event->Buff)[param_index];
                            param_index++;

                            (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                         &((uint8_t *)event->Buff)[param_index],
                                         iotRd->CardRegistry->ActiveCardProtInfoLen);

                            iotRd->CardRegistry->ActiveCardProtSpeed = iotRd->CardRegistry->ActiveCardProtInfo[0];
                            break;

                        default:
                            /* No High-level protocol info available */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = 0;
                            iotRd->CardRegistry->ActiveCardProtSpeed = 0;
                            break;
                    }

                    /* keep reference to active card and set number of cards */
                    iotRd->CardRegistry->ActiveCard = &iotRd->CardRegistry->Cards[iotRd->CardRegistry->NrCards];
                }
                else
                {
                    /* NSC_EventRfDisc_PassPoll_B */

                    /* store device state */
                    current_card->DeviceState = ((uint8_t *)event->Buff)[param_index];
                }
                iotRd->CardRegistry->NrCards++;
                break;

            case NSC_EventRfAct_PassPoll_F:
            case NSC_EventRfDisc_PassPoll_F:
                /* RF-Technology Type */
                current_card->TechType = Tech_TypeF;

                /* SENSF_RES_LEN */
                current_card->TechParams.CardFParams.SENSF_RES_LEN = ((uint8_t *)event->Buff)[param_index];

                (void)memcpy (&current_card->TechParams.CardFParams.SENSF_RES[0],
                              &((uint8_t *)event->Buff)[param_index],
                              current_card->TechParams.CardFParams.SENSF_RES_LEN);
                param_index = (uint8_t)(param_index + current_card->TechParams.CardFParams.SENSF_RES_LEN);

                if (event->EventId == NSC_EventRfAct_PassPoll_F)
                {
                    /* store Protocol-Type - unique per card registry*/
                    iotRd->CardRegistry->ActiveCardProtType = (ptxIoTRd_CardProtocol_t)((uint8_t*)event->Buff)[param_index];
                    param_index++;

                    switch (iotRd->CardRegistry->ActiveCardProtType)
                    {
                        case Prot_NFCDEP:
                            /* ATR-Response includes the Length information at offet 0; store it also in the buffer */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = ((uint8_t *)event->Buff)[param_index];
                            (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                         &((uint8_t *)event->Buff)[param_index],
                                         iotRd->CardRegistry->ActiveCardProtInfoLen);
                            break;

                        default:
                            /* No High-level protocol info available */
                            iotRd->CardRegistry->ActiveCardProtInfoLen = 0;
                            iotRd->CardRegistry->ActiveCardProtSpeed = 0;
                            break;
                    }

                    /* keep reference to active card and set number of cards */
                    iotRd->CardRegistry->ActiveCard = &iotRd->CardRegistry->Cards[iotRd->CardRegistry->NrCards];
                }
                else
                {
                    /* NSC_EventRfDisc_PassPoll_F */

                    /* store device state */
                    current_card->DeviceState = ((uint8_t *)event->Buff)[param_index];
                }
                iotRd->CardRegistry->NrCards++;
                break;

            case NSC_EventRfAct_PassPoll_V:
            case NSC_EventRfDisc_PassPoll_V:
                /* RF-Technology Type */
                current_card->TechType = Tech_TypeV;

                /* RES_FLAG */
                current_card->TechParams.CardVParams.RES_FLAG = ((uint8_t *)event->Buff)[param_index];
                param_index++;

                /* DSFID */
                current_card->TechParams.CardVParams.DSFID = ((uint8_t *)event->Buff)[param_index];
                param_index++;

                /* UID - to be mirrored as reported LSB first by NSC */
                (void)memcpy (&current_card->TechParams.CardVParams.UID[0],
                              &((uint8_t *)event->Buff)[param_index],
                              PTX_IOTRD_TECH_V_UID_MAX_SIZE);
                param_index = (uint8_t)(param_index + PTX_IOTRD_TECH_V_UID_MAX_SIZE);

                if (event->EventId == NSC_EventRfAct_PassPoll_V)
                {
                    /* store Protocol-Type - unique per card registry*/
                    iotRd->CardRegistry->ActiveCardProtType = (ptxIoTRd_CardProtocol_t)((uint8_t*)event->Buff)[param_index];
                    param_index++;

                    /* No High-level protocol available for T5T */

                    /* keep reference to active card and set number of cards */
                    iotRd->CardRegistry->ActiveCard = &iotRd->CardRegistry->Cards[iotRd->CardRegistry->NrCards];
                }
                else
                {
                    /* NSC_EventRfDisc_PassPoll_V */

                    /* store device state */
                    current_card->DeviceState = ((uint8_t *)event->Buff)[param_index];
                }
                iotRd->CardRegistry->NrCards++;
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                break;
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_AddCardToRegistryFromRSP(ptxIoTRd_t *iotRd,
                                                  ptxIoTRd_CardParams_t *cardParams,
                                                  ptxIoTRd_CardProtocol_t protocol,
                                                  uint8_t *activationData,
                                                  size_t activationDataLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != iotRd)
    {
        uint8_t param_index = 0;

        /* Active card info buffer was used to get data from PTX. Now we need just to rearrange this. */
        switch (protocol)
        {
            case Prot_ISODEP:
                if ((NULL != activationData) && (0 != activationDataLen))
                {
                    if (cardParams->TechType == Tech_TypeA)
                    {
                        /* ATS-Response includes the Length information at offet 0; store it also in the buffer */
                        iotRd->CardRegistry->ActiveCardProtInfoLen = activationData[param_index];

                        (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                     &activationData[param_index],
                                     iotRd->CardRegistry->ActiveCardProtInfoLen);

                        param_index = (uint8_t)(param_index + iotRd->CardRegistry->ActiveCardProtInfoLen);
                        iotRd->CardRegistry->ActiveCardProtSpeed = activationData[param_index];
                    }
                    else
                    {
                        /* Tech_TypeB */

                        /* Attrib-Response starts at offset 1 */
                        param_index++;

                        iotRd->CardRegistry->ActiveCardProtInfoLen = activationData[param_index];
                        param_index++;

                        (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                     &activationData[param_index],
                                     iotRd->CardRegistry->ActiveCardProtInfoLen);
                        iotRd->CardRegistry->ActiveCardProtSpeed = iotRd->CardRegistry->ActiveCardProtInfo[0];
                    }
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscProtocolError);
                }
                break;

            case Prot_NFCDEP:
                if ((NULL != activationData) && (0 != activationDataLen))
                {
                    /* ATR-Response includes the Length information at offet 0; store it also in the buffer */
                    iotRd->CardRegistry->ActiveCardProtInfoLen = activationData[param_index];

                    (void)memcpy(&iotRd->CardRegistry->ActiveCardProtInfo[0],
                                 &activationData[param_index],
                                 iotRd->CardRegistry->ActiveCardProtInfoLen);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_NscProtocolError);
                }
                break;

            default:
                /* all other protocols do not use higher level info */
                iotRd->CardRegistry->ActiveCardProtInfoLen = 0;
                iotRd->CardRegistry->ActiveCardProtSpeed = 0;
                break;
        }

        /* keep reference of activated card */
        iotRd->CardRegistry->ActiveCardProtType = protocol;
        iotRd->CardRegistry->ActiveCard = cardParams;
        iotRd->CardRegistry->ActiveCard->DeviceState = (uint8_t)0x00;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}


static ptxNSC_RfConfig_ParamList_t ptxIoTRd_MapRFConfigID(ptxIoTRd_ChipConfigID_t configID)
{
    ptxNSC_RfConfig_ParamList_t nsc_id;

    switch (configID)
    {
    	case RF_Wavebank_0:
			nsc_id = RfCfgParam_Wavebank_0;
			break;

		case RF_Wavebank_1:
			nsc_id = RfCfgParam_Wavebank_1;
			break;

		case RF_Wavebank_2:
			nsc_id = RfCfgParam_Wavebank_2;
			break;

		case RF_Wavebank_3:
			nsc_id = RfCfgParam_Wavebank_3;
			break;

		case RF_Wavebank_4:
			nsc_id = RfCfgParam_Wavebank_4;
			break;

		case RF_Wavebank_5:
			nsc_id = RfCfgParam_Wavebank_5;
			break;

		case RF_Wavebank_6:
			nsc_id = RfCfgParam_Wavebank_6;
			break;

		case RF_Wavebank_7:
			nsc_id = RfCfgParam_Wavebank_7;
			break;

		case RF_Wavebank_8:
			nsc_id = RfCfgParam_Wavebank_8;
			break;

		case RF_Wavebank_9:
			nsc_id = RfCfgParam_Wavebank_9;
			break;

		case RF_Wavebank_10:
			nsc_id = RfCfgParam_Wavebank_10;
			break;

		case RF_Wavebank_11:
			nsc_id = RfCfgParam_Wavebank_11;
			break;

		case RF_Wavebank_12:
			nsc_id = RfCfgParam_Wavebank_12;
			break;

		case RF_Wavebank_13:
			nsc_id = RfCfgParam_Wavebank_13;
			break;

		case RF_Wavebank_14:
			nsc_id = RfCfgParam_Wavebank_14;
			break;

		case RF_Wavebank_15:
			nsc_id = RfCfgParam_Wavebank_15;
			break;

		case RF_Wavebank_16:
			nsc_id = RfCfgParam_Wavebank_16;
			break;

		case RF_Wavebank_17:
			nsc_id = RfCfgParam_Wavebank_17;
			break;

		case RF_Wavebank_18:
			nsc_id = RfCfgParam_Wavebank_18;
			break;

		case RF_Wavebank_19:
			nsc_id = RfCfgParam_Wavebank_19;
			break;

        case RF_Misc:
            nsc_id = RfCfgParam_RegsMisc;
            break;

        case RF_PollA106:
            nsc_id = RfCfgParam_RegsPollA106;
            break;

        case RF_PollA212:
            nsc_id = RfCfgParam_RegsPollA212;
            break;

        case RF_PollA424:
            nsc_id = RfCfgParam_RegsPollA424;
            break;

        case RF_PollA848:
            nsc_id = RfCfgParam_RegsPollA848;
            break;

        case RF_PollB106:
            nsc_id = RfCfgParam_RegsPollB106;
            break;

        case RF_PollB212:
            nsc_id = RfCfgParam_RegsPollB212;
            break;

        case RF_PollB424:
            nsc_id = RfCfgParam_RegsPollB424;
            break;

        case RF_PollB848:
            nsc_id = RfCfgParam_RegsPollB848;
            break;

        case RF_PollF212:
            nsc_id = RfCfgParam_RegsPollF212;
            break;

        case RF_PollF424:
            nsc_id = RfCfgParam_RegsPollF424;
            break;

        case RF_PollV:
            nsc_id = RfCfgParam_RegsPollV;
            break;

        case RF_Listen:
            nsc_id = RfCfgParam_Listen;
            break;

        default:
            nsc_id = RfCfgParam_Undefined;
            break;
    }

    return nsc_id;
}

ptxStatus_t ptxIoTRd_TempSensor_Calibration (ptxIoTRd_t *iotRd, uint8_t Tambient, uint8_t *Tshutdown)
{
    ptxStatus_t status = ptxStatus_Success;
    /*
     * In the following calculations it is assumed that ambient temperature is a positive value i.e.
     * calibration takes place at Tcal >= 0°C.
     * Tsense formula can become negative only if the temperature is less than -225°C
     */

    if ((NULL != iotRd) && (NULL != Tshutdown))
    {
        //Read PTX temperature sensor value. After the readout, it is stored temporarily in system parameters.
        uint8_t sens_val = 0;
        uint8_t temp_val = Tambient;
        int8_t temp_offset;

        status = ptxNSC_ReadTempSensor(iotRd->Nsc, &sens_val);

        if (ptxStatus_Success == status)
        {

            /*
             * Common function to do offset compensation of the ambient temperature value.
             * Ideal value is given by using the Temp sensor-function, when offset is 0.
             * Tamb_word = Round((Tamb *0.0049 + 1.106) *255 / 1.8))
             */
            temp_offset = 0;
            status = ptxIoTRd_TempOffsetComp (iotRd, &temp_offset, &temp_val);

            if (ptxStatus_Success == status)
            {
                /*
                 * Calculate offset: the result is possibly negative. Mixing signed and unsigned.
                 * Toffset_word =  Tsense_word (Chip Temperature Result) – Tamb_word
                 */
                if (sens_val >= temp_val)
                {
                    temp_offset = (int8_t)(sens_val - temp_val);
                } else
                {
                    temp_offset = (int8_t)(temp_val - sens_val);
                    temp_offset = (int8_t)(0 - temp_offset);
                }

                /*
                 * Calculate compensated shutdown temperature.
                 */
                temp_val = *Tshutdown;
                status = ptxIoTRd_TempOffsetComp (iotRd, &temp_offset, &temp_val);
            }

            if (ptxStatus_Success == status)
            {
                //Final result
                *Tshutdown = temp_val;
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_TempOffsetComp (ptxIoTRd_t *iotRd, int8_t *tempOffset, uint8_t *tempVal)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != iotRd) && (NULL != tempOffset) && (NULL != tempVal))
    {
        /*
         * Calculate Target Temperature Word by:
         *      Tsht_dwn_word   = Round[ (Tsht_dwn *0.0049 + 1.106) *255 / 1.8] + Toffset_word
         *                      = Round[ (Tsht_dwn * 49/10000 + 1106/1000) *255 *10 / 18] + Toffset_word
         *                      = Round[ ((Tsht_dwn * 49 + 11060)/10000) * 2550/18] + Toffset_word
         *                      = Round[ (Tsht_dwn * 49 + 11060) * 2550 /180000] + Toffset_word
         *
         * Max. value during calculus: 60065250 := 0x039485E2. Fits into uint32_t
         * If Tsht_dwn is greater than 141, the result will go out of 255 boundaries. This is not taking into account Toffset_word.
         *
         * No floating point arithmetics.
         */

        int8_t temp_offset = *tempOffset;

        uint32_t rest = 0;
        uint32_t temp_calculated = *tempVal;
        temp_calculated = (49u * temp_calculated + 11060u) * 2550u;
        rest = temp_calculated % 180000u;

        temp_calculated = temp_calculated / 180000u;
        if (rest >= 90000u)
        {
            /* Round up */
            temp_calculated += 1;
        }

        if (temp_offset >= 0)
        {
            //Offset is a positive value
            temp_calculated += (uint8_t)temp_offset;
        } else
        {
            //Offset is negative. Make it positive and subtract.
            temp_calculated -= (uint8_t)(((int8_t)0 - temp_offset) & 0x007F);
        }

        if (temp_calculated > 255u)
        {
            st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
        } else
        {
            *tempVal = (uint8_t)temp_calculated;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

static ptxStatus_t ptxIoTRd_Set_T3T_MultiRxMode(ptxIoTRd_t *iotRd, uint8_t enableRxMode, uint32_t timeoutMS)
{
    /*
     * Internal uses a resolution of 128/fc per tick ~ 9,44us; 1 ms is roughly 106 ticks (~1.00059ms)
     * Note: The resolution is limited to 24-bits i.e. the max. value is roughly 158 s.
     */
    const uint32_t BASE_TIMEOUT_1_MS = (uint32_t)106;

    ptxStatus_t status = ptxStatus_Success;

    if (NULL != iotRd)
    {
        size_t rf_ParLen = 2u;
        ptxNSC_RfPar_t rf_Par[rf_ParLen];

        uint32_t timeout_val = (uint32_t)((BASE_TIMEOUT_1_MS * (timeoutMS + 1)) & 0x00FFFFFF);

        if (0 != enableRxMode)
        {
            rf_Par[0].ParmId = RfParameter_Fwt;
            rf_Par[0].Parm.Fwt.Fwt[0] = (uint8_t)((timeout_val >> 16) & (uint8_t)0xFF);
            rf_Par[0].Parm.Fwt.Fwt[1] = (uint8_t)((timeout_val >> 8)  & (uint8_t)0xFF);
            rf_Par[0].Parm.Fwt.Fwt[2] = (uint8_t)((timeout_val >> 0)  & (uint8_t)0xFF);

            rf_Par[1].ParmId = RfParameter_Res_Limit;
            rf_Par[1].Parm.ResLimit.ResLimit = 0;

        } else
        {
            rf_Par[0].ParmId = RfParameter_Fwt;
            rf_Par[0].Parm.Fwt.Fwt[0] = 0;
            rf_Par[0].Parm.Fwt.Fwt[1] = 0;
            rf_Par[0].Parm.Fwt.Fwt[2] = 0;

            rf_Par[1].ParmId = RfParameter_Res_Limit;
            rf_Par[1].Parm.ResLimit.ResLimit = (uint8_t)1;
        }

        status = ptxNSC_RfSetParams(iotRd->Nsc, &rf_Par[0], rf_ParLen);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_GetRfDiscParams(ptxIoTRd_t *iotRd, ptxNSC_RfDiscPars_t *discoverParams)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((iotRd != NULL) && (discoverParams != NULL))
    {
        /* Assign default values for RF-Discovery according to NFC-Forum */

        /* Reset complete structure */
        (void)memset(discoverParams, 0, sizeof(ptxNSC_RfDiscPars_t));

        /*
         * Parameters for general Poll-loop
         */

        /* NFC-Forum Polling Mode. First technology to poll: NFC-A. LPCD-Notification disabled */
        discoverParams->Con_Poll            = (uint8_t)NfcForumMode | (uint8_t)(Init_Poll_A << 4u);

        /* RF-Idle Time (multiple of 32us => 0x0C1C ~ 100ms */
        discoverParams->Con_Idle_Time[0]    = 0u;
        discoverParams->Con_Idle_Time[1]    = 0x0Cu;
        discoverParams->Con_Idle_Time[2]    = 0x1Cu;


        /*
         *  Parameters for Type A Technology
         */

        /*  Poll for Type-A Technology. Enable Anti-Collision. Bail-out deactivated - continue polling for other technologies. */
        discoverParams->Con_Poll_A              = (uint8_t)(0x01u | 0x04u);
        discoverParams->Con_Poll_A_Freq         = 1u;               /* Poll for Type-A during every Poll-cycle */
        discoverParams->Con_Poll_A_Cmd          = 0x52u;            /* Initial Type-A Command */
        discoverParams->Con_Poll_A_Dev_Limit    = 5u;               /* Resolve max. 5 Cards */


        /*
         *  Parameters for Type B Technology
         */
        /* Poll for Type-B Technology. Bail-out deactivated - continue polling for other technologies */
        discoverParams->Con_Poll_B              = 1u;
        discoverParams->Con_Poll_B_Freq         = 1u;               /* Poll for Type-B during every Poll-cycle */
        discoverParams->Con_Poll_B_Cmd[0]       = 0x05u;            /* Initial Type-B Command */
        discoverParams->Con_Poll_B_Cmd[1]       = 0x00u;
        discoverParams->Con_Poll_B_Cmd[2]       = 0x08u;
        discoverParams->Con_Poll_B_Dev_Limit    = 5u;               /* Resolve max. 5 Cards */

        /*
         *  Parameters for Type F Technology
         */

        /* Poll for Type-F Technology at 212 and 424 kps. Bail-out deactivated - continue polling for other technologies */
        discoverParams->Con_Poll_F              = (uint8_t)(0x01u | 0x02u);
        discoverParams->Con_Poll_F_Freq         = 1u;                /* Poll for Type-F during every Poll-cycle */
        discoverParams->Con_Poll_F_Cmd[0]       = 0x06u;             /* Initial Type-F Command */
        discoverParams->Con_Poll_F_Cmd[1]       = 0x00u;
        discoverParams->Con_Poll_F_Cmd[2]       = 0xFFu;
        discoverParams->Con_Poll_F_Cmd[3]       = 0xFFu;
        discoverParams->Con_Poll_F_Cmd[4]       = 0x00u;
        discoverParams->Con_Poll_F_Cmd[5]       = 0x03u;
        discoverParams->Con_Poll_F_Dev_Limit    = 3u;                /* Resolve max. 5 Cards */

        /*
         *  Parameters for Type V Technology
         */
        discoverParams->Con_Poll_V              = 1u;                /* Poll for Type-B Technology */
        discoverParams->Con_Poll_V_Freq         = 1u;                /* Poll for Type-B during every Poll-cycle */
        discoverParams->Con_Poll_V_Cmd[0]       = 0x26u;             /* Initial Type-V Command (Total = 11 Byte(s)). Inventory. */
        discoverParams->Con_Poll_V_Cmd[1]       = 0x01u;
        discoverParams->Con_Poll_V_Dev_Limit    = 50u;               /* Resolve max. 50 Cards */

        /*
         *  Parameters for Type-A/B ISO-DEP protocol
         */
        discoverParams->Con_Poll_Iso_Dep                    = 1u;       /* Enable auto-activation for ISO-DEP protocol */
        discoverParams->Con_Poll_Iso_Dep_Rats_Param         = 0x80u;    /* FSDI = 8 i.e. 256 byte(s) per RF-packet */
        discoverParams->Con_Poll_Iso_Dep_Attrib_Param1      = 0u;
        discoverParams->Con_Poll_Iso_Dep_Attrib_Param2_Fsdi = 8u;       /* FSDI = 8 i.e. 256 byte(s) per RF-packet */
        discoverParams->Con_Poll_Iso_Dep_Attrib_Param3      = 1u;
        discoverParams->Con_Poll_Iso_Dep_Attrib_Param4      = 0u;
        discoverParams->Con_Poll_Iso_Dep_Attrib_Inf_Len     = 0u;

        /*
         *  Parameters for Type-A/F NFC-DEP protocol
         */
        discoverParams->Con_Poll_Nfc_Dep                = 1u;               /* Enable auto-activation for NFC-DEP protocol */
        discoverParams->Con_Poll_Nfc_Dep_Atr_Req_Pp     = 0x20u;            /* LR = 192 byte(s), General Bytes not present (LLCP)*/

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return st;
}

static ptxStatus_t ptxIoTRd_ResetCardRegistry(ptxIoTRd_t *iotRd)
{
    ptxStatus_t _status = ptxStatus_Success;
    if (NULL != iotRd)
    {
        (void)memset(iotRd->CardRegistry, 0, sizeof(ptxIoTRd_CardRegistry_t));
        iotRd->DiscoverState = RF_DISCOVER_STATUS_NO_CARD;
        iotRd->LastRFError = PTX_RF_ERROR_NTF_CODE_NO_ERROR;
        iotRd->LpcdState = RF_LPCD_STATUS_NO_DEVICE;
        iotRd->LPCDNtfCounter = 0;
        (void) ptxIoTRd_ClearRfMsgRcvd(iotRd, &iotRd->RfMsg);
    } else
    {
        _status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return _status;
}

static void ptxIoTRd_CallBackEvents (void *iotRd, ptxNSC_Event_t *event)
{
    uint8_t rf_error_code;

    ptxStatus_t st = ptxStatus_Success;
    ptxHce_EventRecord_t *evt_record = NULL;
    uint16_t evt_index = 0;

    uint8_t *nsc_payload;
    uint16_t nsc_payload_length;

    if ((NULL != iotRd) && (NULL != event))
    {
        ptxIoTRd_t *pIot = (ptxIoTRd_t *)iotRd;

        if (ptxStatus_Comp_IoTReader == pIot->CompId)
        {

            /* NTF-opcode at offset 0 already processed at lower layer, payload of NTF starts at offset 1 */
            nsc_payload = &event->Buff[0];
            nsc_payload_length = (uint16_t)(event->BuffLen);

            st = ptxHce_ReserveEventRecord(&pIot->Hce, &evt_record, &evt_index);

            if ( ptxStatus_Success == st )
            {
                evt_record->EventID = HceEvent_NoEvent;

                switch (event->EventId)
                {
                    case NSC_EventRfAct_PassPoll_A:
                    case NSC_EventRfAct_PassPoll_B:
                    case NSC_EventRfAct_PassPoll_F:
                    case NSC_EventRfAct_PassPoll_V:
                        /* update card registry */
                        (void) ptxIoTRd_AddCardToRegistryFromNTF(pIot, event);
                        /* Card found and activated */
                        pIot->DiscoverState = RF_DISCOVER_STATUS_CARD_ACTIVE;
                        break;

                    case NSC_EventRfDisc_PassPoll_A:
                    case NSC_EventRfDisc_PassPoll_B:
                    case NSC_EventRfDisc_PassPoll_F:
                    case NSC_EventRfDisc_PassPoll_V:
                        /* update card registry */
                        (void) ptxIoTRd_AddCardToRegistryFromNTF(pIot, event);
                        /* Card found - more cards may follow */
                        pIot->DiscoverState = RF_DISCOVER_STATUS_DISCOVER_RUNNING;
                        break;

                    case NSC_EventRfDisc_LastOne:
                        /* all cards found (or device-limits reached) => RF-Discovery finished */
                        pIot->DiscoverState = RF_DISCOVER_STATUS_DISCOVER_DONE;
                        break;

                    case NSC_EventError:
                    case NSC_RfTimeOutError:

                        switch (pIot->DiscoverState)
                        {
                            case RF_DISCOVER_STATUS_NO_CARD:
                            case RF_DISCOVER_STATUS_DISCOVER_RUNNING:
                            case RF_DISCOVER_STATUS_DISCOVER_DONE:
                                /* reset registry & discover-state */
                                (void)ptxIoTRd_ResetCardRegistry(iotRd);
                                pIot->DiscoverState = RF_DISCOVER_STATUS_NO_CARD;
                                break;

                            default:
                                /* nothing to do */
                                break;
                        }

                        if ((event->Buff != NULL) && (event->BuffLen != 0))
                        {
                            /* check received error code */
                            rf_error_code = ((uint8_t *)event->Buff)[0];

                            switch (rf_error_code)
                            {
                                case PTX_NSC_RF_ERROR_NTF_CODE_ERR_THERMAL:
                                    pIot->Nsc->SysState = SystemState_ERR_Temperature;
                                    break;

                                case PTX_NSC_RF_ERROR_NTF_CODE_ERR_EXT_CURRENT_SENSOR:
                                case PTX_NSC_RF_ERROR_NTF_CODE_ERR_OVERCURRENT:
                                    pIot->Nsc->SysState = SystemState_ERR_Overcurrent;
                                    break;

                                default:
                                    /* other error - no impact on system state */
                                    break;
                            }

                            if ((PTX_RF_ERROR_NTF_CODE_ERR_TIMEOUT == rf_error_code)      ||
                                (PTX_RF_ERROR_NTF_CODE_ERR_EMV_COLL == rf_error_code)     ||
                                (PTX_RF_ERROR_NTF_CODE_ERR_TRANSMISSION == rf_error_code) ||
                                (PTX_RF_ERROR_NTF_CODE_ERR_PROTOCOL == rf_error_code)     ||
                                (PTX_RF_ERROR_NTF_CODE_WARNING_PA_OVERCURRENT_LIMIT == rf_error_code))
                                {
                                    pIot->LastRFError = rf_error_code;
                                } else
                                {
                                    pIot->LastRFError = PTX_RF_ERROR_NTF_CODE_UNKNOWN_ERROR;
                                }
                        }

                        /* Update on the RfMsg State. */
                        if (NSC_RfTimeOutError == event->EventId)
                        {
                            pIot->RfMsg.State = RfMsg_RfErrorTimeOut;
                        } else
                        {
                            pIot->RfMsg.State = RfMsg_RfError;
                        }

                        break;

                    case NSC_LPCDTrigered:
                        /* Update on the RfMsg State. */
                        pIot->LpcdState = RF_LPCD_STATUS_DEVICE_FOUND;

                        /* Increment LPDC-NTF counter */
                        if (pIot->LPCDNtfCounter < (uint8_t)255)
                        {
                            pIot->LPCDNtfCounter++;
                        }
                        break;

                    case NSC_Event_RfCtr_ACK:
                        /* Update on the RfMsg State. */
                        pIot->RfMsg.State = RfMsg_CtrlAck;
                        break;

                    case NSC_Event_RfCtr_AttCmd:
                        /* Update on the RfMsg State. */
                        pIot->RfMsg.State = RfMsg_CtrlAttCmd;
                        break;

                    case NSC_EventRfDeact:
                        if (RF_DISCOVER_STATUS_LISTEN_A != pIot->DiscoverState)
                        {
                            /* Update Deactivate-NTF pending State. */

                            pIot->Nsc->DeactivationNTFPending = 0;
                        }
                        else
                        {
                            evt_record->EventID = HceEvent_Deactivated;

                            (void)memcpy(&pIot->BuffNtf[pIot->BuffNtfIndex], &nsc_payload[0], nsc_payload_length);
                            pIot->BuffNtfIndex += nsc_payload_length;

                            evt_record->RxMsgDataLen = pIot->BuffNtfIndex;
                            evt_record->RxMsgData = pIot->BuffNtf;

                            /* Update internal States */
                            pIot->BuffNtfIndex = 0;
                        }

                        break;

                    case NSC_Event_NfcDataMsg:
                        if (RF_DISCOVER_STATUS_LISTEN_A != pIot->DiscoverState)
                        {
                            if ((event->Buff != NULL) && (event->BuffLen <= PTX_IOTRD_RF_MSG_MAX_SIZE))
                            {
                                /* Keep Rf Message received. */
                                (void)memcpy(pIot->RfMsg.Buff, event->Buff, event->BuffLen);
                                pIot->RfMsg.BuffLen = event->BuffLen;
                                pIot->RfMsg.State = RfMsg_RfMsg_Rcv;
                            }
                        }
                        else
                        {
                            evt_record->EventID = HceEvent_Data;

                            /* protect buffer overflow -> no need to set any status*/
                            if ((pIot->BuffNtfIndex + nsc_payload_length) <= PTX_IOTRD_RF_MSG_MAX_SIZE)
                            {
                                (void)memcpy(&pIot->BuffNtf[pIot->BuffNtfIndex], &nsc_payload[0], nsc_payload_length);
                                pIot->BuffNtfIndex += nsc_payload_length;

                                evt_record->RxMsgDataLen = pIot->BuffNtfIndex;
                                evt_record->RxMsgData = pIot->BuffNtf;
                            }
                            else
                            {
                                /*
                                 * By setting this pointer to NULL, the upper-layer can covert this into a BUFFER_TO_SMALL_ERROR.
                                 * Actually it is not needed at all to forward this event, but it could be useful for debugging
                                 * to tell the application that the size of the application rx-buffer is too small.
                                 * If not forwarded, the Reader would just deactivate us or turn off the field.
                                 */
                                evt_record->RxMsgData = NULL;
                            }

                            /* Update internal States */
                            pIot->BuffNtfIndex = 0;
                        }
                        break;

                    case NSC_Event_NfcDataMsg_Chained:
                        if (RF_DISCOVER_STATUS_LISTEN_A != pIot->DiscoverState)
                        {
                            if ((event->Buff != NULL) && (event->BuffLen <= PTX_IOTRD_RF_MSG_MAX_SIZE))
                            {
                                /* Keep Rf Message received. */
                                (void)memcpy(pIot->RfMsg.Buff, event->Buff, event->BuffLen);
                                pIot->RfMsg.BuffLen = event->BuffLen;
                                pIot->RfMsg.State = RfMsg_RfMsg_Chained_Rcv;
                            }
                        }
                        else
                        {
                            /* protect buffer overflow */
                            if ((pIot->BuffNtfIndex + nsc_payload_length) <= PTX_IOTRD_RF_MSG_MAX_SIZE)
                            {
                                (void)memcpy(&pIot->BuffNtf[pIot->BuffNtfIndex], &nsc_payload[0], nsc_payload_length);
                                pIot->BuffNtfIndex += nsc_payload_length;
                            }
                        }
                        break;

                    case NSC_Event_NfcCltMsg:
                        if ((event->Buff != NULL) && (event->BuffLen <= PTX_IOTRD_RF_MSG_MAX_SIZE) &&
                                (event->BuffSecondary != NULL) && (event->BuffSecondaryLen <= PTX_IOTRD_RF_MSG_MAX_SIZE) )
                        {
                            /* Keep Clt Rf Message received. */

                            (void)memcpy(pIot->RfMsg.Buff, event->Buff, event->BuffLen);
                            pIot->RfMsg.BuffLen = event->BuffLen;

                            (void)memcpy(pIot->RfMsg.BuffSecond, event->BuffSecondary, event->BuffSecondaryLen);
                            pIot->RfMsg.BuffSecondLen = event->BuffSecondaryLen;

                            pIot->RfMsg.NumTotalBits = event->NumTotalBitsSecondary;

                            pIot->RfMsg.State = RfMsg_RfClt;
                        }
                        break;

                    case NSC_EventRfField_on:
                        evt_record->EventID = HceEvent_ExtFieldOn;

                        pIot->DiscoverState = RF_DISCOVER_STATUS_LISTEN_A;
                        /*
                         * Payload:
                         *  N.A. => indicated by ID
                         */
                        evt_record->RxMsgDataLen = 0x00u;

                        /* Update internal States */
                        //pIot->BuffNtfIndex = 0;
                        break;

                    case NSC_EventRfField_off:
                        evt_record->EventID = HceEvent_ExtFieldOff;
                        /*
                         * Payload:
                         *  N.A. => indicated by ID
                         */
                        evt_record->RxMsgDataLen = 0x00u;

                        pIot->DiscoverState = RF_DISCOVER_STATUS_NO_CARD;

                        /* Update internal States */
                        pIot->BuffNtfIndex = 0;
                        break;

                    case NSC_EventRfAct_PassListen_A:
                        evt_record->EventID = HceEvent_Activated_ListenA;
                        /*
                         * Payload:
                         *  - Offset  0: RF-Protocol (see ptxNSC.h)
                         *  - Offset  1: RATS-Parameter in case of ISO-DEP RF-Protocol, otherwise (e.g. T2T) not available
                         */
                        (void)memcpy(&pIot->BuffNtf[0], &nsc_payload[0], nsc_payload_length - 2u);
                        evt_record->RxMsgDataLen = (uint8_t)(nsc_payload_length - 2u);
                        evt_record->RxMsgData = pIot->BuffNtf;

                        /* Update internal States */
                        pIot->BuffNtfIndex = 0;

                        pIot->DiscoverState = RF_DISCOVER_STATUS_LISTEN_A;
                        break;

                    default:
                        for (uint8_t i = 0; i < PTX_IOTRD_MAX_EXTENSIONS; i++)
                        {
                            if (NULL != pIot->Extension[i].CBFnExtProcessNtf)
                            {
                                st = pIot->Extension[i].CBFnExtProcessNtf(pIot->Extension[i].ExtensionCtx, iotRd, event);

                                if (ptxStatus_Success == st)
                                {
                                    break;
                                }
                            }
                        }
                        break;
                }

                 /* Event available - add it to the queue ? */
                if (HceEvent_NoEvent != evt_record->EventID)
                {
                    st = ptxHce_AddNewEventPending (&pIot->Hce, evt_index);
                }
            }
        }
    }
}

static ptxStatus_t ptxIoTRd_NSCRfClearCltMode (ptxIoTRd_t *iotRd)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != iotRd)
    {
        size_t rf_ParLen = 6u;
        ptxNSC_RfPar_t rf_Par[rf_ParLen];

        /* Tx Parity ON*/
        rf_Par[0].ParmId = RfParameter_Tx_PAR;
        rf_Par[0].Parm.TxParity.TxParity = 1u;

        /* Rx Parity ON*/
        rf_Par[1u].ParmId = RfParameter_Rx_PAR;
        rf_Par[1u].Parm.RxParity.RxParity = 1u;

        /* Tx CRC ON*/
        rf_Par[2u].ParmId = RfParameter_Tx_CRC;
        rf_Par[2u].Parm.TxCRC.TxCRC = 1u;

        /* Rx CRC ON*/
        rf_Par[3u].ParmId = RfParameter_Rx_CRC;
        rf_Par[3u].Parm.RxCRC.RxCRC = 1u;

        /* FWT */
        rf_Par[4u].ParmId = RfParameter_Fwt;
        rf_Par[4u].Parm.Fwt.Fwt[0]  = 0x00;
        rf_Par[4u].Parm.Fwt.Fwt[1u] = 0x00;
        rf_Par[4u].Parm.Fwt.Fwt[2u] = 0x00;

        /* RES_LIMIT */
        rf_Par[5u].ParmId = RfParameter_Res_Limit;
        rf_Par[5u].Parm.ResLimit.ResLimit = 0x01;

        status = ptxNSC_RfSetParams(iotRd->Nsc, &rf_Par[0], rf_ParLen);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_NSCRfSetCltMode (ptxIoTRd_t *iotRd)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != iotRd)
    {
        size_t rf_ParLen = 6u;
        ptxNSC_RfPar_t rf_Par[rf_ParLen];

        /* Tx Parity OFF*/
        rf_Par[0].ParmId = RfParameter_Tx_PAR;
        rf_Par[0].Parm.TxParity.TxParity = 0;

        /* Rx Parity OFF*/
        rf_Par[1u].ParmId = RfParameter_Rx_PAR;
        rf_Par[1u].Parm.RxParity.RxParity = 0;

        /* Tx CRC OFF*/
        rf_Par[2u].ParmId = RfParameter_Tx_CRC;
        rf_Par[2u].Parm.TxCRC.TxCRC = 0;

        /* Rx CRC OFF*/
        rf_Par[3u].ParmId = RfParameter_Rx_CRC;
        rf_Par[3u].Parm.RxCRC.RxCRC = 0;

        /* FWT */
        rf_Par[4u].ParmId = RfParameter_Fwt;

        // FWT has time units of 128/Fc . This is 9,43 microsec.
        // CLT timeout is has maximum 100 millisec. So, 1060 FWT Time Units.
        // 0x274C

        rf_Par[4u].Parm.Fwt.Fwt[0]  = 0x00;
        rf_Par[4u].Parm.Fwt.Fwt[1u] = 0x27;
        rf_Par[4u].Parm.Fwt.Fwt[2u] = 0x4C;

        /* RES_LIMIT */
        rf_Par[5u].ParmId = RfParameter_Res_Limit;
        rf_Par[5u].Parm.ResLimit.ResLimit = 0x01;

        status = ptxNSC_RfSetParams(iotRd->Nsc, &rf_Par[0], rf_ParLen);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxIoTRd_Manage_DDPC (ptxIoTRd_t *iotRd, uint8_t enableDDPC)
{
    const uint8_t CONFIG_MISC_BUFFER_SIZE = PTX_NSC_MISC_RF_CONFIG_BUFFER_SIZE;
    const uint8_t CONFIG_MISC_OFFSET_DDPC                       = 6;
    const uint8_t CONFIG_MISC_MASK_DDPC                         = 0x02;
    const uint8_t CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW1_VAL    = 11;
    const uint8_t CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW0_VAL    = 12;
    const uint8_t CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH1_VAL   = 13;
    const uint8_t CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH0_VAL   = 14;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t config_misc_buffer[CONFIG_MISC_BUFFER_SIZE];
    uint8_t config_misc_buffer_len = CONFIG_MISC_BUFFER_SIZE;

    (void)memset(&config_misc_buffer[0], 0, CONFIG_MISC_BUFFER_SIZE);

    if (NULL != iotRd)
    {
        /* load MISC.-Section from RF-Config */
        status = ptxNSC_GetMiscRFConfig(iotRd->Nsc, &config_misc_buffer[0], &config_misc_buffer_len);

        if (ptxStatus_Success == status)
        {
            if (0 != enableDDPC)
            {
                /* store original values */
                iotRd->RSSIModeCfg[0] = config_misc_buffer[CONFIG_MISC_OFFSET_DDPC];
                iotRd->RSSIModeCfg[1] = config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW1_VAL];
                iotRd->RSSIModeCfg[2] = config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW0_VAL];
                iotRd->RSSIModeCfg[3] = config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH1_VAL];
                iotRd->RSSIModeCfg[4] = config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH0_VAL];

                /* overwrite values for RSSI-mode */
                config_misc_buffer[CONFIG_MISC_OFFSET_DDPC] = (uint8_t)(config_misc_buffer[CONFIG_MISC_OFFSET_DDPC] | CONFIG_MISC_MASK_DDPC);
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW1_VAL]  = (uint8_t)(0xFF);
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW0_VAL]  = (uint8_t)(0xFF);
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH1_VAL] = (uint8_t)(0xFF);
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH0_VAL] = (uint8_t)(0xFF);

            } else
            {
                /* restore original values */
                config_misc_buffer[CONFIG_MISC_OFFSET_DDPC]                     = iotRd->RSSIModeCfg[0];
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW1_VAL]  = iotRd->RSSIModeCfg[1];
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_LOW0_VAL]  = iotRd->RSSIModeCfg[2];
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH1_VAL] = iotRd->RSSIModeCfg[3];
                config_misc_buffer[CONFIG_MISC_OFFSET_DIGITAL_DPC_TH_HIGH0_VAL] = iotRd->RSSIModeCfg[4];
            }

            ptxIoTRd_ChipConfig_t chip_config;
            (void)memset(&chip_config, 0, sizeof(ptxIoTRd_ChipConfig_t));
            chip_config.ID = RF_Misc;
            chip_config.Value = &config_misc_buffer[0];
            chip_config.Len = config_misc_buffer_len;

            status = ptxIoTRd_Update_ChipConfig (iotRd, 1U, &chip_config);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
    }

    return status;
}


