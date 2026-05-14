// Copyright 2023 Stefan Kerkmann (@KarlK90)
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "usb_report_handling.h"
#include "usb_endpoints.h"
#include "usb_main.h"
#include "usb_types.h"
#include "usb_driver.h"
#include "report.h"
#ifdef DIGITIZER_MODE_TOUCHPAD
#    include "digitizer.h"
#endif

extern usb_endpoint_in_t     usb_endpoints_in[USB_ENDPOINT_IN_COUNT];
extern usb_endpoint_in_lut_t usb_endpoint_interface_lut[TOTAL_INTERFACES];

#ifdef DIGITIZER_MODE_TOUCHPAD
extern uint8_t digitizer_touchpad_get_input_mode(void);
#endif

void usb_set_report(usb_fs_report_t **reports, const uint8_t *data, size_t length) {
    if (*reports == NULL) {
        return;
    }

    (*reports)->last_report = chVTGetSystemTimeX();
    (*reports)->length      = length;
    memcpy(&(*reports)->data, data, length);
}

void usb_get_report(usb_fs_report_t **reports, uint8_t report_id, usb_fs_report_t *report) {
    (void)report_id;
    if (*reports == NULL) {
        return;
    }

    report->length = (*reports)->length;
    memcpy(&report->data, &(*reports)->data, report->length);
}

void usb_reset_report(usb_fs_report_t **reports) {
    if (*reports == NULL) {
        return;
    }

    memset(&(*reports)->data, 0, (*reports)->length);
    (*reports)->idle_rate   = 0;
    (*reports)->last_report = 0;
}

void usb_shared_set_report(usb_fs_report_t **reports, const uint8_t *data, size_t length) {
    uint8_t report_id = data[0];

    if (report_id > REPORT_ID_COUNT || reports[report_id] == NULL) {
        return;
    }

    reports[report_id]->last_report = chVTGetSystemTimeX();
    reports[report_id]->length      = length;
    memcpy(&reports[report_id]->data, data, length);
}

void usb_shared_get_report(usb_fs_report_t **reports, uint8_t report_id, usb_fs_report_t *report) {
    if (report_id > REPORT_ID_COUNT || reports[report_id] == NULL) {
        return;
    }

    report->length = reports[report_id]->length;
    memcpy(&report->data, &reports[report_id]->data, report->length);
}

void usb_shared_reset_report(usb_fs_report_t **reports) {
    for (int i = 0; i <= REPORT_ID_COUNT; i++) {
        if (reports[i] == NULL) {
            continue;
        }
        memset(&reports[i]->data, 0, reports[i]->length);
        reports[i]->idle_rate   = 0;
        reports[i]->last_report = 0;
    }
}

