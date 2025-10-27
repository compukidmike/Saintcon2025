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

/**
 * \addtogroup grp_ptx_api_rf_test RF Test API
 *
 * @{
 */

#ifndef APIS_PTX_RF_TEST_H_
#define APIS_PTX_RF_TEST_H_

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
 * \brief RF-Test Flags for PRBS
 */
#define PTX_RF_TEST_FLAGS_PRBS_TYPE_A_INCLUDE_PARITY            (uint8_t)0x01   /**< Enable/Disable Parity-Generation for Type-A */

/**
 * \brief RF-Test Test ID
 */
typedef enum ptxRF_Test_TestID
{
    RF_TEST_ID_PRBS_9,                                  /**< PRBS-9 Test */
    RF_TEST_ID_PRBS_15,                                 /**< PRBS-15 Test */
    RF_TEST_ID_Carrier,                                 /**< Unmodulated Carrier-On Test */

} ptxRF_Test_TestID_t;

/**
 * \brief RF-Test Test Type
 */
typedef enum ptxRF_Test_TechType
{
    RF_TEST_TECH_A,                                     /**< NFC-A Technology */
    RF_TEST_TECH_B,                                     /**< NFC-B Technology */
    RF_TEST_TECH_F,                                     /**< NFC-F Technology */
    RF_TEST_TECH_V,                                     /**< NFC-V Technology */

} ptxRF_Test_TechType_t;

/**
 * \brief RF-Test Test Type
 */
typedef enum ptxRF_Test_BitRate
{
    RF_TEST_BITRATE_106,                                /**< RF Bitrate 106 kBit/s */
    RF_TEST_BITRATE_212,                                /**< RF Bitrate 212 kBit/s */
    RF_TEST_BITRATE_424,                                /**< RF Bitrate 424 kBit/s */
    RF_TEST_BITRATE_848,                                /**< RF Bitrate 848 kBit/s */

} ptxRF_Test_BitRate_t;

/**
 * \brief RF-Test Test Parameters for PRBS-Test
 */
typedef struct ptxRF_Test_PRBS_Params
{
    ptxRF_Test_TechType_t       Technology;             /**< RF-Technology */
    ptxRF_Test_BitRate_t        Bitrate;                /**< RF-Bitrate */
    uint8_t                     Flags;                  /**< PRBS-specific flags (see above) */

} ptxRF_Test_PRBS_Params_t;

/**
 * \brief RF-Test Test Parameters for Carrier-Test
 */
typedef struct ptxRF_Test_Carrier_Params
{
    uint8_t                     RFU;                  /**< RFU Parameter */

} ptxRF_Test_Carrier_Params_t;

/**
 * \brief RF-Test Parameters
 */
typedef union ptxRF_Test_Params
{
    ptxRF_Test_PRBS_Params_t       PRBS;                /**< PRBS Parameters */
    ptxRF_Test_Carrier_Params_t    Carrier;             /**< Carrier Parameters */

} ptxRF_Test_Params_t;

/**
 * \brief RF-Test Test Parameters
 */
typedef struct ptxRF_Test_TestParams
{
    ptxRF_Test_TestID_t         ID;                     /**< RF-Test-Identifier */
    ptxRF_Test_Params_t         Params;                 /**< RF-Test-specific parameters */

} ptxRF_Test_TestParams_t;

/**
 * \brief RF-Test Initialization Parameters
 */
typedef struct ptxRF_Test_InitParams
{
    struct ptxNSC               *Nsc;                   /**< Initialized instance of NSC component (member of IoT-/POS-component) */

} ptxRF_Test_InitParams_t;

/**
 * \brief RF-Test Component
 */
typedef struct ptxRF_Test
{
    /* Components */
    ptxStatus_Comps_t           CompId;                 /**< Component Id */

    struct ptxNSC               *Nsc;               /**< Initialized instance of NSC component (member of IoT-/POS-component) */

} ptxRF_Test_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the RF-Test Component.
 *
 * \param[in]   rfTestComp              Pointer to an allocated instance of the RF-Test component.
 * \param[in]   initParams              Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxRF_Test_Init (ptxRF_Test_t *rfTestComp, ptxRF_Test_InitParams_t *initParams);

/**
 * \brief Deinitializes the RF-Test Component.
 *
 * \param[in]   rfTestComp              Pointer to an initialized instance of the RF-Test component.
 *
 * \return Status                       indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxRF_Test_Deinit (ptxRF_Test_t *rfTestComp);

/**
 * \brief Performs a given RF-Test using the provided test parameters.
 *
 * \param[in]   rfTestComp              Pointer to an initialized instance of the RF-Test component.
 * \param[in]   testParams              Various test parameters for RF-Test compliance tests.
 *
 * \return Status                       Indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxRF_Test_RunTest(ptxRF_Test_t *rfTestComp, ptxRF_Test_TestParams_t *testParams);

/**
 * \brief Stops an ongoing RF-Test.
 *
 * \param[in]   rfTestComp              Pointer to an initialized instance of the RF-Test component.
 *
 * \return Status                       Indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxRF_Test_StopTest(ptxRF_Test_t *rfTestComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

