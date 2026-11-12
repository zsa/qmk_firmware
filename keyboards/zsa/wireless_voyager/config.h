// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MATRIX_COLS 7
#define MATRIX_ROWS 12

#define MCP23018_TIMEOUT 10

#define IS31FL3731_I2C_ADDRESS_1 IS31FL3731_I2C_ADDRESS_GND
#define IS31FL3731_I2C_ADDRESS_2 IS31FL3731_I2C_ADDRESS_VCC

/* Left-nRF mirror of the right half's IS31, used in wireless mode so the STM32
 * stops addressing the real 0x77 across the unisolated TRRS bus. Bus contention
 * can only pull address bits low, so the mirror must not sit one clear-bit away
 * from 0x77 or from the left's own 0x74 - 0x73 reaches neither. */
#define ZSA_RIGHT_LED_MIRROR_ADDRESS 0x73

#define IS31FL3731_I2C_TIMEOUT 5

#define MOUSEKEY_WHEEL_INTERVAL MOUSEKEY_INTERVAL
#define MOUSEKEY_WHEEL_MAX_SPEED MOUSEKEY_MAX_SPEED
#define MOUSEKEY_WHEEL_TIME_TO_MAX MOUSEKEY_TIME_TO_MAX

// USART1 (PA9=TX, PA10=RX) — nRF54 communication, driven by the wireless
// module's zsa_uart.c. Baud must match the left-nRF's uart00 (current-speed in
// the overlay) and Ignition's UART_DFU_BAUD; mismatch silently breaks the
// wireless link.
#define UART_TX_PIN A9
#define UART_RX_PIN A10
#undef ZSA_WIRELESS_BAUD_RATE
#define ZSA_WIRELESS_BAUD_RATE 1000000

// I2C1 TIMINGR for a 48 MHz kernel clock (RM0316 TIMINGR formula, tPRESC
// 20.8 ns): tSCLL 1.8125 us, tSCLH 541.7 ns, tSCLDEL 125 ns, tSDADEL 0, the
// QMK 72 MHz default's 1.8056 us, 541.7 ns, 111.1 ns, 0 rounded up.
#define I2C1_TIMINGR_PRESC 0U
#define I2C1_TIMINGR_SCLDEL 5U
#define I2C1_TIMINGR_SDADEL 0U
#define I2C1_TIMINGR_SCLH 25U
#define I2C1_TIMINGR_SCLL 86U
