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
    File        : ptxHce.h

    Description : HCE Application Programmers Interface (API)
*/

/**
 * \addtogroup grp_ptx_api_hce PTX HCE API
 *
 * @{
 */

#ifndef COMPS_HCE_PTXHCE_H_
#define COMPS_HCE_PTXHCE_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include "ptxNSC_Event.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DATA DEFINITIONS
 * ####################################################################################################################
 */

/*
 * Forward declarations.
 */
struct ptxPlat;
struct ptxNSC;
struct ptxNSC_Event;

/**
 * \brief HCE RF-Discovery Configuration for Listen-Mode
 */
typedef struct
{
    /** RF-Discovery Loop. Enable Type A technology. */
    uint8_t     ListenTypeA;

    /*
     * RFU
     */

} ptxHce_DiscConfig_t;

/**
 * \brief HCE RF-Protocol Type(s)
 */
typedef enum
{
    HceRfProt_Undefined = 0x00,
    HceRfProt_T2T       = 0x02,
    HceRfProt_IsoDep    = 0x04

} ptxHce_RfProtocol_t;

/**
 * \brief HCE Event Queue Management (Dimensions)
 */
typedef enum
{
    e_MaxNrOfEventRecords           =  32,
    e_SizeOfShortMsgBuffer          =   3,  /* pay-load of medium size data message buffers. */
    e_SizeOfActivationBuffer        =  16,  /* pay-load of medium size data message buffers. */

} ptxHce_EventQueueDimensions_t;

/**
 * \brief HCE Event Queue Management (Ranges)
 */
typedef enum
{
    e_EvtRecordIndexMin = 0,
    e_EvtRecordIndexMax = e_MaxNrOfEventRecords-1,
    e_EvtMaxNrOfRecords,
    e_EvtRecIndexNotSet

} ptxHce_EventQueueRanges_t;

/**
 * \brief HCE Event Identifiers
 */
typedef enum ptxHce_EventType
{
    HceEvent_ExtFieldOn,
    HceEvent_ExtFieldOff,
    HceEvent_Activated_ListenA,
    HceEvent_Data,
    HceEvent_Deactivated,
    HceEvent_NoEvent,

} ptxHce_EventType_t;

/**
 * \brief HCE Event Record Type
 */
typedef struct
{
    ptxHce_EventType_t          EventID;        /**< Event ID */
    uint8_t                     RxMsgDataShortMsgData[e_SizeOfShortMsgBuffer];      /**< Rx message data short msg data */
    uint8_t                    *RxMsgData;      /**< Rx Message data buffer */
    uint32_t                    RxMsgDataLen;   /**< Rx Message data length */

} ptxHce_EventRecord_t;

/**
 * \brief HCE Event Queue
 */
typedef struct
{
    ptxHce_EventQueueRanges_t    NrOfEntries;   /**< Number of entries */
    ptxHce_EventQueueRanges_t    HeadIndex;     /**< Head Index */
    ptxHce_EventQueueRanges_t    TailIndex;     /**< Tail Index */
    ptxHce_EventQueueRanges_t    MaxNrOfEntries;    /**< Maximum number of entries */

    ptxHce_EventRecord_t         Entries[e_MaxNrOfEventRecords]; /**< Event entries */

} ptxHce_EventQueue_t;

/**
 * \brief HCE Main Structure (Context).
 */
typedef struct ptxHce
{
    ptxStatus_Comps_t       CompId;             /**< Component Id. */
    struct ptxPlat          *Plat;              /**< Reference to Plat Component. */
    struct ptxNSC           *Nsc;               /**< Reference to Nsc Component.*/
    ptxHce_EventQueue_t     EventQ;             /**< Event queue */

    uint8_t                 ActivationData[e_SizeOfActivationBuffer];   /**< Activation Data */
    uint8_t                 *AppRxBuffer;       /**< Application Rx Buffer */
    uint32_t                AppRxBufferSize;    /**< Application Rx Buffer size */
    uint32_t                AppRxBufferIndex;   /**< Application Rx Buffer index */

    uint8_t                 DiscoverState;      /**< Discover State */

} ptxHce_t;

/**
 * \name NSC DEACTIVATE-Reasons.
 * @{
 */
#define PTX_HCE_DEACTIVATE_REASON_DESELECT      (uint8_t)0x01       /**< HCE Deactivate reason: Deselect */
#define PTX_HCE_DEACTIVATE_REASON_RELEASE       (uint8_t)0x02       /**< HCE Deactivate reason: Release */
#define PTX_HCE_DEACTIVATE_REASON_FIELD_OFF     (uint8_t)0x03       /**< HCE Deactivate reason: Field off */
/** @} */


