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
    Module      : COMMON API
    File        : ptxIoTRd_COMMON.c

    Description : Common API for common functions.
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT.h"
#include "ptxIoTRd_COMMON.h"
#include "ptxHce_Exchange.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * Example Code-Delays/-Sleeps; used to prevent high-CPU loads on target system if a certain status gets polled frequently.
 * May be adapted on target system
 */
#define PTX_IOTRD_NO_CARD_SLEEP_TIME         (5u)

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

void ptxIoTRdInt_Sleep(ptxIoTRd_t *iotRd, uint32_t timeout)
{
    if(iotRd != NULL)
    {
        (void)ptxPLAT_Sleep(iotRd->Plat, timeout);
    }
}

void ptxIoTRdInt_Get_Card_Details(ptxIoTRd_CardRegistry_t *cardRegistry, ptxIoTRd_CardParams_t *cardParams, uint8_t nr)
{
    if ((NULL != cardRegistry) && (NULL != cardParams))
    {
        switch (cardParams->TechType)
        {
            case Tech_TypeA:
                ptxCommon_PrintF("%02d. RF-Technology = Type-A", nr);
                ptxCommon_PrintF("; SENS_RES: ");
                ptxCommon_Print_Buffer(&cardParams->TechParams.CardAParams.SENS_RES[0], 0u, PTX_IOTRD_TECH_A_SENSRES_MAX_SIZE, 0, 0);
                ptxCommon_PrintF("; NFCID1_LEN: %02X", cardParams->TechParams.CardAParams.NFCID1_LEN);
                ptxCommon_PrintF("; NFCID1: ");
                ptxCommon_Print_Buffer(&cardParams->TechParams.CardAParams.NFCID1[0], 0u, cardParams->TechParams.CardAParams.NFCID1_LEN, 0, 0);
                if (0 != cardParams->TechParams.CardAParams.SEL_RES_LEN)
                {
                    ptxCommon_PrintF("; SEL_RES: %02X", cardParams->TechParams.CardAParams.SEL_RES);
                }

                break;

            case Tech_TypeB:
                ptxCommon_PrintF("%02d. RF-Technology = Type-B", nr);
                ptxCommon_PrintF("; SENSB_RES: ");
                ptxCommon_Print_Buffer(&cardParams->TechParams.CardBParams.SENSB_RES[0], 0u, PTX_IOTRD_TECH_B_SENSB_MAX_SIZE, 0, 0);
                break;

            case Tech_TypeF:
                ptxCommon_PrintF("%02d. RF-Technology = Type-F", nr);
                ptxCommon_PrintF("; SENSF_RES: ");
                ptxCommon_Print_Buffer(&cardParams->TechParams.CardFParams.SENSF_RES[0], 0u, cardParams->TechParams.CardFParams.SENSF_RES_LEN, 0, 0);
                break;

            case Tech_TypeV:
                ptxCommon_PrintF("%02d. RF-Technology = Type-V", nr);
                ptxCommon_PrintF("; DSFID: %02X", cardParams->TechParams.CardVParams.DSFID);
                ptxCommon_PrintF("; RES_FLAGS: %02X", cardParams->TechParams.CardVParams.RES_FLAG);
                ptxCommon_PrintF("; UID: ");
                /* The UID of Type-V is stored LSB first - mirror it for the print to console */
                for (uint8_t i = 0; i < PTX_IOTRD_TECH_V_UID_MAX_SIZE; i++)
                {
                    ptxCommon_PrintF("%02X ", cardParams->TechParams.CardVParams.UID[PTX_IOTRD_TECH_V_UID_MAX_SIZE - 1u -i]);
                }
                break;

            case Tech_TypeExtension:
                ptxCommon_PrintF("%02d. RF-Technology = Type-Extension", nr);
                ptxCommon_PrintF("; Extension Flags: %04X", cardParams->TechParams.CardExtParams.Flags);
                ptxCommon_PrintF("; Extension Technology Parameters: ");
                ptxCommon_Print_Buffer(&cardParams->TechParams.CardExtParams.Param[0], 0u, cardParams->TechParams.CardExtParams.ParamLength, 0, 0);
                break;

            default:
                /* ignore */
                break;
        }

        if (cardRegistry->ActiveCard == cardParams)
        {
            switch (cardRegistry->ActiveCardProtType)
            {
                case Prot_T2T:
                    ptxCommon_PrintF("; Protocol: T2T");
                    break;

                case Prot_T3T:
                    ptxCommon_PrintF("; Protocol....: T3T");
                    break;

                case Prot_ISODEP:
                    ptxCommon_PrintF("; Protocol....: ISO-DEP");
                    if (0 != cardRegistry->ActiveCardProtInfoLen)
                    {
                        switch (cardParams->TechType)
                        {
                            case Tech_TypeA:
                                ptxCommon_PrintF("; PPS1: ");
                                ptxCommon_PrintF("%02X", cardRegistry->ActiveCardProtSpeed);
                                ptxCommon_PrintF("; ATS: ");
                                ptxCommon_Print_Buffer(&cardRegistry->ActiveCardProtInfo[0], 0u, cardRegistry->ActiveCardProtInfoLen, 0, 0);
                                break;

                            case Tech_TypeB:
                                ptxCommon_PrintF("; ATTRIB2: ");
                                ptxCommon_PrintF("%02X", cardRegistry->ActiveCardProtSpeed);
                                ptxCommon_PrintF("; ATTRIB_RES: ");
                                ptxCommon_Print_Buffer(&cardRegistry->ActiveCardProtInfo[0], 0u, cardRegistry->ActiveCardProtInfoLen, 0, 0);
                                break;

                            default:
                                // ignore
                                break;
                        }
                    }
                    break;

                case Prot_NFCDEP:
                    ptxCommon_PrintF("; Protocol: NFC-DEP");
                    if (0 != cardRegistry->ActiveCardProtInfoLen)
                    {
                        switch (cardParams->TechType)
                        {
                            case Tech_TypeA:
                            case Tech_TypeF:
                                ptxCommon_PrintF("; ATR_RES: ");
                                ptxCommon_Print_Buffer(&cardRegistry->ActiveCardProtInfo[0], 0u, cardRegistry->ActiveCardProtInfoLen, 0, 0);
                                break;

                            default:
                                // ignore
                                break;
                        }
                    }
                    break;

                case Prot_T5T:
                    ptxCommon_PrintF("; Protocol: T5T");
                    break;

                case Prot_Extension:
                    ptxCommon_PrintF("; Protocol: Extension; Activation Parameters: ");
                    ptxCommon_Print_Buffer(&cardRegistry->ActiveCardProtInfo[0], 0u, cardRegistry->ActiveCardProtInfoLen, 0, 0);
                    break;

                default:
                    ptxCommon_PrintF("; Protocol: Undefined");
                    break;
            }
        }

        ptxCommon_PrintF("\n");
    }
}

