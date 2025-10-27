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
    File        : ptxFeliCa_DTE.h

    Description : Collection of functions for FeliCa compliance Tests.
*/

/**
 * \addtogroup grp_ptx_api_felica_dte FeliCa DTE API
 *
 * @{
 */

#ifndef APIS_PTX_FELICA_DTE_H_
#define APIS_PTX_FELICA_DTE_H_

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

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief FeliCa-DTE Test Type
 */
typedef enum ptxFeliCa_DTE_BitRate
{
    FELICA_DTE_BITRATE_212 = 1,                             /**< RF Bitrate 212 kBit/s */
    FELICA_DTE_BITRATE_424 = 2,                             /**< RF Bitrate 424 kBit/s */

} ptxFeliCa_DTE_BitRate_t;

/**
 * \brief FeliCa-DTE Test Type
 */
typedef enum ptxFeliCa_DTE_TestID
{
    FELICA_DTE_TestID_PerformanceTest,                      /**< RF-Performance Tests */
    FELICA_DTE_TestID_RWDigProtTest,                        /**< Reader/Writer Digital Protocol Test */

} ptxFeliCa_DTE_TestID_t;

/**
 * \brief FeliCa-DTE Reader/Writer Digital Protocol (Sub-)Test Type
 */
typedef enum ptxFeliCa_DTE_RWDigProt_TestID
{
    FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant1,            /**< Reader/Writer Digital Protocol Test - Only enabling eMoney payments, Variant 1: SC = 0x0000, RC = 0x00 */
    FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant2,            /**< Reader/Writer Digital Protocol Test - Only enabling eMoney payments, Variant 2: SC = 0xFFFF, RC = 0x01 */
    FELICA_DTE_SubTestID_RWDigProt_eMoney_NFCDEP,              /**< Reader/Writer Digital Protocol Test - Enabling eMoney payments and NFC-DEP / P2P */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure,             /**< Reader/Writer Digital Protocol Test - Frame Structure sequences */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure1,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 1 */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure2,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 2 */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure3,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 3 */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure4,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 4 */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure5,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 5 */
    FELICA_DTE_SubTestID_RWDigProt_FrameStructure6,            /**< Reader/Writer Digital Protocol Test - Frame Structure sequence 6 */
    FELICA_DTE_SubTestID_RWDigProt_Setup_DP_POS,               /**< Reader/Writer Digital Protocol Test - Setup DP Test-board - positive Encoding */
    FELICA_DTE_SubTestID_RWDigProt_Setup_DP_NEG,               /**< Reader/Writer Digital Protocol Test - Setup DP Test-board - negative Encoding */
    FELICA_DTE_SubTestID_RWDigProt_Generic,                    /**< Reader/Writer Digital Protocol Test - Generic Test to send any (test-)command from application layer */

} ptxFeliCa_DTE_RWDigProt_TestID_t;

/**
 * \brief FeliCa-DTE Test Parameters for Performance-Tests
 */
typedef struct ptxFeliCa_DTE_PerformanceTest
{
    uint8_t                             NrTests;            /**< Number of Tests/Repetitions */
    uint8_t                             *ResultBuffer;      /**< Pointer to Result-Buffer */
    uint8_t                             ResultBufferSize;   /**< Size of Result-Buffer (must be >= NrTests) */
    ptxFeliCa_DTE_BitRate_t             Bitrate;            /**< Desired Bitrate */
    uint32_t                            TimeOutMS;          /**< Timeout value in [ms] for a single test-run (MUST be set to a value != 0!) */

} ptxFeliCa_DTE_PerformanceTest_t;

/**
 * \brief FeliCa-DTE Test Parameters for Performance-Tests
 */
typedef struct ptxFeliCa_DTE_RWDigProtTest
{
    ptxFeliCa_DTE_RWDigProt_TestID_t    SubTestID;           /**< Sub-test Identifier (Reader / Writer Digital Protocol sequence ID) */
    const uint32_t                      *T_OffGuardTime;     /**< Guard Time between Field-Off and Field-On (default value = 30ms) */
    const uint32_t                      *T_OnGuardTime;      /**< Guard Time between Field-On first Command (default value = 25ms) */
    uint8_t                             *GenericCmdBuffer;   /**< Pointer to Buffer containing generic (application-)command */
    uint32_t                            GenericCmdBufferLen; /**< Length of generic (application-)command */
    uint32_t                            TimeOutMS;           /**< Timeout value in [ms] for a single RF data exchange (MUST be set to a value != 0!) */
    uint8_t                             *ResultBuffer;       /**< Pointer to Result-Buffer */
    uint32_t                            ResultBufferSize;    /**< Size of Result-Buffer (must be >= 256 Bytes) */

} ptxFeliCa_DTE_RWDigProtTest_t;

