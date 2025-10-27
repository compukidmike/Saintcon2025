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
    File        : ptxGPIO.c

    Description : Writes / Reads GPIO pins 4-12 of PTX100x
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxGPIO.h"
#include "ptxNSC_Registers.h"
#include "ptxNSC.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define PTX_NSC_GPIO_NR_05                          (5u)        /**< NSC GPIO NR 05 */
#define PTX_NSC_GPIO_NR_06                          (6u)        /**< NSC GPIO NR 06 */
#define PTX_NSC_GPIO_NR_07                          (7u)        /**< NSC GPIO NR 07 */
#define PTX_NSC_GPIO_NR_08                          (8u)        /**< NSC GPIO NR 08 */
#define PTX_NSC_GPIO_NR_09                          (9u)        /**< NSC GPIO NR 09 */
#define PTX_NSC_GPIO_NR_10                          (10u)       /**< NSC GPIO NR 10 */
#define PTX_NSC_GPIO_NR_11                          (11u)       /**< NSC GPIO NR 11 */
#define PTX_NSC_GPIO_NR_12                          (12u)       /**< NSC GPIO NR 12 */

#define PTX_NSC_GPIO_NR_05_CFG_INDEX                (0x04u)     /**< NSC GPIO NR 05 Config Index */
#define PTX_NSC_GPIO_NR_06_CFG_INDEX                (0x05u)     /**< NSC GPIO NR 06 Config Index */
#define PTX_NSC_GPIO_NR_07_CFG_INDEX                (0x06u)     /**< NSC GPIO NR 07 Config Index */
#define PTX_NSC_GPIO_NR_08_CFG_INDEX                (0x07u)     /**< NSC GPIO NR 08 Config Index */
#define PTX_NSC_GPIO_NR_09_CFG_INDEX                (0x08u)     /**< NSC GPIO NR 09 Config Index */
#define PTX_NSC_GPIO_NR_10_CFG_INDEX                (0x09u)     /**< NSC GPIO NR 10 Config Index */
#define PTX_NSC_GPIO_NR_11_CFG_INDEX                (0x0Au)     /**< NSC GPIO NR 11 Config Index */
#define PTX_NSC_GPIO_NR_12_CFG_INDEX                (0x0Bu)     /**< NSC GPIO NR 12 Config Index */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
static ptxStatus_t ptxGPIO_CheckState (ptxGPIO_t *gpioComp);
static ptxStatus_t ptxGPIO_GetAddress(ptxGPIO_Pin_t gpioNr, uint16_t * gpioHWAddr, uint8_t * gpioCfgOffset);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxGPIO_Init (ptxGPIO_t *gpioComp, ptxGPIO_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != gpioComp) && (NULL != initParams))
    {
        if ((NULL != initParams->Nsc))
        {
            /* clear component */
            (void)memset(gpioComp, 0, sizeof(ptxGPIO_t));

            /* set members */
            gpioComp->Nsc = initParams->Nsc;

            /* assign Component ID */
            gpioComp->CompId = ptxStatus_Comp_GPIO;

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxGPIO_Deinit (ptxGPIO_t *gpioComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO))
    {
        /* nothing to do here */

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxGPIO_Config(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, ptxGPIO_Config_t gpioConfig, ptxGPIO_Flags_t gpioFlags)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO))
    {
        status = ptxGPIO_CheckState(gpioComp);

        if (ptxStatus_Success == status)
        {
            uint16_t gpio_address = 0;
            uint8_t gpio_cfg_index = 0;

            /* get register addresses and configuration positions */
            status = ptxGPIO_GetAddress(gpioNr, &gpio_address, &gpio_cfg_index);

            if (ptxStatus_Success == status)
            {
                switch (gpioConfig)
                {
                    case GPIO_Config_Input:
                        gpioComp->GPIO_Cfg[gpio_cfg_index] = PAD_GPIO5_REG_GPIO5_IE_MASK | PAD_GPIO5_REG_GPIO5_OEN_MASK;

                        if (GPIO_Flags_In_Enable_Internal_Pullup == gpioFlags)
                        {
                            gpioComp->GPIO_Cfg[gpio_cfg_index] |= PAD_GPIO5_REG_GPIO5_PE_MASK;
                        }
                        else
                        {
                            gpioComp->GPIO_Cfg[gpio_cfg_index] &= PAD_GPIO5_REG_GPIO5_PE_MASK_INV;
                        }
                        break;

                    case GPIO_Config_Output:
                        gpioComp->GPIO_Cfg[gpio_cfg_index] = (uint8_t)0x00;

                        if (GPIO_Flags_Out_Enable_High_Driver_Strength == gpioFlags)
                        {
                            gpioComp->GPIO_Cfg[gpio_cfg_index] |= PAD_GPIO5_REG_GPIO5_DS_MASK;
                        }
                        break;

                    default:
                        status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
                        break;
                }
            }

            if (ptxStatus_Success == status)
            {
                status = ptxNSC_Write(gpioComp->Nsc, gpio_address, gpioComp->GPIO_Cfg[gpio_cfg_index]);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxGPIO_Write(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, uint8_t gpioValue)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO))
    {
        status = ptxGPIO_CheckState(gpioComp);

        if (ptxStatus_Success == status)
        {
            uint16_t gpio_address = 0;
            uint8_t gpio_cfg_index = 0;

            /* get register addresses and configuration positions */
            status = ptxGPIO_GetAddress(gpioNr, &gpio_address, &gpio_cfg_index);

            if (ptxStatus_Success == status)
            {
                /* GPIO-x actually configured as output ? */
                if (0 == (gpioComp->GPIO_Cfg[gpio_cfg_index] & PAD_GPIO5_REG_GPIO5_OEN_MASK))
                {
                    /* Attention: The GPIO-outputs are low-active i.e. the logic needs to be inverted here */
                    if (0 != gpioValue)
                    {
                        gpioComp->GPIO_Cfg[gpio_cfg_index] |= PAD_GPIO5_REG_GPIO5_I_MASK;
                    } else
                    {
                        gpioComp->GPIO_Cfg[gpio_cfg_index] &= PAD_GPIO5_REG_GPIO5_I_MASK_INV;
                    }

                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NotPermitted);
                }


            }

            if (ptxStatus_Success == status)
            {
                status = ptxNSC_Write(gpioComp->Nsc, gpio_address, gpioComp->GPIO_Cfg[gpio_cfg_index]);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxGPIO_Read(ptxGPIO_t *gpioComp, ptxGPIO_Pin_t gpioNr, uint8_t *gpioValue)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO)) && (NULL != gpioValue))
    {
        status = ptxGPIO_CheckState(gpioComp);

        if (ptxStatus_Success == status)
        {
            uint16_t gpio_address = 0;
            uint8_t gpio_cfg_index = 0;

            /* get register addresses and configuration positions */
            status = ptxGPIO_GetAddress(gpioNr, &gpio_address, &gpio_cfg_index);

            if (ptxStatus_Success == status)
            {
                /* GPIO-x actually configured as input ? */
                if (PAD_GPIO5_REG_GPIO5_IE_MASK != (gpioComp->GPIO_Cfg[gpio_cfg_index] & PAD_GPIO5_REG_GPIO5_IE_MASK))
                {
                    status = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_NotPermitted);
                }
            }

            uint8_t gpio_value = 0;

            if (ptxStatus_Success == status)
            {
                status = ptxNSC_Read(gpioComp->Nsc, gpio_address, &gpio_value);

                if (ptxStatus_Success == status)
                {
                    *gpioValue = (uint8_t)((gpio_value & PAD_GPIO5_REG_GPIO5_C_MASK) >> PAD_GPIO5_REG_GPIO5_C_POS);
                }
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxGPIO_Write_DAC(ptxGPIO_t *gpioComp, uint8_t dacValue)
{
    const uint8_t DAC_MASK = 0x1FU;

    ptxStatus_t status = ptxStatus_Success;
    uint8_t dac_value = (uint8_t)(dacValue & DAC_MASK);
    uint16_t dac_reg_addr = ANA_RX_AUX_DAC_REG;

    if (PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO))
    {
        status = ptxNSC_Write(gpioComp->Nsc, dac_reg_addr, dac_value);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / CALLBACK(s)
 * ####################################################################################################################
 */

static ptxStatus_t ptxGPIO_CheckState (ptxGPIO_t *gpioComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(gpioComp, ptxStatus_Comp_GPIO))
    {
        ptxNSC_Mode_t current_nsc_state = NscMode_HW;

        status = ptxNSC_GetMode (gpioComp->Nsc, &current_nsc_state);

        if (ptxStatus_Success == status)
        {
            /* Stack and HW must operate in System-mode! */
            if (NscMode_SYS != current_nsc_state)
            {
                status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidState);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_GPIO, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxGPIO_GetAddress(ptxGPIO_Pin_t gpioNr, uint16_t * gpioHWAddr, uint8_t * gpioCfgOffset)
{
    ptxStatus_t ret = ptxStatus_Success;

    switch (gpioNr)
    {
        case GPIO_Pin_5:
            *gpioHWAddr = PAD_GPIO5_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_05_CFG_INDEX;
            break;

        case GPIO_Pin_6:
            *gpioHWAddr = PAD_GPIO6_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_06_CFG_INDEX;
            break;

        case GPIO_Pin_7:
            *gpioHWAddr = PAD_GPIO7_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_07_CFG_INDEX;
            break;

        case GPIO_Pin_8:
            *gpioHWAddr = PAD_GPIO8_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_08_CFG_INDEX;
            break;

        case GPIO_Pin_9:
            *gpioHWAddr = PAD_GPIO9_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_09_CFG_INDEX;
            break;

        case GPIO_Pin_10:
            *gpioHWAddr = PAD_GPIO10_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_10_CFG_INDEX;
            break;

        case GPIO_Pin_11:
            *gpioHWAddr = PAD_GPIO11_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_11_CFG_INDEX;
            break;

        case GPIO_Pin_12:
            *gpioHWAddr = PAD_GPIO12_REG;
            *gpioCfgOffset = PTX_NSC_GPIO_NR_12_CFG_INDEX;
            break;

        default:
            ret = PTX_STATUS(ptxStatus_Comp_NSC, ptxStatus_InvalidParameter);
            break;
    }

    return ret;
}

