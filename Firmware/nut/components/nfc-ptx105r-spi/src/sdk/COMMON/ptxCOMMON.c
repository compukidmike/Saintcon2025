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
    File        : ptxCOMMON.c

    Description : Common API for common functions.
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxCOMMON.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/*
 * Comment / Uncomment this #define to enable output via printf
 */
#define ENABLE_PRINTF_OPTION

#ifdef ENABLE_PRINTF_OPTION
    #include "ptxDBG_PORT.h"
    #include <stdarg.h>
    #include <stdio.h>
#endif

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

void ptxCommon_PrintF(const char *format, ...)
{
#ifdef ENABLE_PRINTF_OPTION
    va_list argptr;
    va_start(argptr, format);

    const size_t max_len = 256u;
    char buffer[max_len];
    buffer[max_len-1] = '\0';

    (void)vsnprintf(buffer, max_len, format, argptr);

    (void)ptxDBGPORT_Write(buffer);

    va_end(argptr);
#else
    (void)format;
#endif
}

void ptxCommon_Print_Buffer(uint8_t *buffer, uint32_t bufferOffset, uint32_t bufferLength, uint8_t addNewLine, uint8_t printASCII)
{
    uint32_t i;
    uint32_t lineIdx = 0;
    uint8_t character_to_print;

    if (NULL != buffer)
    {
        if (0 != bufferLength)
        {
            for (i = 0; (i < bufferLength) && (i < (uint32_t)TX_BUFFER_SIZE); i++)
            {
                if ((i > 0) && ((i % (LINE_LENGTH - 5) == 0)))
                {
                    lineIdx++;
                    ptxCommon_PrintF("\n     ");
                }

                if (0 == printASCII)
                {
                    ptxCommon_PrintF("%02X", (uint8_t)buffer[i + bufferOffset]);
                } else
                {
                    character_to_print = (uint8_t)buffer[i + bufferOffset];
                    /* avoid unintentional interpretation of ascii-commands */
                    if (character_to_print < 0x20)
                    {
                        ptxCommon_PrintF(".");
                    } else
                    {
                        ptxCommon_PrintF("%c", character_to_print);
                    }
                }
            }

            if (0 != addNewLine)
            {
                ptxCommon_PrintF("\n");
            }
        }
    }
}

void ptxCommon_PrintStatusMessage(const char *message, ptxStatus_t st)
{
    if (NULL != message)
    {
        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("%s ... OK\n", message);
        } else
        {
            ptxCommon_PrintF("%s ... ERROR (Status-Code = %04X)\n", message, st);
        }
    }
}

