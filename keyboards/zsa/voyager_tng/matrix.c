// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "voyager_tng.h"
#include "mcp23018.h"
#include "i2c_master.h"
#include "busdev.h"
#include <string.h>

#pragma GCC push_options
#pragma GCC optimize("-O3")

extern matrix_row_t matrix[MATRIX_ROWS];     // debounced values
extern matrix_row_t raw_matrix[MATRIX_ROWS]; // raw values
static matrix_row_t raw_matrix_right[MATRIX_COLS];

#define MCP_ROWS_PER_HAND (MATRIX_ROWS / 2)
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

#define MCP23018_REAL_ADDRESS (MCP23018_DEFAULT_ADDRESS << 1)

extern bool mcp23018_leds[2];
extern bool is_launching;

static busdev_id_t expander_dev = BUSDEV_NONE;

// Columns physically present on each right-hand matrix row (6..11). A bit set
// outside this mask cannot come from a key press.
static const matrix_row_t right_populated_mask[MCP_ROWS_PER_HAND] = {0x3F, 0x3F, 0x3F, 0x3E, 0x04, 0x60};

// How often the real expander is pinged while the mirror is the one in use.
#ifndef VOYAGER_BUS_PROBE_INTERVAL_MS
#    define VOYAGER_BUS_PROBE_INTERVAL_MS 1000
#endif
#ifndef VOYAGER_BUS_PROBE_TIMEOUT_MS
#    define VOYAGER_BUS_PROBE_TIMEOUT_MS 5
#endif
// Consecutive probe timeouts before the bus is declared contended. One is not
// enough: a single timeout can come from noise or from the far side mid-release.
// Pause on the bus after an ownership flip: the far side may keep mastering for
// up to a lease timeout after the grant changes.
#ifndef VOYAGER_BUS_REINIT_DELAY_MS
#    define VOYAGER_BUS_REINIT_DELAY_MS 400
#endif
#ifndef VOYAGER_BUS_TIMEOUT_STREAK
#    define VOYAGER_BUS_TIMEOUT_STREAK 2
#endif

/* Bus ownership. The TRRS segments do not isolate, so the right half's real
 * expander is reachable whenever the cable is plugged - USB or not. Whoever can
 * see it owns the bus, and the other side must stay off it entirely; USB state
 * is not the discriminator. Defaults to "ours" so no grant goes out before the
 * first probe resolves. */
static bool     stm32_owns_right_bus = true;
static uint8_t  bus_probe_timeouts;
static uint32_t last_bus_probe_ms;

/* Whether the last conclusive probe saw the real right expander, i.e. whether
 * the TRRS cable is plugged. Separate from ownership, which latches to "ours"
 * on a timeout streak. */
static bool     real_expander_present;

bool zsa_stm32_owns_right_bus(void) {
    return stm32_owns_right_bus;
}

bool zsa_trrs_plugged(void) {
    return real_expander_present;
}

static void ownership_set(bool owns) {
    if (owns == stm32_owns_right_bus) {
        return;
    }
    stm32_owns_right_bus = owns;
    // Both right-half devices now answer at a different address on a bus that
    // just changed hands: drop them and let the supervisor re-establish them
    // once the far side has had a lease timeout to notice and release.
    busdev_force_down(expander_dev, BUSDEV_REASON_OWNERSHIP);
    voyager_right_led_ownership_changed();
    busdev_hold(VOYAGER_BUS_REINIT_DELAY_MS);
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG
    extern void zsa_rtt_event3(const char *name, int a, int b, int c);
    zsa_rtt_event3(owns ? "bus: -> STM32 owns (phy)" : "bus: -> right owns (mirror)", owns, 0, 0);
#endif
}

