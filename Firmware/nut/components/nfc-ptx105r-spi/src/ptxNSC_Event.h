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
    File        : ptxNSC_Events.h

    Description :
*/

/**
 * \addtogroup grp_ptx_api_nsc_event PTX NSC Stack Events
 *
 * @{
 */

#ifndef COMPS_NSC_PTXNSC_EVENT_H_
#define COMPS_NSC_PTXNSC_EVENT_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \brief NSC Event types - events that reflect the current state of the NSC stack and that
 * are available to application programs via the ptx<product stack>_Get_Event API.
 */
typedef enum ptxNscEventId
{
    NoEvent,                                /**< No Event. */

    /* RF device activation events */
    NSC_EventRfAct_PassPoll_A,              /**< NSC_RF_ACTIVATE_NTF ( NFC-A passive poll mode ) */
    NSC_EventRfAct_PassPoll_B,              /**< NSC_RF_ACTIVATE_NTF ( NFC-B passive poll mode ) */
    NSC_EventRfAct_PassPoll_F,              /**< NSC_RF_ACTIVATE_NTF ( NFC-F passive poll mode ) */
    NSC_EventRfAct_PassPoll_V,              /**< NSC_RF_ACTIVATE_NTF ( NFC-V passive poll mode ) */
    NSC_EventRfAct_Extension,               /**< NSC_RF_ACTIVATE_NTF ( Extension (Prototype) ) */
    NSC_EventRfAct_PassListen_A,            /**< NSC_RF_ACTIVATE_NTF ( NFC-A passive listen mode ) */

    /* RF device discovery events */
    NSC_EventRfDisc_PassPoll_A,             /**< NSC_RF_DISCOVER_NTF ( NFC-A passive poll mode ) */
    NSC_EventRfDisc_PassPoll_B,             /**< NSC_RF_DISCOVER_NTF ( NFC-B passive poll mode ) */
    NSC_EventRfDisc_PassPoll_F,             /**< NSC_RF_DISCOVER_NTF ( NFC-F passive poll mode ) */
    NSC_EventRfDisc_PassPoll_V,             /**< NSC_RF_DISCOVER_NTF ( NFC-V passive poll mode ) */
    NSC_EventRfDisc_Extension,              /**< NSC_RF_DISCOVER_NTF ( Extension (Prototype) ) */
    NSC_EventRfDisc_LastOne,                /**< An empty NSC_RF_DISCOVER_NTF (without RF_TECH and RF_TECH specific
                                             parameters) indicates the end of RF resolution process. */

    /* RF device error events */
    NSC_EventError,                         /**< NSC_ERROR_NTF ( Error ) */

    /* LPCD mechanism detects a device */
    NSC_LPCDTrigered,                       /**< NSC_RF_LPCD_NTF */

    /* RF Timeout Error. */
    NSC_RfTimeOutError,                    /**< NSC_ERROR_NTF ( Error ) */

    /*RF device deactivation events */
    NSC_EventRfDeact,                       /**< NSC_RF_DEACTIVATE_NTF ( RF device deactivation ) */

    /* RF data message events */
    NSC_Event_NfcDataMsg,                   /**< NSC_RF_MSG ( NFC transparent data (raw) message unchained) */
    NSC_Event_NfcCltMsg,                    /**< NSC_RF_CLT_MSG  */
    NSC_Event_NfcDataMsg_Chained,           /**< NSC_RF_MSG_CHAINED ( NFC transparent data (raw) message chained) */
    NSC_Event_RfCtr_ACK,                    /**< NSC_RFD_CTRL. Acknowledge (ACK) for ISO-DEP only.  */
    NSC_Event_RfCtr_AttCmd,                 /**< NSC_RFD_CTRL. Attention (ATN) for NFC-DEP only.  */

    /* RF field status change events */
    NSC_EventRfField_on,                    /**< NSC_RF_FIELD_NTF ( RF field on ) */
    NSC_EventRfField_off,                   /**< NSC_RF_FIELD_NTF ( RF field off ) */

    /* NSC debug events */
    NSC_EventDebug,                         /**< NSC_DEBUG_NTF ( debug ) */

    UnspecifiedEvent                        /**< Unknown Event. */

} ptxNscEventId_t;

/**
 * \brief NSC event information
 */ 
typedef struct ptxNSC_Event
{
    /* OLD NSC event data used past implementations. Used to notify layer above, what kind of
     * asynchronous event had happened on the NSC Component. */
    ptxNscEventId_t          EventId;                           /**< Type of event to handle. */
    uint8_t                  *Buff;                             /**< Buffer for Event. */
    size_t                   BuffLen;                           /**< Length of Buffer for Event. */
    uint8_t                  *BuffSecondary;                    /**< Buffer for Event. */
    size_t                   BuffSecondaryLen;                  /**< Length of Buffer for Event. */
    size_t                   NumTotalBitsSecondary;             /**< Number of bits provided in /ref payload plus /ref payloadSecondary. */
    uint8_t                  RfTechMode;                        /**< Current RF-Technology & -Mode for particular event (only applicable for certain events). */
} ptxNSC_Event_t;

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