/*
 * ####################################################################################################################
 * HCE API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief This function initializes the Host Card Emulation Component (HCE).
 * 
 * \param[in]   hce             Pointer to an allocated instance of the HCE component.
 * \param[in]   plat            Pointer to an initialized instance of the PLAT component.
 * \param[in]   nsc             Pointer to an initialized instance of the NSC component.
 * \param[in]   appRxBuffer     Pointer to an application buffer which is used to receive incoming RF-data.
 * \param[in]   appRxBufferSize Size of the application buffer.     
 * 
 * \return      Status          Indicating whether the operation was successful.
 *                              See \ref ptxStatus_t.
 */ 
ptxStatus_t ptxHce_Init ( ptxHce_t *hce, struct ptxPlat *plat, struct ptxNSC *nsc, uint8_t *appRxBuffer, uint16_t appRxBufferSize );


/**
 * \brief This function De-initializes the Host Card Emulation Component.
 *
 * \param[in]       hce         Pointer to an initialized instance of the HCE component.
 *
 * \return Status               Indicating whether the operation was successful.
 *                              See \ref ptxStatus_t.
 */
ptxStatus_t ptxHce_Deinit ( ptxHce_t *hce );

/**################################################################################
 * GET EVENT
 *
 * \brief This function allows the user to request latest event notification data
 * received from the PTX card emulation device.
 *
 * Events relevant to the HCE application are;
 * - a remote RF field has been detected or lost,
 * - a remote RF device has been detected (discovered),
 * - a remote RF device previously detected has now been activated,
 * - a remote RF device previously activated has now been deactivated,
 * - an error has occurred in the PTX subsystem,
 * - debug information regarding the NSC stack is available.
 * - an RF data message has been received.
 *
 * \param[in]      hce          Pointer to an initialized instance of the HCE component.
 * \param[in,out]  event        Reference to an event record supplied by the AP into which
 *                              event details can be entered.
 *
 * \return Status               Indicating whether the operation was successful.
 *                              See \ref ptxStatus_t.
 */
ptxStatus_t ptxHce_Get_Event ( ptxHce_t *hce, ptxHce_EventRecord_t **event );

/**################################################################################
 * SEND DATA
 *
 * \brief This function allows the currently activated card to send data to a
 *  nearby card reader.
 *
 * - this is not a blocking function, which means that it will return to the caller immediately after the transmit
 *   data has been configured for sending in the serial communications layer.
 *
 * \note In case the ISO-DEP protocol (according to ISO 14443-4) is used, the system takes internally care of the
 * timeout when the reader communicates with a card depending on the information which was exchanged during
 * activation (e.g. ATS for Type-A or ATQB for Type-B). In this case the caller has still to provide a meaningful
 * value (e.g. multiple seconds)! If a card reported a max. timeout of less than a second, the function will return
 * after this timeout has expired even if the timeout parameter has been set to e.g. 5 seconds.
 *
 * \param[in]   hce             Pointer to an initialized instance of the HCE component.
 * \param[in]   tx              User buffer containing the 'raw' RF data message to send.
 * \param[in]   txLength        Length of data to send which is the actual size of the transmit buffer.
 *
 * \return Status               Indicating whether the operation was successful.
 *                              See \ref ptxStatus_t.
 */
ptxStatus_t ptxHce_Send_Data ( ptxHce_t *hce, uint8_t *tx, uint32_t txLength );

/**
 * \brief Reserves an Event Record at the Tail of the Event Queue.
 * 
 * \param[in]       hce             Pointer to an initialized instance of the HCE component.
 * \param[in]       evtRecord       Reference to the Event Record to be reserved at the end of the Event Queue
 * \param[out]      evtRecordIndex  Index of the reserved event
 * 
 * \return Status                   Indicating whether the operation was successful.
 *                                  See \ref ptxStatus_t.
 */

ptxStatus_t ptxHce_ReserveEventRecord ( ptxHce_t *hce, ptxHce_EventRecord_t **evtRecord, uint16_t *evtRecordIndex );
/**
 * \brief Adds a new pending Event Record at the Tail of the Event Queue.
 * 
 * \param[in]       hce             Pointer to an initialized instance of the HCE component.
 * \param[out]      recordIndex     Pointer to the Index of the added Event
 * 
 * \return Status                   Indicating whether the operation was successful.
 *                                  See \ref ptxStatus_t.
 */
ptxStatus_t ptxHce_AddNewEventPending ( ptxHce_t *hce, uint16_t recordIndex );

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

