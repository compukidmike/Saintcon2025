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
    Module      : IOT_READER
    File        : ptx_IOT_READER.h

    Description : API for IOT READER
*/

/**
 * \addtogroup grp_ptx_api_iotrd PTX NSC IoT-Reader API
 *
 * @{
 */

#ifndef APIS_IOT_READER_PTX_IOT_READER_H_
#define APIS_IOT_READER_PTX_IOT_READER_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include "ptxHce.h"
#include "ptxNSC.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \brief Forward declarations.
 */
struct ptxPlat;
struct ptxNSC;

#define PTX_IOTRD_MAX_SUPPORTED_DEVICES                     (uint8_t)50                             /**< Max. supported Cards / RF-Devices */

#define PTX_IOTRD_TECH_A_SENSRES_MAX_SIZE                   (uint8_t)2                              /**< max. Length of Type-A SENS_RES parameter */
#define PTX_IOTRD_TECH_A_NFCID1_MAX_SIZE                    (uint8_t)10                             /**< max. Length of NFCID1 parameter */
#define PTX_IOTRD_TECH_B_SENSB_MAX_SIZE                     (uint8_t)13                             /**< max. Length of SENSB_RES parameter */
#define PTX_IOTRD_TECH_F_SENSF_MAX_SIZE                     (uint8_t)20                             /**< max. Length of SENSF_RES parameter */
#define PTX_IOTRD_TECH_V_UID_MAX_SIZE                       (uint8_t)8                              /**< max. Length of UID TYPE V */
#define PTX_IOTRD_TECH_EXT_PARAM_MAX_SIZE                   (uint8_t)32                             /**< max. Length of Extension Card Parameters */

#define PTX_IOTRD_PROT_ISO_DEP_ATS_MAX_SIZE                 (uint8_t)21                             /**< max. Length of Type-A ISO-DEP.ATS parameter */
#define PTX_IOTRD_PROT_ISO_DEP_ATTRIB_RES_MAX_SIZE          (uint8_t)16                             /**< max. Length of Type-B ISO-DEP.ATTRIB_RES parameter */
#define PTX_IOTRD_PROT_NFC_DEP_ATR_RES_MAX_SIZE             (uint8_t)65                             /**< max. Length of Type-A/F NFC-DEP.ATR_RES parameter */
#define PTX_IOTRD_HIGH_LEVEL_PROT_MAX_SIZE                  PTX_IOTRD_PROT_NFC_DEP_ATR_RES_MAX_SIZE /**< max. General Length of protocol parameter */

#define PTX_IOTRD_RF_BAILOUT_TECH_A                         (uint8_t)1                              /**< RF-Discover Parameter: Bail out after Technology A */
#define PTX_IOTRD_RF_BAILOUT_TECH_B                         (uint8_t)2                              /**< RF-Discover Parameter: Bail out after Technology B */
#define PTX_IOTRD_RF_BAILOUT_TECH_F                         (uint8_t)4                              /**< RF-Discover Parameter: Bail out after Technology F */

/**
 * \name RF-Discovery status values. Returned by ptxIoTRd_Get_Status_Info(Discover).
 * 
 * @{
 */
#define RF_DISCOVER_STATUS_NO_CARD                          (uint8_t)0                              /**< Rf-Discover State. No Card found in discovery loop */
#define RF_DISCOVER_STATUS_CARD_ACTIVE                      (uint8_t)1                              /**< Rf-Discover State. Card Activated */
#define RF_DISCOVER_STATUS_DISCOVER_RUNNING                 (uint8_t)2                              /**< Rf-Discover State. Cards being discovered */
#define RF_DISCOVER_STATUS_DISCOVER_DONE                    (uint8_t)3                              /**< Rf-Discover State. Cards discover process finished */
#define RF_DISCOVER_STATUS_LISTEN_A                         (uint8_t)4                              /**< Rf-Discover State. Card detected in HCE */
#define RF_DISCOVER_STATUS_DISCOVER_UNDEFINED               (uint8_t)0xFF                           /**< Rf-Discover State. Undefined */
/** @} */

/**
 * \name LPCD mechanism state (used for WLC-Stacks, alternatively available via \ref ptxIoTRd_Get_Status_Info).
 * 
 * @{
 */
#define RF_LPCD_STATUS_NO_DEVICE                            (uint8_t)0                              /**< LPCD State. No device found */
#define RF_LPCD_STATUS_DEVICE_FOUND                         (uint8_t)1                              /**< LPCD State. Device found */
/** @} */

/**
 * \name Length of the Rf Message Buffer.
 * 
 * @{
 */
#define PTX_IOTRD_RF_MSG_MAX_SIZE                           (256U)                                  /**< max. Length of Rf Message. */

#define PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE                 (uint8_t)0                              /**< Rf-Deactivation Type 0 - Turn off RF-field */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER             (uint8_t)1                              /**< Rf-Deactivation Type 1 - Restart RF-Discovery */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_SLEEP                (uint8_t)2                              /**< Rf-Deactivation Type 2 - Put remote device to sleep */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_SLEEP_NON_BLOCKING   (uint8_t)3                              /**< Rf-Deactivation Type 3 - Put remote device to sleep (non-blocking) */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_NO_RF_RESET          (uint8_t)4                              /**< Rf-Deactivation Type 4 - Common RF Deactivate but no Field Reset */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE_PROTOCOL        (uint8_t)5                              /**< Rf-Deactivation Type 5 - Use protocol-specific Deactivation, IDLE afterwards */
#define PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER_PROTOCOL    (uint8_t)6                              /**< Rf-Deactivation Type 6 - Use protocol-specific Deactivation, RF-Discovery started afterwards */


#define PTX_IOTRD_RF_DEACTIVATION_SLEEP_ONGOING             (uint8_t)0                              /**< RF-Deactivation sleep is ongoing */
#define PTX_IOTRD_RF_DEACTIVATION_SLEEP_DONE                (uint8_t)1                              /**< RF-Deactivation sleep is done */   
/** @} */

/**
 * \name Generic Defines (independent of Product Type)
 * 
 * @{
 */
#define PTX_SYSTEM_STATUS_OK                                (uint8_t)0                              /**< System Status OK. */
#define PTX_SYSTEM_STATUS_ERR_OVERCURRENT                   (uint8_t)1                              /**< System Status Overcurrent error. */
#define PTX_SYSTEM_STATUS_ERR_TEMPERATURE                   (uint8_t)2                              /**< System Status Temperature error. */
/** @} */

