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
    File        : ptxPlat.cpp

    Description : Platform implementation
*/

#include <Arduino.h>

#include "ptxDBG_PORT.h"
#include "ptxPLAT_INT.h"

namespace {
ptxPlat_t platform;
}  // namespace

ptxStatus_t ptxPLAT_AllocAndInit(
    ptxPlat_t **plat, [[maybe_unused]] ptxPLAT_ConfigPars *initParams) {
  memset(&platform, 0, sizeof(platform));
  platform.CompId = ptxStatus_Comp_PLAT;
#if defined(PTX_INTF_UART)
  const auto status = ptxPlat_uartInit(&platform.Uart, initParams);
#elif defined(PTX_INTF_SPI)
  const auto status = ptxPlat_spiInit(&platform.gpio);
#elif defined(PTX_INTF_I2C)
  const auto status = ptxPlat_i2cInit(&platform.gpio);
#endif
  *plat = &platform;
  return status;
}

ptxStatus_t ptxPLAT_Deinit(struct ptxPlat *plat) {
#if defined(PTX_INTF_UART)
  const auto status = ptxPlat_uartDeinit();
#elif defined(PTX_INTF_SPI)
  const auto status = ptxPlat_spiDeinit(plat->gpio);
#elif defined(PTX_INTF_I2C)
  const auto status = ptxPlat_i2cDeinit();
#endif
  memset(plat, 0, sizeof(*plat));
  return status;
}

ptxStatus_t ptxPLAT_ResetChip(ptxPlat_t *) { return ptxStatus_Success; }

