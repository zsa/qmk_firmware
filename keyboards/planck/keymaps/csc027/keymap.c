#include QMK_KEYBOARD_H
#include "csc027.h"

#define LAYOUT_wrapper(...) LAYOUT_ortho_4x12(__VA_ARGS__)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_QW] = LAYOUT_wrapper(
        _____________________QWERTY_L1_____________________, _____________________QWERTY_R1_____________________,
        _____________________QWERTY_L2_____________________, _____________________QWERTY_R2_____________________,
        _____________________QWERTY_L3_____________________, _____________________QWERTY_R3_____________________,
        _____________________QWERTY_L4_____________________, _____________________QWERTY_R4_____________________
    ),

    [_RS] = LAYOUT_wrapper(
        ______________________RAISE_L1_____________________, ______________________RAISE_R1_____________________,
        ______________________RAISE_L2_____________________, ______________________RAISE_R2_____________________,
        ______________________RAISE_L3_____________________, ______________________RAISE_R3_____________________,
        ______________________RAISE_L4_____________________, ______________________RAISE_R4_____________________
    ),

    [_LW] = LAYOUT_wrapper(
        ______________________LOWER_L1_____________________, ______________________LOWER_R1_____________________,
        ______________________LOWER_L2_____________________, ______________________LOWER_R2_____________________,
        ______________________LOWER_L3_____________________, ______________________LOWER_R3_____________________,
        ______________________LOWER_L4_____________________, ______________________LOWER_R4_____________________
    ),

    [_MS] = LAYOUT_wrapper(
        ______________________MOUSE_L1_____________________, ______________________MOUSE_R1_____________________,
        ______________________MOUSE_L2_____________________, ______________________MOUSE_R2_____________________,
        ______________________MOUSE_L3_____________________, ______________________MOUSE_R3_____________________,
        ______________________MOUSE_L4_____________________, ______________________MOUSE_R4_____________________
    ),

    [_GT] = LAYOUT_wrapper(
        _______________________GIT_L1______________________, _______________________GIT_R1______________________,
        _______________________GIT_L2______________________, _______________________GIT_R2______________________,
        _______________________GIT_L3______________________, _______________________GIT_R3______________________,
        _______________________GIT_L4______________________, _______________________GIT_R4______________________
    ),

    [_CN] = LAYOUT_wrapper(
        ___________________CONVENIENCE_L1__________________, ___________________CONVENIENCE_R1__________________,
        ___________________CONVENIENCE_L2__________________, ___________________CONVENIENCE_R2__________________,
        ___________________CONVENIENCE_L3__________________, ___________________CONVENIENCE_R3__________________,
        ___________________CONVENIENCE_L4__________________, ___________________CONVENIENCE_R4__________________
    ),

    [_GG] = LAYOUT_wrapper(
        ________________General_Game_4x12_L1_______________, ________________General_Game_4x12_R1_______________,
        ________________General_Game_4x12_L2_______________, ________________General_Game_4x12_R2_______________,
        ________________General_Game_4x12_L3_______________, ________________General_Game_4x12_R3_______________,
        ________________General_Game_4x12_L4_______________, ________________General_Game_4x12_R4_______________
    ),

    [_CS] = LAYOUT_wrapper(
        ___________________CSGO_4x12_L1____________________, ___________________CSGO_4x12_R1____________________,
        ___________________CSGO_4x12_L2____________________, ___________________CSGO_4x12_R2____________________,
        ___________________CSGO_4x12_L3____________________, ___________________CSGO_4x12_R3____________________,
        ___________________CSGO_4x12_L4____________________, ___________________CSGO_4x12_R4____________________
    )
};
