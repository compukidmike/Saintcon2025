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
    Module      : IOT Reader
    File        : IotReader.cpp

    Description : IOT Reader library implementation
*/

#include "IotReader.h"

#include <ptxCOMMON.h>
#include <ptxHce_Exchange.h>
#include <ptxIoTRd_COMMON.h>
#include <ptxNDEF.h>
#include <ptxNativeTag_T5T.h>
#include <ptxStatus.h>
#include <ptx_IOT_READER.h>

#include <algorithm>

using namespace PtxIotReader;

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

// Helpers to decode component and status into human-readable strings for logging
static const char *ptxCompToStr(ptxStatus_t status) {
  switch (PTX_GET_COMP(status)) {
    case ptxStatus_Comp_None: return "None";
    case ptxStatus_Comp_NSC: return "NSC";
    case ptxStatus_Comp_POS: return "POS";
    case ptxStatus_Comp_PLAT: return "PLAT";
    case ptxStatus_Comp_IoTReader: return "IoTReader";
    case ptxStatus_Comp_IOTHce: return "IOTHce";
    case ptxStatus_Comp_TCP: return "TCP";
    case ptxStatus_Comp_PERIPH: return "PERIPH";
    case ptxStatus_Comp_NDEF: return "NDEF";
    default: return "UNKNOWN";
  }
}

static const char *ptxStatusToStr(ptxStatus_t status) {
  switch (PTX_GET_STATUS(status)) {
    case ptxStatus_Success_Internal: return "Success_Internal";
    case ptxStatus_InvalidParameter: return "InvalidParameter";
    case ptxStatus_InternalError: return "InternalError";
    case ptxStatus_NotImplemented: return "NotImplemented";
    case ptxStatus_TimeOut: return "TimeOut";
    case ptxStatus_InsufficientResources: return "InsufficientResources";
    case ptxStatus_InterfaceError: return "InterfaceError";
    case ptxStatus_NscProtocolError: return "NscProtocolError";
    case ptxStatus_NoDataAvailable: return "NoDataAvailable";
    case ptxStatus_NotPermitted: return "NotPermitted";
    case ptxStatus_InvalidState: return "InvalidState";
    case ptxStatus_NscRfError: return "NscRfError";
    case ptxStatus_NotSupported: return "NotSupported";
    default: return "UNKNOWN_STATUS";
  }
}

namespace {
const uint32_t noCardSleepTime = 5U;
const uint16_t ndefBufferSize = 512U;
const uint8_t nfcId2Size = 8U;
const uint8_t nfcId2Offset = 2U;
const uint8_t nfcId0Size = 4U;
const uint8_t nfcId0Offset = 2U;
}  // namespace

IotReader::IotReader() {
  m_iotRd = std::make_shared<ptxIoTRd_t>();
  m_ndefComp = std::make_unique<ptxNDEF_t>();
  m_t4tComp = std::make_unique<ptxT4T_t>();
  m_activeCard.protocol = CardProtocol::Undefined;
}

IotReader &IotReader::getReader() {
  static IotReader instance;
  return instance;
}

bool IotReader::begin() {
  auto status = ptxStatus_Success;
  memset(m_iotRd.get(), 0U, sizeof(*m_iotRd));

  ptxIoTRd_InitPars_t initParams;
  memset(&initParams, 0U, sizeof(initParams));
  status = ptxIoTRd_Init(m_iotRd.get(), &initParams);

  /* Make sure the internal card registry is not nullptr and the SDK component
   * is correct. */
  ptxIoTRd_CardRegistry_t *sdkCardRegistry;
  if (status == ptxStatus_Success) {
    ptxIoTRd_Get_Card_Registry(m_iotRd.get(), &sdkCardRegistry);

    if (sdkCardRegistry == nullptr) {
      status = ptxStatus_InternalError;
    }
  }

  if (status != ptxStatus_Success) {
    /* Error codes are documented in ptxStatus.h */
    ptxCommon_PrintF("System Initialization ... ERROR\n");
    printErrorInfo(status);
    ptxIoTRd_Deinit(m_iotRd.get());
  }

  return status == ptxStatus_Success;
}

