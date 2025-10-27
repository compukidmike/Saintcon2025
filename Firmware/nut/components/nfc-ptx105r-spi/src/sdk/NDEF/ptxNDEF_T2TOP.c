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
    Module      : NATIVE TAG API
    File        : ptxNativeTag_T2T.c

    Description : NDEF API for NFC Forum Tag Type 2 (IOT READER - Extension)
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T2TOP.h"
#include "ptxNativeTag_T2T.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \name Buffer limit checkers
 * @{
 */
#define PTX_T2TOP_LOCKAREA_BUFSIZE            (uint8_t)(PTX_T2T_MAX_NUMBER_LOCK_CONTROL * 2)   /**< Buffer size for storing Lock Areas. */
#define PTX_T2TOP_RSVDAREA_BUFSIZE            (uint8_t)(PTX_T2T_MAX_NUMBER_MEMORY_CONTROL * 2) /**< Buffer size for storing Reserved Areas. */
/** @} */

/**
 * \name Important Block Numbers
 * @{
 */
#define PTX_T2TOP_STATIC_LOCK_BLOCK_NUMBER    (uint8_t)0x02    /**< Block number containing Static Lock Bits. */
#define PTX_T2TOP_CC_BLOCK_NUMBER             (uint8_t)0x03    /**< Block number containing CC. */
#define PTX_T2TOP_TLV_AREA_BEGIN_BLOCK        (uint16_t)0x0004 /**< Block number of first TLV area block. */
/** @} */

/**
 * \name CC Offsets and Parameters
 * @{
 */
#define PTX_T2TOP_CC_OFFSET_MAGIC_NUMBER      (uint8_t)0x00    /**< CC Block offset for Magic Number. */
#define PTX_T2TOP_CC_OFFSET_VERSION_INFO      (uint8_t)0x01    /**< CC Block offset for version information. */
#define PTX_T2TOP_CC_OFFSET_TAG_SIZE          (uint8_t)0x02    /**< CC Block offset for tag memory size. */
#define PTX_T2TOP_CC_OFFSET_ACCESS_COND       (uint8_t)0x03    /**< CC Block offset for access conditions. */
#define PTX_T2TOP_CC_MAGIC_NUMBER             (uint8_t)0xE1    /**< T2T Magic Number. */
#define PTX_T2TOP_CC_ACCESS_BITS              (uint8_t)0x0F    /**< Mask for access condition bits. */
#define PTX_T2TOP_CC_MLEN_FACTOR              (uint8_t)8u      /**< Multiplication factor for tag size in bytes. */
/** @} */

/**
 * \name Bit Shift Masks
 * @{
 */
#define PTX_T2TOP_MASK_15                     (uint8_t)0x0F    /**< Mask 15. */
#define PTX_T2TOP_MASK_8                      (uint8_t)0x08    /**< Mask 8. */
#define PTX_T2TOP_MASK_4                      (uint8_t)0x04    /**< Mask 4. */
#define PTX_T2TOP_MASK_1                      (uint8_t)0x01    /**< Mask 1. */
/** @} */

/**
 * \name TLV Parameters
 * @{
 */
#define PTX_T2TOP_NULL_TLV_T                  (uint8_t)0x00    /**< NULL TLV T-value */
#define PTX_T2TOP_LOCK_CONTROL_TLV_T          (uint8_t)0x01    /**< Lock Control TLV T-value. */
#define PTX_T2TOP_MEMORY_CONTROL_TLV_T        (uint8_t)0x02    /**< Memory Control TLV T-value. */
#define PTX_T2TOP_NDEF_TLV_T                  (uint8_t)0x03    /**< NDEF TLV T-value. */
#define PTX_T2TOP_TERMINATOR_TLV_T            (uint8_t)0xFE    /**< Terminator TLV T-value. */
#define PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH     (uint8_t)0x03    /**< Lock Control TLV L-value. */
#define PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH   (uint8_t)0x03    /**< Memory Control TLV L-value. */
#define PTX_T2TOP_TERMINATOR_TLV_LENGTH       (uint8_t)0x01    /**< Terminator TLV length. */
#define PTX_T2TOP_TERMINATOR_TLV_PRESENT      (uint8_t)0x01    /**< Flag to use when a Terminator TLV is found. */
#define PTX_T2TOP_NO_TERMINATOR_TLV           (uint8_t)0x00    /**< Flag to use when no Terminator TLV is found. */
#define PTX_T2TOP_NDEF_L_FIELD_SHORT          (uint8_t)1u      /**< NDEF TLV L-value for short TLVs. */
#define PTX_T2TOP_NDEF_L_FIELD_LONG           (uint8_t)3u      /**< NDEF TLV L-value for long TLVs. */
#define PTX_T2TOP_MAX_NDEF_BYTE_OFFSET        (uint8_t)3u      /**< Maximum offset of data byte within a block. */
/** @} */

/**
 * \name Operation Size Informations
 * @{
 */
#define PTX_T2TOP_BLOCKS_PER_READ             (uint8_t)4u      /**< T2T blocks read per read operation. */
#define PTX_T2TOP_MIN_SIZE_FOR_DLA            (uint8_t)0x30    /**< Minimum memory size for dynamic lock areas to be present. */
#define PTX_T2TOP_MAX_NBR_NDEF_TLV            (uint8_t)1u      /**< Internal maximum of NDEF TLVs to be handled. */
#define PTX_T2TOP_SECTORSIZE                  (uint16_t)256u   /**< T2T sector size. */
#define PTX_T2TOP_LENGTHTHRESHOLD             (uint8_t)0xFF    /**< Max Length for 1 L-Byte, above 3 L-Bytes are used. */
#define PTX_T2TOP_SECTORTHRESHOLD             (uint8_t)0xFF    /**< Block Address threshold within a block where a new Sector will be needed. */
/** @} */

/**
 * \name Device Specifics
 * @{
 */
#define PTX_T2TOP_INTERNAL_BUFLENGTH          (uint8_t)64      /**< Internal work buffer length. */
#define PTX_T2TOP_ACK_CODE                    (uint8_t)0x0A    /**< T2T acknowledge. */
/** @} */

/**
 * \name Access Condition Values
 * @{
 */
#define PTX_T2TOP_ACCESS_GRANTED              (uint8_t)0x00    /**< Access condition granted. */
#define PTX_T2TOP_ACCESS_DENIED               (uint8_t)0xFF    /**< Access condition denied. */
/** @} */

/**
 * \name Lock Procedure Parameters
 * @{
 */
#define PTX_T2TOP_BYTES_LOCKED_PER_BIT        (uint8_t)8u      /**< Default number of bytes locked per lock bit. */
#define PTX_T2TOP_LOCKBITS                    (uint8_t)0xFF    /**< All lock bits of a lock byte set to locked state. */
#define PTX_T2TOP_ACCESS_READONLY             (uint8_t)0xFF    /**< Access condition Read Only. */
/** @} */

/**
 * \name Versioning Parameters
 * @{
 */
#define PTX_T2TOP_VALID_MAJOR_VERSION         (uint8_t)1u      /**< Major version supported. */
#define PTX_T2TOP_VALID_MINOR_VERSION         (uint8_t)0u      /**< Smallest minor version supported. */
/** @} */

/**
 * \name Passive Timeout Acknowledge according to NFC-Forum Standard.
 * @{
 */
#define PTX_T2TOP_PAT_MS                      (uint8_t)2u      /**< Passive acknowledge timeout. */
/** @} */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/**
 * \brief Get CC Parameters.
 *
 * \param[in] t2tOpComp         Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TGetCCInfo (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Handle Tag LC with CC Information.
 *
 * \param[in] t2tOpComp         Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2THandleCCInfo(ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Read Data Blocks.
 *
 * \param[in] t2tOpComp         Pointer to component.
 * \param[in] blockNumber       Block Number to start at.
 * \param[in,out] rx            Rx Buffer to write Data into.
 * \param[in,out] rxLen         Rx Buffer Length, maximum on input, Data length on output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TOpReadBlocks  (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t blockNumber, uint8_t *rx, size_t *rxLen);

/**
 * \brief Write Data Block.
 *
 * \param[in] t2tOpComp         Pointer to component.
 * \param[in] blockNumber       Block Number to write to.
 * \param[in] blockData         Data to write.
 * \param[in] blockDataLen      Length of Data to write.
 * \param[in,out] rx            Rx Buffer to write Response into.
 * \param[in,out] rxLen         Rx Buffer Length, maximum on input, Response length on output.
 * \param[in] msTimeout         Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TOpWriteBlock  (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t blockNumber, uint8_t *blockData, uint8_t blockDataLen, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);

/**
 * \brief Select a Sector.
 * \param[in] t2tOpComp         Pointer to component.
 * \param[in] secNr             Sector Number to select.
 * \param[in,out] rx            Rx Buffer to write Response into.
 * \param[in,out] rxLen         Rx Buffer Length, maximum on input, Response length on output.
 * \param[in] msTimeout         Timeout in ms.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TOpSectorSelect(ptxNDEF_T2TOP_t *t2tOpComp, uint8_t secNr, uint8_t *rx, size_t *rxLen, uint32_t msTimeout);
/**
 * \brief Read all available NDEF TLVs.
 *
 * \param[in] t2tOpComp             Pointer to component.
 * \param[in,out] msgBuffer         Buffer to store Messages.
 * \param[in,out] msgBufferOffset   Offset of Buffer to start filling on input, Bffer length at output.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TReadNDEFTLVs (ptxNDEF_T2TOP_t *t2tOpComp, size_t msgBufferSize, uint8_t *msgBuffer, uint16_t *msgBufferOffset);

/**
 * \brief Read Loop for a single NDEF TLV.
 *
 * \param[in]           t2tOpComp       Pointer to component.
 * \param[in]           msgLen          Message length to read.
 * \param[in,out]       msgBuffer       Buffer to store Messages.
 * \param[in,out]       bytesRead       Number of bytes read.
 * \param[in,out]       msgBufferOffset Offset within message buffer for data read.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TReadLoop(ptxNDEF_T2TOP_t *t2tOpComp, uint32_t msgLen, uint8_t *msgBuffer, uint16_t *bytesRead, uint16_t *msgBufferOffset);

/**
 * \brief Specific TLV to be overwritten.
 *
 * \param[in]           t2tOpComp       Pointer to component.
 * \param[in]           tx              Message data to be written.
 * \param[in]           txLen           Length of Message.
 * \param[in]           ndefTLVIndex    Index of TLV to be overwritten. If greater that existing number, new TLV is added.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TOverwriteMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *tx, size_t txLen, uint8_t ndefTLVIndex);
/**
 * \brief A function called after getting the CC, sweeping the TLV area and getting all already present TLVs on the tag.
 *
 * \param[in] t2tOpComp          Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TTLVAreaCrawler(ptxNDEF_T2TOP_t *t2tOpComp);
/**
 * \brief Find possible TLVs from last read in WorkBuffer.
 *
 * \param[in] t2tOpComp          Pointer to component, contains WorkBuffer to be analyzed.
 * \param[out] bytesToSkip       Pointer to counter of skipable bytes.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TIdentifyTLV   (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t *bytesToSkip);

/**
 * \brief Handle Identification of possible NDEF TLV.
 *
 * \param[in] t2tOpComp         Pointer to component, contains WorkBuffer to be analyzed.
 * \param[in,out] offset        Offset in internal WorkBuffer to start at, offset at which to continue after analysis.
 * \param[out] bytesToSkip      Pointer to counter of skipable bytes.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TNDEFTLV_Handler(ptxNDEF_T2TOP_t *t2tOpComp, uint32_t *offset, uint16_t *bytesToSkip);

/**
 * \brief Update NDEF Length TLV L field.
 *
 * \param[in] t2tOpComp         Pointer to component.
 * \param[in] msgLen            Length to be set.
 * \param[in] lengthSize        Number of Length Bytes to be set.
 * \param[in] ndefTLVIndex      Index of NDEF TLV which is to be updated.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TUpdateLength  (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t msgLen, uint8_t lengthSize, uint8_t ndefTLVIndex);

/**
 * \brief Process LockControl TLVs.
 *
 * \param[in] t2tOpComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TDLAProcessor  (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Process MemoryControl TLVs.
 *
 * \param[in] t2tOpComp             Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TRAProcessor   (ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Check if Area that is to be written to contains a DLA or RA.
 *
 * \param[in] t2tOpComp             Pointer to component.
 * \param[in]
 * \param[in,out] bytesToProcess    Number of Bytes to be written on input, Number of Bytes that can be written on output.
 * \param[out] preserveBytes        Byte Addresses which need to be preserved.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TAreaChecker   (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t *bytesToProcess, uint16_t *preserveBytes);

/**
 * \brief DLA and RA handling during Write.
 *
 * \param[in] t2tOpComp             Pointer to component.
 * \param[in,out] bytesToWrite      Number of Bytes to be written.
 * \param[in,out] bytesWritten      Number of Bytes written after preservation of DLA and RA.
 * \param[in] endOffset             Offset at the end of the last Block to be written.
 * \param[in] preserveAddresses     List of Byte Addresses to be preserved.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TDataPreservation(ptxNDEF_T2TOP_t *t2tOpComp, uint16_t bytesToWrite, uint16_t *bytesWritten, uint8_t endOffset, uint16_t preserveAddresses[]);

/**
 * \brief Function to check whether writing Data would overlap with other TLVs.
 *
 * \param[in] t2tOpComp             Pointer to component.
 * \param[in] overwriteByteAddress  Either start address of TLV to be overwritten, or start address of new memory space.
 * \param[in] overwriteLength       Length of new TLV oject.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TTLVProtection (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t overwriteByteAddress, uint16_t overwriteLength);

/**
 * \brief Generate a NDEF TLV.
 *
 * \param[in] t2tOpComp             Pointer to component.
 * \param[in] tlvLen                Length of the TLV. Dependent on this, either 1 or 3 L-bytes are set.
 * \param[in.out] buff              Buffer receiving the TLV, contains the V data on input.
 * \param[in,out] buffLen           Buffer length. Contains maximum length on input.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TTLVGenerator(ptxNDEF_T2TOP_t *t2tOpComp, size_t tlvLen, uint8_t *buff, uint16_t *buffLen);

/**
 * \brief Loop over a DLA to lock it.
 *
 * \param[in] t2tOpComp               Pointer to component.
 * \param[in] nbrLockBytes            Number of Bytes to be locked.
 * \param[in] nbrReservedBits         Number of reserved, non-lock bits.
 * \param[in] firstOffset             Offset of first Byte within the block.
 * \param[in] lastOffset              Offset of last Byte within the block.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TDLALoop (ptxNDEF_T2TOP_t *t2tOpComp,
                                       uint16_t nbrLockBytes,
                                       uint8_t nbrReservedBits,
                                       uint8_t firstOffset,
                                       uint8_t lastOffset);

/**
 * \brief Default DLA Lock procedure.
 *
 * \param[in] t2tOpComp               Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TLockDefault(ptxNDEF_T2TOP_t *t2tOpComp);

/**
 * \brief Lock using DLAs specified in LockControl TLVs.
 *
 * \param[in] t2tOpComp               Pointer to component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
static ptxStatus_t ptxNDEF_T2TLockDLA(ptxNDEF_T2TOP_t *t2tOpComp);
/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxNDEF_T2TOpOpen (ptxNDEF_T2TOP_t *t2tOpComp, ptxNDEF_T2TOP_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;
    ptxNativeTag_T2T_InitParams_t t2t_init_params;

    if ((NULL != t2tOpComp) && (NULL != initParams))
    {
        if ((NULL != initParams->RxBuffer) &&
                (0 != initParams->RxBufferSize) &&
                (NULL != initParams->WorkBuffer) &&
                (0 != initParams->WorkBufferSize))
        {
            /* clear component */
            (void)memset(t2tOpComp, 0, sizeof(ptxNDEF_T2TOP_t));

            t2tOpComp->WorkBuffer = initParams->WorkBuffer;
            t2tOpComp->WorkBufferSize = initParams->WorkBufferSize;
            t2tOpComp->RxBuffer = initParams->RxBuffer;
            t2tOpComp->RxBufferSize = initParams->RxBufferSize;
            t2tOpComp->LifeCycle = TagLC_NoNDEFTag;
            t2tOpComp->ReadCCVariant = initParams->ReadCCVariant;

            /* initialize lower layer component */
            (void)memset(&t2t_init_params, 0, sizeof(ptxNativeTag_T2T_InitParams_t));
            t2t_init_params = initParams->T2TInitParams;

            status = ptxNativeTag_T2TOpen(&t2tOpComp->NativeTagT2T, &t2t_init_params);

            /* set Component-ID at the end to prevent further calls in case of an error */
            if (ptxStatus_Success == status)
            {
                t2tOpComp->CompId = ptxStatus_Comp_T2TOP;
            }

        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpFormatTag (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_NotImplemented);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpCheckMessage (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    uint16_t non_ndef_bytes = 0;

    /* reset TLV data */
    (void)memset(&t2tOpComp->TLVs, 0, sizeof(ptxNDEF_T2TOP_TLV_t));

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        if (ptxStatus_Success == status)
        {
            status = ptxNDEF_T2TGetCCInfo(t2tOpComp);
        }
        if (ptxStatus_Success == status)
        {
            status = ptxNDEF_T2THandleCCInfo(t2tOpComp);

            if (ptxStatus_Success == status)
            {
                /* read the whole tag and look for TLVs */
                status = ptxNDEF_T2TTLVAreaCrawler(t2tOpComp);
            }

            /* find NDEF available space */
            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TDLAProcessor(t2tOpComp);
                if (ptxStatus_Success == status)
                {
                    non_ndef_bytes = (uint16_t)(non_ndef_bytes + (t2tOpComp->TLVs.NumberOfLockTLVs * (PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH + 2))); /* X (fixed) value bytes, 1 T and 1 L byte per TLV */
                    for (uint8_t i = 0; i < t2tOpComp->TLVs.NumberOfLockTLVs; i++)
                    {
                        non_ndef_bytes = (uint16_t)(non_ndef_bytes + (t2tOpComp->TLVs.LockControlTLVs[i].LockArea[1] - t2tOpComp->TLVs.LockControlTLVs[i].LockArea[0]));
                    }

                    status = ptxNDEF_T2TRAProcessor(t2tOpComp);
                    if (ptxStatus_Success == status)
                    {
                        non_ndef_bytes = (uint16_t)(non_ndef_bytes + (t2tOpComp->TLVs.NumberOfMemoryTLVs * (PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH + 2))); // X (fixed) value bytes, 1 T and 1 L byte per TLV
                        for (uint8_t i = 0; i < t2tOpComp->TLVs.NumberOfMemoryTLVs; i++)
                        {
                            non_ndef_bytes = (uint16_t)(non_ndef_bytes + (t2tOpComp->TLVs.MemoryControlTLVs[i].RsvdArea[1] - t2tOpComp->TLVs.MemoryControlTLVs[i].RsvdArea[0]));
                        }

                        if (PTX_T2TOP_NO_TERMINATOR_TLV != t2tOpComp->TLVs.TerminatorTLV.TerminatorTLVFound)
                        {
                            non_ndef_bytes++;
                        }

                        t2tOpComp->TLVs.AvailableNdefLength = (uint16_t)(t2tOpComp->CCParams.Size - non_ndef_bytes);
                    }
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            t2tOpComp->LastOperationCheck = 1u;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpReadMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *msgBuffer, uint32_t *msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t *msg_buffer;
    size_t msg_buffer_size;
    uint16_t msg_buffer_offset = 0;


    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != msgBuffer) && (NULL != msgLen))
    {
        msg_buffer_size = *msgLen;
        if (0 != msg_buffer_size)
        {
            msg_buffer = msgBuffer;

            switch (t2tOpComp->LifeCycle)
            {
                case TagLC_Initialized:
                /* FALLTHROUGH */
                case TagLC_ReadWrite:
                /* FALLTHROUGH */
                case TagLC_ReadOnly:
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidState);

            }
            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TReadNDEFTLVs(t2tOpComp, msg_buffer_size, msg_buffer, &msg_buffer_offset);

                if (ptxStatus_Success == status)
                {
                    *msgLen = msg_buffer_offset;
                }
                else
                {
                    *msgLen = 0;
                }

                t2tOpComp->LastOperationCheck = 0u;
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
        }

    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpWriteMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *msgBuffer, uint32_t msgLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != msgBuffer))
    {
        status = ptxNDEF_T2TOverwriteMessage(t2tOpComp, msgBuffer, msgLen, 0);
        t2tOpComp->LastOperationCheck = 0u;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpLockTag (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t rx_len;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        /* set static lock area lock bits to 1 */
        if (0 != t2tOpComp->SectorParams.CurrentSector)
        {
            status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, 0, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
        }

        /* set CC access condition to read-only since CC will be locked by static lock bytes */
        if (ptxStatus_Success == status)
        {
            t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_MAGIC_NUMBER] = t2tOpComp->CCParams.MagicNumber;
            t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_VERSION_INFO] = t2tOpComp->CCParams.Version;
            t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_TAG_SIZE]     = (uint8_t)t2tOpComp->CCParams.MLEN;

            /* set CC access condition to read only */
            t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_ACCESS_COND] = (uint8_t)(PTX_T2TOP_ACCESS_READONLY >> PTX_T2TOP_MASK_4);

            rx_len = t2tOpComp->RxBufferSize;
            status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, PTX_T2TOP_CC_BLOCK_NUMBER, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
            if (ptxStatus_Success == status)
            {
                t2tOpComp->LifeCycle = TagLC_ReadOnly;
            }
        }

        /* set static lock bytes */
        if (ptxStatus_Success == status)
        {
            rx_len = t2tOpComp->RxBufferSize;
            status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, PTX_T2TOP_STATIC_LOCK_BLOCK_NUMBER, &t2tOpComp->RxBuffer[0], &rx_len);

            if (ptxStatus_Success == status)
            {
                /* copy internal byte 8 and 9 to work buffer */
                (void)memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[0],(uint32_t)2);

                /* set buffer component for static lock bytes */
                t2tOpComp->WorkBuffer[2] = PTX_T2TOP_LOCKBITS;
                t2tOpComp->WorkBuffer[3] = PTX_T2TOP_LOCKBITS;

                status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, PTX_T2TOP_STATIC_LOCK_BLOCK_NUMBER, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
            }
        }

        if ((PTX_T2TOP_MIN_SIZE_FOR_DLA < t2tOpComp->CCParams.Size) && (ptxStatus_Success == status))
        {
            if (0 == t2tOpComp->TLVs.NumberOfLockTLVs)
            {
                status = ptxNDEF_T2TLockDefault(t2tOpComp);
            }
            else
            {
                status = ptxNDEF_T2TLockDLA(t2tOpComp);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxNDEF_T2TOpClose (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        status = ptxNativeTag_T2TClose(&t2tOpComp->NativeTagT2T);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */
static ptxStatus_t ptxNDEF_T2TGetCCInfo (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t rx_len;
    uint8_t block_nr;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        /* reset current CC-parameters */
        (void)memset(&t2tOpComp->CCParams, 0, sizeof(ptxNDEF_T2TOP_CC_t));
        (void)memset(&t2tOpComp->SectorParams, 0, sizeof(ptxNDEF_T2TOP_Sector_t));

        /* depending on Read-CC-variant, start reading at block 3 (default) or block 0 */
        block_nr = (t2tOpComp->ReadCCVariant == ReadCCVariant_Block_3_Default) ? PTX_T2TOP_CC_BLOCK_NUMBER : 0;

        /* read CC-block and 3 following */
        status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, block_nr, &t2tOpComp->RxBuffer[0], &rx_len);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    if (ptxStatus_Success == status)
    {
        /* depending on Read-CC-variant, fill work-buffer from received
         * Block 0 in case Block 3 was read first or
         * Block 3 in case Block 0 was read first.
         */
        block_nr = (t2tOpComp->ReadCCVariant == ReadCCVariant_Block_3_Default) ? 0: PTX_T2TOP_CC_BLOCK_NUMBER;

        (void)memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[block_nr],PTX_T2T_BLOCK_SIZE);

        t2tOpComp->CCParams.MagicNumber = t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_MAGIC_NUMBER];
        t2tOpComp->CCParams.Version = t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_VERSION_INFO];
        t2tOpComp->CCParams.MLEN = t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_TAG_SIZE];
        t2tOpComp->CCParams.Access = t2tOpComp->WorkBuffer[PTX_T2TOP_CC_OFFSET_ACCESS_COND];

        t2tOpComp->CCParams.ReadAccess = (t2tOpComp->CCParams.Access >> PTX_T2TOP_MASK_4);
        t2tOpComp->CCParams.WriteAccess = (uint8_t)((PTX_T2TOP_CC_ACCESS_BITS != (t2tOpComp->CCParams.Access & PTX_T2TOP_MASK_15)) ? (PTX_T2TOP_ACCESS_GRANTED) : (PTX_T2TOP_ACCESS_DENIED));

        t2tOpComp->CCParams.VersionMajor = (t2tOpComp->CCParams.Version >> PTX_T2TOP_MASK_4);
        t2tOpComp->CCParams.VersionMinor = (t2tOpComp->CCParams.Version & PTX_T2TOP_MASK_15);

        t2tOpComp->CCParams.Size = (uint16_t)((uint16_t)t2tOpComp->CCParams.MLEN * PTX_T2TOP_CC_MLEN_FACTOR); /* MLEN in Bytes */
        t2tOpComp->CCParams.NumberOfBlocks = (uint16_t)((uint16_t)t2tOpComp->CCParams.Size / (uint16_t)PTX_T2T_BLOCK_SIZE);

        t2tOpComp->SectorParams.NumberOfSectors = (uint8_t)((t2tOpComp->CCParams.NumberOfBlocks / PTX_T2TOP_SECTORSIZE) + 1);
    }
    else
    {
        /* nothing to do, return error status */
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2THandleCCInfo(ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        switch (t2tOpComp->CCParams.MagicNumber)
        {
            case PTX_T2TOP_CC_MAGIC_NUMBER:
                t2tOpComp->LifeCycle = TagLC_Initialized;
                break;
            default:
                t2tOpComp->LifeCycle = TagLC_NoNDEFTag;
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_ProtocolError);
                break;
        }

        /*
         * Minor version of device is 0, so check is unnecessary.
         * In case of change to device minor version, the following check should be added to if:
         * || PTX_T2TOP_VALID_MINOR_VERSION > t2tOpComp->CCParams.VersionMinor
         */
        if ((PTX_T2TOP_VALID_MAJOR_VERSION != t2tOpComp->CCParams.VersionMajor) && (ptxStatus_Success == status))
        {
            t2tOpComp->LifeCycle = TagLC_NoNDEFTag;
            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_ProtocolError);
        }

        if (ptxStatus_Success == status)
        {
            switch (t2tOpComp->CCParams.ReadAccess)
            {
                case PTX_T2TOP_ACCESS_GRANTED:
                    break;

                default:
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AccessDenied);
            }
        }
        if (ptxStatus_Success == status)
        {
            switch (t2tOpComp->CCParams.WriteAccess)
            {
                case PTX_T2TOP_ACCESS_GRANTED:
                    t2tOpComp->LifeCycle = TagLC_ReadWrite;
                    break;
                case PTX_T2TOP_ACCESS_DENIED:
                    t2tOpComp->LifeCycle = TagLC_ReadOnly;
                    break;

                default:
                    break;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TOpReadBlocks (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t blockNumber, uint8_t *rx, size_t *rxLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = (uint32_t)t2tOpComp->RxBufferSize;
        status = ptxNativeTag_T2TRead(&t2tOpComp->NativeTagT2T, blockNumber, rx, rxLen, PTX_T2T_DEFAULT_TIMEOUT_MS);

        if (ptxStatus_Success == status)
        {
            if ((0 != *rxLen) && ((PTX_T2TOP_BLOCKS_PER_READ * PTX_T2T_BLOCK_SIZE) != *rxLen))
            {
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_ProtocolError);
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_NscRfError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TOpWriteBlock(ptxNDEF_T2TOP_t *t2tOpComp,
        uint8_t blockNumber,
        uint8_t *blockData,
        uint8_t blockDataLen,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != blockData) && (NULL != rx) && (NULL != rxLen))
    {
        *rxLen = t2tOpComp->RxBufferSize;
        status = ptxNativeTag_T2TWrite(&t2tOpComp->NativeTagT2T, blockNumber, blockData, blockDataLen, rx, rxLen, msTimeout);

        if(ptxStatus_Success == status)
        {
            if (0 != *rxLen)
            {
                status = (PTX_T2TOP_ACK_CODE != rx[0]) ? (PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_ProtocolError)) : (status);
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_NscRfError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }
    return status;
}

static ptxStatus_t ptxNDEF_T2TOpSectorSelect(ptxNDEF_T2TOP_t *t2tOpComp,
        uint8_t secNr,
        uint8_t *rx,
        size_t *rxLen,
        uint32_t msTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != rx) && (NULL != rxLen) && (t2tOpComp->SectorParams.NumberOfSectors >= (secNr + 1)))
    {
        *rxLen = t2tOpComp->RxBufferSize;
        status = ptxNativeTag_T2TSectorSelect(&t2tOpComp->NativeTagT2T, secNr, rx, rxLen, msTimeout);

        if (ptxStatus_Success == status)
        {
            t2tOpComp->SectorParams.CurrentSector = secNr;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }
    return status;
}

static ptxStatus_t ptxNDEF_T2TReadNDEFTLVs (ptxNDEF_T2TOP_t *t2tOpComp, size_t msgBufferSize, uint8_t *msgBuffer, uint16_t *msgBufferOffset)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t current_sector = 0;
    uint16_t msg_length = 0;
    uint8_t length_field_len = 0;
    uint8_t length_field_offset = 0;
    uint8_t ndef_bytes_to_jump;
    uint16_t msg_buffer_offset = 0;
    size_t msg_buffer_size;
    uint16_t bytes_read = 0;
    uint8_t len_block_offset = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != msgBuffer) && (NULL != msgBufferOffset))
    {
        msg_buffer_offset = *msgBufferOffset;
        msg_buffer_size = msgBufferSize;

        for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfNdefTLVs; idx++)
        {
            t2tOpComp->CurrentByteAddress = t2tOpComp->TLVs.NDEFTLV[idx].ByteAddress;
            t2tOpComp->CurrentBlockAddress = (t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
            t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

            /* check sector */
            if (t2tOpComp->CurrentBlock / PTX_T2TOP_SECTORSIZE != t2tOpComp->SectorParams.CurrentSector)
            {
                current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
            }

            /* If the last thing we did was not a CheckMsg, we need to read the L field again. */
            if (!t2tOpComp->LastOperationCheck)
            {
                status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);

                if (ptxStatus_Success == status)
                {
                    length_field_offset = (uint8_t)((t2tOpComp->CurrentByteAddress + 1u) % PTX_T2T_BLOCK_SIZE);
                    length_field_len = (PTX_T2TOP_LENGTHTHRESHOLD != t2tOpComp->RxBuffer[length_field_offset]) ? (PTX_T2TOP_NDEF_L_FIELD_SHORT) : (PTX_T2TOP_NDEF_L_FIELD_LONG);
                }

                t2tOpComp->TLVs.NDEFTLV[idx].LengthByteOffset = length_field_offset;

                if (PTX_T2TOP_NDEF_L_FIELD_SHORT == length_field_len)
                {
                    /* copy length block and get new length */
                    t2tOpComp->TLVs.NDEFTLV[idx].Length = (uint16_t)t2tOpComp->RxBuffer[length_field_offset];
                    memcpy(&t2tOpComp->TLVs.NDEFTLV[idx].LengthBlock[0], &t2tOpComp->RxBuffer[0], PTX_T2T_BLOCK_SIZE);
                }
                else
                {
                    /* copy either one or two blocks of data and get new length */
                    t2tOpComp->TLVs.NDEFTLV[idx].Length = (uint16_t)((((uint16_t)t2tOpComp->RxBuffer[length_field_offset+1] << PTX_T2TOP_MASK_8) | t2tOpComp->RxBuffer[length_field_offset+2]));
                    len_block_offset = (1u <= length_field_offset) ? (0x00) : (0x01);
                    memcpy(&t2tOpComp->TLVs.NDEFTLV[idx].LengthBlock[0],
                           &t2tOpComp->RxBuffer[PTX_T2T_BLOCK_SIZE * len_block_offset],
                           (size_t)(PTX_T2T_BLOCK_SIZE + (PTX_T2T_BLOCK_SIZE * len_block_offset)));
                }
            }

            /* skip L part of TLV and go to first V-byte */
            if (ptxStatus_Success == status)
            {
                msg_length = t2tOpComp->TLVs.NDEFTLV[idx].Length;
                if (PTX_T2TOP_LENGTHTHRESHOLD <= msg_length)
                {
                    ndef_bytes_to_jump = 1 + PTX_T2TOP_NDEF_L_FIELD_LONG;
                }
                else
                {
                    ndef_bytes_to_jump = 1 + PTX_T2TOP_NDEF_L_FIELD_SHORT;
                }

                t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + ndef_bytes_to_jump); //position of first NDEF value byte
            }

            /* check if msgBuffer has enough space for the message */
            if (msg_buffer_size < msg_length)
            {
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TReadLoop(t2tOpComp, msg_length, msgBuffer, &bytes_read, &msg_buffer_offset);                
            }

            if (ptxStatus_Success == status)
            {
                if (msg_length == bytes_read)
                {
                    msg_buffer_offset = (uint16_t)(msg_buffer_offset + msg_length);
                    *msgBufferOffset = msg_buffer_offset;
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_ProtocolError);
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TReadLoop(ptxNDEF_T2TOP_t *t2tOpComp, uint32_t msgLen, uint8_t *msgBuffer, uint16_t *bytesRead, uint16_t *msgBufferOffset)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t end_offset = 0;
    uint8_t current_sector = t2tOpComp->SectorParams.CurrentSector;
    uint8_t previous_sector = current_sector;
    uint16_t bytes_read = 0;
    uint16_t bytes_to_read = 0;
    uint8_t block_offset = 0;
    uint16_t preserve_bytes[PTX_T2TOP_BLOCKS_PER_READ * PTX_T2T_BLOCK_SIZE]; /* reset every operation, read max of 16 */
    uint16_t msg_buffer_offset = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != msgBuffer) && (NULL != bytesRead) && (NULL != msgBufferOffset))
    {
        bytes_read = *bytesRead;
        msg_buffer_offset = *msgBufferOffset;

        while ((msgLen > bytes_read) && (ptxStatus_Success == status))
        {
            block_offset = t2tOpComp->CurrentByteAddress % PTX_T2T_BLOCK_SIZE;
            t2tOpComp->CurrentBlockAddress = (t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
            t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

            end_offset = 0;

            if (current_sector != (t2tOpComp->CurrentBlockAddress + PTX_T2TOP_BLOCKS_PER_READ) / PTX_T2TOP_SECTORSIZE)
            {
                bytes_to_read = (uint16_t)(((PTX_T2TOP_BLOCKS_PER_READ * PTX_T2T_BLOCK_SIZE) - (t2tOpComp->CurrentByteAddress % (PTX_T2TOP_BLOCKS_PER_READ * PTX_T2T_BLOCK_SIZE))) - block_offset);
            }
            else
            {
                bytes_to_read = (uint16_t)((PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ) - block_offset);
            }

            if (previous_sector != (t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE))
            {
                current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
                previous_sector = current_sector;
            }

            /* Next read results in more bytes than needed */
            if (msgLen < (bytes_read + bytes_to_read))
            {
                bytes_to_read = (uint16_t)(msgLen - bytes_read);
                end_offset = (uint8_t)((PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ) - block_offset - bytes_to_read);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TAreaChecker(t2tOpComp, &bytes_to_read, &preserve_bytes[0]);

                if (ptxStatus_Success == status)
                {
                    /* If there are areas to be ignored nearby, read blocks one at a time. */
                    if (0 != preserve_bytes[0])
                    {
                        bytes_to_read = PTX_T2T_BLOCK_SIZE;

                        /* Areas always start at block offset 0. */
                        if (t2tOpComp->CurrentByteAddress != preserve_bytes[0])
                        {
                            bytes_to_read = PTX_T2T_BLOCK_SIZE;
                        }
                        else
                        {
                            bytes_to_read--;
                            /* Look if the full block is reserved. */
                            for (uint8_t i = 1; i < PTX_T2T_BLOCK_SIZE; i++)
                            {
                                if (0 != preserve_bytes[i])
                                {
                                    bytes_to_read--;
                                }
                            }
                        }

                        if (0 != bytes_to_read)
                        {
                            /* Read a single block. */
                            status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
                        }

                        if (ptxStatus_Success == status)
                        {
                            /* Skip the full block. */
                            t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + PTX_T2T_BLOCK_SIZE);
                            memcpy(&msgBuffer[msg_buffer_offset + bytes_read], &t2tOpComp->RxBuffer[PTX_T2T_BLOCK_SIZE - bytes_to_read], (uint32_t)bytes_to_read);
                        }
                    }
                    else
                    {
                        /* No special areas, proceed with full read. */
                        if (ptxStatus_Success == status)
                        {
                            /* Read Next 4 Blocks */
                            status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
                        }

                        if (ptxStatus_Success == status)
                        {
                            t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + bytes_to_read - end_offset);
                            memcpy(&msgBuffer[msg_buffer_offset + bytes_read], &t2tOpComp->RxBuffer[block_offset], (uint32_t)bytes_to_read);
                        }
                    }

                    if ((ptxStatus_Success == status) && (0 != bytes_to_read))
                    {
                        bytes_read = (uint16_t)(bytes_read + bytes_to_read);

                        if (msgLen < bytes_read)
                        {
                            bytes_read = (uint16_t)(bytes_read-end_offset);
                        }
                        else
                        {
                            block_offset = 0;
                            end_offset = 0;
                        }
                    }
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            *bytesRead = bytes_read;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TOverwriteMessage (ptxNDEF_T2TOP_t *t2tOpComp, uint8_t *tx, size_t txLen, uint8_t ndefTLVIndex)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t length_size = 0;
    uint8_t ndef_tlv_header_size;
    uint8_t first_ndef_byte_offset = 0;

    uint16_t msg_len;
    uint8_t *msg_Buffer;
    size_t rx_len;

    uint8_t emptyMsg = 0;
    uint8_t EMPTY_NDEF_MSG_T2T[] = {0xD8, 0x00, 0x00, 0x00};

    uint8_t previous_sector = 0;
    uint8_t current_sector = 0;

    uint16_t bytes_written = 0;
    uint16_t bytes_to_write;
    uint16_t bytes_to_write_checker;

    uint16_t preserve_bytes[PTX_T2TOP_BLOCKS_PER_READ * PTX_T2T_BLOCK_SIZE]; // reset every operation, write max of 4 but uses same preserve buffer
    uint8_t next_preserve = 0;

    uint8_t end_offset = 0;
    uint8_t terminator_offset = 0;

    uint16_t new_begin_address = 0;
    uint8_t last_block[PTX_T2T_BLOCK_SIZE] = {0x00, 0x00, 0x00, 0x00};
    uint16_t last_block_len = PTX_T2T_BLOCK_SIZE;
    uint8_t last_write = 0;
    uint8_t last_block_write = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        switch (t2tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            /* FALLTHROUGH */
            case TagLC_ReadWrite:
                break;

            case TagLC_ReadOnly:
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AccessDenied);
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidState);
                break;
        }

        if (ptxStatus_Success == status)
        {
            if ((0 != txLen) && (NULL != tx))
            {
                msg_Buffer = tx;
                msg_len = (uint16_t)txLen;
            }
            else
            {
                msg_Buffer = &EMPTY_NDEF_MSG_T2T[0];
                msg_len = sizeof(EMPTY_NDEF_MSG_T2T);
                emptyMsg = 1;
            }

            length_size = (msg_len <= ((uint32_t)(PTX_T2TOP_LENGTHTHRESHOLD - 1))) ? (uint8_t)PTX_T2TOP_NDEF_L_FIELD_SHORT : (uint8_t)PTX_T2TOP_NDEF_L_FIELD_LONG;
            ndef_tlv_header_size = (uint8_t)(length_size + 1);

            /* Need to make new TLV? */
            if (t2tOpComp->TLVs.NumberOfNdefTLVs >= ndefTLVIndex + 1)
            {
                /* Will the data fit if we had ALL NDEF space for it? */
                if (txLen <= (uint32_t)t2tOpComp->TLVs.AvailableNdefLength)
                {
                    status = ptxNDEF_T2TTLVProtection(t2tOpComp, t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].ByteAddress, (uint16_t)txLen);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                }

                if (ptxStatus_Success == status)
                {
                    /* reset L-field */
                    status = ptxNDEF_T2TUpdateLength(t2tOpComp, 0, length_size, ndefTLVIndex);

                    if (ptxStatus_Success == status)
                    {
                        /* set internal memory position tracking */
                        t2tOpComp->CurrentByteAddress = t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].ByteAddress;
                        t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + ndef_tlv_header_size);
                        t2tOpComp->CurrentBlockAddress = (t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
                        t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

                        /* check sector */
                        if (t2tOpComp->CurrentBlock / PTX_T2TOP_SECTORSIZE != t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE)
                        {
                            current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                            status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
                        }

                        if (ptxStatus_Success == status)
                        {
                            previous_sector = current_sector;
                            first_ndef_byte_offset = t2tOpComp->CurrentByteAddress % PTX_T2T_BLOCK_SIZE;
                            bytes_to_write = (uint16_t)(PTX_T2T_BLOCK_SIZE - first_ndef_byte_offset);
                            status = ptxNDEF_T2TAreaChecker(t2tOpComp, &bytes_to_write, &preserve_bytes[0]);

                            /* preservation read needed? */
                            if ((ptxStatus_Success == status) &&
                                    ((bytes_to_write != PTX_T2T_BLOCK_SIZE - first_ndef_byte_offset) ||
                                     (0 != first_ndef_byte_offset)) &&
                                    (0 != bytes_to_write))
                            {
                                status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &rx_len);
                                (void)memcpy(&t2tOpComp->WorkBuffer[0],&t2tOpComp->RxBuffer[0],(uint32_t)PTX_T2T_BLOCK_SIZE);
                            }
                            (void)memcpy(&t2tOpComp->WorkBuffer[first_ndef_byte_offset],&msg_Buffer[0],(uint32_t)bytes_to_write);
                            if (ptxStatus_Success == status)
                            {
                                /* Special case, manually preserve Data. */
                                (void)memcpy(&t2tOpComp->WorkBuffer[0],&t2tOpComp->RxBuffer[0],(uint32_t)(first_ndef_byte_offset));

                                for (uint8_t i = 0; i < (PTX_T2T_BLOCK_SIZE-bytes_to_write-first_ndef_byte_offset); i++)
                                {
                                    next_preserve = (uint8_t)(preserve_bytes[i] - t2tOpComp->CurrentByteAddress);

                                    if ((PTX_T2TOP_MAX_NDEF_BYTE_OFFSET) >= next_preserve)
                                    {
                                        t2tOpComp->WorkBuffer[first_ndef_byte_offset+next_preserve] = t2tOpComp->RxBuffer[first_ndef_byte_offset+next_preserve];
                                    }
                                }
                            }
                        }
                    }

                    if ((ptxStatus_Success == status) && (0 != bytes_to_write))
                    {
                        rx_len = t2tOpComp->RxBufferSize;
                        status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                    }
                    if (ptxStatus_Success == status)
                    {
                        /* increment counters, reset preservation */
                        bytes_written = (uint16_t)(bytes_written + bytes_to_write);
                        (void)memset(&preserve_bytes[0],0,PTX_T2T_BLOCK_SIZE);
                        t2tOpComp->CurrentByteAddress = (uint16_t)((t2tOpComp->CurrentBlockAddress + 1) * 4);
                    }
                }
            }
            else
            {
                /* Try to make new TLV. */
                status = ptxNDEF_T2TTLVGenerator(t2tOpComp, txLen, &t2tOpComp->WorkBuffer[0], &msg_len);

                memcpy(&msg_Buffer[0], &t2tOpComp->WorkBuffer[0], msg_len);
                t2tOpComp->CurrentByteAddress = new_begin_address;

            }

            /* write loop until all data is written */
            while ((ptxStatus_Success == status) && (msg_len > bytes_written))
            {
                bytes_to_write = PTX_T2T_BLOCK_SIZE;
                bytes_to_write_checker = bytes_to_write;
                if (msg_len < (bytes_written + PTX_T2T_BLOCK_SIZE))
                {
                    bytes_to_write = (uint16_t)(msg_len - bytes_written);
                    bytes_to_write_checker = bytes_to_write;
                    end_offset = (uint8_t)(PTX_T2T_BLOCK_SIZE-bytes_to_write);
                }

                /* have we reached the last data block? Could we add the terminator TLV? */
                if ((msg_len == (bytes_written + bytes_to_write) &&
                    (t2tOpComp->TLVs.NumberOfNdefTLVs == ndefTLVIndex + 1) &&
                    (t2tOpComp->TLVs.NumberOfMemoryTLVs == 0) &&
                    (t2tOpComp->TLVs.NumberOfLockTLVs == 0) &&
                    (t2tOpComp->CCParams.Size >= msg_len + 1 + length_size + PTX_T2TOP_TERMINATOR_TLV_LENGTH)))
                {
                    last_write = 1;

                    /* last block, add terminator */
                    if (bytes_to_write != PTX_T2T_BLOCK_SIZE)
                    {
                        /* write with last data block */
                        terminator_offset = (uint8_t)(bytes_to_write % PTX_T2T_BLOCK_SIZE);
                        memcpy(&last_block[0], &msg_Buffer[bytes_written], bytes_to_write);
                        last_block[terminator_offset] = PTX_T2TOP_TERMINATOR_TLV_T;
                        bytes_to_write++;
                        bytes_to_write_checker++;
                    }
                    else
                    {
                        /* write on it's own */
                        last_block_write = 1;
                        last_block[0] = PTX_T2TOP_TERMINATOR_TLV_T;
                    }
                }

                t2tOpComp->CurrentBlockAddress = (uint16_t)(t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
                t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

                if (previous_sector != (t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE))
                {
                    current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                    status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
                    previous_sector = current_sector;
                }

                if (ptxStatus_Success == status)
                {
                    status = ptxNDEF_T2TAreaChecker(t2tOpComp, &bytes_to_write, &preserve_bytes[0]);
                }

                /* if we have to skip bytes in the middle, we need to read them first */
                if ((bytes_to_write_checker != bytes_to_write) && (ptxStatus_Success == status) && (0 != bytes_to_write))
                {
                    rx_len = t2tOpComp->RxBufferSize;
                    status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &rx_len);

                    if (ptxStatus_Success == status)
                    {
                        if (PTX_T2T_BLOCK_SIZE <= t2tOpComp->WorkBufferSize)
                        {
                            (void)memcpy(&t2tOpComp->WorkBuffer[0],&msg_Buffer[bytes_written],PTX_T2T_BLOCK_SIZE);
                            status = ptxNDEF_T2TDataPreservation(t2tOpComp, bytes_to_write, &bytes_written, end_offset, &preserve_bytes[0]);
                        }
                        else
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                        }
                    }
                }
                else
                {
                    /* uninterrupted write */
                    if ((ptxStatus_Success == status) && (0 != bytes_to_write))
                    {
                        if (bytes_to_write <= t2tOpComp->WorkBufferSize)
                        {
                            if ((!last_block_write) && (!last_write))
                            {
                                (void)memcpy(&t2tOpComp->WorkBuffer[0],&msg_Buffer[bytes_written],bytes_to_write);
                                rx_len = t2tOpComp->RxBufferSize;
                            }
                            else
                            {
                                (void)memcpy(&t2tOpComp->WorkBuffer[0],&last_block[0],bytes_to_write);
                                rx_len = t2tOpComp->RxBufferSize;
                            }
                        }
                        else
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                        }
                    }
                }

                if (0 != bytes_to_write)
                {
                    /* write data, with possible preservation from above */
                    status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                }

                if (ptxStatus_Success == status)
                {
                    if (0 != bytes_to_write)
                    {
                        bytes_written = (uint16_t)(bytes_written + bytes_to_write);
                        t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + (bytes_to_write - end_offset));
                    }
                    else
                    {
                        t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + PTX_T2T_BLOCK_SIZE);
                    }
                }

                /* do we need to write an extra block still, just for the terminator? */
                if ((ptxStatus_Success == status) && (0 != last_block_write))
                {
                    status = ptxNDEF_T2TAreaChecker(t2tOpComp, &last_block_len, &preserve_bytes[0]);

                    /* if we cannot write the last block fully without collision, we won't (terminator is optional) */
                    if ((PTX_T2T_BLOCK_SIZE == last_block_len) && (ptxStatus_Success == status))
                    {
                        last_block[terminator_offset] = PTX_T2TOP_TERMINATOR_TLV_T;
                        status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &last_block[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                    }
                }
            }

            if (ptxStatus_Success == status)
            {
                /*
                 * update internal length block with space allocated for length bytes
                 * and NDEF-message bytes already set
                 */
                if (PTX_T2TOP_NDEF_L_FIELD_SHORT == length_size)
                {
                    t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset] = 0x00;
                    memcpy(&t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset + 1u],
                            &msg_Buffer[0],
                           (size_t)(PTX_T2T_BLOCK_SIZE - (t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset + 1u)));
                }
                else
                {
                    memset(&t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset],
                            0x00,
                            PTX_T2TOP_NDEF_L_FIELD_LONG);

                    memcpy(&t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset + 1u],
                            &msg_Buffer[0],
                           (size_t)(PTX_T2T_BLOCK_SIZE - (t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthByteOffset + 1u)));
                }
                status = ptxNDEF_T2TUpdateLength(t2tOpComp, msg_len, length_size, ndefTLVIndex);
            }
        }

        if ((t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].Length == bytes_written) && (ptxStatus_Success == status))
        {
            txLen = (uint32_t)t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].Length;

            if (0 == emptyMsg)
            {
                t2tOpComp->LifeCycle = TagLC_ReadWrite;
            }
            else
            {
                t2tOpComp->LifeCycle = TagLC_Initialized;
                status = ptxNDEF_T2TUpdateLength(t2tOpComp, 0, length_size, ndefTLVIndex);
            }
        }
        else
        {
            txLen = 0;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}