bool usb_get_report_cb(USBDriver *driver) {
    usb_control_request_t *setup     = (usb_control_request_t *)driver->setup;
    uint8_t                interface = setup->wIndex;
    uint8_t                report_id = setup->wValue.lbyte;

    static usb_fs_report_t report;

#ifdef DIGITIZER_MODE_TOUCHPAD
    // Handle digitizer/touchpad feature report requests
    // Touchpad interface uses its own report ID space (1-5)
    if (interface == DIGITIZER_INTERFACE) {
        static uint8_t feature_report[2];

        switch (report_id) {
            case 0x02:  // Contact Count Maximum + Pad Type
                // Contact Count Maximum (4 bits) + Pad Type (3 bits) + Surface Switch (1 bit)
                // Pad Type: 0=Clickpad (depressible), 1=Pressure-pad
                // Navigator: 2 contacts max, clickpad (type 0)
                feature_report[0] = 0x02;
                feature_report[1] = (DIGITIZER_TOUCHPAD_CONTACT_COUNT << 0) | (0 << 4) | (0 << 7);
                usbSetupTransfer(driver, feature_report, 2, NULL);
                return true;

            case 0x04:  // Configuration - Input Mode
                // Report current input mode (0 = mouse, 3 = PTP)
                feature_report[0] = 0x04;
                feature_report[1] = digitizer_touchpad_get_input_mode();
                usbSetupTransfer(driver, feature_report, 2, NULL);
                return true;

            case 0x05:  // Function Switch
                // Surface and button switches (both enabled)
                feature_report[0] = 0x05;
                feature_report[1] = 0;  // Both switches enabled (0 = enabled)
                usbSetupTransfer(driver, feature_report, 2, NULL);
                return true;

            case 0x03:  // Certification blob
                // Certification blob - Microsoft's default blob for non-certified devices
                {
                    static const uint8_t cert_blob[257] = {
                        0x03,
                        0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87,
                        0x0d, 0xbe, 0x57, 0x3c, 0xb6, 0x70, 0x09, 0x88,
                        0x07, 0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6,
                        0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2,
                        0x2e, 0x84, 0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43,
                        0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc,
                        0x47, 0x70, 0x1b, 0x59, 0x6f, 0x74, 0x43, 0xc4,
                        0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1, 0x71,
                        0xc7, 0x95, 0x0e, 0x31, 0x55, 0x21, 0xd3, 0xb5,
                        0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19,
                        0x3e, 0xb3, 0xaf, 0x75, 0x81, 0x9d, 0x53, 0xb9,
                        0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c,
                        0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d, 0xa7, 0x26,
                        0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89, 0xd7, 0x3b,
                        0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0, 0x2a,
                        0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8,
                        0xc0, 0x8f, 0x33, 0x13, 0x03, 0xf1, 0xd3, 0xc1,
                        0x0b, 0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7,
                        0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b,
                        0xe8, 0x8a, 0x56, 0xf0, 0x8c, 0xaa, 0xfa, 0x35,
                        0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc,
                        0x2b, 0x53, 0x5c, 0x69, 0x52, 0xd5, 0xc8, 0x73,
                        0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff,
                        0x05, 0xd8, 0x2b, 0x79, 0x9a, 0xe2, 0x34, 0x60,
                        0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc,
                        0x80, 0xe3, 0x0f, 0xbd, 0x65, 0x20, 0x08, 0x13,
                        0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7,
                        0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe, 0x31, 0x48,
                        0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a, 0x89,
                        0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a, 0xe4,
                        0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08,
                        0x24, 0x8b, 0xc4, 0x43, 0xa5, 0xe5, 0x24, 0xc2
                    };
                    usbSetupTransfer(driver, (uint8_t *)cert_blob, sizeof(cert_blob), NULL);
                }
                return true;
        }
    }
#endif

    if (!IS_VALID_INTERFACE(interface) || !IS_VALID_REPORT_ID(report_id)) {
        return false;
    }

    usb_endpoint_in_lut_t ep = usb_endpoint_interface_lut[interface];

    if (!IS_VALID_USB_ENDPOINT_IN_LUT(ep)) {
        return false;
    }

    usb_report_storage_t *report_storage = usb_endpoints_in[ep].report_storage;

    if (report_storage == NULL) {
        return false;
    }

    report_storage->get_report(report_storage->reports, report_id, &report);

    usbSetupTransfer(driver, (uint8_t *)report.data, report.length, NULL);

    return true;
}

static bool run_idle_task = false;

void usb_set_idle_rate(usb_fs_report_t **reports, uint8_t report_id, uint8_t idle_rate) {
    (void)report_id;

    if (*reports == NULL) {
        return;
    }

    (*reports)->idle_rate = idle_rate * 4;

    run_idle_task |= idle_rate != 0;
}

uint8_t usb_get_idle_rate(usb_fs_report_t **reports, uint8_t report_id) {
    (void)report_id;

    if (*reports == NULL) {
        return 0;
    }

    return (*reports)->idle_rate / 4;
}

bool usb_idle_timer_elapsed(usb_fs_report_t **reports, uint8_t report_id) {
    (void)report_id;

    if (*reports == NULL) {
        return false;
    }

    osalSysLock();
    time_msecs_t idle_rate   = (*reports)->idle_rate;
    systime_t    last_report = (*reports)->last_report;
    osalSysUnlock();

    if (idle_rate == 0) {
        return false;
    }

    return chTimeI2MS(chVTTimeElapsedSinceX(last_report)) >= idle_rate;
}

void usb_shared_set_idle_rate(usb_fs_report_t **reports, uint8_t report_id, uint8_t idle_rate) {
    // USB spec demands that a report_id of 0 would set the idle rate for all
    // reports of that endpoint, but this can easily lead to resource
    // exhaustion, therefore we deliberalty break the spec at this point.
    if (report_id == 0 || report_id > REPORT_ID_COUNT || reports[report_id] == NULL) {
        return;
    }

    reports[report_id]->idle_rate = idle_rate * 4;

    run_idle_task |= idle_rate != 0;
}

