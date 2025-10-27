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
    Module      : Examples
    File        : IotRdDemo.cpp

    Description : Implementation of the SDK's reader demo
*/

#include "IotRdDemo.h"

#include <ptxCOMMON.h>
#include <ptxIoTRd_COMMON.h>
#include <ptxNativeTag_T5T.h>
#include <ptx_IOT_READER.h>

#include <array>

using namespace PtxIotRdDemo;

namespace {
ptxIotRdInt_Demo_State_t demoState;
}  // namespace

IotRdDemo::IotRdDemo() {
  m_t5tComp = std::make_unique<ptxNativeTag_T5T_t>();
  demoState = IoTRd_DemoState_Undefined;
  m_context = nullptr;
  m_exit = 1U;
}

IotRdDemo &IotRdDemo::getDemo() {
  static IotRdDemo instance;
  return instance;
}

bool IotRdDemo::begin(std::shared_ptr<ptxIoTRd_t> context) {
  bool success = false;
  if (context != nullptr) {
    m_context = context;
    demoState = IoTRd_DemoState_WaitForActivation;
    m_exit = 0U;
    success = true;
  }
  return success;
}

bool IotRdDemo::run() {
  if (m_exit == 0U) {
    m_systemState = PTX_SYSTEM_STATUS_OK;
    uint8_t lastRfError = PTX_RF_ERROR_NTF_CODE_NO_ERROR;
    ptxStatus_t status = ptxStatus_Success;
    /* check regularly for critical system errors */
    ptxIoTRd_Get_Status_Info(m_context.get(), StatusType_System,
                             &m_systemState);

    if (m_systemState != PTX_SYSTEM_STATUS_OK) {
      /* Handle system-error */
      ptxCommon_PrintF("System ERROR: 0x%02X\n", m_systemState);
      demoState = IoTRd_DemoState_SystemError;
    } else {
      if (status != ptxStatus_Success) {
        ptxCommon_PrintF("ERROR reading system status: ");
        ptxCommon_PrintF("COMP: %d STATUS: %d\n", PTX_GET_COMP(status),
                         PTX_GET_STATUS(status));
        demoState = IoTRd_DemoState_DeactivateReader;
      }
    }

    ptxIoTRd_Get_Status_Info(m_context.get(), StatusType_LastRFError,
                             &lastRfError);

    if (lastRfError == PTX_RF_ERROR_NTF_CODE_WARNING_PA_OVERCURRENT_LIMIT) {
      ptxCommon_PrintF("Warning - Overcurrent Limiter activated!\n");
    }

    switch (demoState) {
      case IoTRd_DemoState_WaitForActivation:
        ptxIoTRdInt_DemoState_WaitForActivation(
            m_context.get(), m_context->CardRegistry, &demoState);
        break;
      case IoTRd_DemoState_SelectCard:
        status = ptxIoTRdInt_DemoState_SelectCard(
            m_context.get(), m_context->CardRegistry, &demoState, &m_exit);
        break;
      case IoTRd_DemoState_DataExchange:
        status = dataExchange();
        demoState = IoTRd_DemoState_DeactivateReader;
        break;
      case IoTRd_DemoState_HostCardEmulation:
        // not needed for this example
        break;
      case IoTRd_DemoState_DeactivateReader:
        status = ptxIoTRdInt_DemoState_DeactivateReader(m_context.get(),
                                                        &demoState, &m_exit);
        break;
      case IoTRd_DemoState_SystemError:
        systemError();
        return false;
      default:
        break;
    }
  }

  return m_exit == 0U;
}

bool IotRdDemo::end() {
  demoState = IoTRd_DemoState_DeactivateReader;
  auto status = ptxNativeTag_T5TClose(m_t5tComp.get());
  m_context = nullptr;
  m_exit = 1U;

  return status == ptxStatus_Success;
}

void IotRdDemo::systemError() {
  switch (m_systemState) {
    case PTX_SYSTEM_STATUS_ERR_OVERCURRENT:
      ptxCommon_PrintF(
          "Error - Critical System-Error (Overcurrent) occurred - "
          "Quit Application\n");
      break;
    case PTX_SYSTEM_STATUS_ERR_TEMPERATURE:
      ptxCommon_PrintF(
          "Error - Critical System-Error (Temperature) occurred - "
          "Quit Application\n");
      break;
    default:
      /* shall never happen */
      break;
  }
  /* reset the system and quit */
  ptxIoTRd_SWReset(m_context.get());
}