/* look for TLVs after CheckMessage has read the memory */
static ptxStatus_t ptxNDEF_T2TTLVAreaCrawler (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        t2tOpComp->CurrentByteAddress = (uint16_t)(PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE);
        t2tOpComp->CurrentBlock = PTX_T2TOP_TLV_AREA_BEGIN_BLOCK;
        t2tOpComp->CurrentBlockAddress = PTX_T2TOP_TLV_AREA_BEGIN_BLOCK;
        uint8_t current_sector = t2tOpComp->SectorParams.CurrentSector;
        uint16_t nbr_mem_bytes = t2tOpComp->CCParams.Size;

        uint8_t nbr_dyn_lock_bit = 0;
        uint8_t nbr_dyn_lock_byte = 0;

        uint16_t bytes_to_skip = 0;

        /* Sweep the TLV Memory area. */
        while ((nbr_mem_bytes >= t2tOpComp->CurrentByteAddress) && (PTX_T2TOP_MAX_NBR_NDEF_TLV > t2tOpComp->TLVs.NumberOfNdefTLVs) && (PTX_T2TOP_NO_TERMINATOR_TLV == t2tOpComp->TLVs.TerminatorTLV.TerminatorTLVFound) && (ptxStatus_Success == status))
        {
            if (((PTX_T2TOP_SECTORTHRESHOLD * (t2tOpComp->SectorParams.CurrentSector + 1)) < t2tOpComp->CurrentBlockAddress) && (t2tOpComp->SectorParams.NumberOfSectors >= (current_sector + 1)))
            {
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, (uint8_t)(t2tOpComp->SectorParams.CurrentSector+1), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
            }
            else
            {
                /* No more sectors. */
                status = ptxStatus_Success;
            }

            if (ptxStatus_Success == status)
            {
                t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);
                status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, (uint8_t)t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TIdentifyTLV(t2tOpComp, &bytes_to_skip);
            }

            bytes_to_skip = (PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ > bytes_to_skip) ? (PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ) : (bytes_to_skip);

            t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + bytes_to_skip);
            t2tOpComp->CurrentBlockAddress = t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE;
        }

        if ((PTX_T2TOP_MIN_SIZE_FOR_DLA < t2tOpComp->CCParams.Size) && (0 == t2tOpComp->TLVs.NumberOfLockTLVs))
        {
            nbr_dyn_lock_bit = (uint8_t)((t2tOpComp->CCParams.Size - ((PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE) * 8u)) / 8u);
            nbr_dyn_lock_bit = (0 != ((t2tOpComp->CCParams.Size - ((PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE) * 8u)) % 8u)) ? ((uint8_t)(nbr_dyn_lock_bit + 1)) : (nbr_dyn_lock_bit);
            nbr_dyn_lock_byte = nbr_dyn_lock_bit / 8u;
            nbr_dyn_lock_byte = (0 != (nbr_dyn_lock_bit % 8u)) ? ((uint8_t)(nbr_dyn_lock_byte + 1)) : (nbr_dyn_lock_byte);

            if (t2tOpComp->SectorParams.NumberOfSectors < ((t2tOpComp->CCParams.Size + (PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE) + nbr_dyn_lock_byte) / PTX_T2TOP_SECTORSIZE) + 1)
            {
                t2tOpComp->SectorParams.NumberOfSectors++;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TIdentifyTLV(ptxNDEF_T2TOP_t *t2tOpComp, uint16_t *bytesToSkip)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t bytes_to_skip = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != bytesToSkip))
    {
        memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[0], t2tOpComp->RxLen);
        for (uint32_t idx = 0; idx < t2tOpComp->RxLen; idx++)
        {
            switch(t2tOpComp->WorkBuffer[idx])
            {
                /* Remember Lock- and Memory-TLV positions to handle them in the processor functions. */
                case PTX_T2TOP_LOCK_CONTROL_TLV_T:
                    /* if L != PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH skip, not a TLV. */
                    if (PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH == t2tOpComp->WorkBuffer[idx+1])
                    {
                        if (PTX_T2TOP_MAX_NUMBER_LOCK_CONTROL > t2tOpComp->TLVs.NumberOfLockTLVs)
                        {
                            t2tOpComp->TLVs.LockControlTLVs[t2tOpComp->TLVs.NumberOfLockTLVs].ByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + idx);
                            t2tOpComp->TLVs.NumberOfLockTLVs++;
                        }
                        else
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                        }
                        /* step over TLV, T+L+L*ValueBytes. */
                        idx = (uint32_t)(idx + (PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH + 1));
                        bytes_to_skip = (uint16_t)(bytes_to_skip + (PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH + 2));
                    }
                    break;

                case PTX_T2TOP_MEMORY_CONTROL_TLV_T:
                    /* if L != PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH skip, not a TLV. */
                    if (PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH == t2tOpComp->WorkBuffer[idx+1])
                    {
                        if (PTX_T2TOP_MAX_NUMBER_MEMORY_CONTROL > t2tOpComp->TLVs.NumberOfMemoryTLVs)
                        {
                            t2tOpComp->TLVs.MemoryControlTLVs[t2tOpComp->TLVs.NumberOfMemoryTLVs].ByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + idx);
                            t2tOpComp->TLVs.NumberOfMemoryTLVs++;
                        }
                        else
                        {
                            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                        }
                        /* step over TLV, T+L+L*ValueBytes. */
                        idx = (uint32_t)(idx + (PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH + 1));
                        bytes_to_skip = (uint16_t)(bytes_to_skip + (PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH + 2));
                    }
                    break;

                case PTX_T2TOP_NDEF_TLV_T:
                    status = ptxNDEF_T2TNDEFTLV_Handler(t2tOpComp, &idx, &bytes_to_skip);
                    break;

                case PTX_T2TOP_TERMINATOR_TLV_T:
                    t2tOpComp->TLVs.TerminatorTLV.ByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + idx);
                    t2tOpComp->TLVs.TerminatorTLV.TerminatorTLVFound = PTX_T2TOP_TERMINATOR_TLV_PRESENT;

                    /* exit loop */
                    idx = (uint32_t)t2tOpComp->RxLen;
                    break;

                case PTX_T2TOP_NULL_TLV_T:
                default:
                    break;
            }
        }

        if (ptxStatus_Success == status)
        {
            *bytesToSkip = bytes_to_skip;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TNDEFTLV_Handler(ptxNDEF_T2TOP_t *t2tOpComp, uint32_t *offset, uint16_t *bytesToSkip)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t length_byte_combined;
    uint16_t bytes_to_skip = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (offset != NULL) && (NULL != bytesToSkip))
    {
        uint32_t idx = *offset;
        uint32_t len_block_idx = 0;
        uint32_t tlv_idx = 0;

        if (PTX_T2TOP_MAX_NUMBER_NDEFTLVS > t2tOpComp->TLVs.NumberOfNdefTLVs)
        {
            t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].ByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + idx);
            tlv_idx = idx;

            /* cache offset within block containing length byte(s), which is directly after T-byte */
            t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].LengthByteOffset = (uint8_t)((t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].ByteAddress + 1u) % PTX_T2T_BLOCK_SIZE);

            /* check if the length byte/s is/are out of bounds of current work buffer content */
            if ((PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ - PTX_T2T_BLOCK_SIZE) < idx)
            {
                /* length byte(s) out of bounds, check sector and add another block */
                if (((t2tOpComp->CurrentBlock + PTX_T2TOP_BLOCKS_PER_READ)/PTX_T2TOP_SECTORSIZE) != (t2tOpComp->CurrentBlock/PTX_T2TOP_SECTORSIZE))
                {
                    status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, (uint8_t)((t2tOpComp->CurrentBlock + (PTX_T2TOP_BLOCKS_PER_READ -1))/PTX_T2TOP_SECTORSIZE), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
                }
                if (ptxStatus_Success == status)
                {
                    status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, (uint8_t)(t2tOpComp->CurrentBlock + PTX_T2TOP_BLOCKS_PER_READ), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
                    if (ptxStatus_Success == status)
                    {
                        /* add the fifth block to the work buffer */
                        (void)memcpy(&t2tOpComp->WorkBuffer[PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ], &t2tOpComp->RxBuffer[0], (uint32_t)(PTX_T2T_BLOCK_SIZE));
                    }
                }
            }

            if (ptxStatus_Success == status)
            {
                idx++;
                /* cache 2 blocks that could both contain parts of the TLV length */
                len_block_idx = tlv_idx - (tlv_idx % PTX_T2T_BLOCK_SIZE);
                memcpy(&t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].LengthBlock[0], &t2tOpComp->WorkBuffer[len_block_idx], PTX_T2T_BLOCK_SIZE*2u);

                /* proceed with length byte(s) */
                if (PTX_T2TOP_LENGTHTHRESHOLD == t2tOpComp->WorkBuffer[idx])
                {
                    /* 3 byte L */
                    length_byte_combined = (uint16_t)(((uint16_t)t2tOpComp->WorkBuffer[idx+1] << PTX_T2TOP_MASK_8) | t2tOpComp->WorkBuffer[idx+2]);
                    t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].Length = length_byte_combined;
                    idx = (uint32_t)(idx + 2); /* Skip leftover L bytes. */
                    bytes_to_skip = (uint16_t)(bytes_to_skip + 3); /* Skip all L bytes. */

                    /* are the length bytes in the first, second, or both cached blocks? */
                    if (1 < len_block_idx)
                    {
                        /* first or both */
                    }
                    else
                    {
                        /* second, push it into first */
                        memcpy(&t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].LengthBlock[0],&t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].LengthBlock[PTX_T2T_BLOCK_SIZE],PTX_T2T_BLOCK_SIZE);
                    }
                }
                else
                {
                    /* 1 byte L */
                    t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].Length = t2tOpComp->WorkBuffer[idx];
                    bytes_to_skip++; /* Skip L byte. */
                }

                /* Skip data bytes of this TLV. */
                bytes_to_skip = (uint16_t)(bytes_to_skip + t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].Length);
                idx = idx + t2tOpComp->TLVs.NDEFTLV[t2tOpComp->TLVs.NumberOfNdefTLVs].Length;

                t2tOpComp->TLVs.NumberOfNdefTLVs++;
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
        }

        *offset = idx;
        *bytesToSkip = bytes_to_skip;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/* update L of NDEF TLV */
