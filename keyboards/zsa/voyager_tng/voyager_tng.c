// Copyright 2026 ZSA Technology Labs, Inc <@zsa>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "i2c_master.h"
#include "busdev.h"

#ifdef COMMUNITY_MODULE_ORYX_ENABLE
#    include "oryx.h"
#endif // COMMUNITY_MODULE_ORYX_ENABLE
#ifdef COMMUNITY_MODULE_DEFAULTS_ENABLE
#     include "defaults.h"
#endif
#ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
#    include "wireless.h"
#endif

keyboard_config_t keyboard_config;

bool mcp23018_leds[2] = {0, 0};
bool is_launching     = false;

#if defined(RGB_MATRIX_ENABLE) && defined(IS31FL3731_I2C_ADDRESS_2)
#    define VOYAGER_RIGHT_LED_DRIVER 1
#    define VOYAGER_RIGHT_LED_REAL_ADDRESS (IS31FL3731_I2C_ADDRESS_2 << 1)

static busdev_id_t led_dev = BUSDEV_NONE;

/* Driver 1 is the real right-half chip while we own its bus, the left nRF's
 * mirror otherwise. It must follow the same signal the matrix does, or we would
 * scan one device and light the other. */
static uint8_t led_right_select_addr(void) {
    return (zsa_stm32_owns_right_bus() ? IS31FL3731_I2C_ADDRESS_2 : ZSA_RIGHT_LED_MIRROR_ADDRESS) << 1;
}

static busdev_step_t led_right_init_step(uint8_t addr, uint8_t step, uint16_t *delay_ms) {
    is31fl3731_set_i2c_address(VOYAGER_RIGHT_LED_DRIVER, addr >> 1);
    return is31fl3731_resync(VOYAGER_RIGHT_LED_DRIVER) ? BUSDEV_STEP_DONE : BUSDEV_STEP_FAIL;
}

/* The frame registers read back unreliably over the right-half cable, but the
 * function registers are readable in operating mode, one byte at a time. Read
 * the shutdown register: 0x01 means running, a reset chip reads 0x00. A single
 * bad readback is not proof of a reset, so it takes two in a row. The mirror is
 * not an IS31 at all and has no function page to read. */
static bool led_right_verify(uint8_t addr) {
    static uint8_t strikes = 0;
    uint8_t        value   = 0;

    if (addr != VOYAGER_RIGHT_LED_REAL_ADDRESS) {
        strikes = 0;
        return true;
    }

    is31fl3731_select_page(VOYAGER_RIGHT_LED_DRIVER, IS31FL3731_COMMAND_FUNCTION);
    i2c_status_t status = i2c_read_register(addr, IS31FL3731_FUNCTION_REG_SHUTDOWN, &value, 1, IS31FL3731_I2C_TIMEOUT);
    is31fl3731_select_page(VOYAGER_RIGHT_LED_DRIVER, IS31FL3731_COMMAND_FRAME_1);

    if (status == I2C_STATUS_SUCCESS && value == 0x01) {
        strikes = 0;
        return true;
    }
    if (strikes < UINT8_MAX) {
        strikes++;
    }
    return strikes < 2;
}

static const busdev_desc_t led_right_desc = {
    .name               = "is31_right",
    .addr               = VOYAGER_RIGHT_LED_REAL_ADDRESS,
    .addr_alt           = 0,
    .settle_ms          = 10,
    .verify_interval_ms = 500,
    .probe_timeout_ms   = 5,
    .max_err_streak     = 1,
    .select_addr        = led_right_select_addr,
    .probe              = NULL,
    .init_step          = led_right_init_step,
    .verify             = led_right_verify,
    .on_state           = NULL,
};

void is31fl3731_write_failed(uint8_t index, i2c_status_t status) {
    if (index == VOYAGER_RIGHT_LED_DRIVER) {
        busdev_report_error(led_dev, status);
    }
}

void voyager_right_led_resync(void) {
    busdev_force_down(led_dev, BUSDEV_REASON_PEER);
}

void voyager_right_led_ownership_changed(void) {
    busdev_force_down(led_dev, BUSDEV_REASON_OWNERSHIP);
}

bool voyager_right_led_up(void) {
    return busdev_is_up(led_dev);
}
#else
void voyager_right_led_resync(void) {}
void voyager_right_led_ownership_changed(void) {}
bool voyager_right_led_up(void) {
    return false;
}
#endif

