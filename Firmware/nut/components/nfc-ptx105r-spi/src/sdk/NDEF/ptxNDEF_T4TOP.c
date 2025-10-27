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
#include "ptxNDEF_T4TOP.h"
#include "ptxNativeTag_T4T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \name Status Flags.
 * @{
 */
#define PTX_T4T_SUCCESS_FLAG_SW1                                (uint8_t)0x90           /**< Status Flag OK 1. */
#define PTX_T4T_SUCCESS_FLAG_SW2                                (uint8_t)0x00           /**< Status Flag OK 2. */
#define PTX_T4T_STATUS_WORD_LENGTH                              (uint8_t)0x02           /**< Status Flag length. */
#define PTX_T4T_SW1_ERROR_WRONG_P1P2                            (uint8_t)0x6A           /**< Status flag Wrong P1P2. */
#define PTX_T4T_SW2_ERROR_FILE_OR_APP_NOT_FOUND                 (uint8_t)0x82           /**< Status flag Application or File not found. */
#define PTX_T4T_SELECT_FAIL_FLAG_SW1                            (uint8_t)0x6A           /**< Status Flag Select failed 1. */
#define PTX_T4T_SELECT_FAIL_FLAG_SW2                            (uint8_t)0x82           /**< Status Flag Select failed 2. */
#define PTX_T4T_READ_FAIL_FLAG_SW1_SIMPLE                       (uint8_t)0x67           /**< Status Flag Read failed 1. */
#define PTX_T4T_READ_FAIL_FLAG_SW2_SIMPLE                       (uint8_t)0x00           /**< Status Flag Read failed 2. */
#define PTX_T4T_READ_FAIL_FLAG_SW1_EXT                          (uint8_t)0x6C           /**< Status Flag Extended Read failed 1. */
/** @} */

/**
 * \name CC and TLV Defines.
 * @{
 */
#define PTX_T4T_NDEF_LENGTH                                     (uint8_t)0x08           /**< NDEF Information length. */
#define PTX_T4T_ENDEF_LENGTH                                    (uint8_t)0x0A           /**< Extended NDEF Information length. */
#define PTX_T4T_MAPPINGVERSION_2_0                              (uint8_t)0x20           /**< Mapping version 2.0. */
#define PTX_T4T_MAPPINGVERSION_MAJOR_2                          (uint8_t)2u             /**< Mapping major version 2. */
#define PTX_T4T_MAPPINGVERSION_MAJOR_3                          (uint8_t)3u             /**< Mapping major version 3. */
#define PTX_T4T_MAPPINGVERSION_3_0                              (uint8_t)0x30           /**< Mapping version 3.0. */
#define PTX_T4T_CC_LEN_MIN                                      (uint8_t)15u            /**< Minimum length of CC file. */
#define PTX_T4T_ACCESS_RW                                       (uint8_t)0x00           /**< Read and write access granted. */
/** @} */

/**
 * \name Defines for NDEF app select
 * @{
 */
#define P1_NDEF_APP_SELECT                                      (uint8_t)0x04           /**< P1 for NDEF Application Select command. */
#define P2_NDEF_APP_SELECT                                      (uint8_t)0x00           /**< P2 for NDEF Application Select command. */
#define LC_FIELD_NDEF_APP_SELECT                                (uint8_t)0x07           /**< Lc field for NDEF Application Select command. */
#define LE_FIELD_NDEF_APP_SELECT                                (uint8_t)0x00           /**< Le field for NDEF Application Select command. */
/** @} */

/**
 * \name Defines for CC file select
 * @{
 */
#define P1_CC_SELECT                                            (uint8_t)0x00           /**< P1 for CC File Select command. */
#define P2_CC_SELECT                                            (uint8_t)0x0C           /**< P2 for CC File Select command. */
#define LC_FIELD_CC_SELECT                                      (uint8_t)0x02           /**< Lc for CC File Select command. */
#define LE_FIELD_CC_SELECT                                      (uint8_t)0x00           /**< Le for CC File Select command. */
#define DATA_FIELD_CC_SELECT                                    (uint16_t)0xE103        /**< CC File Identifier. */
/** @} */

/**
 * \name Defines for NDEF file select
 * @{
 */
#define P1_NDEF_FILE_SELECT                                     (uint8_t)0x00           /**< P1 for NDEF File Select command. */
#define P2_NDEF_FILE_SELECT                                     (uint8_t)0x0C           /**< P2 for NDEF File Select command. */
#define LC_FIELD_NDEF_FILE_SELECT                               (uint8_t)0x02           /**< Lc for NDEF File Select command. */
#define LE_FIELD_NDEF_FILE_SELECT                               (uint8_t)0x00           /**< Le for NDEF File Select command. */
/** @} */
/**
 * \brief NDEF Application select data field.
 */
uint8_t PTX_T4T_DATA_FIELD_NDEF_APP_SELECT[]                    = {0xD2,0x76,0x00,0x00,0x85,0x01,0x01};

/**
 * \name Capacity Defines.
 * @{
 */
#define PTX_T4T_MAX_BYTES_SEND                                  (uint8_t)0xFD           /**< Internal maximum of bytes sendable. */
#define PTX_T4T_MAX_BYTES_RECEIVE                               (uint8_t)0xFD           /**< Internal maximum of bytes receivable. */
#define PTX_T4T_MAX_NLEN                                        (uint16_t)0x7FFD        /**< Maximum NLEN. */
#define PTX_T4T_MAX_ENLEN                                       (uint32_t)0xFFFFFFFA    /**< Maximum Extended NLEN. */
#define PTX_T4T_MAX_READ_LEN                                    (uint8_t)251u           /**< Internal maximum read length. */
#define PTX_T4T_CAPDU_SIZE_ODO                                  (uint8_t)19u            /**< CAPDU size when using ODO. */
#define PTX_T4T_ODO_THRESHOLD                                   (uint16_t)0x7FFF        /**< ODO usage threshold. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Select an App or File on the T4T.
 *
 * \param[in] t4t                   Pointer to component.
 * \param[in] paramByte1            P1.
 * \param[in] paramByte2            P2.
 * \param[in] data                  Data containing App- or File-ID for Selection.
 * \param[in] nbrDataBytes          Length of Data.
 * \param[in] expectedResponseLen   Expected Length of Response.
 * \param[in,out] rx                Response Buffer.
 * \param[in,out] rxLen             Length of Response Buffer on input, Length of Response on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpSelect (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t paramByte1, uint8_t paramByte2, uint8_t *data, uint8_t nbrDataBytes, uint8_t expectedResponseLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/**
 * \brief Read from File in App on T4T.
 * \param[in] t4t                       Pointer to component.
 * \param[in] offset                    Offset at which to start reading.
 * \param[in] nbrExpectedResponseBytes  Expected Length of Response.
 * \param[in,out] rx                    Response Buffer.
 * \param[in,out] rxLen                 Length of Response Buffer on input, Length of Response on output.
 * \param[in] msTimeout                 Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpReadBinary (ptxNDEF_T4TOP_t *t4tOpComp, uint32_t offset, uint8_t nbrExpectedResponseBytes, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/**
 * \brief Write to File in App on T4T.
 *
 * \param[in] t4t                   Pointer to component.
 * \param[in] offset                Offset at which to start writing.
 * \param[in] dataField             Data to be written.
 * \param[in] nbrDataBytes          Length of Data.
 * \param[in,out] rx                Response Buffer.
 * \param[in,out] rxLen             Length of Response Buffer on input, Length of Response on output.
 * \param[in] msTimeout             Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpUpdateBinary (ptxNDEF_T4TOP_t *t4tOpComp, uint32_t offset, uint8_t *dataField, uint8_t nbrDatabytes, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/**
 * \brief Get CC Information.
 *
 * \param[in] t4t                   Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpGetCCInfo (ptxNDEF_T4TOP_t *t4tOpComp);

/**
 * \brief Handle CC Information.
 *
 * \param[in] t4t                   Pointer to component.
 * \param[in] ccInfo                Buffer with CC File data.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpHandleCCInfo (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *ccInfo);

/**
 * \brief Get Length Information of NDEF File.
 *
 * \param[in] t4t                   Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpGetNLEN (ptxNDEF_T4TOP_t *t4tOpComp);

/**
 * \brief Read NDEF File content.
 *
 * \param[in]     t4t                   Pointer to component.
 * \param[in,out] msgBuffer             Message Data Buffer.
 * \param[out]    msgLen                Message Data Length.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpReadFileContent (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Update NDEF File Length.
 *
 * \param[in] t4t                   Pointer to component.
 * \param[in] newLen                New NDEF File Length.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T4TOpUpdateLength (ptxNDEF_T4TOP_t *t4tOpComp, uint32_t newLen);
/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNDEF_T4TOpOpen (ptxNDEF_T4TOP_t *t4tOpComp, ptxNDEF_T4TOP_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;
    ptxNativeTag_T4T_InitParams_t T4T_init_params;

    if ((NULL != t4tOpComp) && (NULL != initParams))
    {
        if ((NULL != initParams->RxBuffer) &&
                (0 != initParams->RxBufferSize))
        {
            /* clear component */
            (void)memset(t4tOpComp, 0, sizeof(ptxNDEF_T4TOP_t));

            t4tOpComp->RxBuffer = initParams->RxBuffer;
            t4tOpComp->RxBufferSize = initParams->RxBufferSize;
            t4tOpComp->LifeCycle = TagLC_NoNDEFTag;

            /* initialize lower layer component */
            (void)memset(&T4T_init_params, 0, sizeof(ptxNativeTag_T4T_InitParams_t));
            T4T_init_params = initParams->T4TInitParams;

            status = ptxNativeTag_T4TOpen(&t4tOpComp->NativeTagT4T, &T4T_init_params);

            /* set Component-ID at the end to prevent futher calls in case of an error */
            if (ptxStatus_Success == status)
            {
                t4tOpComp->CompId = ptxStatus_Comp_T4TOP;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T4TOpFormatTag (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NotImplemented);

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T4TOpCheckMessage (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t rx_len;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        /* select NDEF application */
        t4tOpComp->NativeTagT4T.Fields.nbr_data_length_bytes = 1u;
        t4tOpComp->NativeTagT4T.Fields.nbr_expected_length_bytes = 1u;
        status = ptxNDEF_T4TOpSelect(t4tOpComp,P1_NDEF_APP_SELECT,P2_NDEF_APP_SELECT,
                                     &PTX_T4T_DATA_FIELD_NDEF_APP_SELECT[0],
                                     LC_FIELD_NDEF_APP_SELECT,LE_FIELD_NDEF_APP_SELECT,
                                     &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

        if (ptxStatus_Success == status)
        {
            /* get CC info */
            status = ptxNDEF_T4TOpGetCCInfo(t4tOpComp);

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T4TOpGetNLEN(t4tOpComp);
            }
            else
            {
                t4tOpComp->LifeCycle = TagLC_NoNDEFTag;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T4TOpReadMessage (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    uint32_t msg_buffer_size;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_buffer_size = *msgLen;

        if (0 != msg_buffer_size)
        {
            /* check tag LC */
            switch(t4tOpComp->LifeCycle)
            {
                case TagLC_Initialized:
                case TagLC_ReadWrite:
                case TagLC_ReadOnly:
                    /* OK */
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidState);
                    break;
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T4TOpReadFileContent(t4tOpComp, msgBuffer, &msg_buffer_size);

                if ((ptxStatus_Success != status) || (t4tOpComp->NLEN.DigitNLEN != msg_buffer_size))
                {
                    msg_buffer_size = 0;
                }

                *msgLen = msg_buffer_size;
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T4TOpWriteMessage (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t msgLen)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t msg_len;
    uint32_t bytes_written = 0;
    uint8_t bytes_to_write;
    uint32_t ndef_file_size;
    uint8_t nbr_nlen_bytes;
    uint8_t *msg_buffer;
    uint8_t EMPTY_NDEF_MESSAGE[3] = {0xD0, 0x00, 0x00};
    uint8_t empty_record_len = 0x03;
    uint8_t c_adpu_size;
    size_t rx_len;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        msg_len = msgLen;
        msg_buffer = msgBuffer;
        ndef_file_size = (uint32_t)t4tOpComp->CCParams.NDEFFileSize;
        nbr_nlen_bytes = t4tOpComp->NLEN.NbrNLENBytes;
        c_adpu_size = 5u; /* class, inst, p1, p2, Le */

        /* check tag LC */
        switch (t4tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            case TagLC_ReadWrite:
                /* OK */
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidState);
                break;
        }

        if (0 != msg_len)
        {
            if (ptxStatus_Success == status)
            {
                if (ndef_file_size >= msg_len)
                {
                    if ((uint16_t)(t4tOpComp->CCParams.MLcDigit - c_adpu_size) >= msg_len) /* MLcDigit is by standard 0xFFFF at max */
                    {
                        /* reset NLEN before write */
                        status = ptxNDEF_T4TOpUpdateLength(t4tOpComp, 0);

                        if (ptxStatus_Success == status)
                        {
                            /* write message in one go */
                            status = ptxNDEF_T4TOpUpdateBinary(t4tOpComp,(uint32_t)nbr_nlen_bytes,&msg_buffer[0],(uint8_t)msg_len,
                                                               &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

                            if (ptxStatus_Success == status)
                            {
                                bytes_written = (uint32_t)(bytes_written + msg_len);
                            }
                        }
                    }
                    else
                    {
                        /* reset NLEN before write */
                        status = ptxNDEF_T4TOpUpdateLength(t4tOpComp, 0);

                        if (ptxStatus_Success == status)
                        {
                            while (msg_len > bytes_written)
                            {
                                if (bytes_written + nbr_nlen_bytes >= PTX_T4T_ODO_THRESHOLD)
                                {
                                    c_adpu_size = PTX_T4T_CAPDU_SIZE_ODO; /* Some available bytes are lost due to ODO C-APDU */
                                }

                                bytes_to_write = ((uint32_t)(msg_len - bytes_written) > (uint32_t)(t4tOpComp->CCParams.MLcDigit - c_adpu_size)) ? ((uint8_t)(t4tOpComp->CCParams.MLcDigit - c_adpu_size)) : ((uint8_t)(msg_len - bytes_written));

                                status = ptxNDEF_T4TOpUpdateBinary(t4tOpComp,(uint32_t)(nbr_nlen_bytes+bytes_written),&msg_buffer[bytes_written],bytes_to_write,
                                                                   &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
                                if (ptxStatus_Success == status)
                                {
                                    bytes_written = (uint32_t)(bytes_written + bytes_to_write);
                                }
                            }
                        }
                    }

                    /* message written correctly? */
                    if ((ptxStatus_Success == status) && (msgLen == bytes_written))
                    {
                        /* OK, update (E)NLEN */
                        status = ptxNDEF_T4TOpUpdateLength(t4tOpComp, msg_len);
                    }
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InsufficientResources);
                }

            }
        }
        else
        {
            /* reset NLEN before write */
            status = ptxNDEF_T4TOpUpdateLength(t4tOpComp, 0);

            if (ptxStatus_Success == status)
            {
                /* set empty message */
                msg_buffer = &EMPTY_NDEF_MESSAGE[0];
                status = ptxNDEF_T4TOpUpdateBinary(t4tOpComp,(uint32_t)nbr_nlen_bytes,&msg_buffer[0],empty_record_len,
                                                   &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
                if (ptxStatus_Success == status)
                {
                    /* OK, update (E)NLEN */
                    status = ptxNDEF_T4TOpUpdateLength(t4tOpComp, empty_record_len);
                    if (ptxStatus_Success == status)
                    {
                        t4tOpComp->LifeCycle = TagLC_Initialized;
                    }
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    (void)msgBuffer;
    (void)msgLen;

    return status;
}

ptxStatus_t ptxNDEF_T4TOpLockTag (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_Success); /* there is no procedure to lock the tag via the NDEF API */

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T4TOpClose (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        status = ptxNativeTag_T4TClose(&t4tOpComp->NativeTagT4T);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNDEF_T4TOpSelect (ptxNDEF_T4TOP_t *t4tOpComp,
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

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != data) && (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t4tOpComp->RxBufferSize;

        status = ptxNativeTag_T4TSelect(&t4tOpComp->NativeTagT4T,paramByte1,paramByte2,&data[0],nbrDataBytes,expectedResponseLen,rx,rxLen,msTimeout);

        if (ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                if (PTX_T4T_STATUS_WORD_LENGTH <= *rxLen)
                {
                    if ((PTX_T4T_SUCCESS_FLAG_SW1 == rx[*rxLen-2]) && (PTX_T4T_SUCCESS_FLAG_SW2 == rx[*rxLen-1]))
                    {
                        /* successful select */
                        *rxLen = *rxLen - PTX_T4T_STATUS_WORD_LENGTH;
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
                        if ((PTX_T4T_SW1_ERROR_WRONG_P1P2 == rx[*rxLen-2]) && (PTX_T4T_SW2_ERROR_FILE_OR_APP_NOT_FOUND == rx[*rxLen-1]))
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T4TOP,ptxStatus_NotFound);
                        }
                    }
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpReadBinary (ptxNDEF_T4TOP_t *t4tOpComp,
        uint32_t offset,
        uint8_t nbrExpectedResponseBytes,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t cut_idx = 0;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t4tOpComp->RxBufferSize;

        switch (t4tOpComp->CCParams.MappingMajor)
        {
            case 2:
                status = ptxNativeTag_T4TReadBinary(&t4tOpComp->NativeTagT4T,(uint16_t)offset,nbrExpectedResponseBytes,rx,rxLen,msTimeout);
                break;
            case 3:
                status = ptxNativeTag_T4TReadBinaryODO(&t4tOpComp->NativeTagT4T,offset,nbrExpectedResponseBytes,rx,rxLen,msTimeout);
                if (ptxStatus_Success == status)
                {
                    cut_idx = (0x81 != rx[1]) ? (2u) : (3u);

                    *rxLen = *rxLen - cut_idx; /* remove BER-TLV T and L field */
                    memmove(&rx[0], &rx[cut_idx], *rxLen);
                }
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                break;
        }

        if (ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                if (PTX_T4T_STATUS_WORD_LENGTH <= *rxLen)
                {
                    if ((PTX_T4T_SUCCESS_FLAG_SW1 == rx[*rxLen-2]) && (PTX_T4T_SUCCESS_FLAG_SW2 == rx[*rxLen-1]))
                    {
                        /* successful read */
                        *rxLen = *rxLen - PTX_T4T_STATUS_WORD_LENGTH;
                    }
                    else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
                        if ((PTX_T4T_READ_FAIL_FLAG_SW1_SIMPLE == rx[*rxLen-2]) && (PTX_T4T_READ_FAIL_FLAG_SW2_SIMPLE == rx[*rxLen-1]))
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter); /* wrong length, no further information */
                        }

                        if (PTX_T4T_READ_FAIL_FLAG_SW1_EXT == rx[*rxLen-2])
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter); /* wrong length, t4tOpComp->RxBuffer[rxLen-1] has nbr of available data bytes */
                        }
                    }
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
                }
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpUpdateBinary (ptxNDEF_T4TOP_t *t4tOpComp,
        uint32_t offset,
        uint8_t *dataField,
        uint8_t nbrDatabytes,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != dataField) && (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t4tOpComp->RxBufferSize;

        switch (t4tOpComp->CCParams.MappingMajor)
        {
            case 2:
                status = ptxNativeTag_T4TUpdateBinary(&t4tOpComp->NativeTagT4T,(uint16_t)offset,dataField,nbrDatabytes,rx,rxLen,msTimeout);
                break;
            case 3:
                if (offset >= 0x7FFF)
                {
                    status = ptxNativeTag_T4TUpdateBinaryODO(&t4tOpComp->NativeTagT4T,offset,dataField,nbrDatabytes,rx,rxLen,msTimeout);
                }
                else
                {
                    status = ptxNativeTag_T4TUpdateBinary(&t4tOpComp->NativeTagT4T,(uint16_t)offset,dataField,nbrDatabytes,rx,rxLen,msTimeout);
                }

                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                break;
        }

        if (ptxStatus_Success == status)
        {
            if ((0 != *rxLen) && (PTX_T4T_STATUS_WORD_LENGTH <= *rxLen))
            {
                if ((PTX_T4T_SUCCESS_FLAG_SW1 == rx[*rxLen-2]) && (PTX_T4T_SUCCESS_FLAG_SW2 == rx[*rxLen-1]))
                {
                    /* successful update */
                    *rxLen = *rxLen - PTX_T4T_STATUS_WORD_LENGTH;
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
                }
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_NscRfError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpGetCCInfo (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t rx_len;

    uint8_t cc_le_field = 0x02;
    uint8_t cc_index = 0;
    uint8_t data_field[2];

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        data_field[0] = (uint8_t)((DATA_FIELD_CC_SELECT >> 8u) & 0xFF);
        data_field[1] = (uint8_t)(DATA_FIELD_CC_SELECT & 0xFF);

        /* select CC file (function is to be called AFTER NDEF application select) */
        t4tOpComp->NativeTagT4T.Fields.nbr_data_length_bytes = 1u;
        t4tOpComp->NativeTagT4T.Fields.nbr_expected_length_bytes = 0;
        status = ptxNDEF_T4TOpSelect(t4tOpComp,P1_CC_SELECT,P2_CC_SELECT,
                                     data_field,LC_FIELD_CC_SELECT,LE_FIELD_CC_SELECT,
                                     &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

        if (ptxStatus_Success == status)
        {
            /* read CCLen bytes of CC file */
            t4tOpComp->CCParams.MappingVersion = PTX_T4T_MAPPINGVERSION_2_0; /* preset for CC collection */
            t4tOpComp->CCParams.MappingMajor = 2u;
            t4tOpComp->CCParams.MappingMinor = 0u;
            status = ptxNDEF_T4TOpReadBinary(t4tOpComp,(uint32_t)cc_index,cc_le_field,
                                             &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
            cc_index = (uint8_t)(cc_index + cc_le_field);

            if (ptxStatus_Success == status)
            {
                (void)memcpy(&t4tOpComp->CCParams.CCLen[0],&t4tOpComp->RxBuffer[0],(uint32_t)2);

                /* CCLen <= 0xFF, read in one go (proprietary ignored) */
                cc_le_field = t4tOpComp->CCParams.CCLen[1];

                /* read rest of the CC file */
                status = ptxNDEF_T4TOpReadBinary(t4tOpComp,(uint32_t)cc_index,(uint8_t)(cc_le_field-cc_index),
                                                 &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

                if ((ptxStatus_Success == status) && (PTX_T4T_CC_LEN_MIN - 2u <= rx_len))
                {
                    status = ptxNDEF_T4TOpHandleCCInfo(t4tOpComp, &t4tOpComp->RxBuffer[0]);
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpHandleCCInfo (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *ccInfo)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t data_index = 0;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != ccInfo))
    {
        t4tOpComp->CCParams.MappingVersion = ccInfo[data_index];
        t4tOpComp->CCParams.MappingMajor = (uint8_t)((t4tOpComp->CCParams.MappingVersion >> 4u) & 0x0F);
        t4tOpComp->CCParams.MappingMinor = (uint8_t)(t4tOpComp->CCParams.MappingVersion & 0x0F);
        data_index++;
        t4tOpComp->CCParams.MLeDigit = (uint16_t)(((uint16_t)ccInfo[data_index] << 8) | ccInfo[data_index+1]);
        data_index = (uint8_t)(data_index + 2);
        if (PTX_T4T_MAX_BYTES_RECEIVE < t4tOpComp->CCParams.MLeDigit)
        {
            t4tOpComp->CCParams.MLeDigit = PTX_T4T_MAX_BYTES_RECEIVE;
        }
        t4tOpComp->CCParams.MLcDigit = (uint16_t)(((uint16_t)t4tOpComp->RxBuffer[data_index] << 8) | ccInfo[data_index+1]);
        data_index = (uint8_t)(data_index + 2);
        if (PTX_T4T_MAX_BYTES_SEND < t4tOpComp->CCParams.MLcDigit)
        {
            t4tOpComp->CCParams.MLcDigit = PTX_T4T_MAX_BYTES_SEND;
        }
        if ((t4tOpComp->CCParams.MLeDigit == 0) || (t4tOpComp->CCParams.MLcDigit == 0))
        {
            status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
        }

        switch (t4tOpComp->CCParams.MappingMajor)
        {
            case 2:
                /* NDEF control TLV */
                (void)memcpy(&t4tOpComp->CCParams.NDEFTLV[0],&ccInfo[data_index],(uint8_t)PTX_T4T_NDEF_LENGTH);
                data_index = (uint8_t)(data_index + PTX_T4T_NDEF_LENGTH);
                (void)memcpy(&t4tOpComp->CCParams.NDEFFileIdentifier[0],&t4tOpComp->CCParams.NDEFTLV[2],(uint32_t)2);
                t4tOpComp->CCParams.NDEFFileSize = (uint32_t)(((uint32_t)t4tOpComp->CCParams.NDEFTLV[4] << 8u) | ((uint32_t)t4tOpComp->CCParams.NDEFTLV[5]));
                t4tOpComp->CCParams.NDEFAccessRead = t4tOpComp->CCParams.NDEFTLV[6];
                t4tOpComp->CCParams.NDEFAccessWrite = t4tOpComp->CCParams.NDEFTLV[7];
                break;
            case 3:
                /* ENDEF control TLV */
                (void)memcpy(&t4tOpComp->CCParams.NDEFTLV[0],&ccInfo[data_index],(uint8_t)PTX_T4T_ENDEF_LENGTH);
                data_index = (uint8_t)(data_index + PTX_T4T_ENDEF_LENGTH);
                (void)memcpy(&t4tOpComp->CCParams.NDEFFileIdentifier[0],&t4tOpComp->CCParams.NDEFTLV[2],(uint32_t)2);
                t4tOpComp->CCParams.NDEFFileSize = (uint32_t)((t4tOpComp->CCParams.NDEFTLV[4] << 24u) | (t4tOpComp->CCParams.NDEFTLV[5] << 16u) | (t4tOpComp->CCParams.NDEFTLV[6] << 8u) | (t4tOpComp->CCParams.NDEFTLV[7]));
                t4tOpComp->CCParams.NDEFAccessRead = t4tOpComp->CCParams.NDEFTLV[8];
                t4tOpComp->CCParams.NDEFAccessWrite = t4tOpComp->CCParams.NDEFTLV[9];
                break;
            default:
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                break;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpGetNLEN (ptxNDEF_T4TOP_t *t4tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t rx_len;
    uint8_t nbr_nlen_bytes;
    uint32_t nlen_offset = 0x00;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        t4tOpComp->LifeCycle = TagLC_Initialized;
        /* select NDEF file */
        t4tOpComp->NativeTagT4T.Fields.nbr_data_length_bytes = 1u;
        t4tOpComp->NativeTagT4T.Fields.nbr_expected_length_bytes = 0;

        status = ptxNDEF_T4TOpSelect(t4tOpComp,P1_NDEF_FILE_SELECT,P2_NDEF_FILE_SELECT,
                                     &t4tOpComp->CCParams.NDEFFileIdentifier[0],
                                     LC_FIELD_NDEF_FILE_SELECT,LE_FIELD_NDEF_FILE_SELECT,
                                     &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

        if (ptxStatus_Success == status)
        {
            nbr_nlen_bytes = (2 != t4tOpComp->CCParams.MappingMajor) ? (4u) : (2u);
            t4tOpComp->NLEN.NbrNLENBytes = nbr_nlen_bytes;
            status = ptxNDEF_T4TOpReadBinary(t4tOpComp,nlen_offset,nbr_nlen_bytes,
                                             &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);

            if ((ptxStatus_Success == status) && (nbr_nlen_bytes == rx_len))
            {
                (void)memcpy(&t4tOpComp->NLEN.NLEN[0],&t4tOpComp->RxBuffer[0],(uint32_t)nbr_nlen_bytes);
                t4tOpComp->NLEN.NbrNLENBytes = nbr_nlen_bytes;
            }

            if (ptxStatus_Success == status)
            {
                switch (t4tOpComp->CCParams.MappingMajor)
                {
                    case PTX_T4T_MAPPINGVERSION_MAJOR_2:
                        t4tOpComp->NLEN.DigitNLEN = (uint16_t)(t4tOpComp->NLEN.NLEN[1] | (t4tOpComp->NLEN.NLEN[0] << 8u));
                        break;
                    case PTX_T4T_MAPPINGVERSION_MAJOR_3:
                        t4tOpComp->NLEN.DigitNLEN = (uint32_t)(t4tOpComp->NLEN.NLEN[3] | (t4tOpComp->NLEN.NLEN[2] << 8u) | (t4tOpComp->NLEN.NLEN[1]) << 16u | (t4tOpComp->NLEN.NLEN[0]) << 24u);
                        break;
                    default:
                        t4tOpComp->LifeCycle = TagLC_NoNDEFTag;
                        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                        break;
                }
                if ((0 != t4tOpComp->NLEN.DigitNLEN) && (ptxStatus_Success == status))
                {
                    switch (t4tOpComp->CCParams.NDEFAccessWrite)
                    {
                        case PTX_T4T_ACCESS_RW:
                            t4tOpComp->LifeCycle = TagLC_ReadWrite;
                            break;

                        default:
                            t4tOpComp->LifeCycle = TagLC_ReadOnly;
                            break;
                    }
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpReadFileContent (ptxNDEF_T4TOP_t *t4tOpComp, uint8_t *msgBuffer, uint32_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t rx_len = 0;
    uint16_t bytes_to_read = 0;
    uint32_t bytes_read = 0;
    uint32_t msg_len = 0;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_len = *msgLen;

        /* read NDEF file after (E)NLEN bytes, SELECT was done in check */

        if (((uint32_t)(t4tOpComp->CCParams.MLeDigit-PTX_T4T_STATUS_WORD_LENGTH) > t4tOpComp->NLEN.DigitNLEN)
                && (PTX_T4T_MAX_READ_LEN > t4tOpComp->NLEN.DigitNLEN))
        {
            /* read message in one go */

            status = ptxNDEF_T4TOpReadBinary(t4tOpComp,(uint32_t)t4tOpComp->NLEN.NbrNLENBytes,
                                             (uint8_t)t4tOpComp->NLEN.DigitNLEN,
                                             &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
            bytes_read = (uint32_t)(bytes_read + rx_len);

            if (ptxStatus_Success == status)
            {
                (msg_len >= bytes_read) ?
                        ((void)memcpy(&msgBuffer[0],&t4tOpComp->RxBuffer[0],bytes_read)) :
                        (status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InsufficientResources));
            }
        }
        else
        {
            while ((t4tOpComp->NLEN.DigitNLEN > bytes_read) && (ptxStatus_Success == status))
            {
                bytes_to_read = ((uint32_t)(t4tOpComp->NLEN.DigitNLEN - bytes_read) > (uint32_t)(t4tOpComp->CCParams.MLeDigit)) ? ((uint16_t)(t4tOpComp->CCParams.MLeDigit)) : ((uint16_t)(t4tOpComp->NLEN.DigitNLEN - bytes_read));

                status = ptxNDEF_T4TOpReadBinary(t4tOpComp,(uint32_t)(t4tOpComp->NLEN.NbrNLENBytes+bytes_read),
                                                 (uint8_t)bytes_to_read,
                                                 &t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
                if (ptxStatus_Success == status)
                {
                    (msg_len >= (bytes_read + bytes_to_read)) ?
                            ((void)memcpy(&msgBuffer[bytes_read],&t4tOpComp->RxBuffer[0],bytes_to_read)) :
                            (status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InsufficientResources));
                    bytes_read = (uint32_t)(bytes_read + rx_len);
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            *msgLen = bytes_read;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T4TOpUpdateLength (ptxNDEF_T4TOP_t *t4tOpComp, uint32_t newLen)
{
    ptxStatus_t status = ptxStatus_Success;
    uint8_t lc_field;
    uint8_t length_data_field[4];
    uint8_t nlen_offset = 0;
    size_t rx_len;

    if (PTX_COMP_CHECK(t4tOpComp, ptxStatus_Comp_T4TOP))
    {
        switch (t4tOpComp->CCParams.MappingMajor)
        {
            case PTX_T4T_MAPPINGVERSION_MAJOR_2:
                if (PTX_T4T_MAX_NLEN < newLen)
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                }
                else
                {
                    length_data_field[0] = (uint8_t)(((uint16_t)newLen & 0xFF00) >> 8u);
                    length_data_field[1] = (uint8_t)((uint16_t)newLen & 0x00FF);

                    lc_field = 2u;

                    status = ptxNDEF_T4TOpUpdateBinary(t4tOpComp,nlen_offset,length_data_field,lc_field,&t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
                }
                break;
            case PTX_T4T_MAPPINGVERSION_MAJOR_3:
                if (PTX_T4T_MAX_ENLEN < newLen)
                {
                    status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                }
                else
                {
                    length_data_field[3] = (uint8_t)(newLen & 0x000000FF);
                    length_data_field[2] = (uint8_t)((newLen & 0x0000FF00) >> 8u);
                    length_data_field[1] = (uint8_t)((newLen & 0x00FF0000) >> 16u);
                    length_data_field[0] = (uint8_t)((newLen & 0xFF000000) >> 24u);

                    lc_field = 4;

                    status = ptxNDEF_T4TOpUpdateBinary(t4tOpComp,nlen_offset,length_data_field,lc_field,&t4tOpComp->RxBuffer[0],&rx_len,PTX_T4T_DEFAULT_TIMEOUT_MS);
                }
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
                break;
        }
        if (ptxStatus_Success == status)
        {
            t4tOpComp->NLEN.DigitNLEN = newLen;
            t4tOpComp->NLEN.NbrNLENBytes = lc_field;
            (void)memcpy(&t4tOpComp->NLEN.NLEN[0], &length_data_field[0], (uint32_t)lc_field);

            if (newLen != 0)
            {
                t4tOpComp->LifeCycle = TagLC_ReadWrite;
            }
            else
            {
                t4tOpComp->LifeCycle = TagLC_Initialized;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T4TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

