/* Copyright 2017 Jason Williams
 * Copyright 2018 Jack Humbert
 * Copyright 2021 Doni Crosby
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "is31fl3731.h"
#include "i2c_master.h"
#include "gpio.h"
#include "wait.h"

#define IS31FL3731_PWM_REGISTER_COUNT 144
#define IS31FL3731_LED_CONTROL_REGISTER_COUNT 18

#ifndef IS31FL3731_I2C_TIMEOUT
#    define IS31FL3731_I2C_TIMEOUT 100
#endif

#ifndef IS31FL3731_I2C_PERSISTENCE
#    define IS31FL3731_I2C_PERSISTENCE 0
#endif

uint8_t i2c_addresses[IS31FL3731_DRIVER_COUNT] = {
    IS31FL3731_I2C_ADDRESS_1,
#ifdef IS31FL3731_I2C_ADDRESS_2
    IS31FL3731_I2C_ADDRESS_2,
#    ifdef IS31FL3731_I2C_ADDRESS_3
    IS31FL3731_I2C_ADDRESS_3,
#        ifdef IS31FL3731_I2C_ADDRESS_4
    IS31FL3731_I2C_ADDRESS_4,
#        endif
#    endif
#endif
};

void is31fl3731_set_i2c_address(uint8_t index, uint8_t address) {
    if (index < IS31FL3731_DRIVER_COUNT) {
        i2c_addresses[index] = address;
    }
}

// These buffers match the IS31FL3731 PWM registers 0x24-0xB3.
// Storing them like this is optimal for I2C transfers to the registers.
// We could optimize this and take out the unused registers from these
// buffers and the transfers in is31fl3731_write_pwm_buffer() but it's
// probably not worth the extra complexity.
typedef struct is31fl3731_driver_t {
    uint8_t pwm_buffer[IS31FL3731_PWM_REGISTER_COUNT];
    bool    pwm_buffer_dirty;
    uint8_t led_control_buffer[IS31FL3731_LED_CONTROL_REGISTER_COUNT];
    bool    led_control_buffer_dirty;
} PACKED is31fl3731_driver_t;

is31fl3731_driver_t driver_buffers[IS31FL3731_DRIVER_COUNT] = {{
    .pwm_buffer               = {0},
    .pwm_buffer_dirty         = false,
    .led_control_buffer       = {0},
    .led_control_buffer_dirty = false,
}};

__attribute__((weak)) void is31fl3731_write_failed(uint8_t index, i2c_status_t status) {}

i2c_status_t is31fl3731_write_register(uint8_t index, uint8_t reg, uint8_t data) {
    i2c_status_t status = I2C_STATUS_SUCCESS;
#if IS31FL3731_I2C_PERSISTENCE > 0
    for (uint8_t i = 0; i < IS31FL3731_I2C_PERSISTENCE; i++) {
        status = i2c_write_register(i2c_addresses[index] << 1, reg, &data, 1, IS31FL3731_I2C_TIMEOUT);
        if (status == I2C_STATUS_SUCCESS) break;
    }
#else
    status = i2c_write_register(i2c_addresses[index] << 1, reg, &data, 1, IS31FL3731_I2C_TIMEOUT);
#endif
    if (status != I2C_STATUS_SUCCESS) {
        is31fl3731_write_failed(index, status);
    }
    return status;
}

i2c_status_t is31fl3731_select_page(uint8_t index, uint8_t page) {
    return is31fl3731_write_register(index, IS31FL3731_REG_COMMAND, page);
}

void is31fl3731_write_pwm_buffer(uint8_t index) {
    // Assumes page 0 is already selected.
    // Transmit PWM registers in 9 transfers of 16 bytes.

    // Iterate over the pwm_buffer contents at 16 byte intervals.
    for (uint8_t i = 0; i < IS31FL3731_PWM_REGISTER_COUNT; i += 16) {
        i2c_status_t status = I2C_STATUS_SUCCESS;
#if IS31FL3731_I2C_PERSISTENCE > 0
        for (uint8_t j = 0; j < IS31FL3731_I2C_PERSISTENCE; j++) {
            status = i2c_write_register(i2c_addresses[index] << 1, IS31FL3731_FRAME_REG_PWM + i, driver_buffers[index].pwm_buffer + i, 16, IS31FL3731_I2C_TIMEOUT);
            if (status == I2C_STATUS_SUCCESS) break;
        }
#else
        status = i2c_write_register(i2c_addresses[index] << 1, IS31FL3731_FRAME_REG_PWM + i, driver_buffers[index].pwm_buffer + i, 16, IS31FL3731_I2C_TIMEOUT);
#endif
        // Abandon the rest of this flush: an unresponsive driver would
        // otherwise cost one timeout per remaining block.
        if (status != I2C_STATUS_SUCCESS) {
            is31fl3731_write_failed(index, status);
            return;
        }
    }
}

void is31fl3731_init_drivers(void) {
    i2c_init();

#if defined(IS31FL3731_SDB_PIN)
    gpio_set_pin_output(IS31FL3731_SDB_PIN);
    gpio_write_pin_high(IS31FL3731_SDB_PIN);
#endif

    for (uint8_t i = 0; i < IS31FL3731_DRIVER_COUNT; i++) {
        is31fl3731_init(i);
    }

    for (int i = 0; i < IS31FL3731_LED_COUNT; i++) {
        is31fl3731_set_led_control_register(i, true, true, true);
    }

    for (uint8_t i = 0; i < IS31FL3731_DRIVER_COUNT; i++) {
        is31fl3731_update_led_control_registers(i);
    }
}

// Returns false at the first failed write, so an unresponsive driver costs one
// timeout instead of one per register.
bool is31fl3731_init(uint8_t index) {
    // In order to avoid the LEDs being driven with garbage data
    // in the LED driver's PWM registers, first enable software shutdown,
    // then set up the mode and other settings, clear the PWM registers,
    // then disable software shutdown.

    if (is31fl3731_select_page(index, IS31FL3731_COMMAND_FUNCTION) != I2C_STATUS_SUCCESS) return false;

    // enable software shutdown
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_SHUTDOWN, 0x00) != I2C_STATUS_SUCCESS) return false;
#ifdef IS31FL3731_DEGHOST // set to enable de-ghosting of the array
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_GHOST_IMAGE_PREVENTION, IS31FL3731_GHOST_IMAGE_PREVENTION_GEN) != I2C_STATUS_SUCCESS) return false;
#endif

    // this delay was copied from other drivers, might not be needed
    wait_ms(10);

    // picture mode
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_CONFIG, IS31FL3731_CONFIG_MODE_PICTURE) != I2C_STATUS_SUCCESS) return false;
    // display frame 0
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_PICTURE_DISPLAY, 0x00) != I2C_STATUS_SUCCESS) return false;
    // auto play off, blink off
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_AUTO_PLAY_1, 0x00) != I2C_STATUS_SUCCESS) return false;
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_AUTO_PLAY_2, 0x00) != I2C_STATUS_SUCCESS) return false;
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_DISPLAY_OPTION, 0x00) != I2C_STATUS_SUCCESS) return false;
    // audio sync off
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_AUDIO_SYNC, 0x00) != I2C_STATUS_SUCCESS) return false;

    if (is31fl3731_select_page(index, IS31FL3731_COMMAND_FRAME_1) != I2C_STATUS_SUCCESS) return false;

    // turn off all LEDs in the LED control register
    for (uint8_t i = 0; i < IS31FL3731_LED_CONTROL_REGISTER_COUNT; i++) {
        if (is31fl3731_write_register(index, IS31FL3731_FRAME_REG_LED_CONTROL + i, 0x00) != I2C_STATUS_SUCCESS) return false;
    }

    // turn off all LEDs in the blink control register (not really needed)
    for (uint8_t i = 0; i < IS31FL3731_LED_CONTROL_REGISTER_COUNT; i++) {
        if (is31fl3731_write_register(index, IS31FL3731_FRAME_REG_BLINK_CONTROL + i, 0x00) != I2C_STATUS_SUCCESS) return false;
    }

    // set PWM on all LEDs to 0
    for (uint8_t i = 0; i < IS31FL3731_PWM_REGISTER_COUNT; i++) {
        if (is31fl3731_write_register(index, IS31FL3731_FRAME_REG_PWM + i, 0x00) != I2C_STATUS_SUCCESS) return false;
    }

    if (is31fl3731_select_page(index, IS31FL3731_COMMAND_FUNCTION) != I2C_STATUS_SUCCESS) return false;

    // disable software shutdown
    if (is31fl3731_write_register(index, IS31FL3731_FUNCTION_REG_SHUTDOWN, 0x01) != I2C_STATUS_SUCCESS) return false;

    // select page 0 and leave it selected.
    // most usage after initialization is just writing PWM buffers in page 0
    // as there's not much point in double-buffering
    return is31fl3731_select_page(index, IS31FL3731_COMMAND_FRAME_1) == I2C_STATUS_SUCCESS;
}

void is31fl3731_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    is31fl3731_led_t led;

    if (index >= 0 && index < IS31FL3731_LED_COUNT) {
        memcpy_P(&led, (&g_is31fl3731_leds[index]), sizeof(led));

        if (driver_buffers[led.driver].pwm_buffer[led.r] == red && driver_buffers[led.driver].pwm_buffer[led.g] == green && driver_buffers[led.driver].pwm_buffer[led.b] == blue) {
            return;
        }

        driver_buffers[led.driver].pwm_buffer[led.r] = red;
        driver_buffers[led.driver].pwm_buffer[led.g] = green;
        driver_buffers[led.driver].pwm_buffer[led.b] = blue;
        driver_buffers[led.driver].pwm_buffer_dirty  = true;
    }
}

void is31fl3731_set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < IS31FL3731_LED_COUNT; i++) {
        is31fl3731_set_color(i, red, green, blue);
    }
}

void is31fl3731_set_led_control_register(uint8_t index, bool red, bool green, bool blue) {
    is31fl3731_led_t led;
    memcpy_P(&led, (&g_is31fl3731_leds[index]), sizeof(led));

    uint8_t control_register_r = led.r / 8;
    uint8_t control_register_g = led.g / 8;
    uint8_t control_register_b = led.b / 8;
    uint8_t bit_r              = led.r % 8;
    uint8_t bit_g              = led.g % 8;
    uint8_t bit_b              = led.b % 8;

    if (red) {
        driver_buffers[led.driver].led_control_buffer[control_register_r] |= (1 << bit_r);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_r] &= ~(1 << bit_r);
    }
    if (green) {
        driver_buffers[led.driver].led_control_buffer[control_register_g] |= (1 << bit_g);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_g] &= ~(1 << bit_g);
    }
    if (blue) {
        driver_buffers[led.driver].led_control_buffer[control_register_b] |= (1 << bit_b);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_b] &= ~(1 << bit_b);
    }

    driver_buffers[led.driver].led_control_buffer_dirty = true;
}

void is31fl3731_update_pwm_buffers(uint8_t index) {
    if (driver_buffers[index].pwm_buffer_dirty) {
        is31fl3731_write_pwm_buffer(index);

        driver_buffers[index].pwm_buffer_dirty = false;
    }
}

void is31fl3731_update_led_control_registers(uint8_t index) {
    if (driver_buffers[index].led_control_buffer_dirty) {
        for (uint8_t i = 0; i < IS31FL3731_LED_CONTROL_REGISTER_COUNT; i++) {
            is31fl3731_write_register(index, i, driver_buffers[index].led_control_buffer[i]);
        }

        driver_buffers[index].led_control_buffer_dirty = false;
    }
}

bool is31fl3731_resync(uint8_t index) {
    if (!is31fl3731_init(index)) {
        return false;
    }

    driver_buffers[index].led_control_buffer_dirty = true;
    is31fl3731_update_led_control_registers(index);

    driver_buffers[index].pwm_buffer_dirty = true;
    return true;
}

__attribute__((weak)) bool is31fl3731_should_flush(uint8_t index) {
    return true;
}

void is31fl3731_flush(void) {
#ifdef ZSA_RTT_DEBUG // TEMP DEBUG: flush duration/gap probe for the wireless LED-lag hunt
    extern uint32_t zsa_rtt_cycles(void);
    extern void     zsa_rtt_flush_probe(uint32_t start_cycles);
    uint32_t rtt_t0 = zsa_rtt_cycles();
#endif
    for (uint8_t i = 0; i < IS31FL3731_DRIVER_COUNT; i++) {
        // A skipped driver's pwm_buffer_dirty is left set (update_pwm_buffers()
        // only clears it after actually writing), so nothing is lost - the next
        // permitted flush still sends the latest colors, just later.
        if (!is31fl3731_should_flush(i)) {
            continue;
        }
        is31fl3731_update_pwm_buffers(i);
    }
#ifdef ZSA_RTT_DEBUG
    zsa_rtt_flush_probe(rtt_t0);
#endif
}
