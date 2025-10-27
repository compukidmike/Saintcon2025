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
    Module      : NATIVE TAG API
    File        : ptxNativeTag_T4T.c

    Description : Native Tag API for NFC Forum Tag Type 4 (IOT READER - Extension)
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptx_IOT_READER.h"
#include "ptxNativeTag_T4T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \name T4T Instruction Codes.
 * @{
 */
#define PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING     (uint8_t)0x00   /**< Class BYte for non-secure messaging mode. */
#define PTX_T4T_INSTRUCTION_CODE_SELECT             (uint8_t)0xA4   /**< Select operation code. */
#define PTX_T4T_INSTRUCTION_CODE_READ_BINARY        (uint8_t)0xB0   /**< Read operation code. */
#define PTX_T4T_INSTRUCTION_CODE_READ_BINARY_ODO    (uint8_t)0xB1   /**< Read operation code using ODO. */
#define PTX_T4T_INSTRUCTION_CODE_UPDATE_BINARY      (uint8_t)0xD6   /**< Write operation code. */
#define PTX_T4T_INSTRUCTION_CODE_UPDATE_BINARY_ODO  (uint8_t)0xD7   /**< Write operation code using ODO. */
/** @} */

/**
 * \name T4T TLV Defines.
 * @{
 */
#define PTX_T4T_BER_TLV_T_FIELD                     (uint8_t)0x54   /**< BER-TLV T-value. */
#define PTX_T4T_DATA_TLV_T_FIELD                    (uint8_t)0x53   /**< Data-TLV T-value. */
/** @} */

/**
 * \name T4T Operation Defines.
 * @{
 */
#define PTX_T4T_P1_SELECT_NDEF_APP                  (uint8_t)0x04   /**< Select NDEF Application command P1-value. */
#define PTX_T4T_P2_SELECT_NDEF_APP                  (uint8_t)0x00   /**< Select NDEF Application command P2-value. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Set Command APDU.
 *
 * \param[in] t4t               Pointer to component.
 * \param[in] classByte         APDU Class Byte.
 * \param[in] instructionByte   Instruction Code.
 * \param[in] paramByte1        P1.
 * \param[in] paramByte2        P2.
 * \param[in,out] bytesWritten  Buffer Offset on input and output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNativeTag_T4TSetCommandAPDU (ptxNativeTag_T4T_t *t4tComp,
        uint8_t classByte,
        uint8_t instructionByte,
        uint8_t paramByte1,
        uint8_t paramByte2,
        uint8_t *bytesWritten);

/**
 * \brief Set Command APDU.
 *
 * \param[in] t4t               Pointer to component.
 * \param[in] instructionByte   Instruction Code.
 * \param[in,out] bytesWritten  Buffer Offset on input and output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNativeTag_T4TSetODOCommandAPDU (ptxNativeTag_T4T_t *t4tComp,
        uint8_t classByte,
        uint8_t instructionByte,
        uint8_t *bytesWritten);

/**
 * \brief Parse payload to internal Tx buffer.
 *
 * \param[in] t4tComp           Pointer to component.
 * \param[in,out] idx           Offset within buffer.
 * \param[in] instructionByte   Command code.
 * \param[in] p1                Command P1.
 * \param[in] p2                Command P2.
 * \param[in] useODO            Flag whether ODO is in use.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNativeTag_T4TParsePayload(ptxNativeTag_T4T_t *t4tComp, uint8_t *idx, uint8_t instructionByte, uint8_t p1, uint8_t p2, uint8_t useODO);

/**
 * \brief Set Maximum Transfer Unit.
 *
 * \param[in] useODO                Flag whether ODO is in use.
 * \param[in] updateOperation       Flag whether the Operation is a write.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static uint8_t ptxNativeTag_T4TSetMTU (uint8_t useODO, uint8_t updateOperation);

/**
 * \brief Data Exchange with a T4T.
 *
 * \param[in] t4t               Pointer to component.
 * \param[in] txLen             Length of Data in internal TxBuffer to be sent.
 * \param[in,out] rx            Response Buffer.
 * \param[in,out] rxLen         Length of Response Buffer on input, Length of Response on output.
 * \param[in] msTimeout         Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNativeTag_T4TTransceive (ptxNativeTag_T4T_t *t4tComp, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNativeTag_T4TOpen (ptxNativeTag_T4T_t *t4tComp, ptxNativeTag_T4T_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != t4tComp) && (NULL != initParams))
    {
        if ((NULL != initParams->IotRd) && (NULL != initParams->TxBuffer) && (PTX_T4T_MIN_TX_BUFFER_SIZE <= initParams->TxBufferSize))
        {
            /* clear component */
            (void)memset(t4tComp, 0, sizeof(ptxNativeTag_T4T_t));

            /* set members */
            t4tComp->CompId = ptxStatus_Comp_NativeTag_T4T;
            t4tComp->IotRd = initParams->IotRd;
            t4tComp->TxBuffer = initParams->TxBuffer;

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T4TSelect (ptxNativeTag_T4T_t *t4tComp,
                                    uint8_t paramByte1,
                                    uint8_t paramByte2,
                                    uint8_t *data,
                                    uint8_t nbrDataBytes,
                                    uint8_t expectedResponseLen,
                                    uint8_t *rx,
                                    size_t *rxLen,
                                    uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t p1 = paramByte1;
    uint8_t p2 = paramByte2;
    uint8_t bytes_set = 0;
    uint8_t data_field[nbrDataBytes]; /* nbrDataBytes != 0 is given for select */

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != data) && (NULL != rx) && (NULL != rxLen))
    {
        /* fill the fields */

        t4tComp->Fields.data_length_field[0] = nbrDataBytes;
        (void)memcpy(&data_field[0],&data[0],(uint32_t)nbrDataBytes);
        t4tComp->Fields.nbr_data_length_bytes = 1;
        t4tComp->Fields.nbr_data_bytes = nbrDataBytes;

        if (0 != t4tComp->Fields.nbr_expected_length_bytes)
        {
            t4tComp->Fields.expected_length_field[0] = expectedResponseLen;
        }

        status = ptxNativeTag_T4TSetCommandAPDU(t4tComp,PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING,PTX_T4T_INSTRUCTION_CODE_SELECT,p1,p2,&bytes_set);

        /* push Le byte back, then fill data */
        if (0 != t4tComp->Fields.nbr_expected_length_bytes)
        {
            if (bytes_set > 0)
            {
                t4tComp->TxBuffer[bytes_set+nbrDataBytes-1] = t4tComp->TxBuffer[bytes_set-1];
                (void)memcpy(&t4tComp->TxBuffer[bytes_set-1],&data_field[0],nbrDataBytes);
                bytes_set = (uint8_t)(bytes_set + nbrDataBytes);
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
            }
        }
        else
        {
            (void)memcpy(&t4tComp->TxBuffer[bytes_set],&data_field[0],nbrDataBytes);
            bytes_set = (uint8_t)(bytes_set + nbrDataBytes);
        }

        /* everything ok so far? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T4TTransceive(t4tComp,bytes_set,rx,rxLen,msTimeout);

            if (ptxStatus_Success == status)
            {
                (void)memset(&t4tComp->Fields,0,sizeof(ptxNativeTag_T4T_CommandAPDUFields_t));
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxNativeTag_T4TReadBinary (ptxNativeTag_T4T_t *t4tComp,
                                        uint16_t offset,
                                        uint8_t nbrExpectedResponseBytes,
                                        uint8_t *rx,
                                        size_t *rxLen,
                                        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t bytes_written;
    uint8_t p1;
    uint8_t p2;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != rx) && (NULL != rxLen))
    {

        /* offset in p1 and p2 */
        p1 = (uint8_t)((uint16_t)offset >> 8u);
        p2 = (uint8_t)((uint16_t)offset & ((1u<<8u)-1u));

        /* fill the fields */
        t4tComp->Fields.expected_length_field[0] = nbrExpectedResponseBytes;
        t4tComp->Fields.nbr_data_bytes = 0;
        t4tComp->Fields.nbr_data_length_bytes = 0;
        t4tComp->Fields.nbr_expected_length_bytes = 1;


        status = ptxNativeTag_T4TSetCommandAPDU(t4tComp,PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING,PTX_T4T_INSTRUCTION_CODE_READ_BINARY,p1,p2,&bytes_written);

        /* everything ok so far? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T4TTransceive(t4tComp,bytes_written,rx,rxLen,msTimeout);

            if (ptxStatus_Success == status)
            {
                (void)memset(&t4tComp->Fields,0,sizeof(ptxNativeTag_T4T_CommandAPDUFields_t));
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T4TReadBinaryODO (ptxNativeTag_T4T_t *t4tComp,
        uint32_t offset,
        uint8_t nbrExpectedResponseBytes,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t bytes_written;

    /* offset contained in data field via BER-TLV */
    t4tComp->Fields.ber_tlv_data_field[0] = PTX_T4T_BER_TLV_T_FIELD;
    t4tComp->Fields.ber_tlv_data_field[1] = 0x03;

    t4tComp->Fields.ber_tlv_data_field[2] =  (uint8_t)(offset>>16u); /* byte index 1 (index 0 unused) */
    t4tComp->Fields.ber_tlv_data_field[3] = (uint8_t)((uint16_t)offset >> 8u); /* byte index 2 */
    t4tComp->Fields.ber_tlv_data_field[4] = ((uint16_t)offset & ((1u<<8u)-1u)); /* byte index 3 */

    t4tComp->Fields.data_length_field[0] = sizeof(t4tComp->Fields.ber_tlv_data_field); /* first value */
    t4tComp->Fields.nbr_data_length_bytes = 1;

    /* fill the fields */
    t4tComp->Fields.expected_length_field[0] = (uint8_t)(nbrExpectedResponseBytes + 2);
    t4tComp->Fields.nbr_expected_length_bytes = 1;

    status = ptxNativeTag_T4TSetODOCommandAPDU(t4tComp,PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING,PTX_T4T_INSTRUCTION_CODE_READ_BINARY_ODO,&bytes_written);

    /* everything ok so far? - send data */
    if (ptxStatus_Success == status)
    {
        status = ptxNativeTag_T4TTransceive(t4tComp,bytes_written,rx,rxLen,msTimeout);

        if (ptxStatus_Success == status)
        {
            (void)memset(&t4tComp->Fields,0,sizeof(ptxNativeTag_T4T_CommandAPDUFields_t));
        }
    }

    return status;
}

