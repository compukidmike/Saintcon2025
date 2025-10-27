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
    Module      : Logging
    File        : Logging.cpp

    Description : Logging library implementation
*/

#include "Logging.h"

#include <Arduino.h>

namespace PtxIotReader {

void printNdefMessage(const NdefRecord &record) {
  Serial.println("NDEF record found");
  Serial.print("Format: ");
  const auto format = ndefTypeNameFormats.find(record.typeNameFormat);

  if (format == ndefTypeNameFormats.end())
    Serial.print("ERROR: unknown Type Name Format");
  else
    Serial.println(format->second.c_str());

  const auto messageTypeIndex = static_cast<uint8_t>(record.messageType);

  // call the handler function matching the message type
  if (messageTypeIndex < static_cast<uint8_t>(sizeof(NdefMessageHandlers) /
                                              sizeof(NdefMessageHandlers[0]))) {
    (*NdefMessageHandlers[messageTypeIndex])(record);
  } else {
    Serial.println("ERROR: unknown NDEF message type");
  }
}

void printPayload(const std::vector<uint8_t> &payload) {
  Serial.print("Payload: ");
  Serial.print(payload.size());
  Serial.println(" bytes");
  Serial.print("(0x) ");
  printHexBuffer(payload);
  Serial.println();
}

void printCardInfo(const ActiveCard &card) {
  Serial.print("ID: ");
  Serial.print(card.id.size());
  Serial.println(" bytes");
  Serial.print("(0x) ");
  printHexBuffer(card.id);
  Serial.println();

  const auto technology = rfTechTypes.find(card.technology);
  if (technology == rfTechTypes.end())
    Serial.print("ERROR: unknown card technology");
  else
    Serial.println(technology->second.c_str());

  const auto protocol = cardProtocolTypes.find(card.protocol);
  if (protocol == cardProtocolTypes.end())
    Serial.print("ERROR: unknown card protocol");
  else
    Serial.println(protocol->second.c_str());
}

void printHexBuffer(const std::vector<uint8_t> &buffer, bool printPrefix) {
  for (const auto i : buffer) {
    if (printPrefix) Serial.print("0x");
    if (i <= 0x0F) Serial.print('0');  // padding
    Serial.print(i, HEX);
    Serial.print(" ");
  }
}

void printNdefRawRecord(const NdefRecord &record) {
  Serial.print("Content: ");
  for (const auto &i : record.payload) {
    Serial.print(static_cast<char>(i));
  }
  Serial.println();
  Serial.print("Raw content: ");
  Serial.print(record.payload.size());
  Serial.println(" bytes");
  Serial.print("(0x) ");
  printHexBuffer(record.payload);
  Serial.println();
}

void printNdefMediaRecord(const NdefRecord &record) {
  Serial.print("Type: ");
  Serial.println(record.payloadType.c_str());
  Serial.print("Value: ");
  for (const auto &i : record.payload) {
    Serial.print(static_cast<char>(i));
  }
  Serial.println();
  printPayload(record.payload);
}

void printNdefUriRecord(const NdefRecord &record) {
  Serial.println("Type: URI (U)");
  Serial.print("Value: ");
  Serial.print(ndefUriPrefixes[record.payload.front()].c_str());
  Serial.println(
      std::string(record.payload.begin() + 1U, record.payload.end()).c_str());
  printPayload(record.payload);
}

void printNdefTextRecord(const NdefRecord &record) {
  Serial.println("Type: Text (T)");
  const auto flags = record.payload.front();
  Serial.print("Encoding: ");
  if (flags & 0x80)
    Serial.println("UTF-16");
  else
    Serial.println("UTF-8");
  const auto languageCodeLength = static_cast<uint8_t>(flags & 0x3F);
  Serial.print("Language: ");
  Serial.println(std::string(record.payload.begin() + 1U,
                             record.payload.begin() + 1U + languageCodeLength)
                     .c_str());
  Serial.print("Value: ");
  Serial.println(std::string(record.payload.begin() + 1U + languageCodeLength,
                             record.payload.end())
                     .c_str());
  printPayload(record.payload);
}

void printNdefEmptyRecord([[maybe_unused]] const NdefRecord &record) {
  Serial.println("Empty message");
}
}  // namespace PtxIotReader