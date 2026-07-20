// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "voyager_tng.h"
#include "mcp23018.h"
#include "i2c_master.h"
#include "usb_util.h"
#include <hal.h>

#pragma GCC push_options
#pragma GCC optimize("-O3")

extern matrix_row_t matrix[MATRIX_ROWS];     // debounced values
extern matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t raw_matrix_right[MATRIX_COLS];

#define MCP_ROWS_PER_HAND (MATRIX_ROWS / 2)
#ifndef I2C_DRIVER
#    define I2C_DRIVER I2CD1
#endif
#ifndef VOYAGER_I2C_TIMEOUT
#    define VOYAGER_I2C_TIMEOUT 100
#endif
// Delay between each i2c io expander ops (in MCU cycles)
#ifndef IO_EXPANDER_OP_DELAY
#    define IO_EXPANDER_OP_DELAY 500
#endif
// Minimum spacing between wireless bulk-path right-half reads (in ms). The right half only
// transmits a fresh snapshot into the nRF's TWIS emulation every 3ms (SNAPSHOT_INTERVAL_MS in
// ptx_main.c), so the mirror at 0x20 cannot hold data any fresher than that no matter how often
// we poll it. Scanning faster than this just re-reads identical bytes and burns ~0.63ms of
// clock-stretched i2c per scan for nothing. Undercutting the snapshot period keeps staleness
// bounded well below what the snapshot cadence itself already costs.
#ifndef VOYAGER_RIGHT_SCAN_INTERVAL_MS
#    define VOYAGER_RIGHT_SCAN_INTERVAL_MS 2
#endif

// Bulk window select register on the wireless (nRF TWIS-emulated) 0x20 slave.
// Not a real MCP23018 register - selects the emulated device's fast-path,
// which returns all 7 classic PORTB row reads in one transaction. A classic
// register-0x12 write reverts the slave to per-row emulation.
#define MCP23018_BULK_WINDOW_REG 0x40

extern bool mcp23018_leds[2];
extern bool is_launching;

static uint16_t mcp23018_reset_loop;
uint8_t         mcp23018_errors;

#ifndef VOYAGER_BUS_PROBE_INTERVAL_MS
#    define VOYAGER_BUS_PROBE_INTERVAL_MS 1000
#endif
// The first init after taking the bus can land while the far side is still
// mastering - it has up to a lease timeout to notice the grant stopped. A
// collision during the IODIR writes is not reported as an error (the expander
// ACKs, then reads back scrambled pin directions), so re-init once more after
// the far side has certainly released.
#ifndef VOYAGER_BUS_REINIT_DELAY_MS
#    define VOYAGER_BUS_REINIT_DELAY_MS 400
#endif

/* Bus ownership. The TRRS segments do not isolate, so the right half's real
 * expander is reachable whenever the cable is plugged - USB or not. Whoever can
 * see it owns the bus, and the other side must stay off it entirely; USB state
 * is not the discriminator. Defaults to "ours" so no grant goes out before the
 * first probe resolves. */
static bool     stm32_owns_right_bus = true;
static uint8_t  mcp23018_addr        = MCP23018_DEFAULT_ADDRESS;
static uint32_t last_bus_probe_ms;
static uint32_t bus_reinit_at_ms;
static bool     bus_reinit_pending;

uint8_t mcp23018_init_local(bool first_run);

bool zsa_stm32_owns_right_bus(void) {
    return stm32_owns_right_bus;
}

static void bus_ownership_task(void) {
    if (timer_elapsed32(last_bus_probe_ms) < VOYAGER_BUS_PROBE_INTERVAL_MS) {
        return;
    }
    last_bus_probe_ms = timer_read32();

    // 0x20 is now exclusively the real chip - the mirror moved to 0x27 - so an
    // ACK here means the physical right half is on the bus, nothing else.
    bool phy_present = (i2c_ping_address(MCP23018_DEFAULT_ADDRESS << 1, VOYAGER_I2C_TIMEOUT) == I2C_STATUS_SUCCESS);
    if (phy_present == stm32_owns_right_bus) {
        return;
    }

    stm32_owns_right_bus = phy_present;
    mcp23018_addr        = phy_present ? MCP23018_DEFAULT_ADDRESS : MCP23018_MIRROR_ADDRESS;
    mcp23018_reset_loop  = 0;
    mcp23018_errors      = mcp23018_init_local(false);
    bus_reinit_pending   = true;
    bus_reinit_at_ms     = timer_read32();
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG
    extern void zsa_rtt_event3(const char *name, int a, int b, int c);
    zsa_rtt_event3(phy_present ? "bus: -> STM32 owns (phy)" : "bus: -> right owns (mirror)", mcp23018_addr, mcp23018_errors, 0);
#endif
}