ptxStatus_t ptxNativeTag_T4TUpdateBinary (ptxNativeTag_T4T_t *t4tComp,
        uint16_t offset,
        uint8_t *data,
        uint8_t nbrDataBytes,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t bytes_set = 0;
    uint8_t p1;
    uint8_t p2;
    uint8_t data_field[PTX_T4T_MIN_TX_BUFFER_SIZE];

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != data) && (NULL != rx) && (NULL != rxLen))
    {
        /* offset in p1 and p2 */
        p1 = (uint8_t)((uint16_t)offset >> 8u);
        p2 = (uint8_t)((uint16_t)offset & 0xFF);

        /* fill the fields */
        t4tComp->Fields.data_length_field[0] = nbrDataBytes;
        t4tComp->Fields.nbr_data_length_bytes = 1;
        (void)memcpy(&data_field[0],data,(uint32_t)nbrDataBytes);
        t4tComp->Fields.nbr_data_bytes = nbrDataBytes;

        status = ptxNativeTag_T4TSetCommandAPDU(t4tComp,PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING,PTX_T4T_INSTRUCTION_CODE_UPDATE_BINARY,p1,p2,&bytes_set);

        (void)memcpy(&t4tComp->TxBuffer[bytes_set], &data_field[0], t4tComp->Fields.nbr_data_bytes);
        bytes_set = (uint8_t)(bytes_set + t4tComp->Fields.nbr_data_bytes);

        /* everything ok so far? - send data */
        if (ptxStatus_Success == status)
        {
            status = ptxNativeTag_T4TTransceive(t4tComp,bytes_set,rx,rxLen,msTimeout);

            if (ptxStatus_Success == status)
            {
                (void)memset(&t4tComp->Fields,0,sizeof(ptxNativeTag_T4T_CommandAPDUFields_t));
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNativeTag_T4TUpdateBinaryODO (ptxNativeTag_T4T_t *t4tComp,
        uint32_t offset,
        uint8_t *data,
        uint8_t nbrDataBytes,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t data_field_ODO[nbrDataBytes + 2];
    uint8_t bytes_set = 0;

    /* offset contained in data field via BER-TLV */
    t4tComp->Fields.ber_tlv_data_field[0] = PTX_T4T_BER_TLV_T_FIELD;
    t4tComp->Fields.ber_tlv_data_field[1] = 0x03;

    t4tComp->Fields.ber_tlv_data_field[2] =  (uint8_t)(offset>>16u); //byte index 1 (index 0 unused)
    t4tComp->Fields.ber_tlv_data_field[3] = (uint8_t)((uint16_t)offset >> 8u); //byte index 2
    t4tComp->Fields.ber_tlv_data_field[4] = (uint8_t)((uint16_t)offset & 0xFF); //byte index 3

    /* fill the fields */
    data_field_ODO[0] = 0x53;
    data_field_ODO[1] = nbrDataBytes;
    (void)memcpy(&data_field_ODO[2],data,(uint32_t)nbrDataBytes);

    /* change LC field */
    t4tComp->Fields.nbr_data_length_bytes = 1;
    t4tComp->Fields.data_length_field[0] = (uint8_t)(nbrDataBytes+7); /* data + ODO + 2 bytes data structure */


    t4tComp->Fields.nbr_data_bytes = (uint16_t)(nbrDataBytes+2); /* data + DDO bytes, equal to length of data_field_ODO */

    status = ptxNativeTag_T4TSetODOCommandAPDU(t4tComp,PTX_T4T_CLASS_BYTE_NON_SECURE_MESSAGING,PTX_T4T_INSTRUCTION_CODE_UPDATE_BINARY_ODO,&bytes_set);

    /* everything ok so far? - send data */
    if (ptxStatus_Success == status)
    {
        (void)memcpy(&t4tComp->TxBuffer[bytes_set], &data_field_ODO[0], t4tComp->Fields.nbr_data_bytes);
        bytes_set = (uint8_t)(bytes_set + t4tComp->Fields.nbr_data_bytes);

        status = ptxNativeTag_T4TTransceive(t4tComp,bytes_set,rx,rxLen,msTimeout);

        if (ptxStatus_Success == status)
        {
            (void)memset(&t4tComp->Fields,0,sizeof(ptxNativeTag_T4T_CommandAPDUFields_t));
        }
    }

    return status;
}

ptxStatus_t ptxNativeTag_T4TClose (ptxNativeTag_T4T_t *t4tComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T))
    {
        memset(t4tComp, 0, sizeof(ptxNativeTag_T4T_t));
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNativeTag_T4TSetCommandAPDU (ptxNativeTag_T4T_t *t4tComp,
        uint8_t classByte,
        uint8_t instructionByte,
        uint8_t paramByte1,
        uint8_t paramByte2,
        uint8_t *bytesWritten)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t index = 0;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != bytesWritten))
    {
        t4tComp->TxBuffer[index] = classByte;
        index++;
        t4tComp->TxBuffer[index] = instructionByte;
        index++;
        t4tComp->TxBuffer[index] = paramByte1;
        index++;
        t4tComp->TxBuffer[index] = paramByte2;
        index++;

        status = ptxNativeTag_T4TParsePayload(t4tComp, &index, instructionByte, paramByte1, paramByte2, 0);

        *bytesWritten = index;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNativeTag_T4TSetODOCommandAPDU (ptxNativeTag_T4T_t *t4tComp,
        uint8_t classByte,
        uint8_t instructionByte,
        uint8_t *bytesWritten)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t index = 0;
    uint8_t paramByteODO = 0x00;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != bytesWritten))
    {
        t4tComp->TxBuffer[index] = classByte;
        index++;
        t4tComp->TxBuffer[index] = instructionByte;
        index++;
        t4tComp->TxBuffer[index] = paramByteODO; /* p1 */
        index++;
        t4tComp->TxBuffer[index] = paramByteODO; /* p2 */
        index++;

        status = ptxNativeTag_T4TParsePayload(t4tComp, &index, instructionByte, paramByteODO, paramByteODO, 1);

        *bytesWritten = index;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNativeTag_T4TParsePayload(ptxNativeTag_T4T_t *t4tComp, uint8_t *idx, uint8_t instructionByte, uint8_t p1, uint8_t p2, uint8_t useODO)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t index = 0;
    uint8_t max_transfer = 253u;
    uint8_t ber_tlv_len = 5u;   /* (T, L, V, V, V) */
    uint8_t command_header_len = 4u; /* (class,instruction,p1,p2) */

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != idx))
    {
        index = *idx;
        if ((PTX_T4T_INSTRUCTION_CODE_READ_BINARY == instructionByte) ||
                (PTX_T4T_INSTRUCTION_CODE_READ_BINARY_ODO == instructionByte))
        {
            max_transfer = ptxNativeTag_T4TSetMTU(useODO,0);
        }
        if (PTX_T4T_INSTRUCTION_CODE_UPDATE_BINARY_ODO == instructionByte)
        {
            max_transfer = ptxNativeTag_T4TSetMTU(useODO,1);
        }

        switch (t4tComp->Fields.nbr_data_length_bytes)
        {
            case 0:
                break;
            case 1:
                if (0 != t4tComp->Fields.data_length_field[0])
                {
                    if (max_transfer >= (command_header_len + t4tComp->Fields.data_length_field[0] + 1)) /* (class,instruction,p1,p2) + data + Le */
                    {
                        t4tComp->TxBuffer[index] = t4tComp->Fields.data_length_field[0];
                        index++;
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InsufficientResources);
                    }
                }
                break;
            default:
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter); /* invalid length */
                break;
        }

        if (0x00 != useODO)
        {
            memcpy(&t4tComp->TxBuffer[index], &t4tComp->Fields.ber_tlv_data_field[0], ber_tlv_len);
            index = (uint8_t)(index + ber_tlv_len);
        }

        switch (t4tComp->Fields.nbr_expected_length_bytes)
        {
            case 0:
                /* the NDEF App Select requires a set LE Byte */
                if ((0x00 == useODO) && (PTX_T4T_P1_SELECT_NDEF_APP == p1) && (PTX_T4T_P2_SELECT_NDEF_APP == p2))
                {
                    t4tComp->TxBuffer[index] = t4tComp->Fields.expected_length_field[0];
                    index++;
                    t4tComp->Fields.nbr_expected_length_bytes = 1;
                }
                break;
            case 1:
                t4tComp->TxBuffer[index] = t4tComp->Fields.expected_length_field[0];
                index++;
                break;
            default:
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter); /* invalid length */
                break;
        }

        *idx = index;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}