/** 
 * \name Defines of possible RF error codes. 
 * 
 * @{
 */
#define PTX_RF_ERROR_NTF_CODE_NO_ERROR                      (uint8_t)0x00                           /**< No error. */
#define PTX_RF_ERROR_NTF_CODE_UNKNOWN_ERROR                 (uint8_t)0xFF                           /**< RF error code. Unknown error */
#define PTX_RF_ERROR_NTF_CODE_WARNING_PA_OVERCURRENT_LIMIT  (uint8_t)0x09                           /**< RF error code. PA Warning. Overcurrent limit */
#define PTX_RF_ERROR_NTF_CODE_ERR_EMV_COLL                  (uint8_t)0x11                           /**< RF error code. EMV_Coll error */
#define PTX_RF_ERROR_NTF_CODE_ERR_TIMEOUT                   (uint8_t)0x12                           /**< RF error code. Timeout error */
#define PTX_RF_ERROR_NTF_CODE_ERR_TRANSMISSION              (uint8_t)0x13                           /**< RF error code. Transmission error. */
#define PTX_RF_ERROR_NTF_CODE_ERR_PROTOCOL                  (uint8_t)0x14                           /**< RF error code. Protocol error. */
/** @} */

/**
 * \name Hardware Product-IDs
 *
 * @{
 */
#define PTX_HW_PRODUCT_ID_PTX100X                           (uint8_t)0x00                           /**< Product ID for PTX100x series */
#define PTX_HW_PRODUCT_ID_PTX105X                           (uint8_t)0x01                           /**< Product ID for PTX105x series */
#define PTX_HW_PRODUCT_ID_PTX130X                           (uint8_t)0x02                           /**< Product ID for PTX130x series */
/** @} */

/**
 * \name Platform-dependent I2C speeds/bitrates (Attention: Bitrates highly dependent on the target system and might require adaption!)
 *
 * @{
 */
#define PTX_IOTRD_HOST_SPEED_I2C_100000                     (100000UL)                              /**< I2C Standard (100 kHz). */
#define PTX_IOTRD_HOST_SPEED_I2C_400000                     (400000UL)                              /**< I2C Fast-Mode (400 kHz). */
#define PTX_IOTRD_HOST_SPEED_I2C_1000000                    (1000000UL)                             /**< I2C Fast-Mode-Plus (up to 1 MHz). */
#define PTX_IOTRD_HOST_SPEED_I2C_3400000                    (3400000UL)                             /**< I2C High-Speed-Mode (up to 3.4 MHz). */
#define PTX_IOTRD_HOST_SPEED_I2C_MAX                        PTX_IOTRD_HOST_SPEED_I2C_3400000        /**< I2C Max. Speed/Bitrate */
/** @} */

/**
 * \name Platform-dependent UART speeds/bitrates (Attention: Bitrates highly dependent on the target system and might require adaption!)
 *
 * @{
 */
#define PTX_IOTRD_HOST_SPEED_UART_9600                      (9600UL)                                /**< UART 9600 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_14400                     (14400UL)                               /**< UART 14400 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_19200                     (19200UL)                               /**< UART 19200 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_28800                     (28800UL)                               /**< UART 28800 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_38400                     (38400UL)                               /**< UART 38400 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_57600                     (57600UL)                               /**< UART 57600 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_115200                    (115200UL)                              /**< UART 115200 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_230400                    (230400UL)                              /**< UART 230400 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_460800                    (460800UL)                              /**< UART 460800 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_921600                    (921600UL)                              /**< UART 921600 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_1843200                   (1843200UL)                             /**< UART 1843200 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_3000000                   (3000000UL)                             /**< UART 3000000 Baud. */
#define PTX_IOTRD_HOST_SPEED_UART_MAX                       PTX_IOTRD_HOST_SPEED_UART_3000000       /**< UART Max. Speed/Bitrate */
/** @} */

/**
 * \name Platform-dependent SPI speeds/bitrates (Attention: Bitrates highly dependent on the target system and might require adaption!)
 *
 * @{
 */
#define PTX_IOTRD_HOST_SPEED_SPI_1M                         (1000000UL)                             /**< SPI 1 MBit/s. */
#define PTX_IOTRD_HOST_SPEED_SPI_5M                         (5000000UL)                             /**< SPI 5 MBit/s. */
#define PTX_IOTRD_HOST_SPEED_SPI_10M                        (10000000UL)                            /**< SPI 10 MBit/s. */
#define PTX_IOTRD_HOST_SPEED_SPI_MAX                        PTX_IOTRD_HOST_SPEED_SPI_10M            /**< SPI Max. Speed/Bitrate */
/** @} */

/**
 * \name API Extension Definitions
 *
 * @{
 */
#define PTX_IOTRD_MAX_EXTENSIONS                            (1u)                                    /**< Max. amount of Extensions supported in parallel */
/** @} */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief NSC Revision-Info Types.
 */
typedef enum ptxIoTRd_RevisionType
{
    RevInfo_C_Stack,
    RevInfo_Local_Changes,
    RevInfo_DFY_Code,
    RevInfo_DFY_Toolchain,
    RevInfo_ChipID,
    RevInfo_ProductID,

} ptxIoTRd_RevisionType_t;

/**
 * \brief Chip RF- and System-Configuration Identifiers
 */
typedef enum ptxIoTRd_ChipConfigID
{
    /* RF-Configuration IDs */
    RF_Wavebank_0,
    RF_Wavebank_1,
    RF_Wavebank_2,
    RF_Wavebank_3,
    RF_Wavebank_4,
    RF_Wavebank_5,
    RF_Wavebank_6,
    RF_Wavebank_7,
    RF_Wavebank_8,
    RF_Wavebank_9,
    RF_Wavebank_10,
    RF_Wavebank_11,
    RF_Wavebank_12,
    RF_Wavebank_13,
    RF_Wavebank_14,
    RF_Wavebank_15,
    RF_Wavebank_16,
    RF_Wavebank_17,
    RF_Wavebank_18,
    RF_Wavebank_19,
    RF_Misc,
    RF_PollA106,
    RF_PollA212,
    RF_PollA424,
    RF_PollA848,
    RF_PollB106,
    RF_PollB212,
    RF_PollB424,
    RF_PollB848,
    RF_PollF212,
    RF_PollF424,
    RF_PollV,
    RF_Listen,
    RF_LAST_RF_CONFIG_ENTRY,

    /* System-Configuration IDs */
    SYS_ThermalThreshold = 100,
    SYS_OvercurrentThreshold,
    SYS_ConClockSrc,
    SYS_ConVarLBS,
    SYS_ConNHost,
    SYS_ConNHosCE,

}ptxIoTRd_ChipConfigID_t;