bool IotReader::detectCard(const PollingConfig &config) {
  if (!m_isPolling && !pollingStart(config)) {
    return false;
  }

  auto systemState = PTX_SYSTEM_STATUS_OK;

  auto status =
      ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &systemState);

  if (status != ptxStatus_Success || systemState != PTX_SYSTEM_STATUS_OK) {
    ptxCommon_PrintF("SYSTEM ERROR: status: %d state: %d\n", status,
                     systemState);
    return false;
  }

  auto discoveryStatus = RF_DISCOVER_STATUS_NO_CARD;
  status = ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover,
                                    &discoveryStatus);

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to retrieve discovery status\n");
    printErrorInfo(status);
    return false;
  }

  bool success = false;
  switch (discoveryStatus) {
    case RF_DISCOVER_STATUS_NO_CARD:
      // Wait until card is in the field (off-load CPU with sleep operation)
      ptxIoTRdInt_Sleep(m_iotRd.get(), noCardSleepTime);
      break;
    case RF_DISCOVER_STATUS_CARD_ACTIVE:
      updateCardRegistry();
      success = true;
      break;
    default:
      // not dealing with multiple cards and card selection
      ptxCommon_PrintF("Multiple Cards detected...\n");
      ptxCommon_PrintF("Make sure to place only one card in the field!\n");
      restartDiscovery();
      break;
  }

  return success;
}

bool IotReader::emulateCard(const std::vector<uint8_t> &message) {
  if (message.empty()) {
    ptxCommon_PrintF("ERROR: Empty NDEF message provided\n");
    return false;
  }

  if (!m_isListening && listeningStart() != ptxStatus_Success) {
    return false;
  }

  auto status = ptxStatus_Success;

  // If the T4T component is not initialized or the NDEF message is different,
  // initialize it
  if (message != m_hceNdefMessage) {
    m_hceNdefMessage = message;
    ptxT4T_InitParams_t t4tInitParams;
    t4tInitParams.DefaultNDEFMessage = m_hceNdefMessage.data();
    t4tInitParams.DefaultNDEFMessageLength =
        static_cast<uint16_t>(m_hceNdefMessage.size());
    status = ptxT4T_Init(m_t4tComp.get(), &t4tInitParams);
  }

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to initialize card emulation\n");
    printErrorInfo(status);
    return false;
  }

  auto systemState = PTX_SYSTEM_STATUS_OK;

  status =
      ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &systemState);

  if (status != ptxStatus_Success || systemState != PTX_SYSTEM_STATUS_OK) {
    ptxCommon_PrintF("SYSTEM ERROR: status: %d state: %d\n", status,
                     systemState);
    return false;
  }

  auto discoveryStatus = RF_DISCOVER_STATUS_NO_CARD;
  status = ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover,
                                    &discoveryStatus);

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to retrieve discovery status\n");
    printErrorInfo(status);
    return false;
  }

  if (discoveryStatus == RF_DISCOVER_STATUS_NO_CARD) {
    ptxIoTRdInt_Sleep(m_iotRd.get(), noCardSleepTime);
    return false;
  }

  if (discoveryStatus != RF_DISCOVER_STATUS_LISTEN_A ||
      m_iotRd->Hce.EventQ.NrOfEntries == 0U) {
    return false;
  }

  uint8_t exitLoop = 1U;
  status = ptxHce_EmulateT4T(&m_iotRd->Hce, m_t4tComp.get(), &exitLoop);

  return status == ptxStatus_Success;
}