static uint8_t ptxNativeTag_T4TSetMTU (uint8_t useODO, uint8_t updateOperation)
{
    uint8_t transfer_len;
    if (0x00 == useODO)
    {
        transfer_len = 253u; /* maximal nbr of bytes PTX can send or receive in one go */
    }
    else
    {
        if (updateOperation)
        {
            transfer_len = 253u - 5u - 2u; /* maxPTXbytes - BERTLV - (0x53 Ld(1 byte)) */
        }
        else
        {
            transfer_len = 253u - 5u; /* maxPTXbytes - BERTLV */
        }
    }

    return transfer_len;
}

static ptxStatus_t ptxNativeTag_T4TTransceive (ptxNativeTag_T4T_t *t4tComp, uint32_t txLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t timeout_value;
    size_t rx_len;

    if (PTX_COMP_CHECK(t4tComp, ptxStatus_Comp_NativeTag_T4T) && (NULL != rx) && (NULL != rxLen))
    {
        /* send command */
        timeout_value = msTimeout;
        rx_len = *rxLen;

        status = ptxIoTRd_Data_Exchange(t4tComp->IotRd, &t4tComp->TxBuffer[0], txLen, rx, (uint32_t*)&rx_len, timeout_value);

        /* handle reception part */
        if (ptxStatus_Success == status)
        {
            if (0 == rx_len)
            {
                *rxLen = 0;
                status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_NscRfError);
            }
            else
            {
                *rxLen = rx_len;

                if ((rx[rx_len-2] != 0x90) || (rx[rx_len-1] != 0x00))
                {
                    status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidState);
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_NativeTag_T4T, ptxStatus_InvalidParameter);
    }

    return status;
}



