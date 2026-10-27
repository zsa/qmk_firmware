// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

// I2C for expander and ISSI
#undef STM32_I2C_USE_I2C1
#define STM32_I2C_USE_I2C1 TRUE

// 48 MHz SYSCLK: HSE 8 MHz x 6, USB 48 MHz from PLL/1, APB1/APB2 24 MHz
#undef STM32_PLLMUL_VALUE
#define STM32_PLLMUL_VALUE 6
#undef STM32_USBPRE
#define STM32_USBPRE STM32_USBPRE_DIV1