ptxStatus_t IotReader::ndefInit() {
  auto status = ptxStatus_Success;

  if (m_ndefTxBuffer.empty() && m_ndefRxBuffer.empty() &&
      m_ndefWorkBuffer.empty()) {
    ptxNDEF_InitParams_t ndefInitParams;
    memset(m_ndefComp.get(), 0U, sizeof(*m_ndefComp));
    memset(&ndefInitParams, 0U, sizeof(ndefInitParams));

    m_ndefTxBuffer.resize(ndefBufferSize, 0U);
    m_ndefRxBuffer.resize(ndefBufferSize, 0U);
    m_ndefWorkBuffer.resize(ndefBufferSize, 0U);

    ndefInitParams.IotRd = m_iotRd.get();
    ndefInitParams.TxBuffer = m_ndefTxBuffer.data();
    ndefInitParams.TxBufferSize = m_ndefTxBuffer.size();
    ndefInitParams.RxBuffer = m_ndefRxBuffer.data();
    ndefInitParams.RxBufferSize = m_ndefRxBuffer.size();
    ndefInitParams.WorkBuffer = m_ndefWorkBuffer.data();
    ndefInitParams.WorkBufferSize = m_ndefWorkBuffer.size();

    status = ptxNDEF_Open(m_ndefComp.get(), &ndefInitParams);
  }

  if (status != ptxStatus_Success) printErrorInfo(status);

  return status;
}

bool IotReader::ndefRead(std::vector<uint8_t> &message) {
  if (ndefInit() != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to initialize NDEF component\n");
    return false;
  }

  auto systemState = PTX_SYSTEM_STATUS_OK;

  auto status =
      ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &systemState);

  if (systemState != PTX_SYSTEM_STATUS_OK) {
    ptxCommon_PrintF("SYSTEM ERROR: status: %d state: %d\n", status,
                     systemState);
    return false;
  }

  auto discoveryStatus = RF_DISCOVER_STATUS_NO_CARD;
  status = ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover,
                                    &discoveryStatus);

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to retrieve discovery status\n");
    printErrorInfo(status);
    return false;
  }

  if (discoveryStatus != RF_DISCOVER_STATUS_CARD_ACTIVE) {
    ptxCommon_PrintF("ERROR: No card detected in field\n");
    return false;
  }

  status = ptxNDEF_CheckMessage(m_ndefComp.get());

  if (status == ptxStatusSuccess) {
    std::array<uint8_t, ndefBufferSize> arr;
    auto readLen = static_cast<uint32_t>(arr.size());

    status = ptxNDEF_ReadMessage(m_ndefComp.get(), arr.data(), &readLen);
    if (status == ptxStatus_Success) {
      uint32_t copyLen = std::min(readLen, static_cast<uint32_t>(arr.size()));
      message.clear();
      message.insert(message.begin(), arr.begin(), arr.begin() + copyLen);
    } else {
      message.clear();
      ptxCommon_PrintF("ERROR: Failed to read card\n");
      printErrorInfo(status);
    }
  } else {
    /* Uncomment below to print error info. If there's no NDEF message on the
     * card, it would pollute the console. If there's one, but the library won't
     * read it, it might be useful to know the error code. */
    // ptxCommon_PrintF("Failed to find NDEF message");
    // printErrorInfo(status);
  }

  pollingStop();

  return status == ptxStatus_Success;
}