/**
 * \brief FeliCa-DTE Test Parameters
 */
typedef union ptxFeliCa_DTE_Params
{
    ptxFeliCa_DTE_PerformanceTest_t     PerformanceTest;    /**< FeliCa Performance Test parameters */
    ptxFeliCa_DTE_RWDigProtTest_t       RWDigProtTest;      /**< FeliCa Reader/Writer Digital Protocol Test parameters */

} ptxFeliCa_DTE_Params_t;

/**
 * \brief FeliCa-DTE Test Progress Parameters
 */
typedef struct ptxFeliCa_DTE_TestProgressParams
{
    uint8_t                             ExitProcessing;     /**< Flag to abort an ongoing Test-execution (if applicable) */
    uint8_t                             NrTestsProcessed;   /**< Info how many tests / subsequences were executed so far (if applicable) */

} ptxFeliCa_DTE_TestProgressParams_t;

/**
 * \brief Progress Callback Function
 */
typedef void (*ptxFeliCa_DTE_ProgressFn_t)(ptxFeliCa_DTE_TestProgressParams_t *progressParams);

/**
 * \brief FeliCa-DTE Test Parameters
 */
typedef struct ptxFeliCa_DTE_TestParams
{
    ptxFeliCa_DTE_TestID_t              ID;                 /**< Compliance Test-Identifier */
    ptxFeliCa_DTE_Params_t              Params;             /**< Test Parameter */
    ptxFeliCa_DTE_ProgressFn_t          ProgressCB;         /**< Optional Callback-Function to get notified about the current progress */

} ptxFeliCa_DTE_TestParams_t;

/**
 * \brief FeliCa-DTE Initialization Parameters
 */
typedef struct ptxFeliCa_DTE_InitParams
{
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd         *IoTRdComp;         /**< Main Stack component */
#else
    struct ptxPOS           *POSComp;           /**< Main Stack component */
#endif
} ptxFeliCa_DTE_InitParams_t;

/**
 * \brief FeliCa-DTE Component
 */
typedef struct ptxFeliCa_DTE
{
    /* Components */
    ptxStatus_Comps_t       CompId;             /**< Component Id */

#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd         *IoTRdComp;         /**< Main Stack component */
#else
    struct ptxPOS           *POSComp;           /**< Main Stack component */
#endif
} ptxFeliCa_DTE_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the FeliCa-DTE Component.
 *
 * \param[in]   feliCaDTEComp           Pointer to an allocated instance of the FeliCa-DTE component.
 * \param[in]   initParams              Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxFeliCa_DTE_Init (ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_InitParams_t *initParams);

/**
 * \brief Deinitializes the FeliCa-DTE Component.
 *
 * \param[in]   feliCaDTEComp           Pointer to an initialized instance of the FeliCa-DTE component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxFeliCa_DTE_Deinit (ptxFeliCa_DTE_t *feliCaDTEComp);

/**
 * \brief Enables/Disables the FeliCa-DTE mode in the system and configures the test parameters.
 *
 * \param[in]   feliCaDTEComp           Pointer to an initialized instance of the FeliCa-DTE component.
 * \param[in]   enableMode              Enables (!= 0) or Disables (0) the FeliCa-DTE mode.
 * \param[in]   testParams              Various test parameters for FeliCa compliance tests.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxFeliCa_DTE_EnableMode (ptxFeliCa_DTE_t *feliCaDTEComp, uint8_t enableMode, ptxFeliCa_DTE_TestParams_t *testParams);

/**
 * \brief Performs a FeliCa compliance test with given test parameters.
 *
 * \param[in]   feliCaDTEComp           Pointer to an initialized instance of the FeliCa-DTE component.
 * \param[in]   testParams              Various test parameters for FeliCa compliance tests.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxFeliCa_DTE_RunTest(ptxFeliCa_DTE_t *feliCaDTEComp, ptxFeliCa_DTE_TestParams_t *testParams);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