ptxStatus_t ptxPLAT_GetInitializedTimer(struct ptxPlat *plat,
                                        struct ptxPlatTimer **) {
  if (plat != nullptr) {
    plat->timerTick = 0;
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_Sleep(ptxPlat_t *, uint32_t sleepMs) {
  delay(sleepMs);
  return ptxStatus_Success;
}

ptxStatus_t ptxPLAT_TimerStart(struct ptxPlat *plat, struct ptxPlatTimer *,
                               uint32_t ms, uint8_t isBlock,
                               pptxPlat_TimerCallBack_t fnISRCb, void *ISRCxt) {
  if (plat != nullptr) {
    plat->timerTick = millis() + ms;
    // block if requested by the SDK
    if (isBlock) {
      while (plat->timerTick > millis()) {
        yield();
      }
      plat->timerTick = 0;
      fnISRCb(ISRCxt);
    }
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_TimerIsElapsed(struct ptxPlat *plat, struct ptxPlatTimer *,
                                   uint8_t *isElapsed) {
  if (plat != nullptr && isElapsed != nullptr) {
    *isElapsed = plat->timerTick < millis();
    if (*isElapsed) {
      plat->timerTick = 0;
    }
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_TimerDeinit(struct ptxPlat *plat, struct ptxPlatTimer *) {
  if (plat != nullptr) {
    plat->timerTick = 0;
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

void ptxPLAT_DisableInterrupts(ptxPlat_t *) {
  // not implemented
}

void ptxPLAT_EnableInterrupts(ptxPlat_t *) {
  // not implemented
}

uint16_t ptxDBGPORT_Write(char *message) {
  Serial.print(message);
  return ptxStatus_Success;
}

ptxStatus_t ptxPLAT_TRx([[maybe_unused]] struct ptxPlat *plat, uint8_t *txBuf[],
                        size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[],
                        size_t *rxLen[], size_t numRxBuffers,
                        [[maybe_unused]] uint8_t flags) {
#if !defined(PTX_INTF_I2C)
  if (plat == nullptr) {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
#endif
#if defined(PTX_INTF_UART)
  return ptxPlat_uartTrx(plat->Uart, const_cast<const uint8_t **>(txBuf), txLen,
                         numTxBuffers, rxBuf, rxLen, numRxBuffers);
#elif defined(PTX_INTF_SPI)
  return ptxPlat_spiTrx(plat->gpio, const_cast<const uint8_t **>(txBuf), txLen,
                        numTxBuffers, rxBuf, rxLen, numRxBuffers);
#elif defined(PTX_INTF_I2C)
  return ptxPlat_i2cTrx(const_cast<const uint8_t **>(txBuf), txLen,
                        numTxBuffers, rxBuf, rxLen, numRxBuffers, flags);
#endif
}

ptxStatus_t ptxPLAT_WaitForInterrupt([[maybe_unused]] struct ptxPlat *plat) {
  auto status = ptxStatus_Success;

#if defined(PTX_INTF_UART)
  yield();
#else
  if (plat != nullptr) {
    bool event = false;
    /* Read IRQ first to check if it is already HIGH.*/
    do {
      event = HIGH == digitalRead(plat->gpio->pinIRQ);
      if (!event && plat->timerTick) {
        event = plat->timerTick <= millis();
        yield();
      }
    } while (!event);
  } else {
    status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
#endif

  return status;
}

ptxStatus_t ptxPLAT_StartWaitForRx(struct ptxPlat *plat,
                                   pptxPlat_RxCallBack_t irqCb,
                                   void *ctxIrqCb) {
  if (plat != nullptr && irqCb != nullptr && ctxIrqCb != nullptr) {
    plat->RxCb = irqCb;
    plat->CtxRxCb = ctxIrqCb;
#if !defined(PTX_INTF_UART)
    /* Read IRQ first to check if it is already HIGH.*/
    if (digitalRead(plat->gpio->pinIRQ) == HIGH) {
      /* IRQ is already high, so let's trigger the asynchronous event now to
       * prevent than race condition has happened. */
      plat->RxCb(plat->CtxRxCb);
    }
#endif
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_StopWaitForRx(struct ptxPlat *plat) {
  if (plat != nullptr) {
    plat->timerTick = 0;
    plat->RxCb = nullptr;
    plat->CtxRxCb = nullptr;
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_IsRxPending(struct ptxPlat *plat, uint8_t *isRxPending) {
  if (plat != nullptr && isRxPending != nullptr) {
#if defined(PTX_INTF_UART)
    *isRxPending = ptxPlat_uartIsRxPending();
#else
    /* Read IRQ to check if it is already HIGH.*/
    *isRxPending = digitalRead(plat->gpio->pinIRQ);
#endif
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

uint8_t ptxPLAT_CheckRxActive(ptxPlat_t *) {
#if defined(PTX_INTF_UART)
  return ptxPlat_uartCheckRxActive();
#else
  return 0U;
#endif
}

ptxStatus_t ptxPLAT_TriggerRx(struct ptxPlat *plat) {
  if (plat != nullptr) {
#if defined(PTX_INTF_UART)
    return ptxPlat_uartTriggerRx(plat);
#else
    const uint8_t isRxPending = digitalRead(plat->gpio->pinIRQ);
    if (isRxPending && plat->RxCb) {
      plat->RxCb(plat->CtxRxCb);
    }
#endif
    return ptxStatus_Success;
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

#if defined(PTX_INTF_UART)
ptxStatus_t ptxPLAT_SetIntfSpeed(struct ptxPlat *plat, uint32_t speed) {
  if (plat != nullptr) {
    return ptxPLAT_uartSetIntfSpeed(plat->Uart, speed);
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

uint32_t ptxPLAT_GetIntfSpeed(struct ptxPlat *plat) {
  if (plat != nullptr) {
    return plat->Uart->IntfSpeed;
  } else {
    return 0U;
  }
}

ptxStatus_t ptxPLAT_SetCleanStateRx(struct ptxPlat *plat) {
  if (plat != nullptr) {
    return ptxPLAT_uartSetCleanStateRx(plat->Uart);
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}

ptxStatus_t ptxPLAT_GetReceivedMessage(struct ptxPlat *plat,
                                       uint8_t *rxMessageBuffer,
                                       size_t *rxMessageBufferLen) {
  if (plat != nullptr) {
    return ptxPLAT_uartGetReceivedMessage(plat->Uart, rxMessageBuffer,
                                          rxMessageBufferLen);
  } else {
    return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
  }
}
#endif