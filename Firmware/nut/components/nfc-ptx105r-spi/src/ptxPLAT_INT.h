/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



   THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX105R Arduino
    Module      : PLAT
    File        : ptxPLAT_INT.h

    Description :
*/

#pragma once

#include "ptxPLAT.h"
#include "ptxStatus.h"

#if defined(PTX_INTF_UART)
#include "PtxPlatUART.h"
#elif defined(PTX_INTF_SPI)
#include "PtxPlatSPI.h"
#elif defined(PTX_INTF_I2C)
#include "PtxPlatI2C.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief PLAT structure.
 */
typedef struct ptxPlat {
  ptxStatus_Comps_t CompId; /**< Component Id. */
#if defined(PTX_INTF_UART)
  ptxPLAT_UART_t *Uart; /**< Pointer to UART Context. Platform dependent. */
#else
  ptxPLAT_GPIO_t *gpio; /**< Pointer to GPIO Context. SPI/I2C platform
                 dependent. SPI: IRQ and NSS pin, I2C: IRQ pin */
#endif
  pptxPlat_RxCallBack_t RxCb; /**< Callback function */
  void *CtxRxCb;              /**< Callback Context */
  volatile uint32_t timerTick;
} ptxPlat_t;

#ifdef __cplusplus
}
#endif