static ptxStatus_t ptxNDEF_T2TUpdateLength(ptxNDEF_T2TOP_t *t2tOpComp, uint16_t msgLen, uint8_t lengthSize, uint8_t ndefTLVIndex)
{
    ptxStatus_t status = ptxStatus_Success;
    size_t rx_len;
    uint16_t length_byte_address;
    uint16_t block_address_ndef_len;
    uint8_t length_byte_sector = 0;
    uint8_t block_number_ndef_len;
    uint16_t new_len = msgLen;
    uint8_t lengthBytes[3];
    uint8_t three_len_prefix = 0x00;


    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        switch (t2tOpComp->LifeCycle)
        {
            case TagLC_Initialized:
            /* FALLTHROUGH */
            case TagLC_ReadWrite:
                break;

            case TagLC_ReadOnly:
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AccessDenied);
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidState);
                break;
        }


        /* L is 1 byte */
        if ((ptxStatus_Success == status) && (PTX_T2TOP_NDEF_L_FIELD_SHORT == lengthSize))
        {
            length_byte_address = (uint16_t)(t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].ByteAddress + PTX_T2TOP_NDEF_L_FIELD_SHORT);

            block_address_ndef_len = (length_byte_address / PTX_T2T_BLOCK_SIZE);
            block_number_ndef_len = (uint8_t)(block_address_ndef_len % PTX_T2TOP_SECTORSIZE);
            length_byte_sector = (uint8_t)(block_address_ndef_len / PTX_T2TOP_SECTORSIZE);
            if (t2tOpComp->SectorParams.CurrentSector != length_byte_sector)
            {
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, length_byte_sector, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
            }

            if (ptxStatus_Success == status)
            {
                t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[(length_byte_address % PTX_T2T_BLOCK_SIZE)] = (uint8_t)new_len;
                (void)memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[0], PTX_T2T_BLOCK_SIZE);

                rx_len = t2tOpComp->RxBufferSize;
                status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, block_number_ndef_len, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
            }
            if (ptxStatus_Success == status)
            {
                t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].Length = new_len;

                if (0 == new_len)
                {
                    t2tOpComp->LifeCycle = TagLC_Initialized;
                }
                else
                {
                    t2tOpComp->LifeCycle = TagLC_ReadWrite;
                }
            }
        }

        /* L is 3 bytes */
        if ((ptxStatus_Success == status) && (PTX_T2TOP_NDEF_L_FIELD_LONG == lengthSize))
        {
            length_byte_address = (uint16_t)(t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].ByteAddress + 1);
            block_address_ndef_len = (length_byte_address / PTX_T2T_BLOCK_SIZE);
            block_number_ndef_len = (uint8_t)(block_address_ndef_len % PTX_T2TOP_SECTORSIZE);
            length_byte_sector = (uint8_t)(block_address_ndef_len / PTX_T2TOP_SECTORSIZE);

            three_len_prefix = (new_len != 0) ? (PTX_T2TOP_LENGTHTHRESHOLD) : (0x00);
            lengthBytes[0] = three_len_prefix;
            lengthBytes[1] = (uint8_t)((new_len & 0xFF00) >> PTX_T2TOP_MASK_8);
            lengthBytes[2] = (uint8_t)(new_len & 0x00FF);


            if (t2tOpComp->SectorParams.CurrentSector != length_byte_sector)
            {
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, length_byte_sector, &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
            }

            if ((ptxStatus_Success == status) && (PTX_T2T_BLOCK_SIZE/2) > (length_byte_address % PTX_T2T_BLOCK_SIZE))
            {
                (void)memcpy(&t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[(length_byte_address % PTX_T2T_BLOCK_SIZE)], &lengthBytes[0], sizeof(lengthBytes));
                rx_len = t2tOpComp->RxBufferSize;
                status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, block_number_ndef_len, &t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
            }
            else
            {
                if (ptxStatus_Success == status)
                {
                    (void)memcpy(&t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[(length_byte_address % PTX_T2T_BLOCK_SIZE)], &lengthBytes[0], (uint32_t)((uint32_t)sizeof(lengthBytes) + 1 - (uint32_t)(PTX_T2T_BLOCK_SIZE - (length_byte_address % PTX_T2T_BLOCK_SIZE))));
                    rx_len = t2tOpComp->RxBufferSize;
                    status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, block_number_ndef_len, &t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);

                    /* check sector length */
                    if (ptxStatus_Success == status)
                    {
                        if (PTX_T2TOP_SECTORTHRESHOLD > block_number_ndef_len)
                        {
                            /* second write is in new sector */
                            status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, (uint8_t)(t2tOpComp->SectorParams.CurrentSector + 1), &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
                            if (ptxStatus_Success == status)
                            {
                                /* write to block 0 of next sector */
                                status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, 0, &t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[PTX_T2T_BLOCK_SIZE], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                            }
                        }
                        else
                        {
                            /* second write is in same sector */
                            status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, (uint8_t)(block_number_ndef_len + 1), &t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].LengthBlock[PTX_T2T_BLOCK_SIZE], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                        }
                    }
                }
            }

            if (ptxStatus_Success == status)
            {
                t2tOpComp->TLVs.NDEFTLV[ndefTLVIndex].Length = new_len;
                t2tOpComp->LifeCycle = TagLC_ReadWrite;
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }
    return status;
}