// Second init on a bus the far side has certainly released by now.
static void bus_reinit_task(void) {
    if (!bus_reinit_pending || timer_elapsed32(bus_reinit_at_ms) < VOYAGER_BUS_REINIT_DELAY_MS) {
        return;
    }
    bus_reinit_pending  = false;
    mcp23018_reset_loop = 0;
    mcp23018_errors     = mcp23018_init_local(false);
#ifdef RGB_MATRIX_ENABLE
    // Driver index 1 now addresses a different physical chip than before the
    // handoff - the real IS31 or the left nRF's mirror - and its hardware state
    // (shutdown bit, page, LED control registers) was never established. Re-init
    // here rather than at the ownership change so the 150+ register writes land
    // on a bus the far side has released.
    rgb_matrix_init();
    // init zeroes the hardware PWM registers but not the driver's software
    // cache, so set_color() would early-return until a colour changes. Zero the
    // cache to match, and the next rendered frame repaints every LED.
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_driver.flush();
#endif
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG
    extern void zsa_rtt_event3(const char *name, int a, int b, int c);
    // rows here are what the expander reported BEFORE this re-init: non-zero
    // with errors==0 is the corrupted-config signature.
    zsa_rtt_event3("bus: delayed reinit", mcp23018_errors, raw_matrix_right[0], raw_matrix_right[1]);
#endif
}

bool io_expander_ready(void) {
    uint8_t tx;
    return mcp23018_readPins(mcp23018_addr, mcp23018_PORTA, &tx);
}

