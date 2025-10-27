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
    Module      : HCE API
    File        : ptxHce_Exchange.c

    Description : Data Processing Function for HCE.
*/

/*
 * ####################################################################################################################
 * INCLUDES AND DEFINES
 * ####################################################################################################################
 */
#include "ptxHce_Exchange.h"
#include "ptxCOMMON.h"
#include "ptxStatus.h"

#if 0
#define PTX_HCE_DEACTIVATE_REASON_DESELECT  (0x00)
#define PTX_HCE_DEACTIVATE_REASON_FIELD_OFF (0x01)
#define PTX_HCE_DEACTIVATE_REASON_RELEASE   (0x02)
#endif

static ptxStatus_t ptxHce_HandleEvent(ptxHce_t *hce, ptxT4T_t *tag, ptxHce_EventRecord_t *event);
static ptxStatus_t ptxHce_ProcessData(ptxHce_t *hce, ptxT4T_t *tag, ptxHce_EventRecord_t *event);
static ptxStatus_t ptxHce_DeactivateTag(ptxHce_t *hce, ptxT4T_t *tag, uint8_t reason);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxHce_EmulateT4T (ptxHce_t *hce, ptxT4T_t *tag, uint8_t *exit_loop)
{
    ptxStatus_t st = ptxStatus_Success;
    ptxHce_EventRecord_t *hce_event = NULL;


    if ((NULL != hce) && (NULL != tag))
    {
        do
        {
            st = ptxHce_Get_Event(hce, &hce_event);

            switch(PTX_GET_STATUS(st))
            {
            case ptxStatus_Success:
                st = ptxHce_HandleEvent(hce, tag, hce_event);
                if (ptxStatus_Success != st)
                {
                    ptxCommon_PrintF("Status: %04x\n", st);
                }
                break;

            case ptxStatus_NoDataAvailable:
                break;

            default:
                break;
            }


        } while(!*exit_loop);
    }
    else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce,ptxStatus_InvalidParameter);
    }

    return st;
}

static ptxStatus_t ptxHce_HandleEvent(ptxHce_t *hce, ptxT4T_t *tag, ptxHce_EventRecord_t *event)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != hce) && (NULL != tag) && (NULL != event))
    {
        switch(event->EventID)
        {
        case HceEvent_ExtFieldOn:
            ptxCommon_PrintF("Event: Field ON\n");
            ptxT4T_Reset(tag);
            break;

        case HceEvent_ExtFieldOff:
            ptxCommon_PrintF("Event: Field OFF\n");
            ptxT4T_Reset(tag);
            break;

        case HceEvent_Activated_ListenA:
            ptxCommon_PrintF("Event: Activated LISTEN A\n");
            break;

        case HceEvent_Data:
            ptxCommon_PrintF("Event: DATA\n");
            st = ptxHce_ProcessData(hce, tag, event);
            break;

        case HceEvent_Deactivated:
            ptxCommon_PrintF("Event: DEACTIVATED\n");
            st = ptxHce_DeactivateTag(hce, tag, event->RxMsgData[0]);
            break;

        default:
            break;

        }
    }

    return st;
}

static ptxStatus_t ptxHce_ProcessData(ptxHce_t *hce, ptxT4T_t *tag, ptxHce_EventRecord_t *event)
{
    ptxStatus_t st = ptxStatus_Success;

    uint8_t txBufferSize = 255;
    uint8_t txBuffer[255];

    if ((NULL != hce) && (NULL != tag) && (NULL != event))
    {
        if (NULL != event->RxMsgData)
        {
            ptxCommon_PrintF("RX =");
            ptxCommon_Print_Buffer(event->RxMsgData, 0, event->RxMsgDataLen, 1, 0);

            ptxT4T_Process(tag, &event->RxMsgData[0], (uint8_t)event->RxMsgDataLen, &txBuffer[0], &txBufferSize);
        }
        else
        {
            st = ptxStatus_InvalidParameter;
        }

        if (0 < txBufferSize)
        {
            ptxCommon_PrintF("TX =");
            ptxCommon_Print_Buffer(txBuffer, 0, txBufferSize, 1, 0);

            st = ptxHce_Send_Data(hce, txBuffer, (uint16_t)txBufferSize);
        }
    }

    return st;
}

static ptxStatus_t ptxHce_DeactivateTag(ptxHce_t *hce, ptxT4T_t *tag, uint8_t reason)
{

    ptxStatus_t st = ptxStatus_Success;


    if ((NULL != hce) && (NULL != tag))
    {
        /*
         * Get DEACTIVATE-Reason
         *  => 0x01 = DESELECT received by Reader
         *  => 0x02 = RELEASE received by Reader
         *  => 0x03 = Field turned off by Reader
         */
        switch (reason)
        {
            /* optional - inform upper layers */
            case PTX_HCE_DEACTIVATE_REASON_DESELECT:
                ptxCommon_PrintF("DEACTIVATE Reason: Deselect\n");
                break;

            case PTX_HCE_DEACTIVATE_REASON_FIELD_OFF:
                ptxCommon_PrintF("DEACTIVATE Reason: Field OFF\n");
                break;

            case PTX_HCE_DEACTIVATE_REASON_RELEASE:
                ptxCommon_PrintF("DEACTIVATE Reason: Release\n");
                break;

            default:
                ptxCommon_PrintF("DEACTIVATE Reason: Unknown\n");
                break;
        }
    }

    return st;
}