/* looks through LockControlTLVs and saves the DLAs */
static ptxStatus_t ptxNDEF_T2TDLAProcessor (ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t lock_byte_address;

    uint8_t DLAPosition;
    /* contains: */
    uint8_t NbrMajorOffsets;
    uint8_t NbrMinorOffsets;

    uint16_t DLA_NbrLockBits;
    uint16_t DLA_NbrBytes;

    uint8_t DLAControl;
    /* contains: */
    uint8_t BLPLB_Index;
    uint8_t MOS_DLA;

    uint16_t DLA_FirstByteAddress;
    uint16_t MajorOffset_Size_DLA;


    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        for (uint8_t LockTLV = 0; LockTLV < t2tOpComp->TLVs.NumberOfLockTLVs; LockTLV++)
        {
            lock_byte_address = t2tOpComp->TLVs.LockControlTLVs[LockTLV].ByteAddress;

            if (t2tOpComp->SectorParams.CurrentSector != lock_byte_address / PTX_T2TOP_SECTORSIZE)
            {
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, (uint8_t)(lock_byte_address / PTX_T2TOP_SECTORSIZE), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen, PTX_T2TOP_PAT_MS);
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, (uint8_t)(lock_byte_address / PTX_T2T_BLOCK_SIZE), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
            }

            if (ptxStatus_Success == status)
            {
                (void)memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[0], PTX_T2T_BLOCK_SIZE * 2);
                DLAPosition = t2tOpComp->WorkBuffer[(lock_byte_address % PTX_T2T_BLOCK_SIZE) + 2];
                DLA_NbrLockBits = (uint16_t)t2tOpComp->WorkBuffer[(lock_byte_address % PTX_T2T_BLOCK_SIZE) + 3];
                if (0 == DLA_NbrLockBits)
                {
                    DLA_NbrLockBits = PTX_T2TOP_SECTORSIZE;
                }
                DLAControl = t2tOpComp->WorkBuffer[(lock_byte_address % PTX_T2T_BLOCK_SIZE) + 4];

                NbrMajorOffsets = (DLAPosition >> PTX_T2TOP_MASK_4);
                NbrMinorOffsets = (DLAPosition & 0x0F);

                DLA_NbrBytes = (uint16_t)(DLA_NbrLockBits / 8);
                if (0 != (DLA_NbrLockBits % 8))
                {
                    DLA_NbrBytes++;
                }

                BLPLB_Index= (DLAControl >> PTX_T2TOP_MASK_4);
                MOS_DLA = (DLAControl & 0x0F);

                t2tOpComp->TLVs.LockControlTLVs[LockTLV].BytesLockedPerLockBit = (uint8_t)(1 << BLPLB_Index);

                MajorOffset_Size_DLA = (uint16_t)(1 << MOS_DLA);
                DLA_FirstByteAddress = (uint16_t)((NbrMajorOffsets * MajorOffset_Size_DLA) + NbrMinorOffsets);

                t2tOpComp->TLVs.LockControlTLVs[LockTLV].NbrReservedBits = (uint8_t)(t2tOpComp->TLVs.LockControlTLVs[LockTLV].BytesLockedPerLockBit -
                        (DLA_NbrLockBits % t2tOpComp->TLVs.LockControlTLVs[LockTLV].BytesLockedPerLockBit));
                t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[0] = (uint16_t)DLA_FirstByteAddress;
                t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[1] = (uint16_t)(DLA_FirstByteAddress + DLA_NbrBytes);
                t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[0] = ((uint16_t)(PTX_T2T_BLOCK_SIZE-1 != ((DLA_FirstByteAddress + DLA_NbrBytes)-1) % PTX_T2T_BLOCK_SIZE)) ?
                        ((uint16_t)(t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[1] + 1)) :
                        (0u);
                if (0 != t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[0])
                {
                    t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[1] = (uint16_t)(t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[0] +
                            (PTX_T2T_BLOCK_SIZE - (t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[0] % PTX_T2T_BLOCK_SIZE)));
                }
                else
                {
                    /* assign InternalArea End for easier checking. */
                    t2tOpComp->TLVs.LockControlTLVs[LockTLV].InternalArea[1] = t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[1];
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }
    return status;
}

/* looks through LockControlTLVs and saves the reserved areas */
static ptxStatus_t ptxNDEF_T2TRAProcessor(ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t mem_byte_address;

    uint8_t RAPosition;
    /* contains: */
    uint8_t NbrMajorOffsets;
    uint8_t NbrMinorOffsets;

    uint16_t Rsvd_Area_Size;

    uint8_t RAControl;
    /* contains: */
    uint8_t MOS_RA;

    uint16_t RA_FirstByteAddress;
    uint16_t MajorOffset_Size_RA;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        for (uint8_t MemoryTLV = 0; MemoryTLV < t2tOpComp->TLVs.NumberOfMemoryTLVs; MemoryTLV++)
        {
            mem_byte_address = t2tOpComp->TLVs.MemoryControlTLVs[MemoryTLV].ByteAddress;
            status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, (uint8_t)(mem_byte_address / PTX_T2T_BLOCK_SIZE), &t2tOpComp->RxBuffer[0], &t2tOpComp->RxLen);
            if (ptxStatus_Success == status)
            {
                (void)memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[0], PTX_T2T_BLOCK_SIZE * 2);
                RAPosition = t2tOpComp->WorkBuffer[(mem_byte_address % PTX_T2T_BLOCK_SIZE) + 2];
                Rsvd_Area_Size = (uint16_t)t2tOpComp->WorkBuffer[(mem_byte_address % PTX_T2T_BLOCK_SIZE) + 3];
                if (0 == Rsvd_Area_Size)
                {
                    Rsvd_Area_Size = PTX_T2TOP_SECTORSIZE;
                }
                RAControl = t2tOpComp->WorkBuffer[(mem_byte_address % PTX_T2T_BLOCK_SIZE) + PTX_T2T_BLOCK_SIZE];

                NbrMajorOffsets = (RAPosition >> PTX_T2TOP_MASK_4);
                NbrMinorOffsets = (RAPosition & 0x0F);

                MOS_RA = (RAControl & 0x0F);

                MajorOffset_Size_RA = (uint16_t)(1 << MOS_RA);
                RA_FirstByteAddress = (uint16_t)((NbrMajorOffsets * MajorOffset_Size_RA) + NbrMinorOffsets);

                t2tOpComp->TLVs.MemoryControlTLVs[MemoryTLV].RsvdArea[0] = (uint16_t)RA_FirstByteAddress;
                t2tOpComp->TLVs.MemoryControlTLVs[MemoryTLV].RsvdArea[1] = (uint16_t)(RA_FirstByteAddress + Rsvd_Area_Size);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

/* check RAs and DLAs against current address to ignore, bytes to process is to be set before the operation to either T2T block length or T2T block length * 4 depending on operation read or write  */
static ptxStatus_t ptxNDEF_T2TAreaChecker (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t *bytesToProcess, uint16_t *preserveBytes)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t bytes_to_process = *bytesToProcess;

    uint8_t preserve_counter = 0;

    uint16_t begin_byte = t2tOpComp->CurrentByteAddress;
    uint16_t end_byte = (uint16_t)(begin_byte + bytes_to_process);

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != bytesToProcess) && (NULL != preserveBytes))
    {
        /* reset preserve bytes container */
        (void)memset(&preserveBytes[0],0,(uint32_t)(sizeof(uint16_t)*(PTX_T2T_BLOCK_SIZE * PTX_T2TOP_BLOCKS_PER_READ)));

        for (uint16_t check_address = begin_byte; check_address < end_byte; check_address++)
        {
            /* check DLAs */
            for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfLockTLVs; idx++)
            {
                if ((t2tOpComp->TLVs.LockControlTLVs[idx].LockArea[0] <= check_address) && (t2tOpComp->TLVs.LockControlTLVs[idx].InternalArea[1] > check_address))
                {
                    preserveBytes[preserve_counter] = check_address;
                    preserve_counter++;
                    bytes_to_process--;
                }
            }

            /* check RAs */
            for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfMemoryTLVs; idx++)
            {
                if ((t2tOpComp->TLVs.MemoryControlTLVs[idx].RsvdArea[0] <= check_address) && (t2tOpComp->TLVs.MemoryControlTLVs[idx].RsvdArea[1] > check_address))
                {
                    preserveBytes[preserve_counter] = check_address;
                    preserve_counter++;
                    bytes_to_process--;
                }
            }
        }
        *bytesToProcess = bytes_to_process;
        status = ptxStatus_Success;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TDataPreservation(ptxNDEF_T2TOP_t *t2tOpComp, uint16_t bytesToWrite, uint16_t *bytesWritten, uint8_t endOffset, uint16_t preserveAddresses[])
{
    ptxStatus_t status = ptxStatus_Success;

    size_t rx_len = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != bytesWritten) && (NULL != preserveAddresses))
    {
        uint16_t bytes_written = *bytesWritten;
        uint8_t next_preserve = 0;

        memcpy(&t2tOpComp->WorkBuffer[0], &t2tOpComp->RxBuffer[0], (size_t)(PTX_T2T_BLOCK_SIZE - bytesToWrite - endOffset));

        for (uint8_t i = 0; i < (PTX_T2T_BLOCK_SIZE - bytesToWrite - endOffset); i++)
        {
            next_preserve = (uint8_t)(preserveAddresses[i] - t2tOpComp->CurrentByteAddress);

            if (PTX_T2TOP_MAX_NDEF_BYTE_OFFSET >= next_preserve)
            {
                t2tOpComp->WorkBuffer[next_preserve] = t2tOpComp->RxBuffer[next_preserve];
            }
        }
        rx_len = t2tOpComp->RxBufferSize;
        status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);

        (void)memset(&preserveAddresses[0],0,PTX_T2T_BLOCK_SIZE);
        bytes_written = (uint16_t)(bytes_written + bytesToWrite);
        *bytesWritten = bytes_written;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TTLVProtection (ptxNDEF_T2TOP_t *t2tOpComp, uint16_t overwriteByteAddress, uint16_t overwriteLength)
{
    ptxStatus_t status = ptxStatus_Success;
    uint16_t tlv_address = 0;
    uint16_t tlv_len = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfNdefTLVs; idx++)
        {
            tlv_address = t2tOpComp->TLVs.NDEFTLV[idx].ByteAddress;
            tlv_len = (uint16_t)(t2tOpComp->TLVs.NDEFTLV[idx].Length + 1); // Length of NDEF TLV + length information byte

            /* for NDEF TLVs, overwriting is an option */
            if (tlv_address != overwriteByteAddress)
            {
                /* if not the TLV to be overwritten, check if it would overlap with other NDEF TLVs. */
                if (((tlv_address <= overwriteByteAddress) && ((tlv_address + tlv_len) >= overwriteByteAddress))
                        || (tlv_address <= (overwriteByteAddress + overwriteLength)))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AssertionError);
                }
            }
        }

        if (ptxStatus_Success == status)
        {
            for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfLockTLVs; idx++)
            {
                tlv_address = t2tOpComp->TLVs.LockControlTLVs[idx].ByteAddress;
                tlv_len = PTX_T2TOP_LOCK_CONTROL_TLV_LENGTH + 1; // Length of Lock Control TLV + length information byte length

                if (((tlv_address <= overwriteByteAddress) && ((tlv_address + tlv_len) >= overwriteByteAddress))
                        || ((tlv_address <= (overwriteByteAddress + overwriteLength)) && (tlv_address >= overwriteByteAddress)))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AssertionError);
                }

            }
        }
        if (ptxStatus_Success == status)
        {
            for (uint8_t idx = 0; idx < t2tOpComp->TLVs.NumberOfMemoryTLVs; idx++)
            {
                tlv_address = t2tOpComp->TLVs.MemoryControlTLVs[idx].ByteAddress;
                tlv_len = PTX_T2TOP_MEMORY_CONTROL_TLV_LENGTH + 1; // Length of Memory Control TLV + length information byte length;

                if (((tlv_address <= overwriteByteAddress) && ((tlv_address + tlv_len) >= overwriteByteAddress))
                        || ((tlv_address <= (overwriteByteAddress + overwriteLength)) && (tlv_address >= overwriteByteAddress)))
                {
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AssertionError);
                }

            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}


