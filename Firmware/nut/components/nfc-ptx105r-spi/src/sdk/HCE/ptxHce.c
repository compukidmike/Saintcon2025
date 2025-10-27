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
    Module      : Host Card Emulation (HCE)
    File        : ptxHce.c

    Description : HCE Application Programmers Interface (API)
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxHce.h"

#include "ptxStatus.h"
#include "ptxPLAT.h"
#include "ptxNSC.h"
#include "ptxNSC_System.h"
#include "ptxNSC_Hal.h"
#include "ptxNSC_CE.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxHce_ReadFromTail ( ptxHce_t *hce, uint16_t *recordIndex );
static ptxStatus_t ptxHce_ReadFromHead ( ptxHce_t *hce, uint16_t *recordIndex );
static ptxStatus_t ptxHce_AddToTail ( ptxHce_t *hce, uint16_t recordIndex );
static ptxStatus_t ptxHce_RemoveFromHead ( ptxHce_t *hce, uint16_t *recordIndex );
static ptxStatus_t ptxHce_ReadNextEvent ( ptxHce_t *hce, ptxHce_EventRecord_t **evtRecord, uint16_t *indexOfRecordRead );
static ptxStatus_t ptxHce_ReleaseEvent ( ptxHce_t *hce, ptxHce_EventRecord_t *evtRecord, uint16_t recordIndex );
static ptxStatus_t ptxHce_InitEventQueue ( ptxHce_t *hce );

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxHce_Init ( ptxHce_t *hce, struct ptxPlat *plat, struct ptxNSC *nsc, uint8_t *appRxBuffer, uint16_t appRxBufferSize )
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != hce) && (NULL != appRxBuffer) && (0 != appRxBufferSize) && (NULL != plat) && (NULL != nsc))
    {
        /* Initialize IOT HCE Component. */
        (void)memset (hce, 0, sizeof(ptxHce_t));
        hce->CompId = ptxStatus_Comp_IOTHce;
        hce->AppRxBuffer = appRxBuffer;
        hce->AppRxBufferSize = appRxBufferSize;

        hce->Plat = plat;
        hce->Nsc = nsc;

        if (ptxStatus_Success == st)
        {
            /* Setup default routing table (all RF-technologies to host) */
            st = ptxNSC_Set_Listen_RoutingTable_HCE(hce->Nsc);
        }

        if (ptxStatus_Success == st)
        {
            st = ptxHce_InitEventQueue(hce);
        }

    } else
    {
        st = PTX_STATUS (ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }
    return st;
}