/**
 * \brief NSC System-Info Types.
 */
typedef enum ptxIoTRd_SysInfoType
{
    SysInfo_VDPA_Calibration_Result,

} ptxIoTRd_SysInfoType_t;

/**
 * \brief Chip RF- and System-Configuration Identifiers
 */
typedef struct ptxIoTRd_ChipConfig
{
    ptxIoTRd_ChipConfigID_t ID;             /**< Chip RF and System Config */
    uint8_t                 *Value;         /**< Value */
    uint8_t                 Len;            /**< Length */

}ptxIoTRd_ChipConfig_t;


/**
 * \brief Parameters for Communication/Host Interface Initialization.
 */
typedef struct ptxIoTRd_ComInterface_Params
{
    uint32_t    Speed;                      /**< Interface Speed (SPI, I2C or UART -> see PTX_IOTRD_HOST_SPEED* above). */
    uint8_t     DeviceAddress;              /**< Interface Device-Address (I2C only). */
}ptxIoTRd_ComInterface_Params_t;

/**
 * \brief Parameters for Temperature Sensor Initialization.
 */
typedef struct ptxIoTRd_TempSense_Params
{
    uint8_t     Calibrate;                  /**< Start temperature sensor calibration or not: 1 - start, 0 - don�t start. */
    uint8_t     Tshutdown;                  /**< Expected thermal shutdown treshold value. */
    uint8_t     Tambient;                   /**< Ambient temperature at which temp. sensor calibration takes place. */
}ptxIoTRd_TempSense_Params_t;

/**
 * \brief External Protection Circuitry Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef enum ptxIoTRd_ProtSupply
{
    SupplyPad_GPIO5     = 0x00, /**< Supply for external protection circuitry via GPIO5. */
    SupplyPad_ATEST3    = 0x40  /**< Supply for external protection circuitry via ATEST3. */
} ptxIoTRd_ProtSupply_t;

/**
 * \brief External Protection Attenuation Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef enum ptxIoTRd_ProtAttenuation
{
    Attenuation_Off = 0x00, /**< Attenuation for ISensor disabled. */
    Attenuation_03  = 0x10, /**< Attenuation for ISensor set to 0.3. */
    Attenuation_09  = 0x30  /**< Attenuation for ISensor set to 0.9. */
} ptxIoTRd_ProtAttenuation_t;

/**
 * \brief External Protection Type Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef enum ptxIoTRd_ProtType
{
    ProtType_None           = 0x00, /**< External protection circuitry disabled. */
    ProtType_CurrentSensor  = 0x01, /**< External protection implemented via current sensor. */
    ProtType_CurrentLimiter = 0x02  /**< External protection implemented via current limiter. */
} ptxIoTRd_ProtType_t;

/**
 * \brief External Protection Limiter Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef struct ptxIoTRd_ProtILimiterSettings
{
    uint8_t GuardTime;  /**< Delay time between turning on external limiter and the RF field. */
} ptxIoTRd_ProtILimiterSettings_t;

/**
 * \brief External Current Sensor Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef struct ptxIoTRd_ProtISensorSettings
{
    ptxIoTRd_ProtSupply_t       Supply;         /**< Supply pin for external current sensor/limiter. */
    ptxIoTRd_ProtAttenuation_t  Attenuation;    /**< Current sensor attenuation factor. */
    uint8_t                     Threshold;      /**< Attenuation factor for current sensor. */
} ptxIoTRd_ProtISensorSettings_t;

/**
 * \brief External Protection Settings (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef union ptxIoTRd_ProtSettings
{
    ptxIoTRd_ProtILimiterSettings_t Limiter;    /**< Settings for current limiter. */
    ptxIoTRd_ProtISensorSettings_t  Sensor;     /**< Settings for current sensor. */
} ptxIoTRd_ProtSettings_t;

/**
 * \brief External Protection Parameters (WLC only, to be ignored for stand-alone IoT-operation).
 */
typedef struct ptxIoTRd_Protection_Params
{
    ptxIoTRd_ProtType_t         Type;       /**< Protection type (None or external current sensor/limiter). */
    ptxIoTRd_ProtSettings_t     Settings;   /**< Additional settings (different current sensor/limiter). */
} ptxIoTRd_Protection_Params_t;

/**
 * \brief Parameters for IoTRd Initialization.
 */
typedef struct ptxIoTRd_InitPars
{
    ptxIoTRd_TempSense_Params_t         *TemperatureSensor;   /**< Initialization parameters for temperature sensor. */
    ptxIoTRd_ComInterface_Params_t      *ComInterface;        /**< Initialization parameters for communication interface: SPI, UART or I2C. */
    ptxIoTRd_Protection_Params_t        *ExtProtection;       /**< Initialization parameters for external protection circuitry (current sensor/limiter). */
}ptxIoTRd_InitPars_t;

/**
 * \brief Available higher bitrate identifiers.
 *
 */
typedef struct ptxIoTRd_BitRates
{
    uint8_t BitRate848;                                         /**< Higher bitrate: 848 kBit/s. */
    uint8_t BitRate424;                                         /**< Higher bitrate: 424 kBit/s. */
    uint8_t BitRate212;                                         /**< Higher bitrate: 212 kBit/s. */
} ptxIoTRd_BitRates_t ;

/**
 * \brief Available higher bitrates in Rx- and Tx-direction.
 *
 */
typedef struct ptxIoTRd_HBRConfig_Int
{
    ptxIoTRd_BitRates_t Rx;                                     /**< Higher bitrate settings in Rx-direction. */
    ptxIoTRd_BitRates_t Tx;                                     /**< Higher bitrate settings in Tx-direction. */
} ptxIoTRd_HBRConfig_Int_t;

/**
 * \brief Higher bitrate configuration.
 *
 */
typedef struct ptxIoTRd_HBRConfig
{
    ptxIoTRd_HBRConfig_Int_t  PollA;                            /**< Higher bitrate settings for Poll-Type A. */
    ptxIoTRd_HBRConfig_Int_t  PollB;                            /**< Higher bitrate settings for Poll-Type B. */
} ptxIoTRd_HBRConfig_t;