static ptxStatus_t ptxNDEF_T2TTLVGenerator(ptxNDEF_T2TOP_t *t2tOpComp, size_t tlvLen, uint8_t *buff, uint16_t *buffLen)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t index = 0;
    uint16_t new_begin_address;
    uint8_t new_len_field;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP) && (NULL != buff) && (NULL != buffLen))
    {
        uint16_t msg_len = *buffLen;

        new_begin_address = (uint16_t)(PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE);

        new_len_field = (msg_len >= PTX_T2TOP_LENGTHTHRESHOLD) ? (PTX_T2TOP_NDEF_L_FIELD_LONG) : (PTX_T2TOP_NDEF_L_FIELD_SHORT);
        t2tOpComp->CurrentBlockAddress = (uint16_t)(t2tOpComp->CurrentByteAddress / 4);
        t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

        /* Search memory until we find a space the TLV will fit into, or until we run out of space. */
        do
        {
            status = ptxNDEF_T2TTLVProtection(t2tOpComp, new_begin_address, (uint16_t)(msg_len + new_len_field));
            new_begin_address = (uint16_t)(new_begin_address + PTX_T2T_BLOCK_SIZE);
        } while ((ptxStatus_Success != status) && (t2tOpComp->CCParams.Size > new_begin_address));

        if (ptxStatus_Success == status)
        {
            memcpy(&t2tOpComp->WorkBuffer[0], &buff[0], msg_len);

            if (PTX_T2TOP_LENGTHTHRESHOLD >= tlvLen)
            {
                /* 1 L-Byte */
                if (msg_len >= (tlvLen + (PTX_T2TOP_NDEF_L_FIELD_SHORT + 1)))
                {
                    index = (size_t)(tlvLen + (PTX_T2TOP_NDEF_L_FIELD_SHORT + 1));

                    /* Push data back to make space for T- and L-Bytes. */
                    while(index > 1)
                    {
                        buff[index] = buff[index - (PTX_T2TOP_NDEF_L_FIELD_SHORT + 1)];
                        index--;
                    }
                    *buffLen = (uint16_t)(tlvLen + (PTX_T2TOP_NDEF_L_FIELD_SHORT + 1));

                    status = ptxStatus_Success;
                }
                else
                {
                    *buffLen = 0;
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                }
            }
            else
            {
                /* 3 L-Byte */
                if (*buffLen >= (tlvLen + (PTX_T2TOP_NDEF_L_FIELD_LONG + 1)))
                {
                    index = (size_t)(tlvLen + (PTX_T2TOP_NDEF_L_FIELD_LONG + 1));

                    /* Push data back to make space for T- and L-Bytes. */
                    while(index > 1)
                    {
                        buff[index] = buff[index - (PTX_T2TOP_NDEF_L_FIELD_LONG + 1)];
                        index--;
                    }
                    *buffLen = (uint16_t)(tlvLen + (PTX_T2TOP_NDEF_L_FIELD_LONG + 1));

                    status = ptxStatus_Success;
                }
                else
                {
                    *buffLen = 0;
                    status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InsufficientResources);
                }
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TDLALoop (ptxNDEF_T2TOP_t *t2tOpComp,
                                       uint16_t nbrLockBytes,
                                       uint8_t nbrReservedBits,
                                       uint8_t firstOffset,
                                       uint8_t lastOffset)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t LockedBytes = 0;
    uint8_t bytes_to_write;
    uint8_t incomplete_lock_byte;
    uint8_t current_sector = 0;
    size_t rx_len;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {

        bytes_to_write = (uint8_t)nbrLockBytes;

        current_sector = t2tOpComp->SectorParams.CurrentSector;

        while ((nbrLockBytes > LockedBytes) && (ptxStatus_Success == status) && ((bytes_to_write - PTX_T2T_BLOCK_SIZE) > 0))
        {
            t2tOpComp->CurrentBlockAddress = (t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
            t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);
            /* check sector */
            if (current_sector !=  (t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE))
            {
                current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
            }

            if (ptxStatus_Success == status)
            {
                /* read first block of DLA (following 3 ignored) */
                status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &rx_len);

                if (ptxStatus_Success == status)
                {
                    /* check for leading non-DLA bytes and put them into the work buffer to be written, then fill remaining bytes with 0xFF */
                    if (0 != firstOffset)
                    {
                        (void)memcpy(&t2tOpComp->WorkBuffer[0],&t2tOpComp->RxBuffer[0],(uint32_t)firstOffset);
                    }
                    (void)memset(&t2tOpComp->WorkBuffer[firstOffset],(uint8_t)PTX_T2TOP_LOCKBITS,(uint32_t)(PTX_T2T_BLOCK_SIZE - firstOffset));

                    /* write the leading non-DLA blocks and the lock bits set to 1 */
                    rx_len = t2tOpComp->RxBufferSize;
                    status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
                }
            }
            bytes_to_write = (uint8_t)(bytes_to_write - (PTX_T2T_BLOCK_SIZE - firstOffset));
            LockedBytes = (uint16_t)(LockedBytes + (PTX_T2T_BLOCK_SIZE - firstOffset));
            t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CurrentByteAddress + (PTX_T2T_BLOCK_SIZE - firstOffset));
            firstOffset = 0;
        }

        /* last block handled separately due to possible offset and reserved bits */
        t2tOpComp->CurrentBlockAddress = (t2tOpComp->CurrentByteAddress / PTX_T2T_BLOCK_SIZE);
        t2tOpComp->CurrentBlock = (uint8_t)(t2tOpComp->CurrentBlockAddress % PTX_T2TOP_SECTORSIZE);

        if (ptxStatus_Success == status)
        {
            /* check sector */
            if ((t2tOpComp->CurrentBlock != (t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE)))
            {
                current_sector = (uint8_t)(t2tOpComp->CurrentBlockAddress / PTX_T2TOP_SECTORSIZE);
                status = ptxNDEF_T2TOpSectorSelect(t2tOpComp, current_sector, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2TOP_PAT_MS);
            }

            /* read last block (3 following blocks are ignored) */
            status = ptxNDEF_T2TOpReadBlocks(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->RxBuffer[0], &rx_len);
        }

        if (ptxStatus_Success == status)
        {
            /* fill the DLA bytes with 0xFF and put trailing non-DLA bytes into work buffer */
            (void)memcpy(&t2tOpComp->WorkBuffer[0],&t2tOpComp->RxBuffer[0],(uint32_t)PTX_T2T_BLOCK_SIZE);
            (void)memset(&t2tOpComp->WorkBuffer[firstOffset],PTX_T2TOP_ACCESS_READONLY,(uint32_t)((lastOffset + 1) - firstOffset));

            /* handle reserved bits if last DLA byte is not completely filled with lock bits */
            if (0 != nbrReservedBits)
            {
                /* DLA bytes are filled lsb to msb, push x 0s into the byte for reserved bits */
                incomplete_lock_byte = (uint8_t)((uint8_t)PTX_T2TOP_LOCKBITS >> nbrReservedBits);
                (void)memcpy(&t2tOpComp->WorkBuffer[lastOffset],&incomplete_lock_byte,(uint32_t)1);
            }
            rx_len = t2tOpComp->RxBufferSize;
            status = ptxNDEF_T2TOpWriteBlock(t2tOpComp, t2tOpComp->CurrentBlock, &t2tOpComp->WorkBuffer[0], PTX_T2T_BLOCK_SIZE, &t2tOpComp->RxBuffer[0], &rx_len, PTX_T2T_DEFAULT_TIMEOUT_MS);
            if (ptxStatus_Success == status)
            {
                LockedBytes = (uint16_t)(LockedBytes + ((lastOffset + 1) - firstOffset));
            }
            if ((LockedBytes == nbrLockBytes) && (ptxStatus_Success == status))
            {
                /* everything ok, all lock bits set */
            }
            else
            {
                status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_AssertionError);
            }
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TLockDefault(ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t NbrLockBits;
    uint16_t NbrLockBytes;
    uint8_t first_lock_byte_offset;
    uint8_t last_lock_byte_offset;
    uint16_t last_lock_byte_address;
    uint8_t nbr_reserved_bits = 0;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        /* use default setting for dynamic lock bits */
        NbrLockBits = (uint16_t)((t2tOpComp->CCParams.Size - PTX_T2TOP_MIN_SIZE_FOR_DLA) / PTX_T2TOP_BYTES_LOCKED_PER_BIT);
        if (0 != ((t2tOpComp->CCParams.Size - PTX_T2TOP_MIN_SIZE_FOR_DLA) % PTX_T2TOP_BYTES_LOCKED_PER_BIT))
        {
            NbrLockBits = (uint16_t)(NbrLockBits + ((t2tOpComp->CCParams.Size - PTX_T2TOP_MIN_SIZE_FOR_DLA) % PTX_T2TOP_BYTES_LOCKED_PER_BIT));
        }

        NbrLockBytes = (uint16_t)(NbrLockBits / PTX_T2TOP_BYTES_LOCKED_PER_BIT);
        if (0 != (NbrLockBits % PTX_T2TOP_BYTES_LOCKED_PER_BIT))
        {
            NbrLockBytes++;
            nbr_reserved_bits = (uint8_t)(PTX_T2TOP_BYTES_LOCKED_PER_BIT-(NbrLockBits % PTX_T2TOP_BYTES_LOCKED_PER_BIT));
        }

        /* go to first byte after TLV area */
        t2tOpComp->CurrentByteAddress = (uint16_t)(t2tOpComp->CCParams.Size + (PTX_T2TOP_TLV_AREA_BEGIN_BLOCK * PTX_T2T_BLOCK_SIZE));

        first_lock_byte_offset = (t2tOpComp->CurrentByteAddress % PTX_T2T_BLOCK_SIZE);
        last_lock_byte_address = (uint16_t)(t2tOpComp->CurrentByteAddress + NbrLockBytes - 1);
        last_lock_byte_offset  = (last_lock_byte_address % PTX_T2T_BLOCK_SIZE);

        status = ptxNDEF_T2TDLALoop(t2tOpComp, NbrLockBytes, nbr_reserved_bits, first_lock_byte_offset, last_lock_byte_offset);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxNDEF_T2TLockDLA(ptxNDEF_T2TOP_t *t2tOpComp)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t NbrLockBytes;
    uint8_t first_lock_byte_offset;
    uint8_t last_lock_byte_offset;
    uint8_t nbr_reserved_bits = 0;
    uint16_t first_dla_address;
    uint16_t last_dla_address;

    if (PTX_COMP_CHECK(t2tOpComp, ptxStatus_Comp_T2TOP))
    {
        /* use lock control TLVs */

        /* iterate over all DLAs */
        for (uint8_t LockTLV = 0; LockTLV < t2tOpComp->TLVs.NumberOfLockTLVs; LockTLV++)
        {
            first_dla_address = t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[0];
            last_dla_address  = t2tOpComp->TLVs.LockControlTLVs[LockTLV].LockArea[1];
            nbr_reserved_bits = t2tOpComp->TLVs.LockControlTLVs[LockTLV].NbrReservedBits;

            t2tOpComp->CurrentByteAddress = first_dla_address;
            first_lock_byte_offset = (t2tOpComp->CurrentByteAddress % PTX_T2T_BLOCK_SIZE);
            last_lock_byte_offset = (last_dla_address % PTX_T2T_BLOCK_SIZE);

            NbrLockBytes = (uint16_t)(last_dla_address - first_dla_address);

            status = ptxNDEF_T2TDLALoop(t2tOpComp, NbrLockBytes, nbr_reserved_bits, first_lock_byte_offset, last_lock_byte_offset);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_T2TOP, ptxStatus_InvalidParameter);
    }

    return status;
}