/* Right-half nRF TWIS command endpoint (also its I2C DFU address). */
#define RIGHT_NRF_I2C_ADDRESS (0x54 << 1)
#define RIGHT_NRF_CMD_REG 0x04
#define RIGHT_NRF_CMD_SET_CHARGE_LIMITS 0x02

/* Deliberately unsupervised: a NACK from the nRF must not mark the whole TRRS
 * transport dead. */
bool voyager_right_nrf_set_charge_limits(uint16_t vbus_limit_ma, uint16_t charge_ma) {
    /* cmd, then the two currents as LE16 — the same payload CMD_CHARGE_LIMITS
     * carries to the left half, so one policy reaches both the same way. */
    uint8_t payload[5] = {
        RIGHT_NRF_CMD_SET_CHARGE_LIMITS,
        (uint8_t)(vbus_limit_ma & 0xFF), (uint8_t)(vbus_limit_ma >> 8),
        (uint8_t)(charge_ma & 0xFF),     (uint8_t)(charge_ma >> 8),
    };
    return i2c_write_register(RIGHT_NRF_I2C_ADDRESS, RIGHT_NRF_CMD_REG, payload, sizeof(payload), VOYAGER_I2C_TIMEOUT) == I2C_STATUS_SUCCESS;
}

#ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
/* Read the right half's telemetry struct. A plain read, no register pointer:
 * the slave serves it from offset 0 (see zsa_right_telemetry.h). Unsupervised
 * like the command above — the caller decides what an absent right half
 * means. */
bool voyager_right_nrf_read_telemetry(struct zsa_right_telemetry *out) {
    return i2c_receive(RIGHT_NRF_I2C_ADDRESS, (uint8_t *)out, sizeof(*out), VOYAGER_I2C_TIMEOUT) == I2C_STATUS_SUCCESS;
}
#endif

#if defined(DEFERRED_EXEC_ENABLE)
#    if defined(DYNAMIC_MACRO_ENABLE)
deferred_token  dynamic_macro_token = INVALID_DEFERRED_TOKEN;
static uint32_t dynamic_macro_led(uint32_t trigger_time, void *cb_arg) {
    static bool led_state = true;
    if (!is_launching) {
        led_state = !led_state;
        STATUS_LED_3(led_state);
    }
    return 100;
}

bool dynamic_macro_record_start_kb(int8_t direction) {
    if (!dynamic_macro_record_start_user(direction)) {
        return false;
    }
    if (dynamic_macro_token == INVALID_DEFERRED_TOKEN) {
        STATUS_LED_3(true);
        dynamic_macro_token = defer_exec(100, dynamic_macro_led, NULL);
    }
    return true;
}

bool dynamic_macro_record_end_kb(int8_t direction) {
    if (!dynamic_macro_record_end_user(direction)) {
        return false;
    }
    if (cancel_deferred_exec(dynamic_macro_token)) {
        dynamic_macro_token = INVALID_DEFERRED_TOKEN;
        STATUS_LED_3(false);
    }
    return true;
}
#    endif

static uint32_t startup_exec(uint32_t trigger_time, void *cb_arg) {
    static uint8_t startup_loop = 0;

    switch (startup_loop++) {
        case 0:
            STATUS_LED_1(true);
            STATUS_LED_2(false);
            STATUS_LED_3(false);
            STATUS_LED_4(false);
            break;
        case 1:
            STATUS_LED_2(true);
            break;
        case 2:
            STATUS_LED_3(true);
            break;
        case 3:
            STATUS_LED_4(true);
            break;
        case 4:
            STATUS_LED_1(false);
            break;
        case 5:
            STATUS_LED_2(false);
            break;
        case 6:
            STATUS_LED_3(false);
            break;
        case 7:
            STATUS_LED_4(false);
            break;
        case 8:
            is_launching = false;
            layer_state_set_kb(layer_state);
            return 0;
    }
    return 250;
}
#endif

void keyboard_pre_init_kb(void) {
    // Initialize Reset pins
    gpio_set_pin_input(A8);
    gpio_set_pin_output(A9);
    gpio_write_pin_low(A9);

    gpio_set_pin_output(B5);
    gpio_set_pin_output(B4);
    gpio_set_pin_output(B3);

    gpio_write_pin_low(B5);
    gpio_write_pin_low(B4);
    gpio_write_pin_low(B3);

    // USB D+ pullup. Wireless: held low until the nRF reports VBUS present.
    // Non-wireless: no nRF to report VBUS, so asserted unconditionally.
    gpio_set_pin_output(A15);
#ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
    gpio_write_pin_low(A15);
#else
    gpio_write_pin_high(A15);
#endif

    keyboard_pre_init_user();
}

layer_state_t layer_state_set_kb(layer_state_t state) {
    state = layer_state_set_user(state);
#if !defined(VOYAGER_USER_LEDS)
#    ifdef COMMUNITY_MODULE_ORYX_ENABLE
    if (rawhid_state.status_led_control) {
        return state;
    }
#    endif
    if (is_launching || !keyboard_config.led_level) return state;
    uint8_t layer = get_highest_layer(state);
#    ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
    /* PB5 belongs to the firmware-update indication while it runs. */
    if (!zsa_wireless_dfu_led_red_owned())
#    endif
        STATUS_LED_1(layer & (1 << 0));
#    ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
    /* PB4 belongs to the update indication, then to the pairing blink. */
    if (!zsa_wireless_dfu_led_green_owned() && !zsa_wireless_pair_led_active())
#    endif
        STATUS_LED_2(layer & (1 << 1));
    STATUS_LED_3(layer & (1 << 2));
#    if !defined(CAPS_LOCK_STATUS)
    STATUS_LED_4(layer & (1 << 3));
#    endif
#endif
    return state;
}

#if defined(COMMUNITY_MODULE_WIRELESS_ENABLE) && !defined(VOYAGER_USER_LEDS)
/* Drives the left-half green status LED (G_LED on PB4 = STATUS_LED_2) from the
 * wireless module's pairing blink. It shares the pin with the layer-1
 * indicator, so RELEASE hands the pin back to the current layer state. */
void zsa_wireless_pair_led(uint8_t state) {
#    ifdef COMMUNITY_MODULE_ORYX_ENABLE
    if (rawhid_state.status_led_control) {
        return;
    }
#    endif
    if (is_launching || !keyboard_config.led_level) {
        return;
    }
    if (state == ZSA_STATUS_LED_RELEASE) {
        STATUS_LED_2(get_highest_layer(layer_state) & (1 << 1));
    } else {
        STATUS_LED_2(state == ZSA_STATUS_LED_ON);
    }
}

/* Drives both left-half status LEDs (green G_LED on PB4 = STATUS_LED_2, red
 * R_LED on PB5 = STATUS_LED_1) from the wireless module's firmware-update
 * indication. It outranks both the pairing blink and the layer indicators, so
 * RELEASE hands each pin back: green to the pairing blink when that owns it —
 * it repaints on its next tick — otherwise to layer bit 1, red to layer
 * bit 0. */
void zsa_wireless_dfu_led(uint8_t green, uint8_t red) {
#    ifdef COMMUNITY_MODULE_ORYX_ENABLE
    if (rawhid_state.status_led_control) {
        return;
    }
#    endif
    if (is_launching || !keyboard_config.led_level) {
        return;
    }
    uint8_t layer = get_highest_layer(layer_state);

    if (green == ZSA_STATUS_LED_RELEASE) {
        if (!zsa_wireless_pair_led_active()) {
            STATUS_LED_2(layer & (1 << 1));
        }
    } else {
        STATUS_LED_2(green == ZSA_STATUS_LED_ON);
    }

    if (red == ZSA_STATUS_LED_RELEASE) {
        STATUS_LED_1(layer & (1 << 0));
    } else {
        STATUS_LED_1(red == ZSA_STATUS_LED_ON);
    }
}
#endif

#ifdef COMMUNITY_MODULE_WIRELESS_ENABLE
// Drives the USB D+ pullup from the nRF's reported VBUS state.
void zsa_wireless_vbus_changed(bool present) {
    gpio_write_pin(A15, present);
}

/* Ignition's "soft-enter DFU" flag: RTC backup register 10, read and cleared
 * once by the bootloader on the next boot. The backup domain survives a system
 * reset. ChibiOS enables the PWR clock and leaves PWR_CR_DBP set, but set it
 * again so the write does not depend on that. */
#define IGNITION_DFU_MAGIC 0x424C

void zsa_wireless_enter_bootloader(void) {
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP10R = IGNITION_DFU_MAGIC;
    NVIC_SystemReset();
}
#endif