/**
 * \brief Card RF-Technology Type
 */
typedef enum ptxIoTRd_CardRFTechType
{
    Tech_TypeA   = 0,
    Tech_TypeB   = 1,
    Tech_TypeF   = 2,
    Tech_TypeV   = 6,
    Tech_TypeExtension = 10,
} ptxIoTRd_CardRFTechType_t;

/**
 * \brief Card RF-Protocol Type
 */
typedef enum ptxIoTRd_CardProtocol
{
    Prot_Undefined = 0,
    Prot_T2T       = 2,
    Prot_T3T       = 3,
    Prot_ISODEP    = 4,
    Prot_NFCDEP    = 5,
    Prot_T5T       = 6,
    Prot_Extension = 10,
} ptxIoTRd_CardProtocol_t;

/**
 * \brief Configuration for RF-Discovery Loop
 */
typedef struct ptxIoTRd_DiscConfig
{
    /** RF-Discovery Loop.
       * 0 =     Regular Polling
       * 1 =     Low-Power Card Detection (LPCD)
       * 2-255 = LPCD with every n-th Cycle Regular Polling
       */
    uint8_t     Discover_Mode;

    /** RF-Discovery Loop. Enable Type A technology. */
    uint8_t     PollTypeA;

    /** RF-Discovery Loop. Max. Type A devices to detect */
    uint8_t     PollTypeADeviceLimit;

    /** RF-Discovery Loop. Enable Type B technology. */
    uint8_t     PollTypeB;

    /** RF-Discovery Loop. Max. Type B devices to detect */
    uint8_t     PollTypeBDeviceLimit;

    /** RF-Discovery Loop. Enable Type F technology at 212 Kbps only. */
    uint8_t     PollTypeF212;

    /** RF-Discovery Loop. Enable Type F technology at 424 Kbps only. */
    uint8_t     PollTypeF424;

    /** RF-Discovery Loop. Max. Type F devices to detect */
    uint8_t     PollTypeFDeviceLimit;

    /** RF-Discovery Loop. Enable Type V technology. */
    uint8_t     PollTypeV;

    /** RF-Discovery Loop. Max. Type V devices to detect */
    uint8_t     PollTypeVDeviceLimit;

    /** RF-Discovery Loop. Bail out from Discovery after detecting device based on RF-technology A, B or F (see PTX_IOTRD_RF_BAILOUT_TECH_*). */
    uint8_t     PollBailOutFlags;

    /** RF-Discovery Loop. Allows to set the RF-technology to start with during the next discovery.
      * Note: If start RF-technology is not enabled, Discovery will procced with next enabled technology
      *       always in the order A, B, F and V.
      */
    ptxIoTRd_CardRFTechType_t   PollStartTechnology;

    /** RF-Discovery Loop. Disables auto usage of ISO-DEP Protocol. */
    uint8_t     DisableIsoDepProtocol;

    /** RF-Discovery Loop. Disables auto usage of NFC-DEP Protocol. */
    uint8_t     DisableNfcDepProtocol;

    /** General Bytes to enable LLCP Protocol on top of NFC-DEP Protocol. */
    uint8_t     *ConPollNfcDepAtrReqG;

    /** Length of the General Bytes. */
    uint8_t     ConPollNfcDepAtrReqGLen;

    /** RF-Discovery Loop. IDLE-Time between Polling cycles in [ms]. */
    uint32_t    IdleTime;

    /** RF-Discovery Loop. Activates Stand-by mode when LPCD is used. */
    uint8_t     EnableStandBy;

    /** RF-Discovery Loop. LPCD-Notification enabled (!= 0) or disabled (0). */
    uint8_t     EnableLPCDNotification;

    /** RF-Discovery Loop. Initial Guard Time given in [ms] before the very first Polling-command. */
    uint8_t     PollGuardTime;

    /** RF-Discovery Loop. If enabled, the RF-field / carrier is constantly switched on (no polling!). */
    uint8_t     ContinuousField;

    /** RF Discovery Loop. Listen Type A (HCE) */
    uint8_t     ListenTypeA;

    /** RF-Discovery Loop. Enable HBR */
    uint8_t     EnableHbr;

    /** RF-Discovery Loop. Poll Mode */
    uint8_t     EnableIsoPollMode;

    /** RF-Discovery Loop. Extended ATQB */
    uint8_t     EnableExtdAtqB;

    /** RF-Discovery Loop. AFI */
    uint8_t     AfiValue;

} ptxIoTRd_DiscConfig_t;


/**
 * \brief RF-Technology Type-A Card Parameters
 */
typedef struct ptxIoTRd_CardAParams
{
    uint8_t     SENS_RES[PTX_IOTRD_TECH_A_SENSRES_MAX_SIZE];    /**< RF-Technology Type-A. SENS_RES. */
    uint8_t     NFCID1_LEN;                                     /**< RF-Technology Type-A. NFCID1 Length. */
    uint8_t     NFCID1[PTX_IOTRD_TECH_A_NFCID1_MAX_SIZE];       /**< RF-Technology Type-A. NFCID1. */
    uint8_t     SEL_RES_LEN;                                    /**< RF-Technology Type-A. SEL_RES Length. If 1 SEL_RES present, if 0 not. */
    uint8_t     SEL_RES;                                        /**< RF-Technology Type-A. SEL_RES. */
} ptxIoTRd_CardAParams_t;

/**
 * \brief RF-Technolgy Type-B Card Parameters
 */
typedef struct ptxIoTRd_CardBParams
{
    uint8_t     SENSB_RES[PTX_IOTRD_TECH_B_SENSB_MAX_SIZE];     /**< RF-Technology Type-B. SENSB_RES. */
} ptxIoTRd_CardBParams_t;

/**
 * \brief RF-Technolgy Type-F Card Parameters
 */
typedef struct ptxIoTRd_CardFParams
{
    uint8_t     SENSF_RES_LEN;                                  /**< RF-Technology Type-F. SENSF_RES Length. */
    uint8_t     SENSF_RES[PTX_IOTRD_TECH_F_SENSF_MAX_SIZE];     /**< RF-Technology Type-F. SENSF_RES. */
} ptxIoTRd_CardFParams_t;

/**
 * \brief RF-Technolgy Type-V Card Parameters
 */
