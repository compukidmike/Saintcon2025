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
    File        : ptxIoTRd_COMMON.h

    Description : Common API for common functions.
*/

/**
 * \addtogroup grp_ptx_api_common Common API
 *
 * @{
 */

#ifndef APIS_PTX_IOTRD_COMMON_H_
#define APIS_PTX_IOTRD_COMMON_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxCOMMON.h"
#include "ptx_IOT_READER.h"
#include "ptxHce.h"
#include "ptxT4T.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * ####################################################################################################################
 * DEFINES
 * ####################################################################################################################
 */


/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * The list of possible IoT states for the demo application.
 */
typedef enum ptxIotRdInt_Demo_State
{
    IoTRd_DemoState_WaitForActivation,
    IoTRd_DemoState_DataExchange,
    IoTRd_DemoState_SelectCard,
    IoTRd_DemoState_DeactivateReader,
    IoTRd_DemoState_SystemError,
    IoTRd_DemoState_HostCardEmulation,
    IoTRd_DemoState_Undefined
}ptxIotRdInt_Demo_State_t;

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Get details of a card placed into RF field.
 */
void ptxIoTRdInt_Get_Card_Details(ptxIoTRd_CardRegistry_t *cardRegistry, ptxIoTRd_CardParams_t *cardParams, uint8_t nr);

/**
 * \brief Sleep function: wrapper for sleep functionality based on platform dependent timer.
 */
void ptxIoTRdInt_Sleep(ptxIoTRd_t *iotRd, uint32_t timeout);

/**
 * \brief Application Entry function.
 */ 
int ptxAPP_Entry(void);

/**
 * \brief Function representing demo state "wait for activation"
 */
void ptxIoTRdInt_DemoState_WaitForActivation(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState);

/**
 * \brief Function representing demo state "select card"
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *exitLoop);

/**
 * \brief Function representing demo state "deactivate reader"
 */
ptxStatus_t ptxIoTRdInt_DemoState_DeactivateReader(ptxIoTRd_t *iotRd, ptxIotRdInt_Demo_State_t *demoState, uint8_t *exitLoop);

/**
 * \brief Function representing demo state "system error"
 */
void ptxIoTRdInt_DemoState_SystemError(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *systemState);

/**
 * \brief Function representing demo state "select card", with card type A
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeA(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry);

/**
 * \brief Function representing demo state "select card", with card type B
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeB(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry);

/**
 * \brief Function representing demo state "select card", with card type F
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeF(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry);

/**
 * \brief Function representing demo state "select card", with card type V
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeV(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry);

/**
 * \brief Function representing demo state "select card", with card type Extension
 */
ptxStatus_t ptxIoTRdInt_DemoState_SelectCard_TypeExtension(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry);

/**
 * \brief Function representing demo state "host card emulation", emulating T4T
 */
ptxStatus_t ptxIoTRdInt_DemoState_HostCardEmulation(ptxIotRdInt_Demo_State_t *demoState, ptxHce_t *hce, ptxT4T_t *t4t );


#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */
