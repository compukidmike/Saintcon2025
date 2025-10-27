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
    File        : PTX105R_NDEF_Read.ino

    Description : NDEF read example
*/

/*
How to use the example:
1. Open the Serial Monitor.
2. Approach the antenna with a card. Information about the card and, if found,
   the NDEF message will be printed to the serial monitor. Reading several cards
   at the same time is not supported by the example.
*/

#include <IotReader.h>
#include <Logging.h>

using namespace PtxIotReader;

bool status;
/*
If true, the loop will wait until a character is received
over the console, before reading the next card.
If false, the loop will continuously read any card placed
into the field.
*/
const bool readOneByOne = false;

// Configure polling for type A, B, F and V types
// with 500 ms idle time between cycles
// Low Power Card Detection enabled with regular polling at every 10th cycle
// and stand-by mode enabled
const PollingConfig pollConfig = {.pollTypeA = 1U,
                                  .pollTypeB = 1U,
                                  .pollTypeF212 = 1U,
                                  .pollTypeV = 1U,
                                  .idleTime = 500U,
                                  .discoverMode = 10U,
                                  .enableStandBy = 1U};

void setup() {
  Serial.begin(115200);
  while (!Serial);  // wait for the port to be opened
  // initialize the reader instance
  status = IotReader::getReader().begin();

  if (!status) {
    Serial.println("ERROR: failed to initialize IoT reader");
    return;
  }

  Serial.println("Place a card on the antenna to read it.");
  Serial.println();
}

void loop() {
  if (status) {
    std::vector<uint8_t> messageBuffer;
    if (IotReader::getReader().detectCard(pollConfig)) {
      Serial.println("Card found");
      printCardInfo(IotReader::getReader().getCardInfo());
      if (IotReader::getReader().ndefRead(messageBuffer)) {
        // If an NDEF message was read, display it
        if (messageBuffer.size() > 0) {
          const NdefRecord message =
              IotReader::getReader().getNdefMessage(messageBuffer);
          printNdefMessage(message);
        }
      } else {
        Serial.println("No NDEF record found");
      }
      Serial.println();
      if (readOneByOne) {
        Serial.println();
        Serial.println(
            "Place a card on the antenna and send a character to "
            "read it");
        Serial.println();
        Serial.flush();
        while (!Serial.available());
        while (Serial.available()) {
          Serial.read();
        }
        Serial.println("Scanning for card ...");
        Serial.flush();
      }
    }
  }
}