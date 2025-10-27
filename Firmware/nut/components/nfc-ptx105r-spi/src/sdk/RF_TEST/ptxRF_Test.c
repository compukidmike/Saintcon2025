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
    Module      : PTX1K RF-Test
    File        : ptxRF_Test.h

    Description : Collection of common RF-Test functions (e.g. PRBS).
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT.h"
#include "ptxRF_Test.h"
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    #include "ptx_IOT_READER.h"
#else
    #include "ptxPOS.h"
#endif
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxRF_Test_Init (ptxRF_Test_t *rfTestComp, ptxRF_Test_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != rfTestComp) && (NULL != initParams))
    {
        if ((NULL != initParams->Nsc))
        {
            /* clear component */
            (void)memset(rfTestComp, 0, sizeof(ptxRF_Test_t));

            /* set members */
            rfTestComp->Nsc = initParams->Nsc;

            /* assign Component ID */
            rfTestComp->CompId = ptxStatus_Comp_RF_TEST;

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxRF_Test_Deinit (ptxRF_Test_t *rfTestComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(rfTestComp, ptxStatus_Comp_RF_TEST))
    {
        /* nothing to do here */

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxRF_Test_RunTest(ptxRF_Test_t *rfTestComp, ptxRF_Test_TestParams_t *testParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(rfTestComp, ptxStatus_Comp_RF_TEST)) && (NULL != testParams))
    {
        uint8_t test_params[3];
        (void)memset(&test_params[0], 0, 3);
        size_t test_params_len = 0;

        ptxNSC_RfTest_ID_t test_id = RfTest_PRBS_9;

        switch (testParams->ID)
        {
            case RF_TEST_ID_PRBS_9:
                test_id = RfTest_PRBS_9;
                test_params_len = 3;
                test_params[0] = testParams->Params.PRBS.Technology;
                test_params[1] = testParams->Params.PRBS.Bitrate;
                test_params[2] = testParams->Params.PRBS.Flags;
                break;

            case RF_TEST_ID_PRBS_15:
                test_id = RfTest_PRBS_15;
                test_params_len = 3;
                test_params[0] = testParams->Params.PRBS.Technology;
                test_params[1] = testParams->Params.PRBS.Bitrate;
                test_params[2] = testParams->Params.PRBS.Flags;
                break;

            case RF_TEST_ID_Carrier:
                test_id = RfTest_Carrier;
                test_params_len = 1;
                test_params[0] = 0;
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
                break;
        }

        status = ptxNSC_RfTestRun(rfTestComp->Nsc, test_id, &test_params[0], test_params_len);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxRF_Test_StopTest(ptxRF_Test_t *rfTestComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(rfTestComp, ptxStatus_Comp_RF_TEST))
    {
        status = ptxNSC_RfTestStop(rfTestComp->Nsc);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_RF_TEST, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */

