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
    Module      : Generic NDEF OPERATION API
    File        : ptxNDEF.h

    Description : Generic NDEF Operation API (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_ndef_op Generic NDEF Operation API
 *
 * @{
 */

#ifndef APIS_PTX_NDEF_H_
#define APIS_PTX_NDEF_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T2TOP.h"
#include "ptxNDEF_T3TOP.h"
#include "ptxNDEF_T4TOP.h"
#include "ptxNDEF_T5TOP.h"
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
 * \brief Forward declaration of CardRegistry.
 */
struct ptxIoTRd_CardRegistry;

/**
 * \brief T5T communication mode.
 */
typedef enum ptxNDEF_T5T_Modes
{
    ptxNDEF_T5T_Mode_None,     /**< ptxNDEF_T5T_Mode_None */
    ptxNDEF_T5T_Mode_Addressed,/**< ptxNDEF_T5T_Mode_Addressed */
    ptxNDEF_T5T_Mode_Selected, /**< ptxNDEF_T5T_Mode_Selected */
} ptxNDEF_T5T_Modes_t;

/**
 * \brief NDEF OP Initialization Parameters
 */
typedef struct ptxNDEF_InitParams
{
    void                            *IotRd;                 /**< Allocated instance of IoT-Reader component (see ptxIoTRd_Allocate_Stack()) */
    uint8_t                         *TxBuffer;              /**< Allocated instance of a TxBuffer */
    uint32_t                        TxBufferSize;           /**< Size of the TxBuffer */
    uint8_t                         *RxBuffer;              /**< Allocated instance of an RxBuffer */
    uint32_t                        RxBufferSize;           /**< Size of RxBuffer */
    uint8_t                         *WorkBuffer;            /**< Allocated instance of an WorkBuffer */
    uint32_t                        WorkBufferSize;         /**< Size of WorkBuffer */

    ptxNDEF_T5T_Modes_t             T5TMode;                /**< Flag to do T5T Communication Addressed/Selected despite only 1 Card in the Field. */
} ptxNDEF_InitParams_t;

/**
 * \brief Generic NDEF OP Component
 */
typedef struct ptxNDEF
{
    /* Components */
    ptxStatus_Comps_t               CompId;                /**< Component Id */

    struct ptxIoTRd_CardRegistry    *CardRegistry;          /**< Reference to Internal Card Registry */
    ptxNDEF_T2TOP_t                 T2TOP;                  /**< T2T Operation Component */
    ptxNDEF_T3TOP_t                 T3TOP;                  /**< T3T Operation Component */
    ptxNDEF_T4TOP_t                 T4TOP;                  /**< T4T Operation Component */
    ptxNDEF_T5TOP_t                 T5TOP;                  /**< T5T Operation Component */

} ptxNDEF_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the NDEF OP Component.
 *
 * \param[in]   ndefComp            Pointer to an allocated instance of the NDEF-OP component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_Open (ptxNDEF_t *ndefComp, ptxNDEF_InitParams_t *initParams);

/**
 * \brief Formats a given Tag to INITIALIZED state.
 *
 * \param[in]   ndefComp           Pointer to an initialized instance of the NDEF-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_FormatTag (ptxNDEF_t *ndefComp);

/**
 * \brief Checks if a NDEF-message is present on the given Tag (or not).
 *
 * \param[in]   ndefComp           Pointer to an initialized instance of the NDEF-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_CheckMessage (ptxNDEF_t *ndefComp);

/**
 * \brief Reads a NDEF-message from a given Tag.
 *
 * \param[in]     ndefComp            Pointer to an initialized instance of the NDEF-OP component.
 * \param[in]     msgBuffer           Pointer to buffer holding the read NDEF-message.
 * \param[in,out] msgLen              Size of the buffer (in), Length of the read NDEF-message (out).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_ReadMessage (ptxNDEF_t *ndefComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Writes a NDEF-message onto a given Tag.
 *
 * \param[in] ndefComp            Pointer to an initialized instance of the NDEF-OP component.
 * \param[in] msgBuffer           Pointer to buffer holding the NDEF-message to write.
 * \param[in] msgLen              Size of NDEF-message.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_WriteMessage (ptxNDEF_t *ndefComp, uint8_t *msgBuffer, uint32_t msgLen);

/**
 * \brief Puts a Tag into READ-ONLY state (Attention: This is a irreversible Operation!).
 *
 * \param[in]   ndefComp           Pointer to an initialized instance of the NDEF-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_LockTag (ptxNDEF_t *ndefComp);

/**
 * \brief Unitialize / Close the NDEF OP Component
 *
 * \param[in]   ndefComp           Pointer to an initialized instance of the NDEF-OP component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNDEF_Close (ptxNDEF_t *ndefComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

