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
    Module      : Status
    File        : ptxStatus.h

    Description :
*/

/**
 * \addtogroup grp_ptx_def PTX NSC Stack Status Defines
 * @{
 */

#ifndef COMPS_PTXSTATUS_H_
#define COMPS_PTXSTATUS_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <stdint.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \brief Status Type
 *
 * Status of the operation consists of 2 parts: component ID \ref ptxStatus_Comps_t and status code \ref ptxStatus_Values_t.
 * Component ID defines which stack component is the owner of the operation. Status code holds actual operation result/status information.
 */
typedef uint16_t    ptxStatus_t;

/**
 * \brief Status Code Definitions
 */
typedef enum ptxStatus_Values
{
    ptxStatus_Success_Internal,     /**< \internal The operation completed successfully. Do not specify a component. */
    ptxStatus_InvalidParameter,     /**< The caller has provided invalid value(s) for function parameter(s). */
    ptxStatus_InternalError,        /**< There has been internal error in the function processing */
    ptxStatus_Reserved_0x03,        /**< Don't use. Value reserved */
    ptxStatus_NotImplemented,       /**< The function/command is not implemented. */
    ptxStatus_TimeOut,              /**< The operation has timed out. */
    ptxStatus_InsufficientResources,/**< Insufficient resources available to complete the request; e.g. out of memory. */
    ptxStatus_Reserved_0x07,        /**< Don't use. Value reserved */
    ptxStatus_InterfaceError,       /**< The interface (I/O line, UART, ...) is not accessible or an error has occurred. */
    ptxStatus_NscProtocolError,     /**< Error at NSC protocol */
    /* */
    ptxStatus_NoDataAvailable,      /**< Data or Event not available */
    ptxStatus_NotPermitted,         /**< The operation is not permitted. */
    ptxStatus_InvalidState,         /**< The component state is invalid or the operation requested cannot be performed in the current state. */
    ptxStatus_Reserved_0x0D,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x0E,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x0F,        /**< Don't use. Value reserved */
    /* */

    ptxStatus_Reserved_0x10,        /**< Don't use. Value reserved */
    ptxStatus_NscRfError,           /**< RF-Error */
    ptxStatus_NotSupported,         /**< Feature not supported / allowed in particular scenario */
    ptxStatus_SysErrOvertemperature,/**< Critical system error: over-temperature. */
    ptxStatus_AssertionError,       /**< An assertion has failed. */
    ptxStatus_AccessDenied,         /**< No access to the resource. Use rather for access-rights related cases.*/
    ptxStatus_ACKError,             /**< The request has not been acknowledged or a negative Acknowledge response was received. */
    ptxStatus_SysErrOvercurrent,    /**< Critical system error: over-current. */
    ptxStatus_ProtocolError,        /**< General Protocol Error */
    ptxStatus_ResourceNotFound,     /**< Requested information or resource not available */
    ptxStatus_NotFound,             /**< Entry not found, e.g.: "No such file or directory". */
    ptxStatus_BufferTooSmall,       /**< Buffer to store data not large enough */
    ptxStatus_MFCC_NAK_Received,    /**< NAK-received in MFCC-mode */
    ptxStatus_MFCC_AUTHENT_Failed,  /**< Authentication failed in MFCC-mode */
    ptxStatus_T3T_WriteFlagSet,     /**< Write flag set to ON while trying to read */

    /* */
    ptxStatus_Reserved_0x1A,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x1B,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x1C,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x1D,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x1E,        /**< Don't use. Value reserved */
    ptxStatus_Reserved_0x1F,        /**< Don't use. Value reserved */

    /* ... */
    ptxStatus_MAX                   /**< Maximum count. */

} ptxStatus_Values_t;

/**
 * \brief Component Definitions
 *
 * Common usage of component definitions:
 *  => Create \ref ptxStatus_t type
 *  => Inside function calls with \ref PTX_COMP_CHECK to check that component is not NULL and that it is set as expected
 *     i.e. properly initialized.
 */
