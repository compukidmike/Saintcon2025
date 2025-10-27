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
    File        : ptxNSC_Intf.h

    Description :
*/

/**
 * \addtogroup grp_ptx_api_nsc_hal PTX NSC Stack HAL
 *
 * @{
 */

#ifndef COMPS_NSC_PTXNSC_INTF_H_
#define COMPS_NSC_PTXNSC_INTF_H_


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * ####################################################################################################################
 * DEFINES / TYPES / INTERNALS
 * ####################################################################################################################
 */

/**
 * Forward declaration of NSC Component
 */
struct ptxNSC;


/*
 * ####################################################################################################################
 * FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Asynchronous reception callback.
 *
 * This function is called from ISR. Typical usage e.g. in GPIO IRQ for SPI, or in Rx ISR for UART.
 * It performs reading of PTX buffer to get NSC notification data.
 *
 * \param[in]   nscCtx           Pointer to NSC Context.
 *
 * \return No status returned.
 */
void ptxNSC_GetRx (struct ptxNSC *nscCtx);


#if defined (PTX_INTF_UART)
/**
 * \brief Set Rx control parameters to initial values.
 *
 * This is typically called before starting Tx operation, so that asynchronous Rx operation after Tx could
 * start with expected state.
 *
 * \param[in]   nscCtx           Pointer to NSC Context.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_UARTSetCleanStateRx (struct ptxNSC *nscCtx);

/**
 * \brief Synchronize host and PTX communication interface speed.
 *
 * This is typically called before the call to reset NSC. There might be the case where the host UART interface
 * changes speed although the speed on NSC chip UART interface has not changed. Use this API to set proper speed on
 * the host interface in order to continue with NSC initialization.
 *
 * \param[in]   nscCtx           Pointer to NSC Context.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_UARTComSync(struct ptxNSC *nscCtx);

/**
 * \brief Set default UART speed for the host.
 *
 * This is typically called after NSC reset has been performed.
 * After reset the host interface will operate on the default UART baudrate. However, there might happen that
 * after the call to \ref ptxNSC_UartComSync host UART speed is not the default one.
 * Call this API then to set the default UART speed.
 *
 * \param[in]   nscCtx           Pointer to NSC Context.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_UARTSetDefaultSpeed(struct ptxNSC *nscCtx);

#endif


#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

