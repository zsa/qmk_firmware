// Copyright 2023 ZSA Technology Labs, Inc <@zsa>
// Copyright 2023 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "voyager.h"
#include "mcp23018.h"
#include "i2c_master.h"
#include "busdev.h"

#pragma GCC push_options
#pragma GCC optimize("-O3")

extern matrix_row_t matrix[MATRIX_ROWS];     // debounced values
extern matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t raw_matrix_right[MATRIX_COLS];

#define MCP_ROWS_PER_HAND (MATRIX_ROWS / 2)
#ifndef VOYAGER_I2C_TIMEOUT
#    define VOYAGER_I2C_TIMEOUT 5
#endif

extern bool mcp23018_leds[2];
extern bool is_launching;

static busdev_id_t expander_dev = BUSDEV_NONE;

// Columns physically present on each right-hand matrix row (6..11). A bit set
// outside this mask cannot come from a key press.
static const matrix_row_t right_populated_mask[MCP_ROWS_PER_HAND] = {0x3F, 0x3F, 0x3F, 0x3E, 0x04, 0x60};

static busdev_step_t expander_init_step(uint8_t addr, uint8_t step, uint16_t *delay_ms) {
    if (!mcp23018_set_config(addr >> 1, mcp23018_PORTA, 0b00000000)) {
        return BUSDEV_STEP_FAIL;
    }
    if (!mcp23018_set_config(addr >> 1, mcp23018_PORTB, 0b00111111)) {
        return BUSDEV_STEP_FAIL;
    }
    return BUSDEV_STEP_DONE;
}

// Reads IODIRA/IODIRB and GPPUA/GPPUB back and compares them with what
// expander_init_step() wrote.
static bool expander_verify(uint8_t addr) {
    uint8_t buf[2];

    if (i2c_read_register(addr, 0x00, buf, sizeof(buf), VOYAGER_I2C_TIMEOUT) != I2C_STATUS_SUCCESS) {
        return false;
    }
    if (buf[0] != 0x00 || buf[1] != 0x3F) {
        return false;
    }
    if (i2c_read_register(addr, 0x0C, buf, sizeof(buf), VOYAGER_I2C_TIMEOUT) != I2C_STATUS_SUCCESS) {
        return false;
    }
    return buf[0] == 0x00 && buf[1] == 0x3F;
}

// Both right-half chips share one power rail, and only the expander has a
// trustworthy readback, so its state changes stand in for "the right half
// reset": SETTLING->CONFIGURING is the one transition that follows a probe ACK
// out of ABSENT (a hot-plug), and UP->CONFIGURING means it lost its
// configuration while still ACKing.
static void expander_on_state(busdev_id_t id, busdev_state_t old_state, busdev_state_t new_state, uint8_t reason) {
    if (new_state != BUSDEV_CONFIGURING) {
        return;
    }
    if (old_state == BUSDEV_SETTLING || (old_state == BUSDEV_UP && (reason == BUSDEV_REASON_VERIFY_MISMATCH || reason == BUSDEV_REASON_IMPOSSIBLE_KEY))) {
        voyager_right_led_resync();
    }
}

static const busdev_desc_t expander_desc = {
    .name               = "mcp23018",
    .addr               = MCP23018_DEFAULT_ADDRESS << 1,
    .settle_ms          = 20,
    .verify_interval_ms = 100,
    .probe_timeout_ms   = 5,
    .max_err_streak     = 1,
    .probe              = NULL,
    .init_step          = expander_init_step,
    .verify             = expander_verify,
    .on_state           = expander_on_state,
};

