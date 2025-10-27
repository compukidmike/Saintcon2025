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
    File        : PTX105R_Card_Emulation.ino

    Description : Card emulation example
*/

/*
How to use the example:
1. Open the Serial Monitor. Application data will be printed to the screen,
   which may be observed.
2. Place a reader or a phone on the antenna (NFC must be enabled) to read the
   NDEF message.

By default, the NDEF message https://renesas.com is stored. It can be modified
by changing the vector called "message" below.
*/

#include <IotReader.h>

using namespace PtxIotReader;

bool status;

// renesas.com using https
const std::vector<uint8_t> message = {0xD1, 0x01, 0x0C, 0x55, 0x04, 0x72,
                                      0x65, 0x6E, 0x65, 0x73, 0x61, 0x73,
                                      0x2E, 0x63, 0x6F, 0x6D};

void setup() {
  Serial.begin(115200);
  while (!Serial);  // wait for the port to be opened
  // initialize the reader instance
  status = IotReader::getReader().begin();

  if (!status) {
    Serial.println("ERROR: failed to initialize IoT reader");
    return;
  }

  Serial.println("Card emulation example started.");
  Serial.println("Place a reader on the antenna!");
  Serial.println();
}

void loop() {
  if (status) {
    IotReader::getReader().emulateCard(message);
  }
}