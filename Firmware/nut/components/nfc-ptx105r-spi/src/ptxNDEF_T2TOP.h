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
    Module      : NDEF T2T OPERATION API
    File        : ptxNDEF_T2TOP.h

    Description : Tag Type 2 NDEF Operation API (IOT READER - Extension)
*/

/**
 * \addtogroup grp_ptx_api_T2T_op Tag Type T2T Operation API
 *
 * @{
 */

#ifndef APIS_PTX_NDEF_OP_T2T_H_
#define APIS_PTX_NDEF_OP_T2T_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxNativeTag_T2T.h"
#include "ptxNDEF_Defines.h"
#include "ptxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/**
 * \name T2T Tag specific definitions
 * @{
 */
#define PTX_T2T_DEFAULT_TIMEOUT_MS          (uint32_t)100   /**< T2T default timeout in ms */
#define PTX_T2T_SUPPORTED_VERSION           (uint8_t)0x11   /**< T2T supported version */
#define PTX_T2TOP_MAX_NUMBER_LOCK_CONTROL   (uint8_t)3u     /**< Maximum number of T2T operation lock controls */
#define PTX_T2TOP_MAX_NUMBER_MEMORY_CONTROL (uint8_t)3u     /**< Maximum number of T2T operation memory controls */
#define PTX_T2TOP_MAX_NUMBER_NDEFTLVS       (uint8_t)3u     /**< T2T NDEF Operations Maximum NDEF TLVs to be handled. */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief Variant which block(s) to read first to get the Capability Container CC.
 */
typedef enum ptxNDEF_T2TOP_ReadCCVariant
{
    ReadCCVariant_Block_3_Default,                              /**< Reading CC-Block starts at Block 3 (default) */
    ReadCCVariant_Block_0,                                      /**< Reading CC-Block starts at Block 0 */
} ptxNDEF_T2TOP_ReadCCVariant_t;


/**
 * \brief T2T NDEF OP Initialization Parameters
 */
typedef struct ptxNDEF_T2TOP_InitParams
{
    ptxNativeTag_T2T_InitParams_t        T2TInitParams;         /**< Initialization parameters of lower layer. */

    uint8_t                              *WorkBuffer;           /**< Internal Workbuffer. */
    uint8_t                              *RxBuffer;             /**< Internal Rx-Buffer provided by upper layer (may be shared with other components) */
    size_t                               WorkBufferSize;        /**< Internal Workbuffer Size. */
    size_t                               RxBufferSize;          /**< Internal Rx-Buffer size */
    ptxNDEF_T2TOP_ReadCCVariant_t        ReadCCVariant;         /**< Reading initial NDEF-Content/CC-Block starting at Block 0 or 3 */

} ptxNDEF_T2TOP_InitParams_t;


/**
 * \brief T2T NDEF OP Capability Container Parameters
 */
typedef struct ptxNDEF_T2TOP_CC
{
    uint8_t                         MagicNumber;                    /**< Magic Number */
    uint8_t                         Version;                        /**< Version */
    uint16_t                        MLEN;                           /**< Length */
    uint8_t                         Access;                         /**< Access contains read and write access */

    uint8_t                         ReadAccess;                     /**< read permission */
    uint8_t                         WriteAccess;                    /**< write permission */
    uint8_t                         VersionMajor;                   /**< Major Version */
    uint8_t                         VersionMinor;                   /**< Minor Version */
    uint16_t                        Size;                           /**< Size in bytes */
    uint16_t                        NumberOfBlocks;                 /**< NUmber of Blocks each containing 4 bytes */

} ptxNDEF_T2TOP_CC_t;

/**
 * \brief T2T NDEF TLV position
 */
typedef struct ptxNDEF_T2TOP_NDEF_TLV
{
    uint16_t                        Length;                                     /**< Length in byte */
    uint8_t                         LengthBlock[PTX_T2T_BLOCK_SIZE*2u];        /**< Length blocks cache of NDEF TLV, can be up to two blocks if split. */
    uint8_t                         LengthByteOffset;                           /**< Offset in length-block. */
    uint16_t                        ByteAddress;                    /**< Byte address of T byte */
} ptxNDEF_T2TOP_NDEF_TLV_t;

/**
 * \brief T2T Terminator TLV position
 */
typedef struct ptxNDEF_T2TOP_TERMINATOR_TLV
{
    uint16_t                        ByteAddress;                    /**< Byte address of T byte */
    uint8_t                         TerminatorTLVFound;             /**< Terminator TLV found check, only one needed */

} ptxNDEF_T2TOP_TERMINATOR_TLV_t;

/**
 * \brief T2T Lock Control TLV positions and counter
 */
typedef struct ptxNDEF_T2TOP_LOCK_CTRL_TLV
{
    uint16_t                        ByteAddress;                 /**< Byte addresses of T byte */
    uint16_t                        LockArea[2];                 /**< begin and end byte address of areas [begin,end] */
    uint8_t                         NbrReservedBits;             /**< Reserved Bits for not completely lock-bit-filled bytes */
    uint8_t                         BytesLockedPerLockBit;       /**< Bytes Locked per Lock Bit */
    uint16_t                        InternalArea[2];             /**< Internal Bytes beginning and end. End is always at least the same as LockArea end. */

} ptxNDEF_T2TOP_LOCK_CTRL_TLV;

/**
 * \brief T2T Memory Control TLV positions and counter
 */
typedef struct ptxNDEF_T2TOP_MEMORY_CTRL_TLV
{
    uint16_t                        ByteAddress;              /**< Byte addresses of T byte */
    uint16_t                        RsvdArea[2];              /**< begin and end byte address of areas [begin,end] */

} ptxNDEF_T2TOP_MEMORY_CTRL_TLV_t;

/**
 * \brief T2T NDEF OP TLV byte adresses and counters
 */
typedef struct ptxNDEF_T2TOP_TLV
{
    ptxNDEF_T2TOP_TERMINATOR_TLV_t                      TerminatorTLV;                                          /**< Terminator TLV */
    ptxNDEF_T2TOP_NDEF_TLV_t                            NDEFTLV[PTX_T2TOP_MAX_NUMBER_NDEFTLVS];                 /**< NDEF TLV */
    uint8_t                                             NumberOfNdefTLVs;                                       /**< Number of NDEF TLVs */
    ptxNDEF_T2TOP_LOCK_CTRL_TLV                         LockControlTLVs[PTX_T2TOP_MAX_NUMBER_LOCK_CONTROL];     /**< Lock control TLV */
    uint8_t                                             NumberOfLockTLVs;                                       /**< Number of Lock Control TLVs */
    ptxNDEF_T2TOP_MEMORY_CTRL_TLV_t                     MemoryControlTLVs[PTX_T2TOP_MAX_NUMBER_MEMORY_CONTROL]; /**< Memory control TLV */
    uint8_t                                             NumberOfMemoryTLVs;                                     /**< Number of Memory Control TLVs */
    uint16_t                                            AvailableNdefLength;                                    /**< Available length in byte, not including other TLVs and DLAs/RAs */

} ptxNDEF_T2TOP_TLV_t;

/**
 * \brief T2T NDEF OP sector information
 */ 
typedef struct ptxNDEF_T2TOP_Sector
{
    uint8_t                                             CurrentSector;                              /**< Current sector */
    uint8_t                                             NumberOfSectors;                            /**< Number of Sectors */

} ptxNDEF_T2TOP_Sector_t;


/**
 * \brief T2T NDEF OP Component
 */
typedef struct ptxNDEF_T2TOP
{
    /* Components */
    ptxStatus_Comps_t               CompId;                /**< Component Id */

    ptxNativeTag_T2T_t              NativeTagT2T;          /**< T2T Native Tag Component */
    ptxNDEF_TagLifeCycle_t          LifeCycle;             /**< Tag Life-Cycle */
    uint8_t                         *RxBuffer;             /**< Internal Rx-Buffer provided by upper layer (may be shared with other components) */
    size_t                          RxBufferSize;          /**< Internal Rx-Buffer size */
    size_t                          RxLen;                 /**< Internal Current RxLen */
    uint8_t                         *WorkBuffer;           /**< Internal Work-Buffer */
    size_t                          WorkBufferSize;        /**< Internal Workbuffer Size. */
    ptxNDEF_T2TOP_ReadCCVariant_t   ReadCCVariant;         /**< Reading initial NDEF-Content/CC-Block starting at Block 0 or 3 */

    /* Tag Type specific members */
    ptxNDEF_T2TOP_CC_t              CCParams;              /**< T2T CC Parameters */
    ptxNDEF_T2TOP_TLV_t             TLVs;                  /**< T2T TLVs */
    ptxNDEF_T2TOP_Sector_t          SectorParams;          /**< T2T Sector Parameters */

    /* Position Tracking within the T2T Memory. */
    uint8_t                         CurrentBlock;          /**< Current Block in Memory. */
    uint16_t                        CurrentBlockAddress;   /**< Current Block Address in Memory. */
    uint16_t                        CurrentByteAddress;    /**< Current Byte Address in Memory. */


    /* General Info and / or Greedy Collecting related Parameters */
    uint8_t                         LastOperationCheck;    /**< Flag whether the last operation we did was a CheckMsg. */

} ptxNDEF_T2TOP_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initialize / Open the T2T OP Component.
 *
 * \param[in]   t2tOpComp           Pointer to an allocated instance of the T2T-OP component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpOpen (ptxNDEF_T2TOP_t *t2tOpComp, ptxNDEF_T2TOP_InitParams_t *initParams);

/**
 * \brief Formats a Type 2 Tag to INITIALIZED state.
 *
 * \param[in]   t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpFormatTag (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Checks if a NDEF-message is present on the given Tag (or not).
 *
 * \param[in]   t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpCheckMessage (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Reads a NDEF-message from a given Tag.
 *
 * \param[in]     t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 * \param[in]     msgBuffer           Pointer to buffer holding the read NDEF-message.
 * \param[in,out] msgLen              Size of the buffer (in), Length of the read NDEF-message (out).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpReadMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *msgBuffer, uint32_t *msgLen);

/**
 * \brief Writes a NDEF-message onto a given Tag.
 *
 * \param[in] t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 * \param[in] msgBuffer           Pointer to buffer holding the NDEF-message to write (NULL -> empty NDEF-message is written).
 * \param[in] msgLen              Size of NDEF-message (0 -> empty NDEF-message is written).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpWriteMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *msgBuffer, uint32_t msgLen);

/**
 * \brief Puts a Tag into READ-ONLY state (Attention: This is a irreversible Operation!).
 *
 * \param[in]   t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxNDEF_T2TOpLockTag (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Unitialize / Close the T2T OP Component
 *
 * \param[in]   t2tOpComp           Pointer to an initialized instance of the T2T-OP component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxNDEF_T2TOpClose (ptxNDEF_T2TOP_t *t2tOpComp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