bool IotReader::ndefWrite(const std::vector<uint8_t> &message) {
  if (message.empty()) {
    ptxCommon_PrintF("ERROR: Empty NDEF message provided\n");
    return false;
  }

  if (ndefInit() != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to initialize NDEF component\n");
    return false;
  }

  auto systemState = PTX_SYSTEM_STATUS_OK;

  auto status =
      ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &systemState);

  if (systemState != PTX_SYSTEM_STATUS_OK) {
    ptxCommon_PrintF("SYSTEM ERROR: status: %d state: %d\n", status,
                     systemState);
    return false;
  }

  auto discoveryStatus = RF_DISCOVER_STATUS_NO_CARD;
  status = ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover,
                                    &discoveryStatus);

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to retrieve discovery status\n");
    printErrorInfo(status);
    return false;
  }

  if (discoveryStatus != RF_DISCOVER_STATUS_CARD_ACTIVE) {
    ptxCommon_PrintF("ERROR: No card detected in field\n");
    return false;
  }

  status = ptxNDEF_CheckMessage(m_ndefComp.get());

  if (status == ptxStatusSuccess) {
    status = ptxNDEF_WriteMessage(m_ndefComp.get(),
                                  const_cast<uint8_t *>(message.data()),
                                  message.size());
    if (status != ptxStatus_Success) {
      ptxCommon_PrintF("ERROR: Failed to write card\n");
      printErrorInfo(status);
    }
  } else {
    /* Uncomment below to print error info. If the card is not NDEF formatted,
     * it would pollute the console. If it is, but the library won't write
     * it, it might be useful to know the error code. */
    // ptxCommon_PrintF("Failed to find NDEF formatting");
    // printErrorInfo(status);
  }

  pollingStop();

  return status == ptxStatus_Success;
}

NdefRecord IotReader::getNdefMessage(
    const std::vector<uint8_t> &message) const {
  // assign the raw data to the payload, in case the message won't be parsed
  NdefRecord record{NdefTypeNameFormat::Unknown, NdefMessageType::Other,
                    std::string{}, message};

  if (message.size() < 2U) {
    // message must contain at least 2 bytes
    return record;
  }

  const auto tnfByte = message.front();
  const auto typeNameFormat = static_cast<NdefTypeNameFormat>(tnfByte & 0x07);
  record.typeNameFormat = typeNameFormat;

  if (typeNameFormat == NdefTypeNameFormat::Empty) {
    record.messageType = NdefMessageType::Empty;
    return record;
  }

  if (typeNameFormat == NdefTypeNameFormat::Media) {
    record.messageType = NdefMessageType::Media;
  }

  const bool isShortRecord = tnfByte & 0x10;  // Bit 4 SR - Short record
  const bool hasIDLength =
      tnfByte & 0x08;  // Bit 3 IL - ID LENGTH field is present
  const bool isMessageBegin = tnfByte & 0x80;  // Bit 7 MB - Message Begin
  const bool isMessageEnd = tnfByte & 0x40;    // Bit 6 ME - Message End

  if (!(isMessageBegin && isMessageEnd)) {
    // only single record messages are supported
    record.messageType = NdefMessageType::Other;
    return record;
  }

  uint8_t idLength = 0U;

  const auto typeLength = message.at(1);

  // defaults for short records (SR == 1)
  if (hasIDLength) {
    if (message.size() < 3U) {
      record.messageType = NdefMessageType::Other;
      return record;
    }
    idLength = message.at(3);
  }

  uint8_t typeOffset = 3U + (hasIDLength ? 1U : 0U);
  uint8_t payloadOffset = typeOffset + typeLength + idLength;
  uint32_t payloadLength = message.at(2);

  if (!isShortRecord) {
    if (hasIDLength) {
      if (message.size() < 6U) {
        record.messageType = NdefMessageType::Other;
        return record;
      }
      idLength = message.at(6);
    }

    typeOffset = 6U + (hasIDLength ? 1U : 0U);
    payloadOffset = typeOffset + typeLength + idLength;
    payloadLength =
        static_cast<uint32_t>((message.at(2) << 24) | (message.at(3) << 16) |
                              (message.at(4) << 8) | message.at(5));
  }

  const auto payloadType = std::string(
      message.begin() + typeOffset, message.begin() + typeOffset + typeLength);
  record.payloadType = payloadType;

  // message is shorter than indicated by length fields
  if ((payloadOffset + payloadLength) > static_cast<uint32_t>(message.size())) {
    record.messageType = NdefMessageType::Other;
    return record;
  }

  record.payload =
      std::vector<uint8_t>(message.begin() + payloadOffset,
                           message.begin() + payloadOffset + payloadLength);

  if (payloadType == "U")
    record.messageType = NdefMessageType::URI;
  else if (payloadType == "T")
    record.messageType = NdefMessageType::Text;

  return record;
}

