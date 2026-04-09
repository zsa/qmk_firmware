// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

// I2C for expander and ISSI
#undef STM32_I2C_USE_I2C1
#define STM32_I2C_USE_I2C1 TRUE

// USART1 for nRF54 communication
#undef STM32_SERIAL_USE_USART1
#define STM32_SERIAL_USE_USART1 TRUE
