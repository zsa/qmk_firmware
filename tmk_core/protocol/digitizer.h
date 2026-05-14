// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef DIGITIZER_MODE_TOUCHPAD

// Physical sensor dimensions, in 0.01 inch units (HID convention).
// REQUIRED — no default. Each driver/module must define both.
#    ifndef DIGITIZER_TOUCHPAD_PHYSICAL_WIDTH
#        error "DIGITIZER_TOUCHPAD_PHYSICAL_WIDTH must be defined in touchpad mode (units: 0.01 inch)"
#    endif
#    ifndef DIGITIZER_TOUCHPAD_PHYSICAL_HEIGHT
#        error "DIGITIZER_TOUCHPAD_PHYSICAL_HEIGHT must be defined in touchpad mode (units: 0.01 inch)"
#    endif

// Logical coordinate range reported to the host. Used for both axes.
#    ifndef DIGITIZER_TOUCHPAD_LOGICAL_MAX
#        define DIGITIZER_TOUCHPAD_LOGICAL_MAX 2048
#    endif

// Number of simultaneous contacts. Capped at 5 by the 3-bit contact ID field.
#    ifndef DIGITIZER_TOUCHPAD_CONTACT_COUNT
#        define DIGITIZER_TOUCHPAD_CONTACT_COUNT 2
#    endif
#    if DIGITIZER_TOUCHPAD_CONTACT_COUNT > 5
#        error "DIGITIZER_TOUCHPAD_CONTACT_COUNT cannot exceed 5 (3-bit contact ID)"
#    endif

#endif // DIGITIZER_MODE_TOUCHPAD