typedef enum ptxStatus_Comps
{
    ptxStatus_Comp_None,               /**< No component specified. */
    ptxStatus_Comp_NSC,                /**< NSC Component.  */
    ptxStatus_Comp_POS,                /**< POS. EMVCo Component. */
    ptxStatus_Comp_PLAT,               /**< Platform Dependent Component. */
    ptxStatus_Comp_IoTReader,          /**< IOT Reader component. */

    ptxStatus_Comp_IOTHce,             /**< IOT Host Card Emulation Component. */
    ptxStatus_Comp_TCP,                /**< Tag Communication Protocol Component. */
    ptxStatus_Comp_WLCE,               /**< Wireless Charging Poller Exclusive Component. */
    ptxStatus_Comp_WLCE_PERIPH,        /**< Wireless Charging Poller Exclusive Peripherals Component. */
    ptxStatus_Comp_WLCN,               /**< Wireless Charging Poller NFC Forum Component. */
    ptxStatus_Comp_WLCN_PERIPH,        /**< Wireless Charging Poller NFC Forum Peripherals Component. */
    ptxStatus_Comp_WPT,                /**< Wireless Power Transfer Component. */
    ptxStatus_Comp_PERIPH,             /**< Peripherals Component. */

    ptxStatus_Comp_Reserved_01,        /**< free */
    ptxStatus_Comp_NativeTag_T2T,      /**< T2T Native Tag */
    ptxStatus_Comp_NativeTag_T3T,      /**< T3T Native Tag */
    ptxStatus_Comp_NativeTag_T4T,      /**< T4T Native Tag */
    ptxStatus_Comp_NativeTag_T5T,      /**< T5T Native Tag */

    ptxStatus_Comp_NativeTag_MFCC,     /**< Mifare Classic Compatibility Native Tag*/

    ptxStatus_Comp_Reserved_02,        /**< free */
    ptxStatus_Comp_T2TOP,              /**< T2T Operation Component */
    ptxStatus_Comp_T3TOP,              /**< T3T Operation Component */
    ptxStatus_Comp_T4TOP,              /**< T4T Operation Component */
    ptxStatus_Comp_T5TOP,              /**< T5T Operation Component */

    ptxStatus_Comp_NDEF,               /**< NDEF Component */
    ptxStatus_Comp_WLCD,               /**< Wireless Charger Listener Discrete Component. */
    ptxStatus_Comp_PMU,                /**< WLC Power Management Unit Component. */

    ptxStatus_Comp_REMOTE,             /**< Remote Interface */

    ptxStatus_Comp_WLCD_PERIPH,        /**< Wireless Charging Listener Discrete Peripherals Component. */

    ptxStatus_Comp_SUPPORT,            /**< Internal Test Component */

    ptxStatus_Comp_WLCE_ALPHA,         /**< Wireless Charging Poller Exclusive Alpha Demo. */

    ptxStatus_Comp_GPIO,               /**< GPIO Component */

    ptxStatus_Comp_FELICA_DTE,         /**< FELICA-DTE Component */

    ptxStatus_Comp_RF_TEST,            /**< RF-TEST Component */

    ptxStatus_Comp_FELICA,             /**< FELICA Component (RFU) */

    ptxStatus_Comp_TransparentMode,    /**< Transparent-Mode Component */

    ptxStatus_Comp_TDC,                /**< Transparent Data Channel (TDC)Component */
    
    ptxStatus_Comp_EXT,                /**< Extension Components (Prototype) */

    /* */
    ptxStatus_Comp_MAX                 /**< Maximum count. */

} ptxStatus_Comps_t;


/*
 * ####################################################################################################################
 * DEFINES / MACROS
 * ####################################################################################################################
 */

/**
 * \name Status Code Composition and Decomposition
 *
 * @{
 */

/**
 *   \brief Put together component ID and status into one value.
 *
 *   \note Use only for status values not equal to \ref ptxStatus_Success.
 *
 */
#define PTX_STATUS(_comp_, _st_)        ((ptxStatus_t)((((ptxStatus_t)((uint16_t)_comp_))<<8)|((ptxStatus_t)(((uint16_t)_st_)&0xFFu)))) /* */

/**
 *  \brief Get component ID from a combined status value.
 */
#define PTX_GET_COMP(_cst_)             ((ptxStatus_Comps_t)((uint8_t)((_cst_)>>8))) /* */

/**
 *  \brief Get status code from a combined status value.
 */
#define PTX_GET_STATUS(_cst_)           ((ptxStatus_Values_t)((uint8_t)((_cst_)))) /* */

 /**
  * \brief Status value composition for SUCCESSFUL operation.
  */
#define ptxStatus_Success               PTX_STATUS(ptxStatus_Comp_None,ptxStatus_Success_Internal) /* */

/** @} */

/**
 * \name Component check
 *
 * @{
 */

/**
 * \brief Check whether the component is not NULL and actually what is appears to be.
 */
#define PTX_COMP_CHECK(ptx_p_ctx, ptx_p_comp_id_) \
    ((NULL!=ptx_p_ctx)&&(ptx_p_comp_id_==ptx_p_ctx->CompId)) /* */

/** @} */



#endif /* Guard */

/** @} */