void ptxIoTRdInt_DemoState_WaitForActivation(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState)
{
    if ((NULL != iotRd) && (NULL != cardRegistry) && (NULL != demoState))
    {
        uint8_t discover_status = RF_DISCOVER_STATUS_NO_CARD;
        uint8_t skip_discovery_ongoing_print = 0;

        /* check the state of the ongoing discovery */
        ptxStatus_t st = ptxIoTRd_Get_Status_Info (iotRd, StatusType_Discover, &discover_status);

        if(ptxStatus_Success != st)
        {
            *demoState = IoTRd_DemoState_DeactivateReader;
        } else
        {
            /*
             * Note: Calling "ptxIoTRd_Get_Discover_Status" continuously without any delays in between may cause
             *       a high CPU-load which may lead to overall performance degradations.
             *       Therefore this needs to be adapted for the target application / system.
             **/

            switch (discover_status)
            {
                case RF_DISCOVER_STATUS_NO_CARD:
                    /*
                     * Wait until card is in the field (off-load CPU with Sleep-operation)
                     */
                    ptxIoTRdInt_Sleep(iotRd, PTX_IOTRD_NO_CARD_SLEEP_TIME);
                    break;

                case RF_DISCOVER_STATUS_CARD_ACTIVE:
                    ptxCommon_PrintF("Card activated ... OK!\n");
                    ptxIoTRdInt_Get_Card_Details(cardRegistry, cardRegistry->ActiveCard, 1);
                    *demoState = IoTRd_DemoState_DataExchange;
                    skip_discovery_ongoing_print = 0;
                    break;

                case RF_DISCOVER_STATUS_DISCOVER_RUNNING:
                    if (0 == skip_discovery_ongoing_print)
                    {
                        ptxCommon_PrintF("Multiple Card(s) detected - resolving ... !\n");
                        skip_discovery_ongoing_print = 1u;
                    }
                    break;

                case RF_DISCOVER_STATUS_DISCOVER_DONE:
                    ptxCommon_PrintF("Multiple Card(s) detected - resolved ... OK!\n");
                    for (uint8_t i = 0; i < cardRegistry->NrCards; i++)
                    {
                        ptxIoTRdInt_Get_Card_Details(cardRegistry, &cardRegistry->Cards[i], (uint8_t)(1u + i));
                    }
                    *demoState = IoTRd_DemoState_SelectCard;
                    skip_discovery_ongoing_print = 0;
                    break;

                default:
                    /* Unknown Discover-Status */
                    break;
            }
        }
    } else
    {
        ptxCommon_PrintF("Multiple Card(s) detected - could not resolve ... FAILED!\n");
    }
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *exitLoop)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry) && (NULL != demoState) && (NULL != exitLoop))
    {
        switch (cardRegistry->Cards[0].TechType)
        {
            case Tech_TypeA:
                st = ptxIoTRdInt_DemoState_SelectCard_TypeA(iotRd, cardRegistry);
                break;

            case Tech_TypeB:
                st = ptxIoTRdInt_DemoState_SelectCard_TypeB(iotRd, cardRegistry);
                break;

            case Tech_TypeF:
                st = ptxIoTRdInt_DemoState_SelectCard_TypeF(iotRd, cardRegistry);
                break;

            case Tech_TypeV:
                st = ptxIoTRdInt_DemoState_SelectCard_TypeV(iotRd, cardRegistry);
                break;

            case Tech_TypeExtension:
                st = ptxIoTRdInt_DemoState_SelectCard_TypeExtension(iotRd, cardRegistry);
                break;

            default:
                /* not possible - exist anyway */
                *exitLoop = 1u;
                break;
        }

        if (0 == *exitLoop)
        {
            if (st == ptxStatus_Success)
            {
                ptxCommon_PrintF(" ... OK!\n");
                ptxIoTRdInt_Get_Card_Details(cardRegistry, cardRegistry->ActiveCard, 1);
                *demoState = IoTRd_DemoState_DataExchange;
            }
            else
            {
                ptxCommon_PrintF(" ... ERROR!\n");
                *demoState = IoTRd_DemoState_DeactivateReader;
            }
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_DeactivateReader(ptxIoTRd_t *iotRd, ptxIotRdInt_Demo_State_t *demoState, uint8_t *exitLoop)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != demoState) && (NULL != exitLoop))
    {
        st = ptxIoTRd_Reader_Deactivation(iotRd, PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER);
        ptxCommon_PrintStatusMessage("Restarting RF-Discovery", st);
        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("Waiting for discovered Cards ...\n");
            *demoState = IoTRd_DemoState_WaitForActivation;
        }
        else
        {
            *exitLoop = 1u;
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

void ptxIoTRdInt_DemoState_SystemError(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *systemState)
{
    if ((NULL != iotRd) && (NULL != cardRegistry) && (NULL != demoState) && (NULL != systemState))
    {
        switch (*systemState)
        {
            case PTX_SYSTEM_STATUS_ERR_OVERCURRENT:
                ptxCommon_PrintF("Error - Critical System-Error (Overcurrent) occurred - Quit Application\n");
                break;

            case PTX_SYSTEM_STATUS_ERR_TEMPERATURE:
                ptxCommon_PrintF("Error - Critical System-Error (Temperature) occurred - Quit Application\n");
                break;

            default:
                /* shall never happen */
                break;
        }

        /* reset the system and quit*/
        (void)ptxIoTRd_SWReset(iotRd);

        /* Application shall stop here as it's not useful to continue until the reason of the
         * system-error was found and fixed
         * */
        while (1);
    }
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeA(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry))
    {
        /* check supported protocols */

        /* NFC-DEP ? */
        if (0 != (cardRegistry->Cards[0].TechParams.CardAParams.SEL_RES & 0x40u))
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == NFC_DEP)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_NFCDEP);
        }
        /* ISO-DEP */
        else if (0 != (cardRegistry->Cards[0].TechParams.CardAParams.SEL_RES & 0x20u))
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == ISO_DEP)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_ISODEP);
        }
        else
        {
            /* T2T/MIFARE */
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == T2T)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_T2T);
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeB(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry))
    {
        /* check supported protocols */
        /* ISO-DEP ? */
        if (0 != (cardRegistry->Cards[0].TechParams.CardBParams.SENSB_RES[10] & 0x01u))
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == ISO_DEP)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_ISODEP);
        }
        else
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == Undefined)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_Undefined);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeF(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry))
    {
        /* check supported protocols */
        /* NFC-DEP ? */
        if ((0x01u == cardRegistry->Cards[0].TechParams.CardFParams.SENSF_RES[0]) &&
            (0xFEu == cardRegistry->Cards[0].TechParams.CardFParams.SENSF_RES[1]))
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == NFC_DEP)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_NFCDEP);
        }
        else
        {
            ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == T3T)... ");
            st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_T3T);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeV(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry))
    {
        /* Type-V only supports T5T protocol */
        ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == T5T)... ");
        st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_T5T);
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeExtension(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry)
{
    ptxStatus_t st = ptxStatus_Success;
    if ((NULL != iotRd) && (NULL != cardRegistry))
    {
        ptxCommon_PrintF("Selecting first detected card/protocol (RF-Protocol == Extension)... ");
        st = ptxIoTRd_Activate_Card(iotRd, &cardRegistry->Cards[0], Prot_Extension);
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxIoTRdInt_DemoState_HostCardEmulation(ptxIotRdInt_Demo_State_t *demoState, ptxHce_t *hce, ptxT4T_t *t4t)
{
    ptxStatus_t st = ptxStatus_Success;
    uint8_t exit_loop = 1;

    if ((NULL != demoState) && (NULL != hce) && (NULL != t4t))
    {
        /*
         * Consume all pending events as fast as possible.
         * Stay in HCE due to exclusivity of poll/listen.
         **/
         st = ptxHce_EmulateT4T(hce, t4t, &exit_loop);

         *demoState = IoTRd_DemoState_WaitForActivation;
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }

    return st;
}