bool IotReader::restartDiscovery() const {
  auto const status = ptxIoTRd_Reader_Deactivation(
      m_iotRd.get(), PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER);
  if (status != ptxStatus_Success) {
    ptxCommon_PrintF("ERROR: Failed to restart discovery\n");
    printErrorInfo(status);
  }
  return status == ptxStatus_Success;
}

bool IotReader::pollingStart(const PollingConfig &config) {
  auto success = true;
  m_isPolling = true;
  ptxIoTRd_DiscConfig_t rfDiscoveryConfig;
  memset(&rfDiscoveryConfig, 0, sizeof(rfDiscoveryConfig));

  /* User defined settings */
  rfDiscoveryConfig.PollTypeA = config.pollTypeA;
  rfDiscoveryConfig.PollTypeB = config.pollTypeB;
  rfDiscoveryConfig.PollTypeF212 = config.pollTypeF212;
  rfDiscoveryConfig.PollTypeV = config.pollTypeV;
  rfDiscoveryConfig.IdleTime = config.idleTime;
  rfDiscoveryConfig.Discover_Mode = config.discoverMode;
  rfDiscoveryConfig.EnableStandBy = config.enableStandBy;

  // Try a few times to start discovery - sometimes transient resource/contention
  // issues cause the first attempt to fail. Log heap/stack and decoded PTX
  // status for diagnostics.
  const int max_attempts = 3;
  const TickType_t backoff_ms = pdMS_TO_TICKS(500);
  ptxStatus_t status = ptxStatus_Success;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    UBaseType_t high_water = uxTaskGetStackHighWaterMark(NULL);
    ptxCommon_PrintF("RF-Discovery attempt %d/%d - free_heap=%u high_water=%u\n",
                     attempt, max_attempts, (unsigned)free_heap,
                     (unsigned)(high_water * sizeof(StackType_t)));

    status = ptxIoTRd_Initiate_Discovery(m_iotRd.get(), &rfDiscoveryConfig);

    if (status == ptxStatus_Success) break;

    ptxCommon_PrintF("ERROR: Failed to start RF-Discovery (attempt %d)\n",
                     attempt);
    ptxCommon_PrintF("COMP: %d STATUS: %d (%s:%s)\n", PTX_GET_COMP(status),
                     PTX_GET_STATUS(status), ptxCompToStr(status),
                     ptxStatusToStr(status));
    printErrorInfo(status);

    // Try a safe deactivation/restart of discovery before retrying. This will
    // deassert RF and may clear protocol-level errors in the NSC layer.
    restartDiscovery();

    // Query and log system/discovery status for more debugging context
    uint8_t sysState = 0;
    if (ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &sysState) == ptxStatus_Success) {
      ptxCommon_PrintF("SYSTEM STATE AFTER FAILURE: %u\n", (unsigned)sysState);
    }
    uint8_t discState = 0;
    if (ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover, &discState) == ptxStatus_Success) {
      ptxCommon_PrintF("DISCOVERY STATE AFTER FAILURE: %u\n", discState);
    }

    // If the NSC layer reports a protocol error, try a deeper recovery:
    // deinit/re-init the IoT reader which reinitializes NSC internals.
    if (PTX_GET_STATUS(status) == ptxStatus_NscProtocolError) {
      ptxCommon_PrintF("NscProtocolError detected - attempting full IoT reader reinit\n");
      ptxIoTRd_Deinit(m_iotRd.get());
      ptxIoTRd_InitPars_t initParams;
      memset(&initParams, 0, sizeof(initParams));
      ptxStatus_t rinit = ptxIoTRd_Init(m_iotRd.get(), &initParams);
      ptxCommon_PrintF("IoT reader reinit result: %d\n", rinit);
    }

    if (attempt < max_attempts) vTaskDelay(backoff_ms);
  }

  if (status != ptxStatus_Success) {
    m_isPolling = false;
    success = false;
  }

  return success;
}