ptxStatus_t IotRdDemo::dataExchange() {
  ptxStatus_t status = ptxStatus_InvalidParameter;
  /**
   * Example Code-Delays/-Sleeps; used for better readability of exchanges
   * RF-data on the console application.
   */
  const uint8_t dataExchangeWaitTime = 10U;
  /**
   * Default timeout-values for for RAW-protocols (e.g. T2T, T3T, ...) and
   * standard-protocols (ISO-/NFC-DEP)
   */
  const uint32_t defaultAppTimeoutRaw =
      200U; /**< Application-timeout for raw-protocols */
  const uint32_t defaultAppTimeoutProt =
      5000U; /**< Application-timeout for standard-protocols */
  const uint16_t txBufferSize = 266U;
  const uint16_t rxBufferSize = 266U;
  /* T2T Protocol Example => READ BLOCK 0 */
  const uint8_t protType2Example[] = {0x30, 0x00};
  /* T3T Protocol Example => CHECK BLOCK 0 (NFCID2 to be inserted) */
  const uint8_t protType3Example[] = {0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x00};
  /* T4T/ISO-DEP Protocol Example => SELECT (PPSE) APDU */
  const uint8_t protIsoDepExample[] = {0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50,
                                       0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                                       0x44, 0x44, 0x46, 0x30, 0x31, 0x00};
  /* Peer-to-Peer/NFC-DEP Protocol Example => LLCP - SYMM-Packet = 0x0000 */
  const uint8_t protNfcDepExample[] = {0x00, 0x00};
  std::array<uint8_t, txBufferSize> txData;
  uint32_t txDataLength = 0U;
  bool skipTxDataExchange = false;
  std::array<uint8_t, rxBufferSize> rxData;
  uint32_t rxDataLength = 0U;
  uint32_t appTimeout = defaultAppTimeoutRaw;
  size_t tempRxLen;

  if (m_context != nullptr && m_context->CardRegistry != nullptr) {
    // initialize type 5 tag component
    ptxNativeTag_T5T_InitParams_t t5tInitParams;
    memset(m_t5tComp.get(), 0, sizeof(m_t5tComp));
    memset(&t5tInitParams, 0, sizeof(t5tInitParams));
    t5tInitParams.IotRd = m_context.get();
    t5tInitParams.TxBuffer = txData.data();
    t5tInitParams.TxBufferSize = txData.size();
    t5tInitParams.UID = nullptr;
    t5tInitParams.UIDLen = 0;
    status = ptxNativeTag_T5TOpen(m_t5tComp.get(), &t5tInitParams);

    if (status == ptxStatus_Success) {
      switch (m_context->CardRegistry->ActiveCardProtType) {
        case Prot_T2T:
          txDataLength = sizeof(protType2Example);
          memcpy(txData.data(), &protType2Example[0], txDataLength);
          appTimeout = defaultAppTimeoutRaw;
          break;

        case Prot_T3T:  // FeliCa
          txDataLength = 7U + 8U;
          memcpy(txData.data(), protType3Example, 1U);
          memcpy(&txData[1],
                 &m_context->CardRegistry->ActiveCard->TechParams.CardFParams
                      .SENSF_RES[2],
                 8U);
          memcpy(&txData[9], &protType3Example[1], 6U);
          appTimeout = defaultAppTimeoutRaw;
          break;

        case Prot_ISODEP:  // type 4 tag
          txDataLength = sizeof(protIsoDepExample);
          memcpy(txData.data(), protIsoDepExample, txDataLength);
          appTimeout = defaultAppTimeoutProt;
          break;

        case Prot_NFCDEP:
          txDataLength = sizeof(protNfcDepExample);
          memcpy(txData.data(), protNfcDepExample, txDataLength);
          appTimeout = defaultAppTimeoutProt;
          break;

        case Prot_T5T:
          appTimeout = defaultAppTimeoutRaw;
          rxDataLength = rxBufferSize;

          /* set UID if adressed-mode shall be used */
          ptxNativeTag_T5TSetUID(
              m_t5tComp.get(),
              m_context->CardRegistry->ActiveCard->TechParams.CardVParams.UID,
              8U);
          /* read Block-0 via Native-Tag command-set */
          tempRxLen = static_cast<size_t>(rxDataLength);
          status = ptxNativeTag_T5TReadSingleBlock(
              m_t5tComp.get(), 0U, 0U, rxData.data(), &tempRxLen, appTimeout);
          rxDataLength = static_cast<uint32_t>(tempRxLen);
          ptxCommon_PrintStatusMessage(
              "Execute \"READ_SINGLE_BLOCK\"-command (Block 0)", status);
          skipTxDataExchange = true;
          break;

        default:
          /* Undefined Protocol - Restart RF-Discovery */
          skipTxDataExchange = true;
          break;
      }

      if (!skipTxDataExchange) {
        ptxIoTRdInt_Sleep(m_context.get(), dataExchangeWaitTime);
        rxDataLength = rxBufferSize;
        ptxCommon_PrintF("TX = ");
        ptxCommon_Print_Buffer(txData.data(), 0U, txDataLength, 1U, 0U);
        status =
            ptxIoTRd_Data_Exchange(m_context.get(), txData.data(), txDataLength,
                                   rxData.data(), &rxDataLength, appTimeout);
      }

      if (status == ptxStatus_Success) {
        ptxCommon_PrintF("RX = ");
        ptxCommon_Print_Buffer(rxData.data(), 0U, rxDataLength, 1U, 0U);
      } else {
        ptxCommon_PrintF(
            "ERROR - Module initialization failed! (Error-Code = %04X, RF)\n",
            status);
      }
    } else {
      ptxCommon_PrintF("ERROR - RF-Exchange failed! (Error-Code = %04X, RF)\n",
                       status);
    }

    demoState = IoTRd_DemoState_DeactivateReader;
  }

  return status;
}