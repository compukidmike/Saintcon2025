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
    Module      : Transparent-Mode API
    File        : ptxTransparentMode.c

    Description : Dedicated API to enable low-level NFC-commands to implement custom/proprietary RF-protocols.
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT.h"
#include "ptxTransparentMode.h"
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    #include "ptx_IOT_READER.h"
#else
    #include "ptxPOS.h"
#endif
#include "ptxNSC_Registers.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
#define PTX_TRANSPARENT_MODE_MAX_MTU            (uint8_t)253        /**< Max length for input Tx-data */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
static ptxStatus_t ptxTransparentMode_LoadParams(ptxTransparentMode_t *tmComp, ptxTransparentMode_RFParams_t *rfParams, uint32_t timeoutMS);
static ptxStatus_t ptxTransparentMode_HandleBPrime(ptxTransparentMode_t *tmComp, uint8_t activateBPrime);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxTransparentMode_Init (ptxTransparentMode_t *tmComp, ptxTransparentMode_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != tmComp) && (NULL != initParams))
    {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
        if ((NULL != initParams->IoTRdComp))
#else
        if ((NULL != initParams->POSComp))
#endif
        {
            /* clear component */
            (void)memset(tmComp, 0, sizeof(ptxTransparentMode_t));

            /* set members */
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            tmComp->IoTRdComp = initParams->IoTRdComp;
            tmComp->NscComp = initParams->IoTRdComp->Nsc;
#else
            tmComp->POSComp = initParams->POSComp;
            tmComp->NscComp = initParams->POSComp->Nsc;
#endif

            /* Reset B-Prime Flag */
            tmComp->BPrimeActive = 0;

            /* assign Component ID */
            tmComp->CompId = ptxStatus_Comp_TransparentMode;

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTransparentMode_SetRFParameters (ptxTransparentMode_t *tmComp, ptxTransparentMode_RFParams_t *rfParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode)) && (NULL != rfParams))
    {
        status = ptxTransparentMode_LoadParams(tmComp, rfParams, 0);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTransparentMode_SetField (ptxTransparentMode_t *tmComp, uint8_t state)
{
    ptxStatus_t status = ptxStatus_Success;

#ifdef PTX_TRANSPARENT_MODE_RESET_DEFAULT
    ptxTransparentMode_RFParams_t rf_params = {0};
#endif

    if (PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode))
    {
        if (0 != state)
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            ptxIoTRd_DiscConfig_t disc_cfg = {0};
            disc_cfg.ContinuousField = (uint8_t)1;

            status = ptxIoTRd_Initiate_Discovery (tmComp->IoTRdComp, &disc_cfg);
#else
            ptxPOS_PollConfig_t poll_cfg = {0};
            poll_cfg.ContinuousField = (uint8_t)1;

            status = ptxPOS_Initiate_Polling (tmComp->POSComp, &poll_cfg);
#endif
        } else
        {
#ifdef PTX_TRANSPARENT_MODE_RESET_DEFAULT
            // Reset to default configuration
            rf_params.Tech = TM_RF_Tech_A;
            rf_params.TxRate = TM_RF_Bitrate_106;
            rf_params.RxRate = TM_RF_Bitrate_106;
            rf_params.ResLimit = 1U;
            rf_params.NrTxBits = 0;
            rf_params.Flags = (uint8_t)(PTX_TRANSPARENT_MODE_FLAGS_TX_PARITY | PTX_TRANSPARENT_MODE_FLAGS_RX_PARITY);

            status = ptxTransparentMode_LoadParams(tmComp, &rf_params, 0);
#else
            status = ptxTransparentMode_HandleBPrime(tmComp, 0);
#endif
            if (ptxStatus_Success == status)
            {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
                status = ptxIoTRd_Reader_Deactivation(tmComp->IoTRdComp, PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);
#else
                status = ptxPOS_Reader_Deactivation (tmComp->POSComp, 0);
#endif
            }

#ifdef PTX_PRODUCT_TYPE_IOT_READER
                (void)ptxNSC_SetNrResidualTxBits(tmComp->IoTRdComp->Nsc, 0, 0);
#else
                (void)ptxNSC_SetNrResidualTxBits(tmComp->POSComp->Nsc, 0, 0);
#endif
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTransparentMode_Exchange (ptxTransparentMode_t *tmComp,
                                         ptxTransparentMode_RFParams_t *rfParams,
                                         uint8_t *tx,
                                         uint8_t txLength,
                                         uint8_t *rx,
                                         uint32_t *rxLength,
                                         uint32_t timeoutMS)
{
    /*
     * This function uses the on-chip HW-timer for accurate timeout-measurements for the RF data exchange.
     * The application timeout used in the Data_Exchange-function of POS or IoT will not be used therefore a delta of approx.
     * 500 ms will be added on top as safety-feature.
     */
    const uint32_t TIMEOUT_DELTA = 500;

    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode)) &&
        (NULL != tx) &&
        (PTX_TRANSPARENT_MODE_MAX_MTU >= txLength) &&
        (0 != txLength) &&
        (NULL != rx) &&
        (NULL != rxLength) &&
        (0 != timeoutMS))
    {
        status = ptxTransparentMode_LoadParams(tmComp, rfParams, timeoutMS);

        if (ptxStatus_Success == status)
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            status = ptxIoTRd_Data_Exchange(tmComp->IoTRdComp, tx, (uint32_t)txLength, rx, rxLength, (uint32_t)(timeoutMS + TIMEOUT_DELTA));
#else
            status = ptxPOS_Data_Exchange(tmComp->POSComp, tx, (uint32_t)txLength, rx, rxLength, (uint32_t)(timeoutMS + TIMEOUT_DELTA));
#endif
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTransparentMode_Deinit (ptxTransparentMode_t *tmComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode))
    {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
                (void)ptxNSC_SetNrResidualTxBits(tmComp->IoTRdComp->Nsc, 0, 0);
#else
                (void)ptxNSC_SetNrResidualTxBits(tmComp->POSComp->Nsc, 0, 0);
#endif
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxTransparentMode_LoadParams(ptxTransparentMode_t *tmComp, ptxTransparentMode_RFParams_t *rfParams, uint32_t timeoutMS)
{
    const size_t MAX_NR_RF_PARAMS = 10;

    /*
     * Internal uses a resolution of 128/fc per tick ~ 9,44us; 1 ms is roughly 106 ticks (~1.00059ms)
     * Note: The resolution is limited to 24-bits i.e. the max. value is roughly 158 s.
     */
    const uint32_t BASE_TIMEOUT_1_MS = (uint32_t)106;

    ptxStatus_t status = ptxStatus_Success;

    size_t nr_nsc_rf_params = 0;
    ptxNSC_RfPar_t nsc_rf_params[MAX_NR_RF_PARAMS];
    uint8_t use_bprime_mode = 0;
    uint8_t selected_rf_tech;

    if (PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode))
    {
        (void)memset(&nsc_rf_params, 0, MAX_NR_RF_PARAMS * sizeof(ptxNSC_RfPar_t));

        if (NULL != rfParams)
        {
            selected_rf_tech = (rfParams->Tech == TM_RF_Tech_BPrime) ? (uint8_t)TM_RF_Tech_B : (uint8_t)rfParams->Tech;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Rf_Tech;
            nsc_rf_params[nr_nsc_rf_params].Parm.RfTech.RfTech = selected_rf_tech;
            nr_nsc_rf_params++;

            switch (rfParams->Tech)
            {
                case TM_RF_Tech_A:
                case TM_RF_Tech_B:
                    if ((rfParams->TxRate == TM_RF_Bitrate_26) || (rfParams->RxRate == TM_RF_Bitrate_26))
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }
                    break;

                case TM_RF_Tech_F:
                    if ((rfParams->TxRate == TM_RF_Bitrate_26) || (rfParams->RxRate == TM_RF_Bitrate_26))
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }


                    if ((rfParams->TxRate == TM_RF_Bitrate_106) || (rfParams->TxRate == TM_RF_Bitrate_848))
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }

                    if ((rfParams->RxRate == TM_RF_Bitrate_106) || (rfParams->RxRate == TM_RF_Bitrate_848))
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }
                    break;

                case TM_RF_Tech_V:
                    if ((rfParams->TxRate != TM_RF_Bitrate_26) || (rfParams->RxRate != TM_RF_Bitrate_26))
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }
                    break;

                case TM_RF_Tech_BPrime:
                    if ((rfParams->TxRate == TM_RF_Bitrate_106) && (rfParams->RxRate == TM_RF_Bitrate_106))
                    {
                        use_bprime_mode = 1u;
                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    }
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
                    break;
            }

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Tx_Bit_Rate;
            nsc_rf_params[nr_nsc_rf_params].Parm.TxBitRate.TxBitRate = (uint8_t)rfParams->TxRate;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Rx_Bit_Rate;
            nsc_rf_params[nr_nsc_rf_params].Parm.RxBitRate.RxBitRate = (uint8_t)rfParams->RxRate;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Tx_CRC;
            nsc_rf_params[nr_nsc_rf_params].Parm.TxCRC.TxCRC = (0 != (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_TX_CRC)) ? (uint8_t)0x01 : (uint8_t)0x00;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Rx_CRC;
            nsc_rf_params[nr_nsc_rf_params].Parm.RxCRC.RxCRC = (0 != (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_RX_CRC)) ? (uint8_t)0x01 : (uint8_t)0x00;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Tx_PAR;
            nsc_rf_params[nr_nsc_rf_params].Parm.TxParity.TxParity = (0 != (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_TX_PARITY)) ? (uint8_t)0x01 : (uint8_t)0x00;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Rx_PAR;
            nsc_rf_params[nr_nsc_rf_params].Parm.RxParity.RxParity = (0 != (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_RX_PARITY)) ? (uint8_t)0x01 : (uint8_t)0x00;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Res_Limit;
            nsc_rf_params[nr_nsc_rf_params].Parm.ResLimit.ResLimit = rfParams->ResLimit;
            nr_nsc_rf_params++;

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Tx_Residual_Bits;
            nsc_rf_params[nr_nsc_rf_params].Parm.TxResidualBits.TxResidualBits = rfParams->NrTxBits;
            nr_nsc_rf_params++;

            if ((TM_RF_Tech_A == rfParams->Tech) &&
                (0 == (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_TX_PARITY)) &&
                (0 == (rfParams->Flags & PTX_TRANSPARENT_MODE_FLAGS_TX_CRC)))
            {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
                (void)ptxNSC_SetNrResidualTxBits(tmComp->IoTRdComp->Nsc, 1u, rfParams->NrTxBits);
#else
                (void)ptxNSC_SetNrResidualTxBits(tmComp->POSComp->Nsc, 1u, rfParams->NrTxBits);
#endif
            }
            else
            {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
                (void)ptxNSC_SetNrResidualTxBits(tmComp->IoTRdComp->Nsc, 0, 0);
#else
                (void)ptxNSC_SetNrResidualTxBits(tmComp->POSComp->Nsc, 0, 0);
#endif
            }

        } else
        {
            /* Special Case - If B-Prime Mode was used previously and no RF-Parameters are provided - keep B-Prime active */
            if (0 != tmComp->BPrimeActive)
            {
                use_bprime_mode = 1u;
            }
        }

        if (0 != timeoutMS)
        {
            uint32_t timeout_val = (uint32_t)((BASE_TIMEOUT_1_MS * (timeoutMS + 1)) & 0x00FFFFFF);

            nsc_rf_params[nr_nsc_rf_params].ParmId = RfParameter_Fwt;
            nsc_rf_params[nr_nsc_rf_params].Parm.Fwt.Fwt[0] = (uint8_t)((timeout_val >> 16) & (uint8_t)0xFF);
            nsc_rf_params[nr_nsc_rf_params].Parm.Fwt.Fwt[1] = (uint8_t)((timeout_val >> 8)  & (uint8_t)0xFF);
            nsc_rf_params[nr_nsc_rf_params].Parm.Fwt.Fwt[2] = (uint8_t)((timeout_val >> 0)  & (uint8_t)0xFF);
            nr_nsc_rf_params++;
        }

        if ((nr_nsc_rf_params <= MAX_NR_RF_PARAMS) && (ptxStatus_Success == status))
        {
#ifdef PTX_PRODUCT_TYPE_IOT_READER
            status = ptxNSC_RfSetParams(tmComp->IoTRdComp->Nsc, &nsc_rf_params[0], nr_nsc_rf_params);
#else
            status = ptxNSC_RfSetParams(tmComp->POSComp->Nsc, &nsc_rf_params[0], nr_nsc_rf_params);
#endif

            /* Handle B-Prime Mode */
            if (ptxStatus_Success == status)
            {
                status = ptxTransparentMode_HandleBPrime(tmComp, use_bprime_mode);
            }

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InsufficientResources);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxTransparentMode_HandleBPrime(ptxTransparentMode_t *tmComp, uint8_t activateBPrime)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t reg_address = CL_RX_CONFIG1_REG;
    uint8_t reg_value = 0;

    if (PTX_COMP_CHECK(tmComp, ptxStatus_Comp_TransparentMode))
    {
        /* Activate B-Prime Mode ? */
        if ((0 != activateBPrime) && (0 == tmComp->BPrimeActive))
        {
            status = ptxNSC_Read(tmComp->NscComp, reg_address, &reg_value);

            if (ptxStatus_Success == status)
            {
                reg_value = (uint8_t)(reg_value | CL_RX_CONFIG1_REG_RX_TYPEB_PRIME_EN_MASK);

                status = ptxNSC_Write(tmComp->NscComp, reg_address, reg_value);

                if (ptxStatus_Success == status)
                {
                    tmComp->BPrimeActive = 1u;
                }
            }
        }

        /* Deactivate B-Prime Mode ? */
        if ((0 == activateBPrime) && (0 != tmComp->BPrimeActive))
        {
            status = ptxNSC_Read(tmComp->NscComp, reg_address, &reg_value);

            if (ptxStatus_Success == status)
            {
                reg_value = (uint8_t)(reg_value & CL_RX_CONFIG1_REG_RX_TYPEB_PRIME_EN_MASK_INV);

                status = ptxNSC_Write(tmComp->NscComp, reg_address, reg_value);

                if (ptxStatus_Success == status)
                {
                    tmComp->BPrimeActive = 0u;
                }
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}
