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
    File        : PTX105R_NDEF_Write.ino

    Description : NDEF write example
*/

/*
How to use the example:
1. Open the Serial Monitor.
2. Place a card on the antenna.
3. Observe the output on the serial monitor. Do not remove the card until the
   writing is finished, otherwise it might become corrupted.
4. After the card was written, remove it and place the next card on the antenna.
5. Type a character into the serial monitor and press Enter. Make sure "New
   Line" is set.
6. Observe the output on the console and repeat steps 3 to 5.

By default, the NDEF message https://renesas.com is written. It can be modified
by changing the vector called "message" below.
*/

#include <IotReader.h>
#include <Logging.h>

using namespace PtxIotReader;

bool status;

// renesas.com using https
const std::vector<uint8_t> message = {0xD1, 0x01, 0x0C, 0x55, 0x04, 0x72,
                                      0x65, 0x6E, 0x65, 0x73, 0x61, 0x73,
                                      0x2E, 0x63, 0x6F, 0x6D};

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

  Serial.println(
      "Place a card on the antenna and do not remove it, until it "
      "is written!");
  Serial.println();
}

void loop() {
  if (status) {
    if (IotReader::getReader().detectCard(pollConfig)) {
      Serial.println("Card found");
      printCardInfo(IotReader::getReader().getCardInfo());
      if (IotReader::getReader().ndefWrite(message)) {
        Serial.println("NDEF message written");
      } else {
        Serial.println("Could not write NDEF message");
        Serial.println("Make sure the card is NDEF-formatted");
      }
      Serial.println();
      Serial.println(
          "Place a card on the antenna and send a character to write it (New "
          "Line must be enabled).");
      Serial.println("Do not remove the card, until it is written!");
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