void matrix_init_custom(void) {
    // outputs
    gpio_set_pin_output(B10);
    gpio_set_pin_output(B11);
    gpio_set_pin_output(B12);
    gpio_set_pin_output(B13);
    gpio_set_pin_output(B14);
    gpio_set_pin_output(B15);

    // inputs
    gpio_set_pin_input_low(A0);
    gpio_set_pin_input_low(A1);
    gpio_set_pin_input_low(A2);
    gpio_set_pin_input_low(A3);
    gpio_set_pin_input_low(A6);
    gpio_set_pin_input_low(A7);
    gpio_set_pin_input_low(B0);

    i2c_init();

    if (expander_dev == BUSDEV_NONE) {
        expander_dev = busdev_register(&expander_desc);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool    changed  = false;
    bool    right_ok = busdev_is_up(expander_dev);
    uint8_t mcp_addr = busdev_addr(expander_dev) >> 1;

    // Scanning left and right side of the keyboard for key presses.
    // Left side is scanned by reading the gpio pins directly, right side is scanned by reading the mcp23018 registers.

    matrix_row_t data = 0;
    for (uint8_t row = 0; row <= MCP_ROWS_PER_HAND; row++) {
        // strobe row
        switch (row) {
            case 0:
                gpio_write_pin_high(B10);
                break;
            case 1:
                gpio_write_pin_high(B11);
                break;
            case 2:
                gpio_write_pin_high(B12);
                break;
            case 3:
                gpio_write_pin_high(B13);
                break;
            case 4:
                gpio_write_pin_high(B14);
                break;
            case 5:
                gpio_write_pin_high(B15);
                break;
            case 6:
                break; // Left hand has 6 rows
        }

        // Selecting the row on the right side of the keyboard.
        if (right_ok) {
            if (!mcp23018_set_output(mcp_addr, mcp23018_PORTA, 0b01111111 & ~(1 << (row))) || !mcp23018_set_output(mcp_addr, mcp23018_PORTB, ((uint8_t)!mcp23018_leds[1] << 6) | ((uint8_t)!mcp23018_leds[0] << 7))) {
                busdev_report_error(expander_dev, I2C_STATUS_ERROR);
                right_ok = false;
            }
        }
        // Reading the left side of the keyboard.
        if (row < MCP_ROWS_PER_HAND) {
            // i2c comm incur enough wait time
            if (!right_ok) {
                // need wait to settle pin state
                matrix_io_delay();
            }
            // read col data
            data = ((gpio_read_pin(A0) << 0) | (gpio_read_pin(A1) << 1) | (gpio_read_pin(A2) << 2) | (gpio_read_pin(A3) << 3) | (gpio_read_pin(A6) << 4) | (gpio_read_pin(A7) << 5) | (gpio_read_pin(B0) << 6));
            // unstrobe  row
            switch (row) {
                case 0:
                    gpio_write_pin_low(B10);
                    break;
                case 1:
                    gpio_write_pin_low(B11);
                    break;
                case 2:
                    gpio_write_pin_low(B12);
                    break;
                case 3:
                    gpio_write_pin_low(B13);
                    break;
                case 4:
                    gpio_write_pin_low(B14);
                    break;
                case 5:
                    gpio_write_pin_low(B15);
                    break;
                case 6:
                    break;
            }

            if (current_matrix[row] != data) {
                current_matrix[row] = data;
                changed             = true;
            }
        }

        // Reading the right side of the keyboard.
        if (right_ok) {
            uint8_t rx = 0xFF;
            if (!mcp23018_readPins(mcp_addr, mcp23018_PORTB, &rx)) {
                busdev_report_error(expander_dev, I2C_STATUS_ERROR);
                right_ok = false;
                data     = 0;
            } else {
                data = ~(rx & 0b00111111);
            }
        } else {
            data = 0;
        }

        if (raw_matrix_right[row] != data) {
            raw_matrix_right[row] = data;
            changed               = true;
        }
    }

    for (uint8_t row = 0; row < MCP_ROWS_PER_HAND; row++) {
        current_matrix[11 - row] = 0;
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            current_matrix[11 - row] |= ((raw_matrix_right[6 - col] & (1 << row) ? 1 : 0) << col);
        }
    }

    // An impossible column means the expander lost its direction/pull-up
    // configuration while still ACKing: drop the right hand for this scan and
    // ask the supervisor to rewrite the configuration.
    for (uint8_t i = 0; i < MCP_ROWS_PER_HAND; i++) {
        if (current_matrix[MCP_ROWS_PER_HAND + i] & (matrix_row_t)~right_populated_mask[i]) {
            for (uint8_t j = 0; j < MCP_ROWS_PER_HAND; j++) {
                current_matrix[MCP_ROWS_PER_HAND + j] = 0;
            }
            busdev_report_suspect(expander_dev, BUSDEV_REASON_IMPOSSIBLE_KEY);
            break;
        }
    }

    return changed;
}

// DO NOT REMOVE
// Needed for proper wake/sleep
void matrix_power_up(void) {
    bool temp_launching = is_launching;

    matrix_init_custom();

    is_launching = temp_launching;
    if (!temp_launching) {
        STATUS_LED_1(false);
        STATUS_LED_2(false);
        STATUS_LED_3(false);
        STATUS_LED_4(false);
    }

    // initialize matrix state: all keys off
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
    }
}

bool is_transport_connected(void) {
    return busdev_is_up(expander_dev);
}
#pragma GCC pop_options
