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
    File        : ptxNSC_System.c

    Description :
*/

#include "ptxNSC_System.h"
#include "ptxNSC.h"
#include <string.h>


/*
 * ####################################################################################################################
 * FUNCTIONS API.
 * ####################################################################################################################
 */

ptxStatus_t ptxNSC_System_SetConfig (ptxNSC_t *nscCtx, ptxNSC_System_t *sysParams)
{
    ptxStatus_t ret = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != sysParams))
    {
        (void)memcpy(nscCtx->SysParams, sysParams, sizeof(ptxNSC_System_t));

    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    ret = ptxNSC_CheckSystemState(nscCtx, ret);

    return ret;
}

ptxStatus_t ptxNSC_System_GetConfig (ptxNSC_t *nscCtx, ptxNSC_System_t *sysParams)
{
    ptxStatus_t ret = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC) && (NULL != sysParams))
    {
        (void)memcpy(sysParams, nscCtx->SysParams, sizeof(ptxNSC_System_t));

    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return ret;
}

ptxStatus_t ptxNSC_System_ApplyDefaults (ptxNSC_t *nscCtx)
{
    ptxStatus_t ret = ptxStatus_Success;

    if (PTX_COMP_CHECK(nscCtx, ptxStatus_Comp_NSC))
    {
        nscCtx->SysParams->Version = SYSTEM_DEFAULT_VERSION;
        nscCtx->SysParams->PowerAmpOverCurrThreshold = SYSTEM_DEFAULT_PA_OVERCURRENT_TH;
        nscCtx->SysParams->PowerAmpTempThreshold     = SYSTEM_DEFAULT_PA_TEMPERATURE_TH;
        nscCtx->SysParams->ConClkSource              = SYSTEM_DEFAULT_CON_CLK_SOURCE;
        nscCtx->SysParams->ConVarLBS                 = SYSTEM_DEFAULT_CON_VAR_LBS;
        (void)memcpy(&nscCtx->SysParams->conHost[0],   &SYSTEM_DEFAULT_CON_NHOST[0],    sizeof(SYSTEM_DEFAULT_CON_NHOST));
        (void)memcpy(&nscCtx->SysParams->conHostCE[0], &SYSTEM_DEFAULT_CON_NHOST_CE[0], sizeof(SYSTEM_DEFAULT_CON_NHOST_CE));

    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
    }

    return ret;
}


