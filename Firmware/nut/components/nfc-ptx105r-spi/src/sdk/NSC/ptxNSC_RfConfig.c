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
    Module      : NSC
    File        : ptxNSC_RfConfig.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxNSC.h"
#include "ptxNSC_Hal.h"
#include "ptxPLAT.h"
#include <string.h>


/*
 * ####################################################################################################################
 * DEFINES / TYPES / INTERNALS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxNSC_RfConfig_CheckRsp(ptxNSC_t *nscCtx);
static ptxStatus_t ptxNSC_RfConfig_Send(ptxNSC_t *nscCtx, ptxNSC_RfConfigTlv_t *nsc_rf_cfg_params, uint8_t rfconfig_tlv_count);
static ptxStatus_t ptxNSC_RfConfig_GetFirstTlv(ptxNSC_RfConfigTlv_t *nsc_rf_cfg_params, uint8_t *tlv_index, uint8_t num_of_tlvs, uint8_t *used_indices);
static ptxStatus_t ptxNSC_RfConfig_GetDefaultSettings(ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t *rfConfigTlvCount);
static ptxStatus_t ptxNSC_RfConfig_AddToCommand(uint8_t *nsc_rfConfig_cmd, ptxNSC_RfConfigTlv_t *rf_cfg_tlvs, uint8_t *nsc_rfConfig_index);
static ptxStatus_t ptxNSC_RfConfig_GetConfigType(uint8_t tlv_ID, uint8_t *config_type, uint8_t *wavebank_id);

static ptxStatus_t ptxNSC_RfConfig_GetDefaultParameter(ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t paramId, uint8_t *config_type, uint8_t *wavebank_id);

static ptxStatus_t ptxNSC_RFConfig_GetConfigPointer(uint8_t paramId, uint8_t **rfConfigValue, uint8_t *rfConfigValueLen);
static ptxStatus_t ptxNSC_RFConfig_ReleasePointer(void);
/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxNSC_RfConfig(ptxNSC_t *nscCtx, ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t rfConfigTlvCount)
{
    ptxStatus_t status = ptxStatus_Success;

    /* If the caller provides valid data, use this. Otherwise, use default. */
    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        ptxNSC_RfConfigTlv_t *nsc_rf_cfg_params;
        uint8_t rfconfig_tlv_count = 0;

        if ((NULL != nscRfCfgParams) && (0 != rfConfigTlvCount))
        {
            /* Use RF Config data provided by the caller. */
            nsc_rf_cfg_params = nscRfCfgParams;
            /*
             * Check the maximum allowed number of tlvs and limit this here.
             * Use first rfconfig_tlv_count parameters out of the given array.
             */
            rfconfig_tlv_count = ((RfCfgParam_MaxNum-1) < rfConfigTlvCount) ? (RfCfgParam_MaxNum-1) : rfConfigTlvCount;

            status = ptxNSC_RfConfig_Send(nscCtx, nsc_rf_cfg_params, rfconfig_tlv_count);

        } else
        {
            /* Use default RF Config settings. */
            /* Actually, introduce application specific RfConfig Default Data getter.
             * Since App hasn´t provided data, we need to allocate this here.
             */

            ptxNSC_RfConfigTlv_t default_tlvs[RfCfgParam_MaxNum-1];

            (void)memset(&default_tlvs[0], 0, sizeof(ptxNSC_RfConfigTlv_t) * (RfCfgParam_MaxNum-1));

            status = ptxNSC_RfConfig_GetDefaultSettings(&default_tlvs[0], &rfconfig_tlv_count);

            if(ptxStatus_Success == status)
            {
                /* We assume that tlv count is properly set in the previous call. If anything doesn´t match, should not returned success. */
                nsc_rf_cfg_params =  &default_tlvs[0];
                status = ptxNSC_RfConfig_Send(nscCtx, nsc_rf_cfg_params, rfconfig_tlv_count);
            }

            if(ptxStatus_Success == status)
            {
                /* Optional Call: Release potentially previously allocated resources */
                status = ptxNSC_RFConfig_ReleasePointer();
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    status = ptxNSC_CheckSystemState(nscCtx, status);

    return status;
}


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxNSC_RfConfig_Send(ptxNSC_t *nscCtx, ptxNSC_RfConfigTlv_t *nsc_rf_cfg_params, uint8_t rfconfig_tlv_count)
{
    /*
     * This function parses TLV data and fills output buffer in an optimal way (to cut down the number of transmissions).
     */
    ptxStatus_t status = ptxStatus_Success;

    const uint8_t nsc_rfConfig_cmd_len = 255u;
    uint8_t nsc_rfconfig_minimum_length = 17u;      /* ptxNSC_RfConfig_RegsPollV_len (15) + config type (1) + empty config (1)*/
    uint8_t nsc_rfConfig_index = 0;
    uint8_t nsc_rfConfig_cmd[nsc_rfConfig_cmd_len];
    uint8_t *txBuf[1u];
    size_t txLen[1u];
    ptxNscHal_BufferId_t bufferId = NscWriteBuffer_1;

    ptxStatus_t st_;

    /* Mark TLVs that were already sent. So far not anyone used. */
    uint8_t used_indices[rfconfig_tlv_count];
    (void)memset(&used_indices[0], 0, sizeof(uint8_t)*rfconfig_tlv_count);

    uint8_t num_used_tlvs = 0;
    uint8_t quit_the_loop = 0;
    uint8_t num_retries = 5u;
    uint8_t num_inner_iterations = 3u;
    uint8_t send_the_buffer = 0;

    ptxNSC_RfConfigTlv_t *rf_cfg_tlvs = nsc_rf_cfg_params;
    uint8_t rf_cfg_tlv_count = rfconfig_tlv_count;

    /*
     * THE IDEA:: go through TLVs and fill up the command buffer in an optimal way. This means: do not waste 255Bytes just to send
     * e.g. 100 bytes of data.
     *
     * Inner loop: after the loop finishes, a command is sent. Worst case, fills the buffer after maximum iterations.
     *
     * Outer loop: maximum number of iterations set to 5. This is just a safeguard not to stay in the loop forever.
     * Exit the loop if all TLVs data has been sent.
     *
     * Expected result: should finish after the most 3 iterations. Otherwise, maybe something changed in RfConfig param structures.
     *
     */
    do
    {
        /*
         * INNER LOOP: FILL ONE COMMAND BUFFER
         * Go through all TLVs:
         * 1) Find the first that hasn´t been sent already.
         * 2) Add its data to command buffer.
         * 3) If the space in command buffer is less than defined minimum i.e. not able to fill any other parameter data,
         *    break the loop and send command.
         */

        uint8_t idx = 0;
        /* Calculate length and optimize for transmission. */

        /* INNER LOOP: START */
        for(uint8_t i=0; ((i<rf_cfg_tlv_count) && (i<num_inner_iterations)) && (0 == quit_the_loop); i++)
        {
            /* Go to the first defined, not-empty and not yet used tlv. Also, check parameter values. Skip this TLV if param values not OK. */
            status = ptxNSC_RfConfig_GetFirstTlv(nsc_rf_cfg_params, &idx, rf_cfg_tlv_count, used_indices);
            if(ptxStatus_Success == status)
            {
                /* We got the index of yet unsent TLV. Check if it fits to the command buffer. */
                st_ = ptxNSC_RfConfig_AddToCommand(nsc_rfConfig_cmd, &nsc_rf_cfg_params[idx], &nsc_rfConfig_index);
                if(ptxStatus_Success == st_)
                {
                    if (rf_cfg_tlvs[idx].ID == RfCfgParam_RegsMisc)
                    {
                        /* keep a copy of the MISC-parameters for later use */
                        nscCtx->RFConfigMiscParams.MiscSettingsFlags = 0;
                        if (PTX_NSC_MISC_RF_CONFIG_BUFFER_SIZE >= nsc_rf_cfg_params[idx].Len)
                        {
                            (void) memcpy (&nscCtx->RFConfigMiscParams.MiscSettings[0], &nsc_rf_cfg_params[idx].Value[0], nsc_rf_cfg_params[idx].Len);
                            nscCtx->RFConfigMiscParams.MiscSettings_Len = nsc_rf_cfg_params[idx].Len;
                            nscCtx->RFConfigMiscParams.MiscSettingsFlags = nscCtx->RFConfigMiscParams.MiscSettingsFlags | PTX_NSC_MISC_RF_CONFIG_FLAGS_SET;
                        }
                    }

                    used_indices[idx] = 1u;
                    num_used_tlvs++;
                    idx++;
                } else
                {
                    /* Skip this index since this TLV´s data doesn´t fit into command buffer. */
                    idx++;
                }

                /*
                 * Check if the space left in the command buffer is less than minimum. If so, send the command.
                 * Also, check that index doesn´t get out of boundaries.
                 */
                if((nsc_rfConfig_cmd_len <= (nsc_rfConfig_index + nsc_rfconfig_minimum_length)) || (idx >= rf_cfg_tlv_count))
                {
                    send_the_buffer = 1u;
                    break;
                } else
                {
                    /*
                     * If the loop is about to end because it has reached the max number of iterations, but
                     * the buffer is pretty much empty, try few more times.
                     * Let´s see what is the available space and count number of iterations based on the minimum data chunk.
                     */
                    if(i == (num_inner_iterations-1))
                    {
                        uint8_t space_left = (uint8_t)(nsc_rfConfig_cmd_len - nsc_rfConfig_index);
                        if(50u < space_left)
                        {
                            space_left = (uint8_t)(space_left / nsc_rfconfig_minimum_length);
                            num_inner_iterations = (uint8_t)(i + space_left + (uint8_t)(1));
                        }
                    }
                }
            } else
            {
                quit_the_loop = 1u;

                /* If no more TLVs available, but we used and sent data already, this is not an error, but we reached the end. */
                if(0 < num_used_tlvs)
                {
                    status = ptxStatus_Success;
                }
            }
        } /* INNER LOOP: END */

        /* If no more space in command buffer, send the command. */
        if((1u == send_the_buffer) || (nsc_rfConfig_index > 0))
        {
            /* END OF COMMAND */
            nsc_rfConfig_cmd[nsc_rfConfig_index] = PTX_NSC_RFCONF_LAST_PARAM;
            nsc_rfConfig_index++;

            txBuf[0] = &nsc_rfConfig_cmd[0];
            txLen[0] = nsc_rfConfig_index;

            status = ptxNSC_Send(nscCtx, bufferId, txBuf, txLen, 1u);

            if (ptxStatus_Success == status)
            {
                status = ptxNSC_RfConfig_CheckRsp(nscCtx);

                if (ptxStatus_Success == status)
                {
                    /*
                     * Mark Misc. RF-Config settings as being loaded
                     */
                    if (0 != (nscCtx->RFConfigMiscParams.MiscSettingsFlags & PTX_NSC_MISC_RF_CONFIG_FLAGS_SET))
                    {
                        nscCtx->RFConfigMiscParams.MiscSettingsFlags = nscCtx->RFConfigMiscParams.MiscSettingsFlags | PTX_NSC_MISC_RF_CONFIG_FLAGS_LOADED;
                    }
                }
            }

            if (ptxStatus_Success != status)
            {
                quit_the_loop = 1u;
            }

            if (1u != send_the_buffer)
            {
                /* Adapt the number of iterations if the buffer wasn´t filled as expected. */
                num_inner_iterations = (uint8_t)(num_inner_iterations + 2);
            }

            nsc_rfConfig_index = 0;
            send_the_buffer = 0;
        }

        num_retries--;
    } while((num_retries>0) && (0 == quit_the_loop) && (num_used_tlvs < rf_cfg_tlv_count));

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_AddToCommand(uint8_t *nsc_rfConfig_cmd, ptxNSC_RfConfigTlv_t *rf_cfg_tlvs, uint8_t *nsc_rfConfig_index)
{
    ptxStatus_t status = ptxStatus_Success;

    const size_t nsc_rfConfig_cmd_maxlen = 255u;

    uint8_t *cmd = nsc_rfConfig_cmd;
    ptxNSC_RfConfigTlv_t *tlv = rf_cfg_tlvs;
    size_t idx = *nsc_rfConfig_index;

    /* No data in command buffer. Add OP_CODE and tlv data. */
    if(0 == idx)
    {
        cmd[idx] = PTX_NSC_RFCONF_CMD_OPCODE;
        idx++;
    }

    /*
     * Except parameter length, maximum overhead could be for Type parameters of NSC_RF_CONFIG_CMD: Type of configuration + power mode
     * Plus, we need to account for empty configuration, which designates the end of command.
     */

    if((idx + tlv->Len + 3u) <= nsc_rfConfig_cmd_maxlen)
    {
        uint8_t config_type = 0;
        uint8_t wavebank_id = 0xFF;

        status = ptxNSC_RfConfig_GetConfigType(tlv->ID, &config_type, &wavebank_id);
        if(ptxStatus_Success == status)
        {
            cmd[idx] = config_type;
            idx++;

            /* 0 is irregular value for this parameter. */
            if(0xFF != wavebank_id)
            {
                cmd[idx] = wavebank_id;
                idx++;
            }

            (void) memcpy(&cmd[idx], tlv->Value, tlv->Len);
            idx += tlv->Len;
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
    }

    *nsc_rfConfig_index = (uint8_t)idx;

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_GetFirstTlv(ptxNSC_RfConfigTlv_t *nsc_rf_cfg_params, uint8_t *tlv_index, uint8_t num_of_tlvs, uint8_t *used_indices)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t i;
    uint8_t cfg_type=0;
    uint8_t wavebank=0;

    for(i= *tlv_index; i < num_of_tlvs; i++)
    {
        /*
         * If ID one of the acceptable and if TLV hasn´t been used up to now, we have a candidate.
         * Also, provided length for this parameter has to match to expected non-zero value.
         */
        if((RfCfgParam_Undefined < nsc_rf_cfg_params[i].ID) && (RfCfgParam_MaxNum > nsc_rf_cfg_params[i].ID) && (0 == used_indices[i]))
        {
            ptxNSC_RfConfigTlv_t rf_param;
            /*
             * For non-existing ID, the function will set Len to 0.
             */
            (void) ptxNSC_RfConfig_GetDefaultParameter(&rf_param, nsc_rf_cfg_params[i].ID, &cfg_type, &wavebank);

            if((0 != rf_param.Len) && (nsc_rf_cfg_params[i].Len == rf_param.Len))
            {
                *tlv_index = i;
                break;
            }
        }
    }

    if(i >= num_of_tlvs)
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
    }

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_GetDefaultSettings(ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t *rfConfigTlvCount)
{
    ptxStatus_t status = ptxStatus_Success;

    if( (NULL != nscRfCfgParams) && (NULL != rfConfigTlvCount) )
    {
        uint8_t i;
        uint8_t j = 0;
        uint8_t cfg_type = 0;
        uint8_t wavebank = 0;

        ptxNSC_RfConfigTlv_t *default_tlvs = nscRfCfgParams;

        for(i=(RfCfgParam_Undefined+1); i<RfCfgParam_MaxNum; i++)
        {
            status = ptxNSC_RfConfig_GetDefaultParameter (&default_tlvs[j], i, &cfg_type, &wavebank);
            if(ptxStatus_Success == status )
            {
                j++;
            } else
            {
                /* Something wrong in system configuration. */
                break;
            }
        }

        *rfConfigTlvCount = j;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_GetConfigType(uint8_t tlv_ID, uint8_t *config_type, uint8_t *wavebank_id)
{
    ptxStatus_t status = ptxStatus_Success;

    ptxNSC_RfConfigTlv_t rf_cfg_params;

    status = ptxNSC_RfConfig_GetDefaultParameter(&rf_cfg_params, tlv_ID, config_type, wavebank_id);

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_GetDefaultParameter(ptxNSC_RfConfigTlv_t *nscRfCfgParams, uint8_t paramId, uint8_t *config_type, uint8_t *wavebank_id)
{
    ptxStatus_t status = ptxStatus_Success;

    /* retrieve Pointers and Lengths of RF-configuration TLV */
    status = ptxNSC_RFConfig_GetConfigPointer(paramId, &nscRfCfgParams->Value, &nscRfCfgParams->Len);

    if (ptxStatus_Success == status)
    {
        switch(paramId)
        {
        case RfCfgParam_Wavebank_0:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_0;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_0;
				break;

			case RfCfgParam_Wavebank_1:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_1;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_1;
				break;

			case RfCfgParam_Wavebank_2:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_2;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_2;
				break;

			case RfCfgParam_Wavebank_3:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_3;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_3;
				break;

			case RfCfgParam_Wavebank_4:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_4;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_4;
				break;

			case RfCfgParam_Wavebank_5:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_5;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_5;
				break;

			case RfCfgParam_Wavebank_6:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_6;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_6;
				break;

			case RfCfgParam_Wavebank_7:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_7;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_7;
				break;

			case RfCfgParam_Wavebank_8:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_8;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_8;
				break;

			case RfCfgParam_Wavebank_9:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_9;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_9;
				break;

			case RfCfgParam_Wavebank_10:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_10;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_10;
				break;

			case RfCfgParam_Wavebank_11:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_11;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_11;
				break;

			case RfCfgParam_Wavebank_12:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_12;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_12;
				break;

			case RfCfgParam_Wavebank_13:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_13;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_13;
				break;

			case RfCfgParam_Wavebank_14:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_14;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_14;
				break;

			case RfCfgParam_Wavebank_15:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_15;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_15;
				break;

			case RfCfgParam_Wavebank_16:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_16;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_16;
				break;

			case RfCfgParam_Wavebank_17:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_17;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_17;
				break;

			case RfCfgParam_Wavebank_18:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_18;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_18;
				break;

			case RfCfgParam_Wavebank_19:
				nscRfCfgParams->ID = RfCfgParam_Wavebank_19;
				*config_type = PTX_NSC_RFCONF_TX_POWER_MODE;
				*wavebank_id = PTX_NSC_RFCONF_WAVEBANK_19;
				break;

            case RfCfgParam_RegsPollA106:
                nscRfCfgParams->ID = RfCfgParam_RegsPollA106;
                *config_type = PTX_NSC_RFCONF_POLL_A_106;
                break;

            case RfCfgParam_RegsPollA212:
                nscRfCfgParams->ID = RfCfgParam_RegsPollA212;
                *config_type = PTX_NSC_RFCONF_POLL_A_212;
                break;

            case RfCfgParam_RegsPollA424:
                nscRfCfgParams->ID = RfCfgParam_RegsPollA424;
                *config_type = PTX_NSC_RFCONF_POLL_A_424;
                break;

            case RfCfgParam_RegsPollA848:
                nscRfCfgParams->ID = RfCfgParam_RegsPollA848;
                *config_type = PTX_NSC_RFCONF_POLL_A_848;
                break;

            case RfCfgParam_RegsPollB106:
                nscRfCfgParams->ID = RfCfgParam_RegsPollB106;
                *config_type = PTX_NSC_RFCONF_POLL_B_106;
                break;

            case RfCfgParam_RegsPollB212:
                nscRfCfgParams->ID = RfCfgParam_RegsPollB212;
                *config_type = PTX_NSC_RFCONF_POLL_B_212;
                break;

            case RfCfgParam_RegsPollB424:
                nscRfCfgParams->ID = RfCfgParam_RegsPollB424;
                *config_type = PTX_NSC_RFCONF_POLL_B_424;
                break;

            case RfCfgParam_RegsPollB848:
                nscRfCfgParams->ID = RfCfgParam_RegsPollB848;
                *config_type = PTX_NSC_RFCONF_POLL_B_848;
                break;

            case RfCfgParam_RegsPollF212:
                nscRfCfgParams->ID = RfCfgParam_RegsPollF212;
                *config_type = PTX_NSC_RFCONF_POLL_F_212;
                break;

            case RfCfgParam_RegsPollF424:
                nscRfCfgParams->ID = RfCfgParam_RegsPollF424;
                *config_type = PTX_NSC_RFCONF_POLL_F_424;
                break;

            case RfCfgParam_RegsPollV:
                nscRfCfgParams->ID = RfCfgParam_RegsPollV;
                *config_type = PTX_NSC_RFCONF_POLL_V;
                break;

            case RfCfgParam_Listen:
                nscRfCfgParams->ID = RfCfgParam_Listen;
                *config_type = PTX_NSC_RFCONF_LISTEN;
                break;

            case RfCfgParam_RegsMisc:
                nscRfCfgParams->ID = RfCfgParam_RegsMisc;
                *config_type = PTX_NSC_RFCONF_MISC;
                break;

            default:
                nscRfCfgParams->ID = 0;
                *config_type = PTX_NSC_RFCONF_LAST_PARAM;
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                break;
        }
    }

    return status;
}

static ptxStatus_t ptxNSC_RFConfig_GetConfigPointer(uint8_t paramId, uint8_t **rfConfigValue, uint8_t *rfConfigValueLen)
{
    /*
     * This function is used to retrieve a pointer to specific RF-Configuration which may be stored in any memory
     * (internal or external) and can be overwritten to meet target specific requirements.
     *
     * Note: RF-Configuration in general belongs to the NSC-domain, therefore this function is not part of the
     *       PLAT-component.
     *
     * The provided reference implementation returns pointers and lengths taken from the provided files
     * ptxNSC_RfConfigVal.c and ptxNSC_RfConfigVal.h. If this function gets overwritten, both files may be
     * excluded from the build (adaptations in CMake build-configuration required).
     * */

    ptxStatus_t status = ptxStatus_Success;

    if( (NULL != rfConfigValue) && (NULL != rfConfigValueLen) )
    {
        switch(paramId)
        {
        	case RfCfgParam_Wavebank_0:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_0_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_0[0];
				break;

        	case RfCfgParam_Wavebank_1:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_1_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_1[0];
				break;

			case RfCfgParam_Wavebank_2:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_2_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_2[0];
				break;

			case RfCfgParam_Wavebank_3:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_3_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_3[0];
				break;

			case RfCfgParam_Wavebank_4:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_4_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_4[0];
				break;

			case RfCfgParam_Wavebank_5:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_5_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_5[0];
				break;

			case RfCfgParam_Wavebank_6:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_6_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_6[0];
				break;

			case RfCfgParam_Wavebank_7:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_7_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_7[0];
				break;

			case RfCfgParam_Wavebank_8:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_8_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_8[0];
				break;

			case RfCfgParam_Wavebank_9:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_9_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_9[0];
				break;

			case RfCfgParam_Wavebank_10:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_10_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_10[0];
				break;

			case RfCfgParam_Wavebank_11:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_11_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_11[0];
				break;

			case RfCfgParam_Wavebank_12:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_12_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_12[0];
				break;

			case RfCfgParam_Wavebank_13:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_13_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_13[0];
				break;

			case RfCfgParam_Wavebank_14:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_14_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_14[0];
				break;

			case RfCfgParam_Wavebank_15:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_15_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_15[0];
				break;

			case RfCfgParam_Wavebank_16:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_16_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_16[0];
				break;

			case RfCfgParam_Wavebank_17:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_17_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_17[0];
				break;

			case RfCfgParam_Wavebank_18:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_18_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_18[0];
				break;

			case RfCfgParam_Wavebank_19:
				*rfConfigValueLen = ptxNSC_RfConfig_Wavebank_19_len;
				*rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_Wavebank_19[0];
				break;

            case RfCfgParam_RegsPollA106:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollA106_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollA106[0];
                break;

            case RfCfgParam_RegsPollA212:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollA212_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollA212[0];
                break;

            case RfCfgParam_RegsPollA424:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollA424_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollA424[0];
                break;

            case RfCfgParam_RegsPollA848:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollA848_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollA848[0];
                break;

            case RfCfgParam_RegsPollB106:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollB106_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollB106[0];
                break;

            case RfCfgParam_RegsPollB212:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollB212_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollB212[0];
                break;

            case RfCfgParam_RegsPollB424:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollB424_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollB424[0];
                break;

            case RfCfgParam_RegsPollB848:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollB848_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollB848[0];
                break;

            case RfCfgParam_RegsPollF212:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollF212_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollF212[0];
                break;

            case RfCfgParam_RegsPollF424:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollF424_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollF424[0];
                break;

            case RfCfgParam_RegsPollV:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsPollV_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsPollV[0];
                break;

            case RfCfgParam_Listen:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsListen_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsListen[0];
                break;

            case RfCfgParam_RegsMisc:
                *rfConfigValueLen = ptxNSC_RfConfig_RegsMisc_len;
                *rfConfigValue = (uint8_t*)&ptxNSC_RfConfig_RegsMisc[0];
                break;

            default:
                *rfConfigValueLen = 0;
                *rfConfigValue = NULL;
                status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InternalError);
                break;
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNSC_RFConfig_ReleasePointer(void)
{
    ptxStatus_t status = ptxStatus_Success;

    /* nothing to do in the reference implementation */

    return status;
}

static ptxStatus_t ptxNSC_RfConfig_CheckRsp(ptxNSC_t *nscCtx)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t nsc_rfConfig_resp_len = 0;
    uint8_t *nsc_rfConfig_resp = NULL;

    status = ptxNSC_ReceiveRsp(nscCtx, &nsc_rfConfig_resp, &nsc_rfConfig_resp_len, PTX_NSC_TRANSFER_TO);

    if ((ptxStatus_Success == status) && (NULL != nsc_rfConfig_resp) && (nsc_rfConfig_resp_len > 0))
    {
        if ((PTX_NSC_RFCONF_RESP_LEN == nsc_rfConfig_resp_len) && (PTX_NSC_RFCONF_RSP_OPCODE == nsc_rfConfig_resp[0]))
        {
            status = ptxNSC_ProcessRspErrorCode (nsc_rfConfig_resp[1u]);
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NscProtocolError);
        }
    }

    return status;
}


