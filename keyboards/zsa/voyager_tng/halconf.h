// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define HAL_USE_I2C TRUE
#define HAL_USE_SERIAL TRUE
#define SERIAL_DEFAULT_BITRATE 1000000
#define SERIAL_BUFFERS_SIZE 64

#include_next <halconf.h>