#ifdef RGB_MATRIX_ENABLE
// clang-format off
const is31fl3731_led_t PROGMEM g_is31fl3731_leds[RGB_MATRIX_LED_COUNT] = {
/* Refer to IS31 manual for these locations
 *   driver
 *   |  R location
 *   |  |      G location
 *   |  |      |      B location
 *   |  |      |      | */
    {0, C2_2,  C1_2,  C4_3},
    {0, C2_3,  C1_3,  C3_3},
    {0, C2_4,  C1_4,  C3_4},
    {0, C2_5,  C1_5,  C3_5},
    {0, C2_6,  C1_6,  C3_6},
    {0, C2_7,  C1_7,  C3_7},
    {0, C2_8,  C1_8,  C3_8},
    {0, C8_1,  C7_1,  C9_1},
    {0, C8_2,  C7_2,  C9_2},
    {0, C8_3,  C7_3,  C9_3},
    {0, C8_4,  C7_4,  C9_4},
    {0, C8_5,  C7_5,  C9_5},
    {0, C8_6,  C7_6,  C9_6},
    {0, C2_10,  C1_10,  C4_11},
    {0, C2_11,  C1_11,  C3_11},
    {0, C2_12,  C1_12,  C3_12},
    {0, C2_13,  C1_13,  C3_13},
    {0, C2_14,  C1_14,  C3_14},
    {0, C2_15,  C1_15,  C3_15},
    {0, C2_16,  C1_16,  C3_16},
    {0, C8_9,  C7_9,  C9_9},
    {0, C8_10,  C7_10,  C9_10},
    {0, C8_11,  C7_11,  C9_11},
    {0, C8_12,  C7_12,  C9_12},
    {0, C8_13,  C7_13,  C9_13},
    {0, C8_14,  C7_14,  C9_14},

    {1, C2_7,  C1_7,  C3_7},
    {1, C2_6,  C1_6,  C3_6},
    {1, C2_5,  C1_5,  C3_5},
    {1, C2_4,  C1_4,  C3_4},
    {1, C2_3,  C1_3,  C3_3},
    {1, C2_2,  C1_2,  C4_3},

    {1, C8_5,  C7_5,  C9_5},
    {1, C8_4,  C7_4,  C9_4},
    {1, C8_3,  C7_3,  C9_3},
    {1, C8_2,  C7_2,  C9_2},
    {1, C8_1,  C7_1,  C9_1},
    {1, C2_8,  C1_8,  C3_8},

    {1, C2_14,  C1_14,  C3_14},
    {1, C2_13,  C1_13,  C3_13},
    {1, C2_12,  C1_12,  C3_12},
    {1, C2_11,  C1_11,  C3_11},
    {1, C2_10,  C1_10,  C4_11},
    {1, C8_6,  C7_6,  C9_6},

    {1, C8_12,  C7_12,  C9_12},
    {1, C8_11,  C7_11,  C9_11},
    {1, C8_10,  C7_10,  C9_10},
    {1, C8_9,  C7_9,  C9_9},
    {1, C2_16,  C1_16,  C3_16},
    {1, C2_15,  C1_15,  C3_15},

    {1, C8_14,  C7_14,  C9_14},
    {1, C8_13,  C7_13,  C9_13},
};
// clang-format on
#endif

#ifdef SWAP_HANDS_ENABLE
// swap-hands action needs a matrix to define the swap
// clang-format off
const keypos_t PROGMEM hand_swap_config[MATRIX_ROWS][MATRIX_COLS] = {
    /* Left hand, matrix positions */
    {{6,6}, {5,6}, {4,6}, {3,6}, {2,6}, {1,6},{0,6}},
    {{6,7}, {5,7}, {4,7}, {3,7}, {2,7}, {1,7},{0,7}},
    {{6,8}, {5,8}, {4,8}, {3,8}, {2,8}, {1,8},{0,8}},
    {{6,9}, {5,9}, {4,9}, {3,9}, {2,9}, {1,9},{0,9}},
    {{6,10},{5,10},{4,10},{3,10},{2,10},{1,10},{0,10}},
    {{6,11},{5,11},{4,11},{3,11},{2,11},{1,11},{0,11}},
    /* Right hand, matrix positions */
    {{6,0}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0},{0,0}},
    {{6,1}, {5,1}, {4,1}, {3,1}, {2,1}, {1,1},{0,1}},
    {{6,2}, {5,2}, {4,2}, {3,2}, {2,2}, {1,2},{0,2}},
    {{6,3}, {5,3}, {4,3}, {3,3}, {2,3}, {1,3},{0,3}},
    {{6,4}, {5,4}, {4,4}, {3,4}, {2,4}, {1,4},{0,4}},
    {{6,5}, {5,5}, {4,5}, {3,5}, {2,5}, {1,5},{0,5}},
};
// clang-format on
#endif