typedef struct ptxIoTRd_CardVParams
{
    uint8_t     RES_FLAG;                                       /**< RF-Technology Type-V. RES_FLAG. */
    uint8_t     DSFID;                                          /**< RF-Technology Type-V. DSFID. */
    uint8_t     UID[PTX_IOTRD_TECH_V_UID_MAX_SIZE];             /**< RF-Technology Type-V. UID. */
} ptxIoTRd_CardVParams_t;

/**
 * \brief RF-Technolgy Extension Card Parameters (Prototype/RFU)
 */
typedef struct ptxIoTRd_CardExtensionParams
{
    uint16_t Flags;                                         /**< Custom/Extension-specific Flags. */
    uint8_t  ParamLength;                                   /**< Extension Card Parameter Length. */
    uint8_t  Param[PTX_IOTRD_TECH_EXT_PARAM_MAX_SIZE];      /**< Extension Card Parameters. */
} ptxIoTRd_CardExtensionParams_t;

/**
 * \brief General Card RF-Technology specific Parameters
 */
typedef union ptxIoTRd_CardTechParams
{
    ptxIoTRd_CardAParams_t          CardAParams;            /**< RF-Technology specific parameters. Type A parameters. */
    ptxIoTRd_CardBParams_t          CardBParams;            /**< RF-Technology specific parameters. Type B parameters. */
    ptxIoTRd_CardFParams_t          CardFParams;            /**< RF-Technology specific parameters. Type F parameters. */
    ptxIoTRd_CardVParams_t          CardVParams;            /**< RF-Technology specific parameters. Type V parameters. */
    ptxIoTRd_CardExtensionParams_t  CardExtParams;          /**< RF-Technology specific parameters. Extension parameters (Prototype/RFU). */
} ptxIoTRd_CardTechParams_t;

/**
 * \brief Generic Status / State Identifier
 */
typedef enum ptxIoTRd_StatusType
{
    StatusType_System,
    StatusType_Discover,
    StatusType_DeactivateSleep,
    StatusType_LastRFError,
    StatusType_LPCDNtfCounter,
    StatusType_Lpcd,
} ptxIoTRd_StatusType_t;

/**
 * \brief General Card Parameters (=> Card Registry Entry)
 */
typedef struct ptxIoTRd_CardParams
{
    ptxIoTRd_CardRFTechType_t   TechType;                                            /**< General Card Parameters. Technology type. */
    ptxIoTRd_CardTechParams_t   TechParams;                                          /**< General Card Parameters. Technology parameters. */
    uint8_t                     DeviceState;                                         /**< General Card Parameters.Device State. */
} ptxIoTRd_CardParams_t;

/**
 * \brief Card Registry structure
 */
typedef struct ptxIoTRd_CardRegistry
{
    ptxIoTRd_CardParams_t       Cards[PTX_IOTRD_MAX_SUPPORTED_DEVICES];                 /**< Registry. Cards parameters. */
    uint8_t                     NrCards;                                                /**< Registry. Number of Cards. */
    ptxIoTRd_CardParams_t       *ActiveCard;                                            /**< Registry. Active Card. */
    ptxIoTRd_CardProtocol_t     ActiveCardProtType;                                     /**< Registry. Active Card Protocol.  Note: Availability of High-level protocol information is optional (e.g. not available for T2T, T5T) */
    uint8_t                     ActiveCardProtInfoLen;                                  /**< Registry. Active Card Length of protocol info. */
    uint8_t                     ActiveCardProtInfo[PTX_IOTRD_HIGH_LEVEL_PROT_MAX_SIZE]; /**< Registry. Active Card protocol info. */
    uint8_t                     ActiveCardProtSpeed;                                    /**< Registry. Active Card Speed. */
} ptxIoTRd_CardRegistry_t;

/**
 * \brief Rf Message State
 */
typedef enum ptxIoTRd_RfMsgState
{
    RfMsg_NotReceived,
    RfMsg_RfMsg_Rcv,
    RfMsg_RfMsg_Chained_Rcv,
    RfMsg_RfError,
    RfMsg_RfErrorTimeOut,
    RfMsg_CtrlAck,
    RfMsg_CtrlAttCmd,
    RfMsg_RfClt
}ptxIoTRd_RfMsgState_t;

/**
 * \brief Type of Check Presence Mechanism
 */
typedef enum ptxIoTRd_CheckPresType
{
    PresCheck_A,                                                            /**< Mechanism A. Presence Check. ISO-DEP NACK. */
    PresCheck_B                                                             /**< Mechanism B. Presence Check. ISO-DEP EMPTY FRAME. */
} ptxIoTRd_CheckPresType_t;

/**
 * \brief RSSI Mode
 */
typedef enum ptxIoTRd_RSSI_Mode
{
    RSSI_Mode_Enabled,                                                      /**< Enable RSSI-Mode */
    RSSI_Mode_Disabled,                                                     /**< Disable RSSI-Mode */
} ptxIoTRd_RSSI_Mode_t;

/**
 * \brief Rf Message Registry structure
 */
typedef struct ptxIoTRd_RfMsg
{
        ptxIoTRd_RfMsgState_t     State;                                    /**< RF Message state */
        uint8_t                   Buff[PTX_IOTRD_RF_MSG_MAX_SIZE];          /**< RF Message Buffer */
        size_t                    BuffLen;                                  /**< RF Message Buffer length */
        uint8_t                   BuffSecond[PTX_IOTRD_RF_MSG_MAX_SIZE];    /**< RF Message 2nd Buffer */
        size_t                    BuffSecondLen;                            /**< RF Message 2nd Buffe length */
        size_t                    NumTotalBits;                             /**< RF Message num of total bits */
} ptxIoTRd_RfMsg_t; 

/**
 * \brief Extension Prototype (RFU): Handler for NTF Processing at API-level.
 */
typedef ptxStatus_t (*pExtProcessNTF_t)(void *extCtx, void *iotRdComp, void *nscEvent);

/**
 * \brief Extension Prototype (RFU): Handler for Activate-/Select-Command Processing at API-level.
 */
typedef ptxStatus_t (*pExtActivateCMD_t)(void *extCtx, void *iotRdComp, ptxIoTRd_CardParams_t *cardParams, ptxIoTRd_CardProtocol_t protocol);

/**
 * \brief Extension Prototype (RFU): Extension Component Structure.
 */
