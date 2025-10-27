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
    Module      : PTX1K GPIO API
    File        : ptxGPIO.h

    Description : Writes / Reads GPIO pins 5-12 and provides access to 5-bit DAC of PTX100x
*/

/**
 * \addtogroup grp_ptx_api_gpio GPIO Operation API
 *
 * @{
 */

#ifndef APIS_PTX_GPIO_H_
#define APIS_PTX_GPIO_H_

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

/**
 * \brief GPIO Flags
 */
#define PTX_GPIO_FLAGS_ENABLE_INTERNAL_PULLUP           (uint8_t)0x01   /**< Enable internal pull-up resistor (input mode) */
#define PTX_GPIO_FLAGS_ENABLE_HIGH_DRIVER_STRENGTH      (uint8_t)0x02   /**< Enable high output driver strength (output mode) */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief GPIO Initialization Parameters
 */
typedef struct ptxGPIO_InitParams
{
        struct ptxNSC               *Nsc;               /**< Initialized instance of NSC component (member of IoT-/POS-component) */

} ptxGPIO_InitParams_t;

/**
 * \brief Supported GPIO-pins
 */
typedef enum ptxGPIO_Pin
{
    GPIO_Pin_5      = 5,
    GPIO_Pin_6      = 6,
    GPIO_Pin_7      = 7,
    GPIO_Pin_8      = 8,
    GPIO_Pin_9      = 9,
    GPIO_Pin_10     = 10,
    GPIO_Pin_11     = 11,
    GPIO_Pin_12     = 12,

} ptxGPIO_Pin_t;

/**
 * \brief Supported GPIO-Flags
 */
typedef enum ptxGPIO_Flags
{
    GPIO_Flags_None,
    GPIO_Flags_In_Enable_Internal_Pullup,
    GPIO_Flags_Out_Enable_High_Driver_Strength,

} ptxGPIO_Flags_t;

/**
 * \brief GPIO Config
 */
typedef enum ptxGPIO_Config
{
    GPIO_Config_Input,
    GPIO_Config_Output,

} ptxGPIO_Config_t;

/**
 * \brief GPIO Component
 */
typedef struct ptxGPIO
{
    /* Components */
    ptxStatus_Comps_t           CompId;                 /**< Component Id */

    struct ptxNSC               *Nsc;                   /**< Reference to NSC Component.*/
    uint8_t                     GPIO_Cfg[12];           /**< NSC Configuration value for each GPIO-Pin */

} ptxGPIO_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the GPIO Component.
 *
 * \param[in]   gpioComp            Pointer to an allocated instance of the GPIO-component.
 * \param[in]   initParams          Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful.
 */
ptxStatus_t ptxGPIO_Init (ptxGPIO_t *gpioComp, ptxGPIO_InitParams_t *initParams);

/**
 * \brief Deinitializes the GPIO Component.
 *
 * \param[in]   gpioComp            Pointer to an initialized instance of the GPIO component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxGPIO_Deinit (ptxGPIO_t *gpioComp);

/**
 * \brief Configures a selected GPIO-pin to work as input or output
 *
 * \param[in]   gpioComp            Pointer to an initialized instance of the GPIO component.
 * \param[in]   gpioNr              GPIO number.
 * \param[in]   gpioConfig          GPIO configuration (input, output, special function).
 * \param[in]   gpioFlags           GPIO flags (e.g. enable internal pull-up resistor, enable high driver strength).
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxGPIO_Config(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, ptxGPIO_Config_t gpioConfig, ptxGPIO_Flags_t gpioFlags);

/**
 * \brief Writes the selected GPIO-pin high or low.
 *
 * \param[in]   gpioComp            Pointer to an initialized instance of the GPIO component.
 * \param[in]   gpioNr              GPIO number.
 * \param[in]   gpioValue           Value to set (!= 0 = High, 0 = Low).
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxGPIO_Write(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, uint8_t gpioValue);

/**
 * \brief Reads the current value of a selected GPIO-pin
 *
 * \param[in]   gpioComp            Pointer to an initialized instance of the GPIO component.
 * \param[in]   gpioNr              GPIO number.
 * \param[out]  gpioValue           Pointer to variable storing GPIO-value.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxGPIO_Read(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, uint8_t *gpioValue);

/**
 * \brief Sets 5-bit value to DAC (pin DAC_0)
 *
 * \param[in]   gpioComp            Pointer to an initialized instance of the GPIO component.
 * \param[in]   dacValue            5-bit DAC-value.
 *
 * \return Status, indicating whether the operation was successful. See ptxStatus_t.
 *
 */
ptxStatus_t ptxGPIO_Write_DAC(ptxGPIO_t *gpioComp, uint8_t dacValue);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */

