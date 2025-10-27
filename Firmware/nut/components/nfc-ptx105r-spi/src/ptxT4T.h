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
    File        : ptxT4T.h

    Description :
*/

#ifndef PTX_T4T_
#define PTX_T4T_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <stdint.h>
#include "ptxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define PTX_T4T_CC_FILE_SIZE                (uint16_t)15    /**< Fixed Capability Container-File size (supporting 1 NDEF-FIle) */
#define PTX_T4T_CC_FILE_SIZE_H              (uint8_t)((PTX_T4T_CC_FILE_SIZE >> 8) & 0xFF)
#define PTX_T4T_CC_FILE_SIZE_L              (uint8_t)((PTX_T4T_CC_FILE_SIZE >> 0) & 0xFF)

#define PTX_T4T_CC_MAPPING_VERSION          (uint16_t)0x20  /**< NDEF-Mapping Version */

#define PTX_T4T_CC_MLE                      (uint16_t)128   /**< Max. data size that can be read from the T4T using a single ReadBinary command. */
#define PTX_T4T_CC_MLE_H                    (uint8_t)((PTX_T4T_CC_MLE >> 8) & 0xFF)
#define PTX_T4T_CC_MLE_L                    (uint8_t)((PTX_T4T_CC_MLE >> 0) & 0xFF)

#define PTX_T4T_CC_MLC                      (uint16_t)128   /**< Max. data size that can be sent to the T4T using a single UpdateBinary command. */
#define PTX_T4T_CC_MLC_H                    (uint8_t)((PTX_T4T_CC_MLC >> 8) & 0xFF)
#define PTX_T4T_CC_MLC_L                    (uint8_t)((PTX_T4T_CC_MLC >> 0) & 0xFF)

#define PTX_T4T_NDEF_MESSAGE_FILE_SIZE      (uint16_t)256   /**< 256 Byte(s) NDEF File Size consisting of 2 Byte(s) length + 254 Byte(s) message */
#define PTX_T4T_NDEF_MESSAGE_FILE_SIZE_H    (uint8_t)((PTX_T4T_NDEF_MESSAGE_FILE_SIZE >> 8) & 0xFF)
#define PTX_T4T_NDEF_MESSAGE_FILE_SIZE_L    (uint8_t)((PTX_T4T_NDEF_MESSAGE_FILE_SIZE >> 0)&  0xFF)

#define PTX_T4T_NDEF_ACCESS_FULL_READ       (uint8_t)0x00   /**< Read-Access from NDEF-File */
#define PTX_T4T_NDEF_ACCESS_FULL_WRITE      (uint8_t)0x00   /**< Write-Access to NDEF-File */

/**
 * NFC Forum Application-ID
 */
static const uint8_t PTX_T4T_APPLICATION_IDENTIFIER[] =
{
        0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01,
};

/**
 * NFC Forum Capability Container File-ID
 */
static const uint8_t PTX_T4T_CC_FILE_IDENTIFIER[] =
{
        0xE1, 0x03,
};

/**
 * NFC Forum NDEF File-ID
 */
static const uint8_t PTX_T4T_NDEF_FILE_IDENTIFIER[] =
{
        0xE1, 0x04,
};

static const uint8_t PTX_T4T_CC_FILE[] =
{
        /* Capability Container-File Part */
        PTX_T4T_CC_FILE_SIZE_H, PTX_T4T_CC_FILE_SIZE_L,
        PTX_T4T_CC_MAPPING_VERSION,
        PTX_T4T_CC_MLE_H, PTX_T4T_CC_MLE_L,
        PTX_T4T_CC_MLC_H, PTX_T4T_CC_MLC_L,

        /* NDEF-File Control Part (TLV-Format) */
        0x04,                                               /**< T-Field: NDEF-File-Control TLV Tag */
        0x06,                                               /**< L-Field: NDEF-File-Control TLV Length */
        0xE1, 0x04,                                         /**< V-Field: NDEF File Identifier (see definition above) */
        PTX_T4T_NDEF_MESSAGE_FILE_SIZE_H, PTX_T4T_NDEF_MESSAGE_FILE_SIZE_L,
        PTX_T4T_NDEF_ACCESS_FULL_READ,
        PTX_T4T_NDEF_ACCESS_FULL_WRITE,
};

/**
 * T4T Internal Application States
 */
typedef enum ptxT4T_State
{
    T4T_State_Reset,
    T4T_State_NDEF_Application_Selected,
    T4T_State_CC_File_Selected,
    T4T_State_NDEF_File_Selected,

}ptxT4T_State_t;

/**
 * T4T Initialization Parameters
 */
typedef struct ptxT4T_InitParams
{
    uint8_t                 *DefaultNDEFMessage;
    uint16_t                DefaultNDEFMessageLength;

}ptxT4T_InitParams_t;

/**
 * T4T interface structure
 */
typedef struct ptxT4T
{
    ptxStatus_Comps_t       CompId;                                         /**< Component Id. */
    ptxT4T_State_t          State;                                          /**< (Application) State of the T4T processor. */
    uint8_t                 NDEFMessage[PTX_T4T_NDEF_MESSAGE_FILE_SIZE];

}ptxT4T_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the T4T component.
 *
 * This function initializes the T4T component and allows to pass customer-specific initialization parameters.
 *
 * \param[in]   t4tComp          Pointer to Tag Interface component.
 * \param[in]   initParams       Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxT4T_Init (ptxT4T_t *t4tComp, ptxT4T_InitParams_t *initParams);

/**
 * \brief Reset Application.
 *
 * This function is used to reset the internal application state (e.g. to be called when a field-off event is detected at a higher level).
 *
 * \param[in]   t4tComp          Pointer to Tag Interface component.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxT4T_Reset (ptxT4T_t *t4tComp);

/**
 * \brief Update NDEF-Message.
 *
 * This function is optional and allows to update the default NDEF message (set via ptxT4T_Init()).
 *
 * \param[in]   t4tComp          Pointer to Tag Interface component.
 * \param[in]   ndefMessage      Pointer to new NDEF-message.
 * \param[in]   ndefMessageLengh Size / Length of new NDEF-message.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxT4T_UpdateNDEFMessage (ptxT4T_t *t4tComp, uint8_t *ndefMessage, uint16_t ndefMessageLengh);

/**
 * \brief T4T command processor / interpreter.
 *
 * This function is the main entry point for any T4T command processing after initialization. It takes the received command as an input
 * and prepares a corresponding response which is stored in the in/out Tx-buffer and -length parameters.
 *
 * \param[in]       t4tComp         Pointer to Tag Interface component.
 * \param[in]       rxData          Pointer to the received data / command from the Reader.
 * \param[in]       rxDataLen       Length of the received data / command.
 * \param[in]       txData          Buffer to store response data.
 * \param[in,out]   txDdataLen      In: Size of the buffer, Out: Length of the response data to be sent.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxT4T_Process(ptxT4T_t *t4tComp, uint8_t *rxData, uint8_t rxDataLen, uint8_t *txData, uint8_t *txDdataLen);

/**
 * \brief De-Initializes / close the T4T component.
 *
 * This function frees up previously allocated ressources (if any).
 *
 * \param[in]   t4tComp          Pointer to Tag Interface component.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxT4T_DeInit (ptxT4T_t *t4tComp);

#ifdef __cplusplus
}
#endif

#endif /* Guard */