typedef struct ptxIoTRd_Extension
{
    uint8_t             ExtensionID;        /**< Unique Register ID */
    pExtProcessNTF_t    CBFnExtProcessNtf;  /**< Callback Function to handle NTF-processing via Extension (RFU) */
    pExtActivateCMD_t   CBFnExtActivateCmd; /**< Callback Function to handle Activate-/Select-Command */
    void                *ExtensionCtx;      /**< Extension-specific context. */
} ptxIoTRd_Extension_t;

/**
 * \brief IOT Reader Main Structure (Context).
 */
typedef struct ptxIoTRd
{
        ptxStatus_Comps_t           CompId;                             /**< Component Id. */
        struct ptxPlat              *Plat;                              /**< Reference to Plat Component.*/
        struct ptxNSC               *Nsc;                               /**< Reference to Nsc Component.*/
        ptxIoTRd_CardRegistry_t     *CardRegistry;                      /**< Pointer to Card Registry. */
        uint8_t                     DiscoverState;                      /**< State of the discover process. */
        uint8_t                     LpcdState;                          /**< State of the LPCD mechanism process. */
        ptxIoTRd_RfMsg_t            RfMsg;                              /**< Rf Message received. */
        uint8_t                     LastRFError;                        /**< Last received RF-Error Status */
        uint8_t                     BuffNtf[PTX_IOTRD_RF_MSG_MAX_SIZE]; /**< POS buffer to keep NTF.*/
        size_t                      BuffNtfLen;                         /**< Bytes used of /ref BuffNtf.*/
        uint32_t                    BuffNtfIndex;                       /**< NTF Buffer index */
        ptxHce_t                    Hce;                                /**< Host Card Emulation Subcomponent */
        ptxNSC_PollType_t           PollMode;                           /**< Poll Mode */
        uint8_t                     rfResetFlag;                        /**< Flag to be set at RF resets, for use in T5T NativeTag etc. */
        uint8_t                     RSSIModeCfg[5];                     /**< Internal Config-Buffer for RSSI-Mode */
        uint8_t                     LPCDNtfCounter;                     /**< Counts every incoming LPCD-Notification (max. 255) */
        uint8_t                     StandbyActive;                      /**< Flag indicating the Stand-by mode is currently used (e.g LPCD during RF-Discovery/-Polling) */
        uint8_t                     NrExtensions;                           /**< Current number of registered Extensions (RFU). */
        ptxIoTRd_Extension_t        Extension[PTX_IOTRD_MAX_EXTENSIONS];    /**< Extensions Handlers (RFU). */

}ptxIoTRd_t;


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the NSC component.
 *
 * This initialization needs to be done early on, in order to be able to do a temperature sensor calibration
 * independently from \ref ptxIoTRd_Init. This function performs an initialization of the NSC interface.
 *
 * @param[in]  iotRd			Pointer to an allocated instance of the IoT-Reader component
 * @param[in]  initParams       Pointer to an allocated instance of \ref ptxIoTRd_ComInterface_Params_t
 * @return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_InitNSC(ptxIoTRd_t *iotRd, ptxIoTRd_ComInterface_Params_t *initParams);

/**
 * \brief Initialize the IoT Reader Component and the PTX1K.
 *
 * This function has to be called before any other API functions.
 * It performs software initialization and configuration for PTX1K.
 *
 * \param[in]   iotRd           Pointer to an allocated instance of the IoT-Reader component
*  \param[in]   initParams      Pointer to initialization parameters structure.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Init(ptxIoTRd_t *iotRd, ptxIoTRd_InitPars_t *initParams);


/**
 * \brief Initiate RF-Discovery according to NFC-Forum.
 *
 * This function starts the RF-Discovery procedure as defined in the NFC-Forum.
 *
 * \param[in]   iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   discConfig      Pointer to RF-Discovery structure (if set to NULL - default values will be used).
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Initiate_Discovery (ptxIoTRd_t *iotRd, ptxIoTRd_DiscConfig_t *discConfig);


/**
 * \brief Updates the RF- and System-Configuration parameters of the NFC hardware.
 *
 * This function allows to change RF- and System-Configuration parameters at runtime.
 *
 * \param[in]   iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   nrConfigs       Number of chip configurations.
 * \param[in]   configParams    Pointer to chip configuration structures.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Update_ChipConfig (ptxIoTRd_t *iotRd, uint8_t nrConfigs, ptxIoTRd_ChipConfig_t *configParams);

/**
 * \brief Configures HBR
 *
 * This function allows to configure HBR parameters
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   configParams        Pointer to configurations.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_ConfigHBR (ptxIoTRd_t *iotRd, ptxIoTRd_HBRConfig_t *configParams);

/**
 * \brief Get various revisions of system (C-Stack, DFY-Code/-Toolchain, Chip-ID, Local changes, Product-ID etc.).
 *
 * Note: Chip-ID can only be called after successful execution of "ptxIoTRd_Init", otherwise ID is set to 0.
 *
 * Note: Product-ID can only be called after successful execution of "ptxIoTRd_Init", otherwise ID is set to 0xFF.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   revisionType        Type of Revision.
 * \param[out]  revisionInfo        Revision information.
 *                                  Product-ID:
 *                                  0x00:   PTX100x
 *                                  0x01:   PTX105x
 *                                  0x02:   PTX130x
 *                                  0xFF:   Unknown/Invalid Product-ID
 *                                  Others: RFU
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Get_Revision_Info (ptxIoTRd_t *iotRd, ptxIoTRd_RevisionType_t revisionType, uint32_t * revisionInfo);


/**
 * \brief Access the internal card registry.
 *
 * This function can be used to access the internal card registry to retrieve a cards detailed information.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[out]  cardRegistry        Pointer to a pointer holding the internal card registry.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Get_Card_Registry (ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t **cardRegistry);


/**
 * \brief Activates a specific card.
 *
 * This function has to be used to activate / select a specific card in case multiple cards are available in the card registry.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   cardParams          Pointer to Card (-parameters) to activate.
 * \param[in]   protocol            (RF-)Protocol to use.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Activate_Card (ptxIoTRd_t *iotRd, ptxIoTRd_CardParams_t *cardParams, ptxIoTRd_CardProtocol_t protocol);


/**
 * \brief Exchange data with an already activated card.
 *
 * This function is blocking function, it means that it will return either when a response is received
 * or when the TimeOut provided elapses.
 *
 * \note In case the ISO- or NFC-DEP protocol is used, the system takes internally care of the timeout
 *       when the reader communicates with a card or remote device. In this case the caller has still to
 *       provide a meaningful value (e.g. multiple seconds). If a card reported a max. timeout of less than
 *       a second, the function will return after this timeout has expired even if the timeout parameter has been
 *       set to e.g. 5 seconds.
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       tx              Buffer containing the data to send.
 * \param[in]       txLength        Length of "tx".
 * \param[out]      rx              Pointer to buffer where the APDU will be received.
 * \param[in,out]   rxLength        As input, capacity of "rx". As output, actual number of bytes written on "rx".
 * \param[in]       msAppTimeout    Application-timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Data_Exchange (ptxIoTRd_t *iotRd, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout);


/**
 * \brief Enable or disable Bits Exchange Option.
 *
 * \note This function enables/disables the Type-A bit-exchange mode in state "DATA EXCHANGE" (sometimes also referred to as state POLL_ACTIVE).
 *       This mode can be used only for Cards / Tags which supports the RF-protocol "T2T".
 *       The bit-exchange mode works stand-alone i.e. it needs to be enabled before usage and disabled after usage - at least before the next call
 *       to /ref ptxIoTRd_Data_Exchange.
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       enable          If 1 = enable, 0 = disable. Other value error.
 *
 * \return Status, indicating whether the operation was successful.
 */

