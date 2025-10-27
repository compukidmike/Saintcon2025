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
    File        : ptxNSC_Rd.h

    Description :
*/

#ifndef COMPS_NSC_NSC_RD_PTXNSC_RD_H_
#define COMPS_NSC_NSC_RD_PTXNSC_RD_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxStatus.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * Forward declaration
 */
struct ptxNSC;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Function used to check the presence check on ISO-DEP protocol. Mechanism NACK.
 *
 * \param[in]   nscCtx              Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Rd_RfPressCheck_Nack (struct ptxNSC *nscCtx);


/**
 * \brief Function used to check the presence check on ISO-DEP protocol. Mechanism Empty Frame.
 *
 * \param[in]   nscCtx              Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Rd_RfPressCheck_EmptyFrame (struct ptxNSC *nscCtx);


/**
 * \brief Function used to check the presence check on NFC-DEP protocol. Mechanism Empty Frame.
 *
 * \param[in]   nscCtx              Pointer to an initialized instance of the NSC.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Rd_RfPressCheck_AttentionCmd (struct ptxNSC *nscCtx);


/**
 * \brief T5T Isolated EoF.
 *
 * \param[in]       nscCtx              Pointer to the component structure.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_T5T_IsolatedEoF(struct ptxNSC *nscCtx);

/**
 * \brief Function used to send NSC Rf Clt Msg to the chip.
 *
 * \param[in]   nscCtx              Pointer to an initialized instance of the NSC.
 * \param[in]   pldBytes            Pointer to the payload bytes.
 * \param[in]   pldParityBits       Pointer to the parity bits (as bytes) of the payload bytes.
 * \param[in]   length              Length of /ref pldBytes and /ref pldParityBits.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxNSC_Rd_RfCltMsg (struct ptxNSC *nscCtx, uint8_t *pldBytes, uint8_t *pldParityBits, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* Guard */

