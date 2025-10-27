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
    File        : Logging.h

    Description : Logging library
*/

#pragma once

#include <array>

#include "IotReader.h"

namespace PtxIotReader {
/**
 * \brief Mapping of protocol types to strings.
 */
const std::map<CardProtocol, std::string> cardProtocolTypes{
    {CardProtocol::Undefined, "Protocol: Undefined"},
    {CardProtocol::T2T, "Protocol: Type-2"},
    {CardProtocol::T3T, "Protocol: Type-3"},
    {CardProtocol::ISODEP, "Protocol: ISO-DEP"},
    {CardProtocol::NFCDEP, "Protocol: NFC-DEP"},
    {CardProtocol::T5T, "Protocol: Type-5"},
};

/**
 * \brief Mapping of RF-technology types to strings.
 */
const std::map<CardTech, std::string> rfTechTypes{
    {CardTech::TypeA, "RF-technology: Type-A"},
    {CardTech::TypeB, "RF-technology: Type-B"},
    {CardTech::TypeF, "RF-technology: Type-F"},
    {CardTech::TypeV, "RF-technology: Type-V"},
};

/**
 * \brief Mapping of NDEF Type Name Formats types to strings.
 */
const std::map<NdefTypeNameFormat, std::string> ndefTypeNameFormats{
    {NdefTypeNameFormat::Empty, "Empty"},
    {NdefTypeNameFormat::WellKnown, "NFC Well-Known"},
    {NdefTypeNameFormat::Media, "MIME Media-Type"},
    {NdefTypeNameFormat::AbsoluteURI, "Absolute URI"},
    {NdefTypeNameFormat::External, "External"},
    {NdefTypeNameFormat::Unknown, "Unknown"},
    {NdefTypeNameFormat::Unchanged, "Unchanged"},
    {NdefTypeNameFormat::Reserved, "Reserved"},
};

/**
 * \brief Array of NDEF message URI prefixes.
 */
const std::array<std::string, 36> ndefUriPrefixes{
    "",
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "tel:",
    "mailto:",
    "ftp://anonymous:anonymous@",
    "ftp://ftp.",
    "ftps://",
    "sftp://",
    "smb://",
    "nfs://",
    "ftp://",
    "dav://",
    "news:",
    "telnet://",
    "imap:",
    "rtsp://",
    "urn:",
    "pop:",
    "sip:",
    "sips:",
    "tftp:",
    "btspp://",
    "btl2cap://",
    "btgoep://",
    "tcpobex://",
    "irdaobex://",
    "file://",
    "urn:epc:id:",
    "urn:epc:tag:",
    "urn:epc:pat:",
    "urn:epc:raw:",
    "urn:epc:",
    "urn:nfc:",
};

/**
 * \brief Print the format, type and payload of the NDEF message.
 * \param[in] record The NDEF record to be printed.
 */
void printNdefMessage(const PtxIotReader::NdefRecord &record);

/**
 * \brief Handle empty NDEF record.
 * \param[in] record The record to be handled.
 */
void printNdefEmptyRecord(const PtxIotReader::NdefRecord &record);

/**
 * \brief Print NDEF Text record data.
 * \param[in] record The message to be printed.
 */
void printNdefTextRecord(const PtxIotReader::NdefRecord &record);

/**
 * \brief Print NDEF URI record data.
 * \param[in] record The message to be printed.
 */
void printNdefUriRecord(const PtxIotReader::NdefRecord &record);

/**
 * \brief Print NDEF Media record data.
 * \param[in] record The message to be printed.
 */
void printNdefMediaRecord(const PtxIotReader::NdefRecord &record);

/**
 * \brief Print the raw content of the NDEF record in ASCII and hexadecimal
 * format.
 * \param[in] record The message to be printed.
 */
void printNdefRawRecord(const PtxIotReader::NdefRecord &record);

/**
 * \brief Print the length and content of the NDEF message's payload in
 * hexadecimal format.
 * \param[in] payload The payload to be printed.
 */
void printPayload(const std::vector<uint8_t> &payload);

/**
 * \brief Print details about the discovered card:
 *  ID length
 *  ID in hexadecimal format
 *  RF-technology type
 *  Protocol type
 * \param[in] card Card details structure.
 */
void printCardInfo(const PtxIotReader::ActiveCard &card);

/**
 * \brief Print a buffer in hexadecimal format with '0' padding for single digit
 * values.
 * \param[in] buffer Data to be printed.
 * \param[in] printPrefix Print "0x" before each value.
 */
void printHexBuffer(const std::vector<uint8_t> &buffer,
                    bool printPrefix = false);

/**
 * \brief Array of pointers to NDEF message handlers.
 */
static void (*NdefMessageHandlers[])(const PtxIotReader::NdefRecord &record) = {
    printNdefEmptyRecord, printNdefTextRecord, printNdefUriRecord,
    printNdefMediaRecord, printNdefRawRecord,
};
}  // namespace PtxIotReader