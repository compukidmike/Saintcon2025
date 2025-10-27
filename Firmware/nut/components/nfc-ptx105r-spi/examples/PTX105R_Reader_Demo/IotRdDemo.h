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
    File        : IotRdDemo.h

    Description : Interface for the implementation of the SDK's reader demo
*/

#pragma once

#include <cstdint>
#include <memory>

// forward declarations
typedef uint16_t ptxStatus_t;
typedef struct ptxT4T ptxT4T_t;
typedef struct ptxIoTRd ptxIoTRd_t;
typedef struct ptxNativeTag_T5T ptxNativeTag_T5T_t;

namespace PtxIotRdDemo {
/**
 *  IOT Reader Demo singleton class.
 */

class IotRdDemo {
 public:
  /**
   * \brief Delete copy constructor.
   */
  IotRdDemo(const IotRdDemo &obj) = delete;

  /**
   * \brief Gets the reference to the singleton class instance.
   * \return Reference to IotRdDemo instance.
   */
  static IotRdDemo &getDemo();

  /**
   * \brief Initialize the demo.
   * \param context Pointer to the SDK's ptxIoTRd_t structure.
   * \return True or false in case of success or failure.
   */
  bool begin(std::shared_ptr<ptxIoTRd_t> context);

  /**
   * \brief This function runs the internal state machine, which controls the
   * NFC communication sequence:
   *  1. Polling for cards
   *  2. Activate card
   *  3. Exchange data with card
   *  4. Deactivate card
   *  5. Error state in case of a critical system error
   * \return True or false in case of success or failure.
   */
  bool run();

  /**
   * \brief Deinitialize the demo.
   * \return True or false in case of success or failure.
   */
  bool end();

 private:
  std::shared_ptr<ptxIoTRd_t> m_context;
  std::unique_ptr<ptxNativeTag_T5T_t> m_t5tComp;
  uint8_t m_exit;
  uint8_t m_systemState;
  IotRdDemo();
  void systemError();
  ptxStatus_t dataExchange();
};
}  // namespace PtxIotRdDemo