// Turns one ping of the real expander address into an ownership decision. 0x20
// is exclusively the real chip - the mirror lives at 0x27 - so an ACK there
// means the physical right half is on the bus, nothing else.
//
// Three outcomes, not two. SUCCESS is the right half answering. ERROR is a
// clean NAK: the wire works and nothing is on it, so the right half may keep
// the bus. TIMEOUT is the bus itself being unusable - held low, or a second
// master driving it - and must NOT be read as "absent", because that keeps the
// grant alive and leaves both halves mastering, a state the probe can never see
// its way out of: the bus stays stuck, so every later probe times out too.
// Claim ownership instead, which stops the grant and makes the right half
// release within a lease timeout; if nothing was there, the next probe NAKs and
// hands the bus back.
static void ownership_from_probe(i2c_status_t status) {
    if (status == I2C_STATUS_TIMEOUT) {
        if (bus_probe_timeouts < VOYAGER_BUS_TIMEOUT_STREAK) {
            bus_probe_timeouts++;
        }
        if (bus_probe_timeouts >= VOYAGER_BUS_TIMEOUT_STREAK) {
            ownership_set(true);
        }
        return;
    }
    bus_probe_timeouts = 0;
    /* Only a conclusive probe moves cable presence: SUCCESS is the real chip
     * answering, a clean NAK is nothing on the wire. Timeouts are contention,
     * handled above, and leave it unchanged. */
    real_expander_present = (status == I2C_STATUS_SUCCESS);
    ownership_set(status == I2C_STATUS_SUCCESS);
}

// The expander has no alternate address to alternate through: ownership decides
// which of the two is the right one to talk to.
static uint8_t expander_select_addr(void) {
    return (stm32_owns_right_bus ? MCP23018_DEFAULT_ADDRESS : MCP23018_MIRROR_ADDRESS) << 1;
}

static i2c_status_t expander_probe(uint8_t addr) {
    i2c_status_t status = i2c_ping_address(addr, VOYAGER_BUS_PROBE_TIMEOUT_MS);
    if (addr == MCP23018_REAL_ADDRESS) {
        ownership_from_probe(status);
    }
    return status;
}

// Complete configuration file, registers 0x00..0x0D (BANK0), written and
// verified as one block: corrupted bus traffic can reach any register (an
// address one bit off the expander's writes here), and a stray IPOL or
// GPINTEN bit fakes key state with IODIR/GPPU still intact.
static const uint8_t expander_config[14] = {
    0x00, 0x3F, // IODIRA outputs, IODIRB rows in / LEDs out
    0x00, 0x00, // IPOLA/B no inversion
    0x00, 0x00, // GPINTENA/B interrupts off
    0x00, 0x00, // DEFVALA/B
    0x00, 0x00, // INTCONA/B
    0x00, 0x00, // IOCON (twice, both banks' view)
    0x00, 0x3F, // GPPUA none, GPPUB pull-ups on the rows
};

static busdev_step_t expander_init_step(uint8_t addr, uint8_t step, uint16_t *delay_ms) {
    if (i2c_write_register(addr, 0x00, expander_config, sizeof(expander_config), VOYAGER_I2C_TIMEOUT) != I2C_STATUS_SUCCESS) {
        return BUSDEV_STEP_FAIL;
    }
    return BUSDEV_STEP_DONE;
}

// Reads IODIRA/IODIRB and GPPUA/GPPUB back and compares them with what
// expander_init_step() wrote. Only the real chip is read: the nRF's emulation
// answers a different register model, and the impossible-key mask is the safety
// net there.
static bool expander_verify(uint8_t addr) {
    if (addr != MCP23018_REAL_ADDRESS) {
        return true;
    }

    uint8_t buf[sizeof(expander_config)];

    if (i2c_read_register(addr, 0x00, buf, sizeof(buf), VOYAGER_I2C_TIMEOUT) != I2C_STATUS_SUCCESS) {
        return false;
    }
    return memcmp(buf, expander_config, sizeof(buf)) == 0;
}

// Both real right-half chips share one power rail, and only the expander has a
// trustworthy readback, so its state changes stand in for "the right half
// reset": SETTLING->CONFIGURING is the one transition that follows a probe ACK
// out of ABSENT (a hot-plug), and UP->CONFIGURING means it lost its
// configuration while still ACKing. On the mirror the two are unrelated - the
// LED "chip" is the left nRF, not the right half.
static void expander_on_state(busdev_id_t id, busdev_state_t old_state, busdev_state_t new_state, uint8_t reason) {
    if (new_state != BUSDEV_CONFIGURING || busdev_addr(id) != MCP23018_REAL_ADDRESS) {
        return;
    }
    if (old_state == BUSDEV_SETTLING || (old_state == BUSDEV_UP && (reason == BUSDEV_REASON_VERIFY_MISMATCH || reason == BUSDEV_REASON_IMPOSSIBLE_KEY))) {
        voyager_right_led_resync();
    }
}

