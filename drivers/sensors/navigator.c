// Copyright 2025 ZSA Technology Labs, Inc <contact@zsa.io>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Navigator Trackball Smooth Scrolling
 *
 * Enhanced scrolling algorithm that eliminates deadzones and provides natural,
 * responsive scrolling for both slow and fast movements.
 *
 * Key Features:
 * - No initial deadzone - scrolling starts immediately with any movement
 * - Smooth acceleration - speed increases naturally with faster movement
 * - Fractional accumulation - sub-pixel movements accumulate until triggering scroll
 * - Reduced jitter - consistent consumption prevents oscillation
 *
 * Configuration Parameters (add to keymap config.h):
 * - NAVIGATOR_SCROLL_DIVIDER: Lower = more sensitive (default: 10)
 * - NAVIGATOR_SCROLL_THRESHOLD: Minimum to scroll (default: 0f)
 * - NAVIGATOR_SCROLL_ACCELERATION: Speed multiplier (default: 1.5f)
 * - NAVIGATOR_SCROLL_MAX_SPEED: Maximum speed limit (default: 8.0f)
 *
 * Algorithm:
 * 1. Accumulate input as floating-point values
 * 2. When accumulated >= 1.0, trigger scrolling with acceleration
 * 3. Subtract exactly 1.0 from accumulation regardless of output
 * 4. Gentle decay (2% per frame) only after 20 frames of inactivity
 */

#include "quantum.h"
#include "navigator.h"

float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;

bool set_scrolling = false;
bool navigator_turbo = false;
bool navigator_aim = false;

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    // Turbo mode is used to increase the speed of the mouse cursor
    // by multiplying the x and y values by a factor.
    if (navigator_turbo) {
        mouse_report.x *= NAVIGATOR_TURBO_MULTIPLIER;
        mouse_report.y *= NAVIGATOR_TURBO_MULTIPLIER;
    }
    // Aim mode is used to slow down the mouse cursor
    // by dividing the x and y values by a factor.
    if (navigator_aim) {
        mouse_report.x /= NAVIGATOR_AIM_DIVIDER;
        mouse_report.y /= NAVIGATOR_AIM_DIVIDER;
    }
    if (set_scrolling) {
        // Accumulate scroll movement
        scroll_accumulated_h += (float)mouse_report.x / NAVIGATOR_SCROLL_DIVIDER;
        scroll_accumulated_v += (float)mouse_report.y / NAVIGATOR_SCROLL_DIVIDER;

        // This allows fractional accumulation to build up before triggering scroll
        float abs_h = (scroll_accumulated_h < 0) ? -scroll_accumulated_h : scroll_accumulated_h;
        float abs_v = (scroll_accumulated_v < 0) ? -scroll_accumulated_v : scroll_accumulated_v;

        float scroll_h = 0.0f;
        float scroll_v = 0.0f;

        if (abs_h >= 1.0f) {
            // Simple acceleration for faster movements
            float speed_h = 1.0f + ((abs_h - 1.0f) * NAVIGATOR_SCROLL_ACCELERATION);
            if (speed_h > NAVIGATOR_SCROLL_MAX_SPEED) {
                speed_h = NAVIGATOR_SCROLL_MAX_SPEED;
            }
            scroll_h = (scroll_accumulated_h > 0) ? speed_h : -speed_h;
        }

        if (abs_v >= 1.0f) {
            float speed_v = 1.0f + ((abs_v - 1.0f) * NAVIGATOR_SCROLL_ACCELERATION);
            if (speed_v > NAVIGATOR_SCROLL_MAX_SPEED) {
                speed_v = NAVIGATOR_SCROLL_MAX_SPEED;
            }
            scroll_v = (scroll_accumulated_v > 0) ? speed_v : -speed_v;
        }

#ifdef NAVIGATOR_SCROLL_INVERT_X
        mouse_report.h = (int8_t)scroll_h;
#else
        mouse_report.h = (int8_t)-scroll_h;
#endif

#ifdef NAVIGATOR_SCROLL_INVERT_Y
        mouse_report.v = (int8_t)-scroll_v;
#else
        mouse_report.v = (int8_t)scroll_v;
#endif


        // Subtract proportional to the base scroll (before acceleration) to prevent jitter
        if (abs_h >= 1.0f) {
            scroll_accumulated_h -= (scroll_accumulated_h > 0) ? 1.0f : -1.0f;
        }
        if (abs_v >= 1.0f) {
            scroll_accumulated_v -= (scroll_accumulated_v > 0) ? 1.0f : -1.0f;
        }


        // Much gentler decay and only after longer idle periods
        static uint8_t idle_counter_h = 0, idle_counter_v = 0;

        if (mouse_report.x == 0 && mouse_report.h == 0) {
            idle_counter_h++;
            if (idle_counter_h > 20) {  // Only decay after 20 frames of no input
                scroll_accumulated_h *= 0.98f;  // Very gentle decay
            }
        } else {
            idle_counter_h = 0;
        }

        if (mouse_report.y == 0 && mouse_report.v == 0) {
            idle_counter_v++;
            if (idle_counter_v > 20) {
                scroll_accumulated_v *= 0.98f;
            }
        } else {
            idle_counter_v = 0;
        }

        mouse_report.x = 0;
        mouse_report.y = 0;
    }
    return mouse_report;
}