bool IotReader::pollingStop() {
  m_isPolling = false;
  m_isListening = false;
  auto const status = ptxIoTRd_Reader_Deactivation(
      m_iotRd.get(), PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);

  return status == ptxStatus_Success;
}

ptxStatus_t IotReader::listeningStart() {
  m_isListening = true;
  ptxIoTRd_DiscConfig_t rfDiscoveryConfig;
  memset(&rfDiscoveryConfig, 0U, sizeof(rfDiscoveryConfig));
  rfDiscoveryConfig.ListenTypeA = 1U;
  rfDiscoveryConfig.IdleTime = 100U;

  // Retry loop similar to pollingStart() - helps recover from transient errors
  const int max_attempts = 5;
  const TickType_t backoff_ms = pdMS_TO_TICKS(500);
  ptxStatus_t status = ptxStatus_Success;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    UBaseType_t high_water = uxTaskGetStackHighWaterMark(NULL);
    ptxCommon_PrintF("RF-Listening attempt %d/%d - free_heap=%u high_water=%u\n",
                     attempt, max_attempts, (unsigned)free_heap,
                     (unsigned)(high_water * sizeof(StackType_t)));

    status = ptxIoTRd_Initiate_Discovery(m_iotRd.get(), &rfDiscoveryConfig);
    if (status == ptxStatus_Success) break;

    ptxCommon_PrintF("ERROR: Failed to start RF-Listening (attempt %d)\n",
                     attempt);
    ptxCommon_PrintF("COMP: %d STATUS: %d (%s:%s)\n", PTX_GET_COMP(status),
                     PTX_GET_STATUS(status), ptxCompToStr(status),
                     ptxStatusToStr(status));
    printErrorInfo(status);

    // Attempt a small deactivation/reset of discovery and log additional state
    restartDiscovery();
    uint8_t sysState = 0;
    if (ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_System, &sysState) == ptxStatus_Success) {
      ptxCommon_PrintF("SYSTEM STATE AFTER FAILURE: %u\n", (unsigned)sysState);
    }
    uint8_t discState = 0;
    if (ptxIoTRd_Get_Status_Info(m_iotRd.get(), StatusType_Discover, &discState) == ptxStatus_Success) {
      ptxCommon_PrintF("DISCOVERY STATE AFTER FAILURE: %u\n", discState);
    }

    if (PTX_GET_STATUS(status) == ptxStatus_NscProtocolError) {
      ptxCommon_PrintF("NscProtocolError detected - attempting full IoT reader reinit\n");
      ptxIoTRd_Deinit(m_iotRd.get());
      ptxIoTRd_InitPars_t initParams;
      memset(&initParams, 0, sizeof(initParams));
      ptxStatus_t rinit = ptxIoTRd_Init(m_iotRd.get(), &initParams);
      ptxCommon_PrintF("IoT reader reinit result: %d\n", rinit);
    }

    if (attempt < max_attempts) vTaskDelay(backoff_ms);
  }

  if (status != ptxStatus_Success) {
    m_isListening = false;
  }

  return status;
}

void IotReader::sleep(uint32_t timeout) const {
  ptxIoTRdInt_Sleep(m_iotRd.get(), timeout);
}