uint8_t mcp23018_init_local(bool first_run) {
    if (!first_run) {
        i2cStop(&I2C_DRIVER);
    }
    wait_ms(10);
    // Try releasing special pins for a short time
    palSetLineMode(B6, PAL_MODE_INPUT);
    palSetLineMode(B7, PAL_MODE_INPUT);
    wait_ms(10);
    palSetLineMode(B6, PAL_MODE_ALTERNATE(4) | PAL_OUTPUT_TYPE_OPENDRAIN);
    palSetLineMode(B7, PAL_MODE_ALTERNATE(4) | PAL_OUTPUT_TYPE_OPENDRAIN);

    uint8_t errors = 0;
    errors += !mcp23018_set_config(mcp23018_addr, mcp23018_PORTA, 0b00000000);
    errors += !mcp23018_set_config(mcp23018_addr, mcp23018_PORTB, 0b00111111);

    return errors;
}

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

    mcp23018_errors = mcp23018_init_local(true);

    if (!mcp23018_errors) {
        is_launching = true;
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    bus_ownership_task();
    bus_reinit_task();
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG: trace mcp23018 error/recovery transitions (hotplug flash hunt)
    extern void    zsa_rtt_event(const char *name);
    static uint8_t rtt_prev_errors;
    if (!rtt_prev_errors && mcp23018_errors) {
        zsa_rtt_event("mcp: errors 0 -> nonzero (right half declared gone)");
    }
    rtt_prev_errors = mcp23018_errors;
#endif
    // Attempt to reset the mcp23018 if it's not initialized
    if (mcp23018_errors) {
        if (++mcp23018_reset_loop > 0x1FFF) {
            bool rtt_probe_ok = io_expander_ready();
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG
            zsa_rtt_event(rtt_probe_ok ? "mcp: probe OK -> recovery + rgb_matrix_init"
                                       : "mcp: probe failed, still gone");
#endif
            if (rtt_probe_ok) {
                // If we managed to initialize the mcp23018 - we need to reinitialize the matrix / layer state. During an electric discharge the i2c peripherals might be in a weird state. Giving a delay and resetting the MCU allows to recover from this.
                mcp23018_reset_loop = 0;
                mcp23018_errors     = mcp23018_init_local(false);
#ifdef RGB_MATRIX_ENABLE
                // Re-initing the LED driver is the ESD remedy this recovery exists for, and
                // it costs ~35ms of dark LEDs (is31fl3731_init() zeroes the hardware PWM
                // registers, then waits). That's only worth paying on the wired bus, where
                // 0x20 is the real expander and its disappearance means the bus really did
                // get scrambled. In wireless mode 0x20 is the nRF's emulation of the right
                // half, which comes and goes routinely (link drop, out of range) with the
                // left-side IS31 completely unaffected - so re-initing there just flashes
                // the matrix for no reason.
                if (usb_connected_state()) {
                    rgb_matrix_init();
                    // rgb_matrix_init() re-zeroes the driver's hardware PWM registers but not
                    // its software cache, so set_color() early-returns until a color actually
                    // changes. Zero the cache too so it matches hardware and the next rendered
                    // frame repaints every LED.
                    rgb_matrix_set_color_all(0, 0, 0);
                    rgb_matrix_driver.flush();
                }
#endif
            } else {
                // Probe failed - retry again in ~4s instead of hammering the bus every scan.
                mcp23018_reset_loop = 0;
            }
        }
    }

    // Scanning left and right side of the keyboard for key presses.
    // Left side is scanned by reading the gpio pins directly, right side is scanned by reading the mcp23018 registers.
    //
    // In wireless mode with no USB, the device at 0x20 is the left nRF's TWIS emulation of the
    // right half (single-axis hardware policy: wireless => never the real MCP23018). It exposes a
    // bulk window (reg 0x40) that returns all 7 row reads in a single write+read pair instead of
    // the classic 7 x (2 writes + 1 read) - collapses ~4.3ms of clock-stretched i2c per scan.
    bool use_bulk_path = !stm32_owns_right_bus && !mcp23018_errors;

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
        // Bulk path selects rows implicitly (all 7 rows come back from one read), so no per-row select here.
        if (!use_bulk_path && !mcp23018_errors) {
            // select row
            mcp23018_errors += !mcp23018_set_output(mcp23018_addr, mcp23018_PORTA, 0b01111111 & ~(1 << (row)));
            mcp23018_errors += !mcp23018_set_output(mcp23018_addr, mcp23018_PORTB, ((uint8_t)!mcp23018_leds[1] << 6) | ((uint8_t)!mcp23018_leds[0] << 7));
        }
        // Reading the left side of the keyboard.
        if (row < MCP_ROWS_PER_HAND) {
            // i2c comm incur enough wait time
            if (mcp23018_errors || use_bulk_path) {
                // Classic path gets its settle time for free from the per-row mcp23018 i2c traffic
                // above. The bulk path (like the error path) issues no i2c between strobe and read,
                // so it needs the same explicit delay to let the pin state settle.
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
        // Bulk path fills raw_matrix_right in one shot after this loop instead of per-row.
        if (!use_bulk_path) {
            if (!mcp23018_errors) {
                for (uint16_t i = 0; i < IO_EXPANDER_OP_DELAY; i++) {
                    __asm__("nop");
                }
                uint8_t rx;
                mcp23018_errors += !mcp23018_readPins(mcp23018_addr, mcp23018_PORTB, &rx);
                data = ~(rx & 0b00111111);
                for (uint16_t i = 0; i < IO_EXPANDER_OP_DELAY; i++) {
                    __asm__("nop");
                }
            } else {
                data = 0;
            }

            if (raw_matrix_right[row] != data) {
                raw_matrix_right[row] = data;
                changed               = true;
            }
        }
    }

    if (use_bulk_path) {
        static uint32_t last_bulk_read_ms;
        // Rate-limit only the i2c transaction itself. Skipping it leaves raw_matrix_right (and
        // therefore changed / current_matrix) exactly as they were - the mirror hasn't produced
        // new data any faster than SNAPSHOT_INTERVAL_MS anyway, so the held-over values are still
        // current. Left-side scanning and matrix_io_delay() above are unaffected by this gate.
        if (timer_elapsed32(last_bulk_read_ms) >= VOYAGER_RIGHT_SCAN_INTERVAL_MS) {
            last_bulk_read_ms = timer_read32();

            // Single write + read pair replaces the 7 x (2 writes + 1 read) classic per-row traffic.
            uint8_t led_bits        = ((uint8_t)!mcp23018_leds[1] << 6) | ((uint8_t)!mcp23018_leds[0] << 7);
            uint8_t addr            = mcp23018_addr << 1;
            uint8_t tx[2]           = {MCP23018_BULK_WINDOW_REG, led_bits};
            uint8_t rx[MATRIX_COLS] = {0};

            bool bulk_ok = (i2c_transmit(addr, tx, sizeof(tx), VOYAGER_I2C_TIMEOUT) == I2C_STATUS_SUCCESS);
            if (!bulk_ok) {
                mcp23018_errors++;
            } else if (i2c_receive(addr, rx, sizeof(rx), VOYAGER_I2C_TIMEOUT) != I2C_STATUS_SUCCESS) {
                mcp23018_errors++;
                bulk_ok = false;
            }

            for (uint8_t row = 0; row < MATRIX_COLS; row++) {
                // On failure report all keys released (like the classic error path's data = 0) —
                // a zeroed rx would otherwise invert to "all keys pressed".
                matrix_row_t bulk_data = bulk_ok ? (matrix_row_t)(~(rx[row] & 0b00111111)) : 0;
                if (raw_matrix_right[row] != bulk_data) {
                    raw_matrix_right[row] = bulk_data;
                    changed               = true;
                }
            }
        }
    }

    for (uint8_t row = 0; row < MCP_ROWS_PER_HAND; row++) {
        current_matrix[11 - row] = 0;
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            current_matrix[11 - row] |= ((raw_matrix_right[6 - col] & (1 << row) ? 1 : 0) << col);
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
    return (bool)(mcp23018_errors == 0);
}
#pragma GCC pop_options
