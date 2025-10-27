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
    Module      : NSC System Features
    File        : ptxNSC_System.h

    Description :
*/

/**
 * \addtogroup grp_ptx_api_nsc_sys PTX NSC Stack System
 *
 * @{
 */

#ifndef PTXNSC_SYSTEM_H_
#define PTXNSC_SYSTEM_H_

#include "ptxStatus.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES / INTERNALS
 * ####################################################################################################################
 */

/**
 * \name System Features - Config Tags
 * 
 * @{
 */
# define SYSTEM_CFG_TAG_VERSION                         (0x00U)                                    /**< Config Tag version */
# define SYSTEM_CFG_TAG_PA_OVERCURRENT_TH               (0x01U)                                    /**< Config Tag Power Amp Overcurrent threshold */
# define SYSTEM_CFG_TAG_PA_TEMPERATURE_TH               (0x02U)                                    /**< Config Tag Power Amp Temperature threshold */
# define SYSTEM_CFG_TAG_CON_CLK_SOURCE                  (0x03U)                                    /**< Config Tag Clock synthesizer source */
# define SYSTEM_CFG_TAG_CON_VAR_LBS                     (0x04U)                                    /**< Config Tag reference division select */
# define SYSTEM_CFG_TAG_CON_NHOST                       (0x05U)                                    /**< Config Tag Division factor for Clock synthesizer. */
# define SYSTEM_CFG_TAG_CON_NHOST_CE                    (0x06U)                                    /**< Config Tag Division factor for Clock synthesizer in CE-mode. */
/** @} */

/**
 * \name Default Values
 * 
 * @{
 */
# define SYSTEM_DEFAULT_VERSION                         (0x10U)                                    /**< System Default version */
# define SYSTEM_DEFAULT_PA_OVERCURRENT_TH               (0x04U)                                    /**< System Default Power Amp Overcurrent threshold */
# define SYSTEM_DEFAULT_PA_TEMPERATURE_TH               (0xFFU)                                    /**< System Default Power Amp Temperature threshold */
# define SYSTEM_DEFAULT_CON_CLK_SOURCE                  (0x00U)                                    /**< System Default Clock synthesizer source */
# define SYSTEM_DEFAULT_CON_VAR_LBS                     (0x20U)                                    /**< System Default reference division select */
static const uint8_t SYSTEM_DEFAULT_CON_NHOST[4]        = {0x78U, 0x00U, 0x00U, 0x00U};            /**< System Default Division factor for Clock synthesizer. */
static const uint8_t SYSTEM_DEFAULT_CON_NHOST_CE[4]     = {0x78U, 0x40U, 0x00U, 0x00U};            /**< System Default Division factor for Clock synthesizer in CE-mode. */
/** @} */

/*
 * Forward declaration
 */
struct ptxNSC_System;
struct ptxNSC;

/**
 * \brief PTX NSC system values
 */ 
typedef struct ptxNSC_System
{
    uint8_t Version;                        /**< System Feature Version>*/
    uint8_t UseExtCfg;                      /**< Global-Flag: Use configuration values from Function call>*/
    uint8_t PowerAmpTempThreshold;          /**< Power Amplifier: Temperature Threshold.>*/
    uint8_t PowerAmpOverCurrThreshold;      /**< Power Amplifier: Over-current Threshold.>*/
    uint8_t ConClkSource;                   /**< Source of Clock synthesizer.>*/
    uint8_t ConVarLBS;                      /**< Reference Division Select Parameter.>*/
    uint8_t conHost[4];                     /**< Division factor for Clock synthesizer.>*/
    uint8_t conHostCE[4];                   /**< Division factor for Clock synthesizer in CE-mode.>*/
} ptxNSC_System_t;


/*
 * ####################################################################################################################
 * FUNCTIONS API.
 * ####################################################################################################################
 */

/**
 * \brief Function used to directly set System-Config parameters.
 *
 * \param [in]    nscCtx            Pointer to the component structure.
 * \param [in]    sysParams         Pointer to the System Config Parameters.
 *
 * \return Status of the operation. See definition of \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_System_SetConfig (struct ptxNSC *nscCtx, ptxNSC_System_t *sysParams);

/**
 * \brief Function used to retrieve the current System-Config parameters.
 *
 * \param [in]    nscCtx            Pointer to the component structure.
 * \param [out]   sysParams         Pointer to the System Config Parameters.
 *
 * \return Status of the operation. See definition of \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_System_GetConfig (struct ptxNSC *nscCtx, ptxNSC_System_t *sysParams);

/**
 * \brief Function used to apply default System-Config parameters.
 *
 * \param [in]    nscCtx            Pointer to the component structure.
 *
 * \return Status of the operation. See definition of \ref ptxStatus_t.
 */
ptxStatus_t ptxNSC_System_ApplyDefaults (struct ptxNSC *nscCtx);

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