ptxStatus_t ptxIoTRd_Bits_Exchange_Mode (ptxIoTRd_t *iotRd, uint8_t enable);

/**
 * \brief Exchanges Type-A RF-technology aligned raw RF bit-streams in state "DATA EXCHANGE" based on Cards / Tags supporting the T2T protocol.
 *
 * This function is a blocking function; It means that I will return either when a raw bitstream is received
 * or when an Error occurs (e.g. TimeOut Error, RfError ).
 *
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       tx              Buffer containing payload bytes to be sent.
 * \param[in]       txPar           Buffer containing parity bits to be sent.
 * \param[in]       txLength        Length of /ref tx and /ref txPar. Length must be the same for /ref tx and /ref txPar.
 * \param[out]      rx              Buffer provided by caller to be filled in with payload bytes.
 * \param[out]      rxPar           Buffer provided by caller to be filled in with parity bits.
 * \param[in,out]   rxLength        As input parameter, capacity of /ref rx and /ref rxPar.
 *                                  As output parameter, bytes written on /ref rx and /ref rxPar.
 * \param[out]      numTotBits      Total number of bits processed.
 * \param[in]       msTimeout       TimeOut in ms that the function is going to wait for receiving bitstream from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Bits_Exchange (ptxIoTRd_t *iotRd, uint8_t *tx, uint8_t *txPar, size_t txLength,
                                                                    uint8_t *rx, uint8_t *rxPar, size_t *rxLength, size_t *numTotBits, uint32_t msTimeout);


/**
 * \brief Check the presence of an activated Card.
 *
 * \note This function shall be called when a Card has been activated
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       presCheckType   Type of presence check mechanism on ISO-DEP protocol.
 *                                  If higher protocol is not ISO-DEP this parameter is ignored.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_RF_PresenceCheck (ptxIoTRd_t *iotRd, ptxIoTRd_CheckPresType_t presCheckType);


/**
 * \brief This function sends EoF command to a T5T activated card and waits for an answer.
 *
 * This function is blocking function, it means that it will return either when a response is received
 * or when the TimeOut provided elapses.
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[out]      rx              Pointer to buffer where the APDU will be received.
 * \param[in,out]   rxLength        As input, capacity of "rx". As output, actual number of bytes written on "rx".
 * \param[in]       msAppTimeout    Application-timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_T5T_IsolatedEoF (ptxIoTRd_t *iotRd, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout);

/**
 * \brief This function sends EoF command to a T3T activated card and waits for an answer.
 *
 * This function is blocking function, it means that it will return either when a response is received
 * or when the TimeOut provided elapses.
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       systemCode      System-Code.
 * \param[in]       requestCode     Request-Code.
 * \param[in]       tsn             Timeslot-Number.
 * \param[out]      rx              Pointer to buffer where the APDU will be received.
 * \param[in,out]   rxLength        As input, capacity of "rx". As output, actual number of bytes written on "rx".
 * \param[in]       msAppTimeout    Application-timeout in ms that the function is going to wait for receiving data from the card.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_T3T_SENSFRequest (ptxIoTRd_t *iotRd, uint16_t systemCode, uint8_t requestCode, uint8_t tsn, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout);

/**
 * \brief Perform generic reader deactivation.
 *
 * This function performs three types of deactivation.
 * Value 1: The Reader will automatically restart the RF-Discover procedure.
 * Value 2: The Reader tries to put the remote Target into Sleep-state.
 * Others:  The Reader will turn off the RF-Field and move back to IDLE-state
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   deactivationType    1 = Restart RF-Discover, 2 = Sleep, Others = IDLE
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Reader_Deactivation (ptxIoTRd_t *iotRd, uint8_t deactivationType);

/**
 * \brief Set reader power mode.
 *
 * This function puts the reader into one of the predefined power modes, to cut down energy consumption.
 * Default mode is Active, which enables all the features and peripherals. In Active mode device exhibits full operability
 * with highest power consumption.
 * StandBy power mode is the mode with the lowest power consumption, with most of the features turned off. No RF communication possible.
 * WakeUp from StandBy mode is possible only via communication interface.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   newPowerMode        PowerMode to be set:
 *                                  0x00 = Active mode (WakeUp is automatically performed if device is in StandBy).
 *                                  0x01 = StandBy mode.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Set_Power_Mode (ptxIoTRd_t *iotRd, uint8_t newPowerMode);


/**
 * \brief Get System-internal Information.
 *
 * This function is used to retrieve system internal information (mostly related to HW-settings).
 * Currently Supported are:
 * a) VDPA-Calibration Result..: Number of desired calibration-cycles to be stored at infoBuffer[0] prior to call
 *
 * \param[in]     iotRd                     Pointer to an initialized instance of the IoT-Reader component
 * \param[in]     infoType                  Requested type of information.
 * \param[out]    infoBuffer                Pointer to buffer where information shall be stored.
 * \param[in,out] infoBufferLength          Size of buffer (in), Length of information (out)
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Get_System_Info (ptxIoTRd_t *iotRd, ptxIoTRd_SysInfoType_t infoType, uint8_t *infoBuffer, uint8_t *infoBufferLength);

/**
 * \brief SW Reset.
 *
 * This function performs a Soft Reset operation and waits for the timeout to elapse as there is no response from the chip.
 *
 * Note: The function ptxIoTRd_Init() needs to be called afterwards.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_SWReset (ptxIoTRd_t *iotRd);

/**
 * \brief Close the IoT Reader Component.
 *
 * This function closes the IOT and releases the resources used. It has to be called as the last function before  the stop
 * of the library usage.
 *
 * \param[in]   iotRd               Pointer to an initialized instance of the IoT-Reader component
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxIoTRd_Deinit(ptxIoTRd_t *iotRd);

/**
 * \brief Get various status- and state-information.
 *
 * This function represents a common getter-function to check the status of
 * - the overall System:
 *   This status indicates whether the PTX100x is operational or if a critical system error like
 *   overcurrent- or temperature-error occurred. In case of an error, the system must be restarted via a call to "ptxPOS_SWReset".
 * - the RF-Discovery:
 *   This status indicates whether the RF-Discovery procedure has discovered a card or if (for example) an EMV collision occurred.
 * - the status of DEACTIVATE(SLEEP_NON_BLOCKING):
 *   This status indicates whether the requested halt/sleep procedure (non-blocking) is finished.
 *
 * \param[in]   iotRd            Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   statusType       Defines type of status / state info to get.
 * \param[out]  statusInfo       Status / State info.<br>
 *                               StatusType_System (General PTX100x System-State):<br>
 *                                0x00 = PTX100x Operational / OK.<br>
 *                                0x01 = PTX100x Halted => Overcurrent Error.<br>
 *                                0x02 = PTX100x Halted => Temperature Error.<br>
 *                               StatusType_Discover (Status of RF-Discovery procedure):<br>
 *                                0x00 = No card discovered<br>
 *                                0x01 = Card found and activated<br>
 *                                0x02 = Multiple cards found, discovery ongoing<br>
 *                                0x03 = Multiple cards found, discovery finished - user needs to select which card to activate<br>
 *                               StatusType_DeactivateSleep (Status of DEACTIVATE(SLEEP_NON_BLOCKING)):<br>
 *                                0x00 = Deactivate (Sleep non-blocking) operation ongoing / not finished.<br>
 *                                0x01 = Deactivate (Sleep non-blocking) operation finished.<br>
 *                               StatusType_LastRFError (Last received RF-Error):<br>
 *                                0x00 = No Error.<br>
 *                                0xFF = Unknown Error.<br>
 *                                0x09 = PA Overcurrent limiter activated (warning).<br>
 *                                0x11 = RF Collision error (valid for EMVCo poll mode).<br>
 *                                0x12 = RF Timeout error.<br>
 *                                0x13 = RF Transmission error (CRC/Parity).<br>
 *                                0x14 = RF Protocol error.<br>
 *                                StatusType_LPCDNtfCounter (Number of received LPCD-Notifications):<br>
 *                                Others = RFU.<br>
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Get_Status_Info (ptxIoTRd_t *iotRd, ptxIoTRd_StatusType_t statusType, uint8_t *statusInfo);

/**
 * \brief Perform PTX1K integrated temperature sensor calibration and calculate calibrated temperature shutdown temperature.
 * The outcome of this process is the calibrated temperature shutdown temperature value, returned in Tshutdown parameter.
 * This value is used in every further call to ptxIoTRd_Init_NSC().
 *
 * The function ptxIoTRd_TempSensor_Calibration() starts Temperature offset calculation, which has to be performed
 * in controlled environment conditions - at a defined ambient temperature (typically in production at 25°C).
 * Offset calculation has to be done at least once for every PTX1K IC.
 * Expected over-temperature protection threshold has to be re-calculated with that offset. This is done automatically within this function call.
 * Temperature threshold is set in PTX1K with ptxIoTRd_Init_NSC().
 *
 * RECOMMENDATION: call ptxIoTRd_TempSensor_Calibration() and store the result (compensated Tsht) for future use.
 * Then, reuse the value in every further \ref ptxIoTRd_Init procedure. No need to restart the whole Temperature Calibration procedure all over again.
 *
 * \param[in]       iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]       Tambient        Ambient temperature to be used in calculations. This is expected environment temperature when calibration takes place.
 *                                  Typically it is 25°C, but it is left to the user to decide.
 * \param[in,out]   Tshutdown       Input: expected value of over-temperature threshold.
 *                                  Output: the result of sensor calibration - calibrated over-temperature threshold value.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_TempSensor_Calibration (ptxIoTRd_t *iotRd, uint8_t Tambient, uint8_t *Tshutdown);

/**
 * \brief This function puts the system / stack into RSSI-mode or deactivates the RSSI-mode.
 *
 * The function takes an optional parameter "rssiRefreshPeriod" which determines the internal refresh-rate for the RSSI-measurement
 * according to the equation "Refresh Rate in ms = 2 ^ (rssiRefreshPeriodInt - 1). If the parameter is not used (NULL), a default
 * value of 1 ms is used.
 *
 * \param[in]   iotRd                   Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   rssiMode                Defines type of status / state info to get.
 * \param[in]   rssiRefreshPeriodInt    Refresh Period Integer (Valid Range 1 - 16 or NULL (= default value of 1 ms).
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Set_RSSI_Mode (ptxIoTRd_t *iotRd, ptxIoTRd_RSSI_Mode_t rssiMode, uint8_t *rssiRefreshPeriodInt);

/**
 * \brief This function reads the current RSSI-value.
 *
 * To call this function properly, the RSSI-mode needs to be enabled upfront via \ref ptxIoTRd_Set_RSSI_Mode and disabled afterwards using
 * the same function.
 *
 * \param[in]   iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[out]  rssiValue       Measured RSSI-Value
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxIoTRd_Get_RSSI_Value (ptxIoTRd_t *iotRd, uint16_t *rssiValue);

/**
 * \brief Register an IoT-Extension (Prototype/RFU).
 *
 * \param[in]   iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   extensionID     ID of Extension to be registered within component.
 * \param[in]   extension       Pointer to Extension parameter structure.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxIoTRd_Register_Extension(ptxIoTRd_t *iotRd, uint8_t extensionID, ptxIoTRd_Extension_t *extension);

/**
 * \brief De-Register an IoT-Extension (Prototype/RFU).
 *
 * \param[in]   iotRd           Pointer to an initialized instance of the IoT-Reader component
 * \param[in]   extensionID     ID of Extension to be de-registered within component.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxIoTRd_DeRegister_Extension(ptxIoTRd_t *iotRd, uint8_t extensionID);

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */

