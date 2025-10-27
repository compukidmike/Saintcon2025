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
    Module      : PTX1K Felica-DTE
    File        : ptxFeliCa_DTE.c

    Description : Collection of functions for FeliCa compliance Tests.
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT.h"
#include "ptxFeliCa_DTE.h"
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    #include "ptx_IOT_READER.h"
#else
    #include "ptxPOS.h"
#endif
#include "ptxPLAT.h"
#include <string.h>


/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
#define PTX_FELICA_DTE_TECH                         (uint8_t)2
#define PTX_FELICA_DTE_RW_DP_T_OFF_DEFAULT_MS       (uint32_t)30
#define PTX_FELICA_DTE_RW_DP_T_ON_DEFAULT_MS        (uint32_t)25
#define PTX_FELICA_DTE_PERFORMANCE_T_ON_DEFAULT_MS  (uint32_t)25
#define PTX_FELICA_DTE_RW_MIN_RESULT_BUFFER_SIZE    (uint32_t)256
#define PTX_FELICA_DTE_RX_BUFFER_SIZE               (uint32_t)32
/**
 * \brief Temporary Test-data for Reader/Writer Digital Protocol Tests
 */
typedef struct ptxFeliCa_DTE_TempTestData
{
    ptxFeliCa_DTE_TestParams_t          *TestParams;
    ptxFeliCa_DTE_TestProgressParams_t  ProgressParams;
    ptxStatus_t                         TestStatus;
    uint8_t                             *Tx;
    uint32_t                            TxLen;
    uint8_t                             Rx[PTX_FELICA_DTE_RX_BUFFER_SIZE];
    uint32_t                            RxLen;

} ptxFeliCa_DTE_TempTestData_t;

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
static ptxStatus_t ptxFeliCa_DTE_RunPerformanceTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_eMoney(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_eMoneyNFC(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_FrameStruct(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_SetDP(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData);
static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_Generic(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData);
static ptxStatus_t ptxFeliCa_DTE_Data_Exchange(ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength);
static ptxStatus_t ptxFeliCa_DTE_HandleCarrier (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t switchOn);
static ptxStatus_t ptxFeliCa_DTE_SetParams (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t rfTech, uint8_t rfBitrate, uint32_t timeout);
static ptxStatus_t ptxFeliCa_DTE_Sleep (ptxFeliCa_DTE_t *feliCaDTEComp, uint32_t timeMS);
static ptxStatus_t ptxFeliCa_DTE_StoreTestSequences(ptxFeliCa_DTE_TempTestData_t *tempTestData);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxFeliCa_DTE_Init (ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != feliCaDTEComp) && (NULL != initParams))
    {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
        if ((NULL != initParams->IoTRdComp))
#else
        if ((NULL != initParams->POSComp))
#endif
        {
            /* clear component */
            (void)memset(feliCaDTEComp, 0, sizeof(ptxFeliCa_DTE_t));

            /* set members */
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            feliCaDTEComp->IoTRdComp = initParams->IoTRdComp;
#else
            feliCaDTEComp->POSComp = initParams->POSComp;
#endif

            /* assign Component ID */
            feliCaDTEComp->CompId = ptxStatus_Comp_FELICA_DTE;

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxFeliCa_DTE_Deinit (ptxFeliCa_DTE_t *feliCaDTEComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE))
    {
        /* nothing to do here */

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxFeliCa_DTE_EnableMode (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t enableMode, ptxFeliCa_DTE_TestParams_t *testParams)
{
    ptxStatus_t status = ptxStatus_Success;
    uint32_t guard_time;

    if (PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE))
    {
        size_t rf_par_len = 0;
        ptxNSC_RfPar_t rf_par[10];

        (void)memset(rf_par, 0, rf_par_len * sizeof(ptxNSC_RfPar_t));

        if (0 != enableMode)
        {
            if (NULL != testParams)
            {
                if (testParams->ID == FELICA_DTE_TestID_RWDigProtTest)
                {
                    guard_time = (NULL != testParams->Params.RWDigProtTest.T_OffGuardTime) ? *testParams->Params.RWDigProtTest.T_OffGuardTime : PTX_FELICA_DTE_RW_DP_T_OFF_DEFAULT_MS;
                    status = ptxFeliCa_DTE_Sleep(feliCaDTEComp, guard_time);
                }

                if (ptxStatus_Success == status)
                {
                    status = ptxFeliCa_DTE_HandleCarrier(feliCaDTEComp, enableMode);

                    if (ptxStatus_Success == status)
                    {
                        switch (testParams->ID)
                        {
                            case FELICA_DTE_TestID_PerformanceTest:

                                status = ptxFeliCa_DTE_Sleep(feliCaDTEComp, PTX_FELICA_DTE_PERFORMANCE_T_ON_DEFAULT_MS);

                                if (ptxStatus_Success == status)
                                {
                                    status = ptxFeliCa_DTE_SetParams(feliCaDTEComp,
                                                                     PTX_FELICA_DTE_TECH,
                                                                     (uint8_t)testParams->Params.PerformanceTest.Bitrate,
                                                                     testParams->Params.PerformanceTest.TimeOutMS);
                                }
                                break;

                            case FELICA_DTE_TestID_RWDigProtTest:
                                status = ptxFeliCa_DTE_SetParams(feliCaDTEComp,
                                                                 PTX_FELICA_DTE_TECH,
                                                                 (uint8_t)FELICA_DTE_BITRATE_212,
                                                                 testParams->Params.RWDigProtTest.TimeOutMS);

                                if (ptxStatus_Success == status)
                                {
                                    guard_time = (NULL != testParams->Params.RWDigProtTest.T_OnGuardTime) ? *testParams->Params.RWDigProtTest.T_OnGuardTime : PTX_FELICA_DTE_RW_DP_T_ON_DEFAULT_MS;
                                    status = ptxFeliCa_DTE_Sleep(feliCaDTEComp, guard_time);
                                }
                                break;

                            default:
                                status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
                                break;
                        }
                    }
                }

            } else
            {
                status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
            }

        } else
        {
            status = ptxFeliCa_DTE_SetParams(feliCaDTEComp, 0, 0, 0);

            if (ptxStatus_Success == status)
            {
                status = ptxFeliCa_DTE_HandleCarrier(feliCaDTEComp, enableMode);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxFeliCa_DTE_RunTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE)) && (NULL != testParams))
    {
        switch (testParams->ID)
        {
            case FELICA_DTE_TestID_PerformanceTest:
                status = ptxFeliCa_DTE_RunPerformanceTest(feliCaDTEComp, testParams);
                break;

            case FELICA_DTE_TestID_RWDigProtTest:
                status = ptxFeliCa_DTE_RunRFDigProtTest(feliCaDTEComp, testParams);
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
                break;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */

static ptxStatus_t ptxFeliCa_DTE_RunPerformanceTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams)
{
    /* Note: Length-byte always prepended by the HW */
    uint8_t TX_SEQ_SENSF_REQ[] = {0x00, 0xFF, 0xFF, 0x00, 0x00};

    ptxStatus_t status = ptxStatus_Success;

    uint8_t rx_buffer[PTX_FELICA_DTE_RX_BUFFER_SIZE];
    uint32_t rx_len;

    ptxFeliCa_DTE_TestProgressParams_t progress_params;
    (void)memset(&progress_params, 0, sizeof(ptxFeliCa_DTE_TestProgressParams_t));

    if ((PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE)) && (NULL != testParams))
    {
        if ((testParams->Params.PerformanceTest.NrTests <= testParams->Params.PerformanceTest.ResultBufferSize) &&
            (NULL != (testParams->Params.PerformanceTest.ResultBuffer)))
        {
            for (uint8_t i = 0; i < testParams->Params.PerformanceTest.NrTests; i++)
            {
                (void)memset(&rx_buffer[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
                rx_len = PTX_FELICA_DTE_RX_BUFFER_SIZE;
                status = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp,
                                                     &TX_SEQ_SENSF_REQ[0],
                                                     (uint32_t)sizeof(TX_SEQ_SENSF_REQ),
                                                     &rx_buffer[0],
                                                     &rx_len);

                progress_params.NrTestsProcessed++;

                if (ptxStatus_Success == status)
                {
                    /* Test OK */
                    testParams->Params.PerformanceTest.ResultBuffer[i] = 'O';

                } else if ((ptxStatus_NscRfError == PTX_GET_STATUS(status)) || (ptxStatus_TimeOut == PTX_GET_STATUS(status)))
                {
                    /* Test FAIL */
                    testParams->Params.PerformanceTest.ResultBuffer[i] = '.';

                    /* reset status */
                    status = ptxStatus_Success;
                }
                else
                {
                    /* cancel test */
                    break;
                }

                /* inform upper layer about progress and provide option to quit (both optional) */
                if (NULL != testParams->ProgressCB)
                {
                    testParams->ProgressCB(&progress_params);
                }

                if (0 != progress_params.ExitProcessing)
                {
                    /* cancel test */
                    break;
                }
            }
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams)
{
    ptxStatus_t status = ptxStatus_Success;

    ptxFeliCa_DTE_TempTestData_t temp_test_data;
    (void)memset(&temp_test_data, 0, sizeof(ptxFeliCa_DTE_TempTestData_t));

    if ((PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE)) && (NULL != testParams))
    {
        if ((PTX_FELICA_DTE_RW_MIN_RESULT_BUFFER_SIZE <= testParams->Params.RWDigProtTest.ResultBufferSize) &&
            (NULL != (testParams->Params.RWDigProtTest.ResultBuffer)))
        {
            (void)memset(&testParams->Params.RWDigProtTest.ResultBuffer[0], 0, testParams->Params.RWDigProtTest.ResultBufferSize);
            testParams->Params.RWDigProtTest.ResultBufferSize = 0;

            temp_test_data.TestParams = testParams;
            (void)memset(&temp_test_data.ProgressParams, 0, sizeof(ptxFeliCa_DTE_TestProgressParams_t));

            switch (testParams->Params.RWDigProtTest.SubTestID)
            {
                case FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant1:
                case FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant2:
                    status = ptxFeliCa_DTE_RunRFDigProtTest_eMoney(feliCaDTEComp, &temp_test_data);
                    break;

                case FELICA_DTE_SubTestID_RWDigProt_eMoney_NFCDEP:
                    status = ptxFeliCa_DTE_RunRFDigProtTest_eMoneyNFC(feliCaDTEComp, &temp_test_data);
                    break;

                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure1:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure2:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure3:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure4:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure5:
                case FELICA_DTE_SubTestID_RWDigProt_FrameStructure6:
                    status = ptxFeliCa_DTE_RunRFDigProtTest_FrameStruct(feliCaDTEComp, &temp_test_data);
                    break;

                case FELICA_DTE_SubTestID_RWDigProt_Setup_DP_POS:
                case FELICA_DTE_SubTestID_RWDigProt_Setup_DP_NEG:
                    status = ptxFeliCa_DTE_RunRFDigProtTest_SetDP(feliCaDTEComp, &temp_test_data);
                    break;

                case FELICA_DTE_SubTestID_RWDigProt_Generic:
                    if ((NULL != testParams->Params.RWDigProtTest.GenericCmdBuffer) && (0 != testParams->Params.RWDigProtTest.GenericCmdBufferLen))
                    {
                        status = ptxFeliCa_DTE_RunRFDigProtTest_Generic(feliCaDTEComp, &temp_test_data);

                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
                    }
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
                    break;
            }

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_eMoney(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t TX_SEQ_EMONEY_VARIANT_1[] =  {0x00, 0x12, 0xFC, 0x01, 0x00};
    uint8_t TX_SEQ_EMONEY_VARIANT_2[] =  {0x00, 0xFF, 0xFF, 0x01, 0x00};

    if (tempTestData->TestParams->Params.RWDigProtTest.SubTestID == FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant1)
    {
        tempTestData->Tx          = &TX_SEQ_EMONEY_VARIANT_1[0];
        tempTestData->TxLen       = (uint32_t)sizeof(TX_SEQ_EMONEY_VARIANT_1);

    } else
    {
        tempTestData->Tx          = &TX_SEQ_EMONEY_VARIANT_2[0];
        tempTestData->TxLen       = (uint32_t)sizeof(TX_SEQ_EMONEY_VARIANT_2);
    }

    (void)memset(&tempTestData->Rx[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
    tempTestData->RxLen = PTX_FELICA_DTE_RX_BUFFER_SIZE;

    tempTestData->TestStatus = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp, &tempTestData->Tx[0], tempTestData->TxLen, &tempTestData->Rx[0], &tempTestData->RxLen);

    status = ptxFeliCa_DTE_StoreTestSequences(tempTestData);

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_eMoneyNFC(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t TX_SEQ_EMONEY_NFC_SEQ_0[] = {0x00, 0xFF, 0xFF, 0x00, 0x00};
    uint8_t TX_SEQ_EMONEY_NFC_SEQ_1[] = {0x00, 0xFF, 0xFF, 0x00, 0x00};
    uint8_t TX_SEQ_EMONEY_NFC_SEQ_2[] = {0x00, 0xFF, 0xFF, 0x01, 0x00};

    for (int i = 0; i < 3; i++)
    {
        switch (i)
        {
            case 1:
                tempTestData->Tx          = &TX_SEQ_EMONEY_NFC_SEQ_1[0];
                tempTestData->TxLen       = (uint32_t)sizeof(TX_SEQ_EMONEY_NFC_SEQ_1);
                break;

            case 2:
                tempTestData->Tx          = &TX_SEQ_EMONEY_NFC_SEQ_2[0];
                tempTestData->TxLen       = (uint32_t)sizeof(TX_SEQ_EMONEY_NFC_SEQ_2);
                break;

            case 0:
            default:
                tempTestData->Tx          = &TX_SEQ_EMONEY_NFC_SEQ_0[0];
                tempTestData->TxLen       = (uint32_t)sizeof(TX_SEQ_EMONEY_NFC_SEQ_0);
                break;
        }

        (void)memset(&tempTestData->Rx[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
        tempTestData->RxLen = PTX_FELICA_DTE_RX_BUFFER_SIZE;

        tempTestData->TestStatus = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp, &tempTestData->Tx[0], tempTestData->TxLen, &tempTestData->Rx[0], &tempTestData->RxLen);

        status = ptxFeliCa_DTE_StoreTestSequences(tempTestData);

        tempTestData->ProgressParams.NrTestsProcessed++;

        /* inform upper layer about progress and provide option to quit (both optional) */
        if (NULL != tempTestData->TestParams->ProgressCB)
        {
            tempTestData->TestParams->ProgressCB(&tempTestData->ProgressParams);
        }

        if (0 != tempTestData->ProgressParams.ExitProcessing)
        {
            /* cancel test */
            break;
        }

        if (ptxStatus_Success != status)
        {
            break;
        }

        /* guard time before next Packet */
        (void)ptxFeliCa_DTE_Sleep(feliCaDTEComp, (uint32_t)5);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_FrameStruct(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t TX_SEQ_FRAME_STRUCTURE_0[]    = {0x00, 0xFF, 0xFF, 0x01, 0x03};
    uint8_t TX_SEQ_FRAME_STRUCTURE_1[]    = {0x00, 0xFF, 0xFF, 0x01, 0x00};
    uint8_t TX_SEQ_FRAME_STRUCTURE_2[]    = {0x06, 0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x05, 0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x04};
    uint8_t TX_SEQ_FRAME_STRUCTURE_3[]    = {0x06, 0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x05, 0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x02};
    uint8_t TX_SEQ_FRAME_STRUCTURE_4[]    = {0x06, 0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x05, 0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x05};
    uint8_t TX_SEQ_FRAME_STRUCTURE_5[]    = {0x06, 0x02, 0xFE, 0x11, 0x22, 0x33, 0x44, 0x05, 0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x09};

    int sequence_start_nr;
    int sequence_end_nr;

    /* determine which sequence to run */
    switch (tempTestData->TestParams->Params.RWDigProtTest.SubTestID)
    {
        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure1:
            sequence_start_nr = 0;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure2:
            sequence_start_nr = 1;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure3:
            sequence_start_nr = 2;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure4:
            sequence_start_nr = 3;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure5:
            sequence_start_nr = 4;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        case FELICA_DTE_SubTestID_RWDigProt_FrameStructure6:
            sequence_start_nr = 5;
            sequence_end_nr   = sequence_start_nr + 1;
            break;

        /* all sequences */
        default:
            sequence_start_nr = 0;
            sequence_end_nr = 6;
            break;
    }

    for (int i = sequence_start_nr; i < sequence_end_nr; i++)
    {
        switch (i)
        {
            case 0:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_0[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_0);
                break;

            case 1:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_1[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_1);
                break;

            case 2:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_2[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_2);
                break;

            case 3:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_3[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_3);
                break;

            case 4:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_4[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_4);
                break;

            case 5:
                tempTestData->Tx    = &TX_SEQ_FRAME_STRUCTURE_5[0];
                tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_FRAME_STRUCTURE_5);
                break;

            default:
                /* not possible */
                break;
        }

        (void)memset(&tempTestData->Rx[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
        tempTestData->RxLen = PTX_FELICA_DTE_RX_BUFFER_SIZE;

        tempTestData->TestStatus = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp, &tempTestData->Tx[0], tempTestData->TxLen, &tempTestData->Rx[0], &tempTestData->RxLen);

        status = ptxFeliCa_DTE_StoreTestSequences(tempTestData);

        tempTestData->ProgressParams.NrTestsProcessed++;

        /* inform upper layer about progress and provide option to quit (both optional) */
        if (NULL != tempTestData->TestParams->ProgressCB)
        {
            tempTestData->TestParams->ProgressCB(&tempTestData->ProgressParams);
        }

        if (0 != tempTestData->ProgressParams.ExitProcessing)
        {
            /* cancel test */
            break;
        }

        if (ptxStatus_Success != status)
        {
            break;
        }

        /* guard time before next Packet */
        (void)ptxFeliCa_DTE_Sleep(feliCaDTEComp, (uint32_t)5);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_SetDP(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t TX_SEQ_SETP_DP_POS[] = {0xFA, 0x01, 0x00};
    uint8_t TX_SEQ_SETP_DP_NEG[] = {0xFA, 0x01, 0xFF};

    if (tempTestData->TestParams->Params.RWDigProtTest.SubTestID == FELICA_DTE_SubTestID_RWDigProt_Setup_DP_POS)
    {
        tempTestData->Tx    = &TX_SEQ_SETP_DP_POS[0];
        tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_SETP_DP_POS);

    } else
    {
        tempTestData->Tx    = &TX_SEQ_SETP_DP_NEG[0];
        tempTestData->TxLen = (uint32_t)sizeof(TX_SEQ_SETP_DP_NEG);
    }

    (void)memset(&tempTestData->Rx[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
    tempTestData->RxLen = PTX_FELICA_DTE_RX_BUFFER_SIZE;

    tempTestData->TestStatus = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp, &tempTestData->Tx[0], tempTestData->TxLen, &tempTestData->Rx[0], &tempTestData->RxLen);

    status = ptxFeliCa_DTE_StoreTestSequences(tempTestData);

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_RunRFDigProtTest_Generic(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    tempTestData->Tx    = &tempTestData->TestParams->Params.RWDigProtTest.GenericCmdBuffer[0];
    tempTestData->TxLen = tempTestData->TestParams->Params.RWDigProtTest.GenericCmdBufferLen;

    (void)memset(&tempTestData->Rx[0], 0, PTX_FELICA_DTE_RX_BUFFER_SIZE);
    tempTestData->RxLen = PTX_FELICA_DTE_RX_BUFFER_SIZE;

    tempTestData->TestStatus = ptxFeliCa_DTE_Data_Exchange(feliCaDTEComp, &tempTestData->Tx[0], tempTestData->TxLen, &tempTestData->Rx[0], &tempTestData->RxLen);

    status = ptxFeliCa_DTE_StoreTestSequences(tempTestData);

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_Data_Exchange(ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength)
{
    ptxStatus_t status = ptxStatus_Success;

    /* use large enough value - must exceed test timeout value set within ptxFeliCa_DTE_EnableMode */
    uint32_t timeout_ms = (uint32_t)50000;

    if ((PTX_COMP_CHECK(feliCaDTEComp, ptxStatus_Comp_FELICA_DTE)) && (NULL != tx) && (0 != txLength) && (NULL != rxLength))
    {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
        status = ptxIoTRd_Data_Exchange(feliCaDTEComp->IoTRdComp, &tx[0], txLength, &rx[0], rxLength, timeout_ms);
#else
        status = ptxPOS_Data_Exchange(feliCaDTEComp->POSComp, &tx[0], txLength, &rx[0], rxLength, timeout_ms);
#endif
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_HandleCarrier (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t switchOn)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != feliCaDTEComp)
    {
        if (0 != switchOn)
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            ptxIoTRd_DiscConfig_t disc_cfg = {0};
            disc_cfg.ContinuousField = (uint8_t)1;

            status = ptxIoTRd_Initiate_Discovery (feliCaDTEComp->IoTRdComp, &disc_cfg);
#else
            ptxPOS_PollConfig_t poll_cfg = {0};
            poll_cfg.ContinuousField = (uint8_t)1;

            status = ptxPOS_Initiate_Polling (feliCaDTEComp->POSComp, &poll_cfg);
#endif
        } else
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            status = ptxIoTRd_Reader_Deactivation(feliCaDTEComp->IoTRdComp, PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);
#else
            status = ptxPOS_Reader_Deactivation (feliCaDTEComp->POSComp, 0);
#endif
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_SetParams (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t rfTech, uint8_t rfBitrate, uint32_t timeout)
{
    /*
     * Internal uses a resolution of 128/fc per tick ~ 9,44us; 1 ms is roughly 106 ticks (~1.00059ms)
     * Note: The resolution is limited to 24-bits i.e. the max. value is roughly 158 s.
     */
    const uint32_t BASE_TIMEOUT_1_MS = (uint32_t)106;

    ptxStatus_t status = ptxStatus_Success;

    if (NULL != feliCaDTEComp)
    {
        size_t rf_par_len = 0;
        ptxNSC_RfPar_t rf_par[10];

        (void)memset(rf_par, 0, rf_par_len * sizeof(ptxNSC_RfPar_t));

        /* RF-Technology (default) */
        rf_par[rf_par_len].ParmId = RfParameter_Rf_Tech;
        rf_par[rf_par_len].Parm.RfTech.RfTech = rfTech;
        rf_par_len++;

        /* FWT (default)*/
        uint32_t timeout_val = (uint32_t)((BASE_TIMEOUT_1_MS * timeout) & 0x00FFFFFF);
        rf_par[rf_par_len].ParmId = RfParameter_Fwt;
        rf_par[rf_par_len].Parm.Fwt.Fwt[0] = (uint8_t)((timeout_val >> 16) & (uint8_t)0xFF);
        rf_par[rf_par_len].Parm.Fwt.Fwt[1] = (uint8_t)((timeout_val >> 8)  & (uint8_t)0xFF);
        rf_par[rf_par_len].Parm.Fwt.Fwt[2] = (uint8_t)((timeout_val >> 0)  & (uint8_t)0xFF);
        rf_par_len++;

        /* TX-Bitrate */
        rf_par[rf_par_len].ParmId = RfParameter_Tx_Bit_Rate;
        rf_par[rf_par_len].Parm.TxBitRate.TxBitRate = rfBitrate;
        rf_par_len++;

        /* RX-Bitrate */
        rf_par[rf_par_len].ParmId = RfParameter_Rx_Bit_Rate;
        rf_par[rf_par_len].Parm.RxBitRate.RxBitRate = rfBitrate;
        rf_par_len++;

        /* TX-CRC */
        rf_par[rf_par_len].ParmId = RfParameter_Tx_CRC;
        rf_par[rf_par_len].Parm.TxCRC.TxCRC = (uint8_t)1;
        rf_par_len++;

        /* RX-CRC */
        rf_par[rf_par_len].ParmId = RfParameter_Rx_CRC;
        rf_par[rf_par_len].Parm.RxCRC.RxCRC = (uint8_t)1;
        rf_par_len++;

        /* RES_LIMIT */
        rf_par[rf_par_len].ParmId = RfParameter_Res_Limit;
        rf_par[rf_par_len].Parm.ResLimit.ResLimit = (uint8_t)1;
        rf_par_len++;

        if (ptxStatus_Success == status)
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
                status = ptxNSC_RfSetParams(feliCaDTEComp->IoTRdComp->Nsc, rf_par, rf_par_len);
#else
                status = ptxNSC_RfSetParams(feliCaDTEComp->POSComp->Nsc, rf_par, rf_par_len);
#endif
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_Sleep (ptxFeliCa_DTE_t *feliCaDTEComp, uint32_t timeMS)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != feliCaDTEComp) && (0 != timeMS))
    {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
        status = ptxPLAT_Sleep(feliCaDTEComp->IoTRdComp->Plat, timeMS);
#else
        status = ptxPLAT_Sleep(feliCaDTEComp->POSComp->Plat, timeMS);
#endif
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxFeliCa_DTE_StoreTestSequences(ptxFeliCa_DTE_TempTestData_t *tempTestData)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != tempTestData)
    {
        uint32_t index = tempTestData->TestParams->Params.RWDigProtTest.ResultBufferSize;

        tempTestData->TestParams->Params.RWDigProtTest.ResultBuffer[index] = (uint8_t)(tempTestData->TxLen + 1);
        index++;

        (void)memcpy(&tempTestData->TestParams->Params.RWDigProtTest.ResultBuffer[index], &tempTestData->Tx[0], tempTestData->TxLen);
        index = (uint32_t)(index + tempTestData->TxLen);

        if ((0 != tempTestData->RxLen) && (ptxStatus_Success == tempTestData->TestStatus))
        {
            uint32_t rx_len = tempTestData->RxLen - 1U;

            tempTestData->TestParams->Params.RWDigProtTest.ResultBuffer[index] = (uint8_t)(rx_len + 1U);
            index++;

            (void)memcpy(&tempTestData->TestParams->Params.RWDigProtTest.ResultBuffer[index], &tempTestData->Rx[0], rx_len);
            index = (uint32_t)(index + rx_len);
        }

        tempTestData->TestParams->Params.RWDigProtTest.ResultBufferSize = index;

        if (ptxStatus_Success != tempTestData->TestStatus)
        {
            status = tempTestData->TestStatus;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_FELICA_DTE, ptxStatus_InvalidParameter);
    }

    return status;
}