uint8_t usb_shared_get_idle_rate(usb_fs_report_t **reports, uint8_t report_id) {
    if (report_id > REPORT_ID_COUNT || reports[report_id] == NULL) {
        return 0;
    }

    return reports[report_id]->idle_rate / 4;
}

bool usb_shared_idle_timer_elapsed(usb_fs_report_t **reports, uint8_t report_id) {
    if (report_id > REPORT_ID_COUNT || reports[report_id] == NULL) {
        return false;
    }

    osalSysLock();
    time_msecs_t idle_rate   = reports[report_id]->idle_rate;
    systime_t    last_report = reports[report_id]->last_report;
    osalSysUnlock();

    if (idle_rate == 0) {
        return false;
    }

    return chTimeI2MS(chVTTimeElapsedSinceX(last_report)) >= idle_rate;
}

void usb_idle_task(void) {
    if (!run_idle_task) {
        return;
    }

    static usb_fs_report_t report;
    bool                   non_zero_idle_rate_found = false;

    for (int ep = 0; ep < USB_ENDPOINT_IN_COUNT; ep++) {
        usb_report_storage_t *report_storage = usb_endpoints_in[ep].report_storage;

        if (report_storage == NULL) {
            continue;
        }

#if defined(SHARED_EP_ENABLE)
        if (ep == USB_ENDPOINT_IN_SHARED) {
            for (int report_id = 1; report_id <= REPORT_ID_COUNT; report_id++) {
                osalSysLock();
                non_zero_idle_rate_found |= report_storage->get_idle(report_storage->reports, report_id) != 0;
                osalSysUnlock();

                if (usb_endpoint_in_is_inactive(&usb_endpoints_in[ep]) && report_storage->idle_timer_elasped(report_storage->reports, report_id)) {
                    osalSysLock();
                    report_storage->get_report(report_storage->reports, report_id, &report);
                    osalSysUnlock();
                    send_report(ep, &report.data, report.length);
                }
            }
            continue;
        }
#endif

        osalSysLock();
        non_zero_idle_rate_found |= report_storage->get_idle(report_storage->reports, 0) != 0;
        osalSysUnlock();

        if (usb_endpoint_in_is_inactive(&usb_endpoints_in[ep]) && report_storage->idle_timer_elasped(report_storage->reports, 0)) {
            osalSysLock();
            report_storage->get_report(report_storage->reports, 0, &report);
            osalSysUnlock();
            send_report(ep, &report.data, report.length);
        }
    }

    run_idle_task = non_zero_idle_rate_found;
}

bool usb_get_idle_cb(USBDriver *driver) {
    usb_control_request_t *setup     = (usb_control_request_t *)driver->setup;
    uint8_t                interface = setup->wIndex;
    uint8_t                report_id = setup->wValue.lbyte;

    static uint8_t _Alignas(4) idle_rate;

    if (!IS_VALID_INTERFACE(interface) || !IS_VALID_REPORT_ID(report_id)) {
        return false;
    }

    usb_endpoint_in_lut_t ep = usb_endpoint_interface_lut[interface];

    if (!IS_VALID_USB_ENDPOINT_IN_LUT(ep)) {
        return false;
    }

    usb_report_storage_t *report_storage = usb_endpoints_in[ep].report_storage;

    if (report_storage == NULL) {
        return false;
    }

    idle_rate = report_storage->get_idle(report_storage->reports, report_id);

    usbSetupTransfer(driver, &idle_rate, 1, NULL);

    return true;
}

bool usb_set_idle_cb(USBDriver *driver) {
    usb_control_request_t *setup     = (usb_control_request_t *)driver->setup;
    uint8_t                interface = setup->wIndex;
    uint8_t                report_id = setup->wValue.lbyte;
    uint8_t                idle_rate = setup->wValue.hbyte;

    if (!IS_VALID_INTERFACE(interface) || !IS_VALID_REPORT_ID(report_id)) {
        return false;
    }

    usb_endpoint_in_lut_t ep = usb_endpoint_interface_lut[interface];

    if (!IS_VALID_USB_ENDPOINT_IN_LUT(ep)) {
        return false;
    }

    usb_report_storage_t *report_storage = usb_endpoints_in[ep].report_storage;

    if (report_storage == NULL) {
        return false;
    }

    report_storage->set_idle(report_storage->reports, report_id, idle_rate);

    usbSetupTransfer(driver, NULL, 0, NULL);

    return true;
}
