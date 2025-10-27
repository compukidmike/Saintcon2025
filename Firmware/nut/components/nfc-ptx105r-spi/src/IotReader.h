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
    File        : IotReader.h

    Description : IOT Reader library using the NON-OS PTX IOT Reader SDK v7.2.0.
*/

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// forward declarations
typedef uint16_t ptxStatus_t;
typedef struct ptxIoTRd ptxIoTRd_t;
typedef struct ptxT4T ptxT4T_t;
typedef struct ptxNativeTag_T5T ptxNativeTag_T5T_t;
typedef struct ptxNativeTag_T5T_InitParams ptxNativeTag_T5T_InitParams_t;
typedef struct ptxNDEF ptxNDEF_t;

namespace PtxIotReader {
static const uint8_t ptxStatusSuccess = 0U; /**< Success status code. */

/**
 * \brief NDEF payload types.
 */
enum class NdefMessageType : uint8_t {
  Empty, /**< NFC Forum "Empty" type (Type Name Format == 0). */
  Text,  /**< NFC Forum Well-known-type "T". */
  URI,   /**< NFC Forum Well-known-type "U". */
  Media, /**< MIME media-type. */
  Other, /**< Not parsed by the library. */
};

/**
 * \brief NDEF Type Name Formats.
 */
enum class NdefTypeNameFormat : uint8_t {
  Empty,
  WellKnown,
  Media,
  AbsoluteURI,
  External,
  Unknown,
  Unchanged,
  Reserved,
};

/**
 * \brief NDEF record data.
 */
typedef struct iotRd_ndefRecord {
  NdefTypeNameFormat typeNameFormat; /**< Type Name Format. */
  NdefMessageType messageType;       /**< Message type. */
  std::string payloadType;           /**< Payload type. */
  std::vector<uint8_t> payload;      /**< Payload data. */
} NdefRecord;

/**
 * \brief Generic Status / State Identifier.
 */
enum class StatusType : uint8_t {
  System,
  Discover,
  DeactivateSleep,
  LastRFError,
};

/**
 * \brief Card RF-Protocol types.
 */
enum class CardProtocol : uint8_t {
  Undefined = 0U,
  T2T = 2U,
  T3T = 3U,
  ISODEP = 4U,
  NFCDEP = 5U,
  T5T = 6U,
};

/**
 * \brief Card RF-Tech types.
 */
enum class CardTech : uint8_t {
  TypeA = 0U,
  TypeB = 1U,
  TypeF = 2U,
  TypeV = 6U,
};

/**
 * \brief Card details structure.
 */
typedef struct iotRd_activeCard {
  std::vector<uint8_t> id; /**< Card ID. */
  CardTech technology;     /**< Card RF-technology. */
  CardProtocol protocol;   /**< Card protocol. */
} ActiveCard;

/**
 * \brief RF-polling configurations.
 */
typedef struct iotRd_pollingConfig {
  uint8_t pollTypeA;     /**< Enable Type A technology (!= 0) or disable (0). */
  uint8_t pollTypeB;     /**< Enable Type B technology (!= 0) or disable (0). */
  uint8_t pollTypeF212;  /**< Enable Type F technology at 212 Kbps (!= 0) or
                            disable (0). */
  uint8_t pollTypeV;     /**< Enable Type V technology (!= 0) or disable (0). */
  uint32_t idleTime;     /**< IDLE-Time between Polling cycles in [ms]. */
  uint8_t discoverMode;  /** 0 = Regular Polling, 1 =
                            Low-Power Card Detection (LPCD), 2-255 = LPCD with
                            every n-th Cycle Regular Polling */
  uint8_t enableStandBy; /** Activates Stand-by mode when LPCD is used. */
} PollingConfig;

/**
 *  IOT Reader singleton class.
 */
class IotReader {
 public:
  /**
   * \brief Delete copy constructor.
   */
  IotReader(const IotReader &obj) = delete;

  /**
   * \brief Gets the reference to the singleton class instance.
   * \return Reference to IotReader instance.
   */
  static IotReader &getReader();

  /**
   * \brief Initialize the IOT reader.
   * \return True or false in case of success or failure.
   */
  bool begin();

  /**
   * \brief Deactivate and deinitialize the IOT reader.
   * \return True or false in case of success or failure.
   */
  bool end();

  /**
   * \brief Polls for the configured card technology types.
   * Polling will be stared, if not already ongoing.
   * \param[in] config Polling configuration.
   * \return True, if a card is activated. False in any other
   * case, including when multiple cards are detected.
   */
  bool detectCard(const PollingConfig &config);

  /**
   * \brief Start emulating a Type-4 card with a custom NDEF message.
   * \param[in] message NDEF message.
   * \return True if reader was detected and data exchange was successful. False
   * in any other cases.
   */
  bool emulateCard(const std::vector<uint8_t> &message);