static const busdev_desc_t expander_desc = {
    .name               = "mcp23018",
    .addr               = MCP23018_REAL_ADDRESS,
    .addr_alt           = 0,
    .settle_ms          = 20,
    .verify_interval_ms = 100,
    .probe_timeout_ms   = VOYAGER_BUS_PROBE_TIMEOUT_MS,
    .max_err_streak     = 1,
    .select_addr        = expander_select_addr,
    .probe              = expander_probe,
    .init_step          = expander_init_step,
    .verify             = expander_verify,
    .on_state           = expander_on_state,
};

// While we do not own the bus, expander_select_addr() keeps the supervisor on
// the mirror and nothing ever addresses the real expander, so a right half
// appearing on the wire would go unnoticed. Ping it directly. Not needed the
// other way round: the far side cannot take a bus we own, and if the right half
// goes away the scan errors take the expander down, after which its own probes
// see the NAKs.
static void bus_ownership_task(void) {
    if (stm32_owns_right_bus) {
        return;
    }
    if (timer_elapsed32(last_bus_probe_ms) < VOYAGER_BUS_PROBE_INTERVAL_MS) {
        return;
    }
    last_bus_probe_ms = timer_read32();

    ownership_from_probe(i2c_ping_address(MCP23018_REAL_ADDRESS, VOYAGER_BUS_PROBE_TIMEOUT_MS));
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

    i2c_init();

    if (expander_dev == BUSDEV_NONE) {
        expander_dev = busdev_register(&expander_desc);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    bus_ownership_task();

    bool    right_ok = busdev_is_up(expander_dev);
    uint8_t mcp_addr = busdev_addr(expander_dev) >> 1;

    // Scanning left and right side of the keyboard for key presses.
    // Left side is scanned by reading the gpio pins directly, right side is scanned by reading the mcp23018 registers.
    //
    // In wireless mode with no USB, the device at 0x27 is the left nRF's TWIS emulation of the
    // right half. It exposes a bulk window (reg 0x40) that returns all 7 row reads in a single
    // write+read pair instead of the classic 7 x (2 writes + 1 read) - collapses ~4.3ms of
    // clock-stretched i2c per scan.
    bool use_bulk_path = !stm32_owns_right_bus && right_ok;

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
        if (!use_bulk_path && right_ok) {
            if (!mcp23018_set_output(mcp_addr, mcp23018_PORTA, 0b01111111 & ~(1 << (row))) || !mcp23018_set_output(mcp_addr, mcp23018_PORTB, ((uint8_t)!mcp23018_leds[1] << 6) | ((uint8_t)!mcp23018_leds[0] << 7))) {
                busdev_report_error(expander_dev, I2C_STATUS_ERROR);
                right_ok = false;
            }
        }
        // Reading the left side of the keyboard.
        if (row < MCP_ROWS_PER_HAND) {
            // i2c comm incur enough wait time
            if (!right_ok || use_bulk_path) {
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
            uint8_t addr            = mcp_addr << 1;
            uint8_t tx[2]           = {MCP23018_BULK_WINDOW_REG, led_bits};
            uint8_t rx[MATRIX_COLS] = {0};

            i2c_status_t status = i2c_transmit(addr, tx, sizeof(tx), VOYAGER_I2C_TIMEOUT);
            if (status == I2C_STATUS_SUCCESS) {
                status = i2c_receive(addr, rx, sizeof(rx), VOYAGER_I2C_TIMEOUT);
            }
            if (status != I2C_STATUS_SUCCESS) {
                busdev_report_error(expander_dev, status);
            }

            for (uint8_t row = 0; row < MATRIX_COLS; row++) {
                // On failure report all keys released (like the classic error path's data = 0) —
                // a zeroed rx would otherwise invert to "all keys pressed".
                matrix_row_t bulk_data = (status == I2C_STATUS_SUCCESS) ? (matrix_row_t)(~(rx[row] & 0b00111111)) : 0;
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
