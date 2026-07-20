// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include "quantum.h"

extern bool mcp23018_leds[];

#define MCP23018_DEFAULT_ADDRESS 0b0100000

#define STATUS_LED_1(status) gpio_write_pin(B5, (bool)(status))
#define STATUS_LED_2(status) gpio_write_pin(B4, (bool)(status))
#define STATUS_LED_3(status) mcp23018_leds[0] = (bool)(status)
#define STATUS_LED_4(status) mcp23018_leds[1] = (bool)(status)

typedef union {
    uint32_t raw;
    struct {
        bool    disable_layer_led : 1;
        bool    led_level : 1;
        uint8_t led_level_res : 2; // DO NOT REMOVE
        uint8_t navigator_cpi : 3;
    };
} keyboard_config_t;

extern keyboard_config_t keyboard_config;

#ifndef VOYAGER_I2C_TIMEOUT
#    define VOYAGER_I2C_TIMEOUT 100
#endif

bool is_transport_connected(void);

/* Command the right-half nRF (TWIS slave, 7-bit 0x54) to raise its VBUS
 * input current limit to its devicetree value. Idempotent; the nPM1300
 * auto-reverts to the startup limit on VBUS removal. */
bool voyager_right_nrf_vbus_ilim_high(void);