  /**
   * \brief Read tag types 2-5 using the generic NDEF component.
   * By default, polling will be stopped after the operation.
   * \param[in,out] message Buffer to store read message.
   * \return True or false in case of success or failure.
   */
  bool ndefRead(std::vector<uint8_t> &message);

  /**
   * \brief Write tag types 2-5 using the generic NDEF component.
   * By default, polling will be stopped after the operation.
   * \param[in] message Message to be written.
   * \return True or false in case of success or failure.
   */
  bool ndefWrite(const std::vector<uint8_t> &message);

  /**
   * \brief Start polling for cards.
   * \param[in] config PollingConfig structure to configure polling.
   * \return True or false in case of success or failure.
   */
  bool pollingStart(const PollingConfig &config = {1U, 1U, 1U, 1U, 500U, 10U,
                                                   1U});

  /**
   * \brief Stop polling for cards.
   * \return True or false in case of success or failure.
   */
  bool pollingStop();

  /**
   * \brief Restarts polling and resets internal card registry.
   * \return True or false in case of success or failure.
   */
  bool restartDiscovery() const;

  /**
   * \brief Wrapper for sleep functionality in ms, based on platform dependent
   * timer.
   */
  void sleep(uint32_t timeout) const;

  /**
   * \brief Returns a pointer to the SDK's ptxIoTRd_t structure.
   * \return Shared pointer to the SDK's ptxIoTRd_t structure.
   */
  std::shared_ptr<ptxIoTRd_t> getContext() const { return m_iotRd; };

  /**
   * \brief Returns information about the active card.
   * \return Reference to the active card structure.
   */
  ActiveCard const &getCardInfo() const { return m_activeCard; };

  /**
   * \brief Parses NDEF record data.
   * \param[in] message NDEF record to be parsed.
   * \return NDEF record structure containing the Type Name Format, payload type
   * and payload contents.
   */
  NdefRecord getNdefMessage(const std::vector<uint8_t> &message) const;

  /**
   * \brief Function to get internal status information.
   * \param[in] statusType Defines type of status / state info to get.
   * StatusType_Discover (Status of RF-Discovery procedure):
   *  0x00 = No card discovered.
   *  0x01 = Card found and activated.
   *  0x02 = Multiple cards found, discovery ongoing.
   *  0x03 = Multiple cards found, discovery finished - user needs to select
   *  which card to activate.
   * StatusType_DeactivateSleep (Status of non-blocking deactivate sleep):
   *  0x00 = Deactivate (Sleep non-blocking) operation ongoing / not finished.
   *  0x01 = Deactivate (Sleep non-blocking) operation finished.
   * StatusType_LastRFError (Last received RF-Error):
   *  0x00 = No Error.
   *  0xFF = Unknown Error.
   *  0x09 = Overcurrent limiter activated (warning).
   *  0x12 = RF Timeout error.
   *  0x13 = RF Transmission error (CRC/Parity).
   *  0x14 = RF Protocol error.
   * \return Pair of success/error code and the status for the requested type.
   */
  std::pair<ptxStatus_t, uint8_t> getStatusInfo(StatusType statusType) const;

  /**
   * \brief Function to print a message with a status code.
   * \param[in] message Pointer to message.
   * \param[in] status Status code.
   */
  void printStatusMessage(const char *message, ptxStatus_t status) const;

  /**
   * \brief Function to print buffer data in hex format, starting from
   * the offset and length defined by input parameters. Add new line after
   * printing the data if addNewLine not 0.
   */
  void printBuffer(uint8_t *buffer, uint32_t bufferOffset,
                   uint32_t bufferLength, uint8_t addNewLine,
                   uint8_t printASCII) const;

  /**
   * \brief Function to print component and error code based on the provided
   * status code.
   * \param[in] status Status code.
   */
  void printErrorInfo(ptxStatus_t status) const;

  /**
   * \brief Function to print revision information from the PTX chip.
   */
  void printRevisionInfo() const;

 private:
  std::shared_ptr<ptxIoTRd_t> m_iotRd;
  std::unique_ptr<ptxNDEF_t> m_ndefComp;
  std::unique_ptr<ptxT4T_t> m_t4tComp;
  std::vector<uint8_t> m_ndefTxBuffer{};
  std::vector<uint8_t> m_ndefRxBuffer{};
  std::vector<uint8_t> m_ndefWorkBuffer{};
  std::vector<uint8_t> m_hceNdefMessage{};
  ActiveCard m_activeCard;
  IotReader();
  ptxStatus_t deInit();
  ptxStatus_t ndefInit();
  void updateCardRegistry();
  ptxStatus_t listeningStart();
  bool m_isPolling{false};
  bool m_isListening{false};
};
}  // namespace PtxIotReader