#ifdef CAPS_LOCK_STATUS
bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);
    if (res) {
        STATUS_LED_4(led_state.caps_lock);
    }
    return res;
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
    switch (keycode) {
#if !defined(VOYAGER_USER_LEDS)
        case LED_LEVEL:
            if (record->event.pressed) {
                keyboard_config.led_level ^= 1;
                eeconfig_update_kb(keyboard_config.raw);
                if (keyboard_config.led_level) {
                    layer_state_set_kb(layer_state);
                } else {
                    STATUS_LED_1(false);
                    STATUS_LED_2(false);
                    STATUS_LED_3(false);
                    STATUS_LED_4(false);
                }
            }
            break;
#endif
#ifdef RGB_MATRIX_ENABLE
        case TOGGLE_LAYER_COLOR:
            if (record->event.pressed) {
                keyboard_config.disable_layer_led ^= 1;
                if (keyboard_config.disable_layer_led) rgb_matrix_set_color_all(0, 0, 0);
                eeconfig_update_kb(keyboard_config.raw);
            }
            break;
        case RGB_TOG:
        case QK_RGB_MATRIX_TOGGLE:
            if (record->event.pressed) {
                switch (rgb_matrix_get_flags()) {
                    case LED_FLAG_ALL: {
                        rgb_matrix_set_flags(LED_FLAG_NONE);
                        rgb_matrix_set_color_all(0, 0, 0);
                    } break;
                    default: {
                        rgb_matrix_set_flags(LED_FLAG_ALL);
                    } break;
                }
            }
            return false;
#endif
    }
    return true;
}

void keyboard_post_init_kb(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_matrix_enable_noeeprom();
#endif
#ifdef VOYAGER_RIGHT_LED_DRIVER
    led_dev = busdev_register(&led_right_desc);
#endif

    keyboard_config.raw = eeconfig_read_kb();

    if (!keyboard_config.led_level && !keyboard_config.led_level_res) {
        keyboard_config.led_level     = true;
        keyboard_config.led_level_res = 0b11;
        eeconfig_update_kb(keyboard_config.raw);
    }
#if defined(DEFERRED_EXEC_ENABLE)
    is_launching = true;
    defer_exec(500, startup_exec, NULL);
#endif
    keyboard_post_init_user();
}

void eeconfig_init_kb(void) { // EEPROM is getting reset!
    keyboard_config.raw           = 0;
    keyboard_config.led_level     = true;
    keyboard_config.led_level_res = 0b11;
    eeconfig_update_kb(keyboard_config.raw);
    eeconfig_init_user();
}

// STM32 system bootloader address for F303
#define STM32_SYSMEM_ADDRESS 0x1FFFD800
#define BOOTLOADER_MAGIC 0xDEADBEEF

extern uint32_t __ram0_end__;

static volatile uint32_t *magic_addr(void) {
    /* Last 4 bytes of RAM — hide the arithmetic from -Warray-bounds */
    volatile uintptr_t p = (uintptr_t)&__ram0_end__ - 4;
    return (volatile uint32_t *)p;
}

__attribute__((weak)) void bootloader_jump(void) {
    *magic_addr() = BOOTLOADER_MAGIC;
    NVIC_SystemReset();
}

__attribute__((weak)) void mcu_reset(void) {
    NVIC_SystemReset();
}

void enter_bootloader_mode_if_requested(void) {
    if (*magic_addr() == BOOTLOADER_MAGIC) {
        *magic_addr() = 0;

        struct {
            uint32_t stack_top;
            void (*entrypoint)(void);
        } const *bootloader = (const void *)STM32_SYSMEM_ADDRESS;

        __disable_irq();

        SysTick->CTRL = 0;
        SysTick->VAL  = 0;
        SysTick->LOAD = 0;

        for (int i = 0; i < ARRAY_SIZE(NVIC->ICER); i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        __set_CONTROL(0);
        __set_MSP(bootloader->stack_top);
        __enable_irq();

        bootloader->entrypoint();
        while (true) {}
    }
}