ptxStatus_t ptxHce_Deinit ( ptxHce_t *hce )
{
    ptxStatus_t st = ptxStatus_Success;

    if (NULL != hce)
    {
        hce->Plat = NULL;
        hce->Nsc = NULL;

        hce->AppRxBuffer = NULL;
        hce->AppRxBufferSize = 0;
    } else
    {
        st = PTX_STATUS (ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;
}

ptxStatus_t ptxHce_Send_Data (ptxHce_t *hce, uint8_t *tx, uint32_t txLength)
{
    ptxStatus_t st = ptxStatus_Success;

    if (PTX_COMP_CHECK(hce, ptxStatus_Comp_IOTHce) && (tx != NULL) && (txLength > 0))
    {
        /* Num of bytes pending to be transfered. */
        uint32_t txLength_pending = txLength;
        /* Num of bytes already sent. */
        uint32_t txLength_sent = 0;

        uint32_t nsc_max_transfer_unit = 0;
        (void) ptxNSC_Get_Mtu(hce->Nsc, &nsc_max_transfer_unit);

        /* Tx Rf Data. */
        do
        {
            if (txLength_pending > nsc_max_transfer_unit)
            {
                /* Chaining needed. */
                st = ptxNSC_RfDataMsgTx(hce->Nsc, &tx[txLength_sent], (size_t) nsc_max_transfer_unit, 1u);

                txLength_pending -= nsc_max_transfer_unit;
                txLength_sent += nsc_max_transfer_unit;
            } else
            {
                /* No Chaining needed. */
                st = ptxNSC_RfDataMsgTx(hce->Nsc, &tx[txLength_sent], (size_t) txLength_pending, 0);
                txLength_pending = 0;
            }

        } while((ptxStatus_Success == st) && (txLength_pending > 0));
    }
    return st;

}

ptxStatus_t ptxHce_Get_Event ( ptxHce_t *hce, ptxHce_EventRecord_t **event )
{
    ptxStatus_t st = ptxStatus_Success;
    uint16_t record_index = 0;

    if (PTX_COMP_CHECK(hce, ptxStatus_Comp_IOTHce))
    {
        /* Let's check first if there has been anything received. */
        (void)ptxPLAT_TriggerRx(hce->Plat);

        /* Get the next event currently pending in the event queue. This event is currently
         * the oldest event pending and is positioned at the end (tail) of the queue. */
        st = ptxHce_ReadNextEvent(hce, event, &record_index);

        if (ptxStatus_Success == st)
        {
            st = ptxHce_ReleaseEvent(hce, *event, record_index);
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxIoTHce_Get_Event */


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxHce_InitEventQueue ( ptxHce_t *hce )
{
    ptxStatus_t st = ptxStatus_Success;

    if (hce != NULL)
    {
        /* Save and disable CPU interrupts. */
        ptxPLAT_DisableInterrupts (hce->Plat);

        hce->EventQ.NrOfEntries = 0;
        hce->EventQ.HeadIndex = e_EvtRecordIndexMin;
        hce->EventQ.TailIndex = e_EvtRecordIndexMin;
        hce->EventQ.MaxNrOfEntries = e_EvtMaxNrOfRecords;

        /* Restore original CPU interrupt state. */
        ptxPLAT_EnableInterrupts (hce->Plat);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_InitEventQueue */

static ptxStatus_t ptxHce_ReadFromTail ( ptxHce_t *hce, uint16_t *recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;

    if ((hce != NULL) && (recordIndex != NULL))
    {
        /* Save and disable CPU interrupts. */
        ptxPLAT_DisableInterrupts (hce->Plat);

        /* Check that the event queue is not full? */
        if (hce->EventQ.NrOfEntries < hce->EventQ.MaxNrOfEntries)
        {
            /* Get the index of the event record currently at the tail of the queue. */
            *recordIndex = hce->EventQ.TailIndex;

        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InternalError);
        }

        /* Restore original CPU interrupt state. */
        ptxPLAT_EnableInterrupts (hce->Plat);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_ReadFromTail */


static ptxStatus_t ptxHce_ReadFromHead ( ptxHce_t *hce, uint16_t *recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;

    if ((hce != NULL) && (recordIndex != NULL))
    {
        /* Save and disable CPU interrupts. */
        ptxPLAT_DisableInterrupts(hce->Plat);

        /* Check that the event queue is not empty? */
        if (0 != hce->EventQ.NrOfEntries)
        {
            /* Get the index of the event record currently at the head of the queue. */
            *recordIndex = hce->EventQ.HeadIndex;
        }
        else
        {
            st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_NoDataAvailable);
        }

        /* Restore original CPU interrupt state. */
        ptxPLAT_EnableInterrupts(hce->Plat);

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_ReadFromHead */

static ptxStatus_t ptxHce_AddToTail ( ptxHce_t *hce, uint16_t recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;

    if (hce != NULL)
    {
        /* Save and disable CPU interrupts. */
        ptxPLAT_DisableInterrupts(hce->Plat);

        /* Check that the event queue is not full? */
        if (hce->EventQ.NrOfEntries < hce->EventQ.MaxNrOfEntries)
        {
            /* Check that the user is referring to the current and valid event record
             * at the end (tail) of the queue. This test assumes that ptxNSC_ReserveEventRecord
             * had been called prior to this action. */
            if (recordIndex == hce->EventQ.TailIndex)
            {
                /* all ok */
                if (hce->EventQ.TailIndex == 0 )
                {  /* then at the beginning of the array of records. */
                    hce->EventQ.TailIndex = (hce->EventQ.MaxNrOfEntries) - 1;
                }
                else
                {
                    hce->EventQ.TailIndex--;
                }

                hce->EventQ.NrOfEntries++;

            } else
            {
                st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InternalError);
            }
        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InternalError);
        }

        /* Restore original CPU interrupt state. */
        ptxPLAT_EnableInterrupts(hce->Plat) ;

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_AddToTail */


static ptxStatus_t ptxHce_RemoveFromHead ( ptxHce_t *hce, uint16_t *recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;

    if ((hce != NULL) && (recordIndex != NULL))
    {
        /* Save and disable CPU interrupts. */
        ptxPLAT_DisableInterrupts(hce->Plat);

        /* Check that the event queue is not empty? */
        if (0 != hce->EventQ.NrOfEntries)
        {
            *recordIndex = hce->EventQ.HeadIndex;

            if ( hce->EventQ.HeadIndex == 0 )
            {
                /* then head at the beginning of the array of records. */
                hce->EventQ.HeadIndex = (hce->EventQ.MaxNrOfEntries) - 1;
            } else
            {
                hce->EventQ.HeadIndex--;
            }

            hce->EventQ.NrOfEntries--;
        }

        /* Restore original CPU interrupt state. */
        ptxPLAT_EnableInterrupts(hce->Plat) ;

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_RemoveFromHead */

ptxStatus_t ptxHce_ReserveEventRecord ( ptxHce_t *hce, ptxHce_EventRecord_t **evtRecord, uint16_t *recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;
    uint16_t record_index;
    ptxHce_EventRecord_t *evt_record;

    if ((hce != NULL) && (recordIndex != NULL))
    {
        st = ptxHce_ReadFromTail(hce, &record_index);

        if (ptxStatus_Success == st)
        {
            evt_record = &hce->EventQ.Entries[record_index];
            *evtRecord = evt_record;
            *recordIndex = record_index;

            /* Initialise event record to a known state. */
            evt_record->EventID        = HceEvent_NoEvent;
            evt_record->RxMsgData      = NULL;
            evt_record->RxMsgDataLen   = 0;
            (void)memset(&evt_record->RxMsgDataShortMsgData[0], 0xFF, e_SizeOfShortMsgBuffer);
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;
} /* END ptxNSC_ReserveEventRecord */

/*
################################################################################################
*
*   Title:          Add New Event Pending
*
*   Description:    Adds an event record to the end (tail) of the event queue.
*                   Event record should have been previously 'reserved' prior filling with event
*                   data and appending to the end of a queue. This record becomes the latest and
*                   'newest' event pending.
*
*   Interface:      Add New Event Pending ( queue identity, event record index ) response
*
*       where       response = [ 0=success ]
*
################################################################################################
*/
ptxStatus_t ptxHce_AddNewEventPending ( ptxHce_t *hce, uint16_t recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;

    st = ptxHce_AddToTail(hce, recordIndex);

    return st;

} /* END ptxHce_AddNewEventPending */


/*
################################################################################################
*
*   Title:              Read Next Event Pending
*
*   Description:        Non destructive read from the front (head) of the queue. That is, the
*                       oldest event record pending is read.
*
*                       Reads the event record at the head of the queue, which is the oldest
*                       currently pending event. Non destructive read so the event is not
*                       actually removed.
*
*   Interface:          Read Next Event Pending ( queue identity ) read record index
*
################################################################################################
*/
static ptxStatus_t ptxHce_ReadNextEvent ( ptxHce_t *hce, ptxHce_EventRecord_t **evtRecord, uint16_t *indexOfRecordRead )
{
    ptxStatus_t st = ptxStatus_Success;
    uint16_t record_index;

    st = ptxHce_ReadFromHead(hce, &record_index);

    if (ptxStatus_Success == st)
    {
        *evtRecord = &hce->EventQ.Entries[record_index];
        *indexOfRecordRead = record_index;
    }

    return st;

} /* END ptxHce_ReadNextEvent */


/*
################################################################################################
*
*   Title:          Release Event Record
*
*   Description:    Release an event record from the event 'pending' queue. This removes the
*                   current record at the front (head) of the queue, which is the oldest pending
*                   event in the queue.
*
*   Interface:      Release Event Pending ( queue identity ) deleted record index
*
################################################################################################
*/
static ptxStatus_t ptxHce_ReleaseEvent ( ptxHce_t *hce, ptxHce_EventRecord_t *evtRecord, uint16_t recordIndex )
{
    ptxStatus_t st = ptxStatus_Success;
    uint16_t head_index;

    if ((hce != NULL) && (evtRecord != NULL))
    {
        /* Verify that the buffer reference information is valid */
        if (evtRecord == &hce->EventQ.Entries[recordIndex])
        {
            st = ptxHce_RemoveFromHead(hce, &head_index);
        } else
        {
            st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
        }

    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IOTHce, ptxStatus_InvalidParameter);
    }

    return st;

} /* END ptxHce_ReleaseEvent */


