/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of
       conditions and the following disclaimer in the documentation and/or other
       materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



    THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX1K
    Module      : COMMON API
    File        : ptxCOMMON.h

    Description : Common API for common functions.
*/

/**
 * \addtogroup grp_ptx_api_common Common API
 *
 * @{
 */

#ifndef APIS_PTX_COMMON_H_
#define APIS_PTX_COMMON_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "ptxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * ####################################################################################################################
 * DEFINES
 * ####################################################################################################################
 */

/**
 * Command line input parameters for main.
 */
#define MAX_NUMBER_OF_IN_PARAMETERS     5

/**
 * Define Tx- and Rx-buffer size for the data exchanged via RF.
 * Tx- and Rx-buffers data is sent/received with API IoTRd_Data_Exchange.
 */
#define TX_BUFFER_SIZE                          (266u)                          /**< TX-Buffer. Length */
#define RX_BUFFER_SIZE                          (266u)                          /**< RX-Buffer. Length */
#define NDEF_BUFFER_SIZE                        (256u)                          /**< NDEF-Buffer. Length */


/**
 * Maximum number of characters to be printed per line.
 */
#define LINE_LENGTH                             (80u)                           /**< Maximum number of characters to be printed per line. */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/*
 * Target-platform specific PrintF Helper-function
 */
void ptxCommon_PrintF(const char *format, ...);


/*
 * Helper function to print buffer data in hex format, starting from the offset and length defined by input parameters.
 * Add new line after printing the data if addNewLine not 0.
 */
void ptxCommon_Print_Buffer(uint8_t *buffer, uint32_t bufferOffset, uint32_t bufferLength, uint8_t addNewLine, uint8_t printASCII);

/*
 * Helper Function to print Application status values with error-codes
 */
void ptxCommon_PrintStatusMessage(const char *message, ptxStatus_t st);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */
