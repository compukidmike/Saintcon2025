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

    Project     : Generic
    Module      : NFC Forum T4T Emulation w. NDEF
    File        : ptxT4T.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <string.h>
#include <stdint.h>
#include "ptxT4T.h"
#include "ptxHCE_ISO7816.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * LOCAL INTEGRATION FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
static void ptxT4T_SetStatusWord(uint8_t *txData, uint16_t txDataOffset, uint16_t statusWord);
static int ptxT4T_CheckID(const uint8_t *id1, uint8_t id1Len, const uint8_t *id2, uint8_t id2Len);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxT4T_Init (ptxT4T_t *t4tComp, ptxT4T_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ( (NULL != t4tComp) && (NULL != initParams))
    {
        (void)memset(t4tComp, 0, sizeof(ptxT4T_t));

        t4tComp->State = T4T_State_Reset;
        t4tComp->CompId = ptxStatus_Comp_T4TOP;

        /* set initial NDEF-message */
        status = ptxT4T_UpdateNDEFMessage(t4tComp, initParams->DefaultNDEFMessage, initParams->DefaultNDEFMessageLength);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxT4T_Reset (ptxT4T_t *t4tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_T4TOP))
    {
        t4tComp->State = T4T_State_Reset;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxT4T_UpdateNDEFMessage (ptxT4T_t *t4tComp, uint8_t *ndefMessage, uint16_t ndefMessageLengh)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_T4TOP)) &&
        (NULL != ndefMessage) &&
        (0 != ndefMessageLengh) &&
        (PTX_T4T_NDEF_MESSAGE_FILE_SIZE >= (ndefMessageLengh + 2U)))
    {
        /* Update NLEN-field */
        t4tComp->NDEFMessage[0] = (uint8_t)((ndefMessageLengh >> 8) & 0xFF);
        t4tComp->NDEFMessage[1] = (uint8_t)((ndefMessageLengh >> 0) & 0xFF);

        /* Update NDEF-message */
        (void)memcpy(&t4tComp->NDEFMessage[2], ndefMessage, ndefMessageLengh);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxT4T_Process(ptxT4T_t *t4tComp, uint8_t *rxData, uint8_t rxDataLen, uint8_t *txData, uint8_t *txDdataLen)
{
    ptxStatus_t status = ptxStatus_Success;

    /* APDU specific variables */
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
    uint8_t le;

    uint16_t file_offset;
    uint16_t nr_bytes_to_process;
    int result;
    const uint8_t *active_file;
    uint16_t active_file_size;
    uint16_t tx_status_word;
    uint16_t tx_offset = 0;
    uint8_t tx_length = PTX_HCE_ISO7816_SW_LEN;

    if ((PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_T4TOP)) &&
        (NULL != rxData) &&
        (0 != rxDataLen) &&
        (NULL != txData) &&
        (NULL != txDdataLen))
    {
        /* at least 2 Bytes are needed to set a Status-Word */
        if (*txDdataLen >= (uint32_t)PTX_HCE_ISO7816_SW_LEN)
        {
            if (rxDataLen >= PTX_HCE_ISO7816_HEADER_LEN)
            {
                /* get minimum APDU header variables*/
                cla = rxData[PTX_HCE_ISO7816_OFFSET_CLA];
                ins = rxData[PTX_HCE_ISO7816_OFFSET_INS];
                p1 = rxData[PTX_HCE_ISO7816_OFFSET_P1];
                p2 = rxData[PTX_HCE_ISO7816_OFFSET_P2];

                /* any supported Command from the NFC Forum T4T specification must use CLA-Byte == 0 */
                if (0x00 == cla)
                {
                    /* check supported commands */
                    switch (ins)
                    {
                        case PTX_HCE_ISO7816_INS_SELECT:
                            /* Select must consist of at least 6 Byte(s) */
                            if (rxDataLen >= 6U)
                            {
                                lc = rxData[PTX_HCE_ISO7816_OFFSET_LC];

                                /* Select by Name ? */
                                if (((uint8_t)0x04 == p1) && ((uint8_t)0x00 == p2))
                                {
                                    /* NDEF-Application to be selected -> LC must be exactly 7 Bytes */
                                    if (lc == (uint8_t)0x07)
                                    {
                                        /* Note: NDEF-Application is always possible from any state - no need for any checks here */

                                        /* check Application ID */
                                        result = ptxT4T_CheckID(&rxData[PTX_HCE_ISO7816_OFFSET_CDATA], lc, &PTX_T4T_APPLICATION_IDENTIFIER[0], (uint8_t)sizeof(PTX_T4T_APPLICATION_IDENTIFIER));

                                        /* Application-ID match ? */
                                        if (0 == result)
                                        {
                                            t4tComp->State = T4T_State_NDEF_Application_Selected;
                                            tx_status_word = PTX_HCE_ISO7816_SW_SUCCESS;

                                        } else
                                        {
                                            tx_status_word = PTX_HCE_ISO7816_SW_FILE_NOT_FOUND;
                                        }

                                    } else
                                    {
                                        tx_status_word = PTX_HCE_ISO7816_SW_FILE_NOT_FOUND;
                                    }
                                }
                                else if (((uint8_t)0x00 == p1) && ((uint8_t)0x0C == p2))
                                {
                                    /* NDEF- or CC-File to be selected -> LC must be exactly 2 Bytes in both cases */
                                    if (lc == (uint8_t)0x02)
                                    {
                                        if (T4T_State_Reset != t4tComp->State)
                                        {
                                            /* check which File should be selected */
                                            /* check Application ID */
                                            result = ptxT4T_CheckID(&rxData[PTX_HCE_ISO7816_OFFSET_CDATA], lc, &PTX_T4T_CC_FILE_IDENTIFIER[0], (uint8_t)sizeof(PTX_T4T_CC_FILE_IDENTIFIER));

                                            if (0 == result)
                                            {
                                                t4tComp->State = T4T_State_CC_File_Selected;
                                                tx_status_word = PTX_HCE_ISO7816_SW_SUCCESS;

                                            } else
                                            {
                                                result = ptxT4T_CheckID(&rxData[PTX_HCE_ISO7816_OFFSET_CDATA], lc, &PTX_T4T_NDEF_FILE_IDENTIFIER[0], (uint8_t)sizeof(PTX_T4T_NDEF_FILE_IDENTIFIER));

                                                if (0 == result)
                                                {
                                                    t4tComp->State = T4T_State_NDEF_File_Selected;
                                                    tx_status_word = PTX_HCE_ISO7816_SW_SUCCESS;

                                                } else
                                                {
                                                    tx_status_word = PTX_HCE_ISO7816_SW_FILE_NOT_FOUND;
                                                }
                                            }

                                        } else
                                        {
                                            tx_status_word = PTX_HCE_ISO7816_SW_FILE_NOT_FOUND;
                                        }

                                    } else
                                    {
                                        tx_status_word = PTX_HCE_ISO7816_SW_CONDITIONS_NOT_SATISFIED;
                                    }
                                }
                                else
                                {
                                    tx_status_word = PTX_HCE_ISO7816_SW_FILE_NOT_FOUND;
                                }

                            } else
                            {
                                tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
                            }
                            break;

                        case PTX_HCE_ISO7816_INS_READ_BINARY:
                            /* ReadBinary must consist of exactly 5 Byte(s) */
                            if (rxDataLen >= PTX_HCE_ISO7816_HEADER_LEN)
                            {
                                le = rxData[PTX_HCE_ISO7816_OFFSET_LE];

                                /* ReadBinary-operations are only allowed if the NDEF- or CC-file is selected */
                                if ((T4T_State_NDEF_File_Selected == t4tComp->State) || (T4T_State_CC_File_Selected == t4tComp->State))
                                {
                                    file_offset = (uint16_t)((p1 << 8) | p2);

                                    active_file = (T4T_State_NDEF_File_Selected == t4tComp->State) ? t4tComp->NDEFMessage : PTX_T4T_CC_FILE;
                                    active_file_size = (T4T_State_NDEF_File_Selected == t4tComp->State) ? PTX_T4T_NDEF_MESSAGE_FILE_SIZE : PTX_T4T_CC_FILE_SIZE;


                                    /* check out-of-bounds / correct length */
                                    if ((uint16_t)(file_offset + le) <= active_file_size)
                                    {
                                        if ((uint16_t)le <= PTX_T4T_CC_MLE)
                                        {
                                            /* Note: Byte LE == 0x00 means that the Card should send everything it has (limited by MAX_LE) */
                                            //nr_bytes_to_process = le == 0x00 ? ((PTX_T4T_CC_MLE > active_file_size) ? active_file_size : PTX_T4T_CC_MLE) : (uint16_t)le;
                                            nr_bytes_to_process = le == 0x00 ? PTX_T4T_CC_MLE : (uint16_t)le;

                                            /* Tx-Buffer large enough */
                                            if (nr_bytes_to_process <= *txDdataLen)
                                            {
                                                /* "Read" actual data */
                                                (void)memcpy(&txData[0], &active_file[file_offset], nr_bytes_to_process);
                                                tx_offset = nr_bytes_to_process;
                                                tx_status_word = PTX_HCE_ISO7816_SW_SUCCESS;
                                                tx_length = (uint8_t)(tx_length + nr_bytes_to_process);

                                            } else
                                            {
                                                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InsufficientResources);
                                            }

                                        } else
                                        {
                                            tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
                                        }

                                    } else
                                    {
                                        tx_status_word = PTX_HCE_ISO7816_SW_INCORRECT_P1P2;
                                    }

                                } else
                                {
                                    tx_status_word = PTX_HCE_ISO7816_SW_CONDITIONS_NOT_SATISFIED;
                                }
                            } else
                            {
                                tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
                            }
                            break;

                        case PTX_HCE_ISO7816_INS_UPDATE_BINARY:
                            /* ReadBinary must consist of at least 6 Byte(s) */
                            if (rxDataLen >= 6U)
                            {
                                lc = rxData[PTX_HCE_ISO7816_OFFSET_LC];

                                /* UpdateBinary-operations are only allowed if the NDEF-file is selected */
                                if (T4T_State_NDEF_File_Selected == t4tComp->State)
                                {
                                    file_offset = (uint16_t)((p1 << 8) | p2);

                                    /* check out-of-bounds / correct length */
                                    if ((uint16_t)(file_offset + lc) <= PTX_T4T_NDEF_MESSAGE_FILE_SIZE)
                                    {
                                        if ((uint16_t)lc <= PTX_T4T_CC_MLC)
                                        {
                                            nr_bytes_to_process = (uint16_t)lc;

                                            /* "Write" actual data */
                                            (void)memcpy(&t4tComp->NDEFMessage[file_offset], &rxData[PTX_HCE_ISO7816_OFFSET_CDATA], nr_bytes_to_process);
                                            tx_status_word = PTX_HCE_ISO7816_SW_SUCCESS;

                                        } else
                                        {
                                            tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
                                        }

                                    } else
                                    {
                                        tx_status_word = PTX_HCE_ISO7816_SW_INCORRECT_P1P2;
                                    }

                                } else
                                {
                                    tx_status_word = PTX_HCE_ISO7816_SW_CONDITIONS_NOT_SATISFIED;
                                }

                            } else
                            {
                                tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
                            }
                            break;

                        default:
                            tx_status_word = PTX_HCE_ISO7816_SW_FUNC_NOT_SUPPORTED;
                            break;
                    }

                } else
                {
                    tx_status_word = PTX_HCE_ISO7816_SW_COMMAND_NOT_ALLOWED;
                }

            } else
            {
                tx_status_word = PTX_HCE_ISO7816_SW_WRONG_LENGTH;
            }

            /* set Tx-Data (Status-Word) */
            if (ptxStatus_Success == status)
            {
                ptxT4T_SetStatusWord(txData, tx_offset, tx_status_word);
                *txDdataLen = tx_length;

            } else
            {
                *txDdataLen = 0;
            }

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InsufficientResources);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxT4T_DeInit (ptxT4T_t *t4tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_T4TOP))
    {
        /** Nothing to do */

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * LOCAL HELPER FUNCTIONS
 * ####################################################################################################################
 */

static void ptxT4T_SetStatusWord(uint8_t *txData, uint16_t txDataOffset, uint16_t statusWord)
{
    /* Note: Pointers / Offsets are assumed to be valid / checked by the calling function */

    txData[txDataOffset + 0] = (uint8_t)((statusWord >> 8) & (uint8_t)0xFF);
    txData[txDataOffset + 1] = (uint8_t)((statusWord >> 0) & (uint8_t)0xFF);
}

static int ptxT4T_CheckID(const uint8_t *id1, uint8_t id1Len, const uint8_t *id2, uint8_t id2Len)
{
    int ret_value = -1;

    /* Note: Pointers / Offsets are assumed to be valid / checked by the calling function */

    if (id1Len == id2Len)
    {
        ret_value = memcmp(id1, id2, id1Len);
    }

    return ret_value;
}