void IotReader::updateCardRegistry() {
  auto *sdkActiveCard = m_iotRd->CardRegistry->ActiveCard;
  m_activeCard.protocol =
      static_cast<CardProtocol>(m_iotRd->CardRegistry->ActiveCardProtType);
  m_activeCard.technology = static_cast<CardTech>(sdkActiveCard->TechType);
  m_activeCard.id.clear();

  switch (sdkActiveCard->TechType) {
    case Tech_TypeA:
      m_activeCard.id.insert(
          m_activeCard.id.begin(), sdkActiveCard->TechParams.CardAParams.NFCID1,
          sdkActiveCard->TechParams.CardAParams.NFCID1 +
              sdkActiveCard->TechParams.CardAParams.NFCID1_LEN);
      break;
    case Tech_TypeB:
      m_activeCard.id.insert(
          m_activeCard.id.begin(),
          &sdkActiveCard->TechParams.CardBParams.SENSB_RES[nfcId0Offset],
          &sdkActiveCard->TechParams.CardBParams.SENSB_RES[nfcId0Offset] +
              nfcId0Size);
      break;
    case Tech_TypeF:
      m_activeCard.id.insert(
          m_activeCard.id.begin(),
          &sdkActiveCard->TechParams.CardFParams.SENSF_RES[nfcId2Offset],
          &sdkActiveCard->TechParams.CardFParams.SENSF_RES[nfcId2Offset] +
              nfcId2Size);
      break;
    case Tech_TypeV:
      m_activeCard.id.insert(
          m_activeCard.id.begin(), sdkActiveCard->TechParams.CardVParams.UID,
          sdkActiveCard->TechParams.CardVParams.UID +
              sizeof(sdkActiveCard->TechParams.CardVParams.UID));
      break;
    case Tech_TypeExtension:
      // nothing to do
      break;
  }
}

bool IotReader::end() { return deInit() == ptxStatus_Success; }

ptxStatus_t IotReader::deInit() {
  m_isPolling = false;
  m_isListening = false;
  m_ndefTxBuffer.clear();
  m_ndefRxBuffer.clear();
  m_ndefWorkBuffer.clear();
  m_hceNdefMessage.clear();
  m_activeCard.id.clear();
  m_activeCard.protocol = CardProtocol::Undefined;
  ptxIoTRd_Reader_Deactivation(m_iotRd.get(),
                               PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);
  return ptxIoTRd_Deinit(m_iotRd.get());
}

std::pair<ptxStatus_t, uint8_t> IotReader::getStatusInfo(
    StatusType type) const {
  uint8_t info;
  auto const sdkType = static_cast<ptxIoTRd_StatusType_t>(type);
  ptxStatus_t status = ptxIoTRd_Get_Status_Info(m_iotRd.get(), sdkType, &info);

  return std::make_pair(status, info);
}

void IotReader::printBuffer(uint8_t *buffer, uint32_t bufferOffset,
                            uint32_t bufferLength, uint8_t addNewLine,
                            uint8_t printASCII) const {
  ptxCommon_Print_Buffer(buffer, bufferOffset, bufferLength, addNewLine,
                         printASCII);
}

void IotReader::printStatusMessage(const char *message,
                                   ptxStatus_t status) const {
  ptxCommon_PrintStatusMessage(message, status);
}

void IotReader::printRevisionInfo() const {
  auto status = ptxStatus_Success;
  uint32_t revInfo;

  ptxCommon_PrintF("\n");
  ptxCommon_PrintF("Revision Information:\n");

  /* get C-Stack Revision */
  status = ptxIoTRd_Get_Revision_Info(m_iotRd.get(), RevInfo_C_Stack, &revInfo);
  if (ptxStatus_Success == status) {
    ptxCommon_PrintF("C-Stack Revision.......: 0x%04X\n", revInfo);
  }

  /* get Chip-ID/-revision */
  status = ptxIoTRd_Get_Revision_Info(m_iotRd.get(), RevInfo_ChipID, &revInfo);
  if (ptxStatus_Success == status) {
    ptxCommon_PrintF("Chip-ID................: 0x%02X\n", revInfo);
  }

  ptxCommon_PrintF("\n");

  if (status != ptxStatus_Success) {
    ptxCommon_PrintF(
        "Print Revision Information...FAILED (Internal Error)\n\n");
  }
}

void IotReader::printErrorInfo(ptxStatus_t status) const {
  /* Error codes are documented in ptxStatus.h
     ptxStatus_Comps_t is the component code
     ptxStatus_Values_t is the status code */
  ptxCommon_PrintF("COMP: %d STATUS: %d\n", PTX_GET_COMP(status),
                   PTX_GET_STATUS(status));
}