#define _BASE 0
#define _MID 1
#define _RAISE 2
#define _ADJUST 3
#define _GAME 4
#define _PHOTOSHOP 5
#define _INDESIGN 6
#define _SNAKE 7

#ifdef RGBLIGHT_ENABLE
    extern rgblight_config_t rgblight_config;
    rgblight_config_t        RGB_current_config;
#endif

#ifdef RGB_MATRIX_ENABLE
    extern rgb_config_t rgb_matrix_config;
    rgb_config_t        RGB_current_config;
#endif

#ifdef AUDIO_ENABLE
    extern audio_config_t audio_config;
#endif

#ifndef KEYLOGGER_LENGTH
    #define KEYLOGGER_LENGTH ((int)(OLED_DISPLAY_HEIGHT / OLED_FONT_WIDTH))
#endif

#define SLEEP_TIMEOUT 45000
#define SCREEN_TIMEOUT 20000

static uint32_t oled_timer = 0;
static char keylog_str[KEYLOGGER_LENGTH + 1] = {"\n"};
static uint16_t log_timer = 0;
static const char PROGMEM lower_log[0xFF] = {
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    ' ', ' ', ' ', ' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',  // 0x
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '1', '2',  // 1x
    '3', '4', '5', '6', '7', '8', '9', '0',  20,  19,  27,  26,  22, '-', '=', '[',  // 2x
    ']','\\', '#', ';','\'', '`', ',', '.', '/', 128, ' ', ' ', ' ', ' ', ' ', ' ',  // 3x
    ' ', ' ', ' ', ' ', ' ', ' ', 'p', 's', ' ', ' ', ' ', ' ',  16, ' ', ' ', ' ',  // 4x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1', '2', '3', '4', '5', '6', '7',  // 5x
    '8', '9', '0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 6x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 7x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 8x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 9x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Ax
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Bx
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Cx
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Dx
    'c', 's', 'a', 'c', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Ex
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '        // Fx
};

static const char PROGMEM upper_log[0xFF] = {
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    ' ', ' ', ' ', ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  // 0x
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '1', '2',  // 1x
    '3', '4', '5', '6', '7', '8', '9', '0',  20,  19,  27,  26,  22, '-', '=', '[',  // 2x
    ']','\\', '#', ';','\'', '`', ',', '.', '/', 128, ' ', ' ', ' ', ' ', ' ', ' ',  // 3x
    ' ', ' ', ' ', ' ', ' ', ' ', 'P', 'S', ' ', ' ', ' ', ' ',  16, ' ', ' ', ' ',  // 4x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '1', '2', '3', '4', '5', '6', '7',  // 5x
    '8', '9', '0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 6x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 7x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 8x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // 9x
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Ax
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Bx
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Cx
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Dx
    'C', 'S', 'A', 'C', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  // Ex
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '        // Fx
};

void add_keylog(uint16_t keycode);

bool mute_enabled = false;
#ifdef ENCODER_ENABLE
bool encoder_enabled = false;
#endif

void add_keylog(uint16_t keycode) {
  if ((keycode >= QK_MOD_TAP && keycode <= QK_MOD_TAP_MAX) || (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) || (keycode >= QK_MODS && keycode <= QK_MODS_MAX)) {
    keycode = keycode & 0xFF;
  } else if (keycode > 0xFF) {
    keycode = 0;
  }

  for (uint8_t i = (KEYLOGGER_LENGTH - 1); i > 0; --i) {
    keylog_str[i] = keylog_str[i - 1];
  }

  if (keycode < (sizeof(lower_log) / sizeof(char))) {
    keylog_str[0] = pgm_read_byte(&lower_log[keycode]);
  }

  log_timer = timer_read();
};

void update_log(void) {
    if (timer_elapsed(log_timer) > 750) {
        add_keylog(0);
    }
}

// keylogger
void render_keylogger_status(void) {
    oled_write_P(PSTR("klggr"), false);
    oled_write(keylog_str, false);
    oled_write_ln_P(PSTR(" "), false);
}

// caps/num lock
void render_keylock_status(uint8_t led_usb_state) {
    oled_write_P(PSTR("lock:"), false);
    oled_write_ln_P(PSTR("caps"), led_usb_state & (1 << USB_LED_CAPS_LOCK));
    oled_write_ln_P(PSTR("numl"), led_usb_state & (1 << USB_LED_NUM_LOCK));
    //oled_write_ln_P(PSTR("SCRL"), led_usb_state & (1 << USB_LED_SCROLL_LOCK));
    oled_write_ln_P(PSTR(" "), false);
}

// mod identifier text
void render_mod_status_text(uint8_t modifiers) {
    oled_write_P(PSTR("mods:"), false);
    oled_write_ln_P(PSTR("shft"), (modifiers & MOD_MASK_SHIFT));
    oled_write_ln_P(PSTR("ctrl"), (modifiers & MOD_MASK_CTRL));
    oled_write_ln_P(PSTR("alt"), (modifiers & MOD_MASK_ALT));
    //oled_write_ln_P(PSTR("WIN"), (modifiers & MOD_MASK_GUI));
}

// oled OLED_ROTATION_270
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
}

static void render_statement_logo(void) {
  static const char PROGMEM statement_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0x20, 0x10, 0x10,
    0x10, 0xF0, 0x10, 0x10, 0x10, 0x10,
    0xC8, 0x00, 0x00, 0x00, 0xC0, 0x00,
    0x00, 0xF0, 0x08, 0x08, 0x08, 0x10,
    0x00, 0x00, 0xFF, 0x04, 0x04, 0x3B,
    0x40, 0x20, 0x00, 0x00, 0x83, 0x04,
    0x00, 0x00, 0x81, 0x7F, 0x01, 0x01,
    0x00, 0x00, 0x0F, 0x10, 0x10, 0x10,
    0x0F, 0x10, 0x00, 0x01, 0x02, 0x02,
    0x02, 0x81, 0x40, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x00, 0x00,
    0x00, 0xE0, 0x10, 0x10, 0x70, 0x90,
    0xE0, 0x18, 0x00, 0x20, 0xFE, 0x21,
    0x02, 0x00, 0x08, 0xFF, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1F, 0x20, 0x20,
    0x20, 0x20, 0x1F, 0x00, 0x00, 0x08,
    0x07, 0x00, 0x00, 0x00, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x40, 0x20, 0x20, 0x20, 0xE0,
    0x20, 0x20, 0x20, 0x20, 0x90, 0x00,
    0x00, 0x00, 0x80, 0x00, 0x00, 0xE0,
    0x10, 0x10, 0x10, 0x20, 0x00, 0x00,
    0xFF, 0x08, 0x08, 0x76, 0x80, 0x40,
    0x00, 0x00, 0x07, 0x08, 0x00, 0x00,
    0x02, 0xFF, 0x02, 0x02, 0x00, 0x00,
    0x1F, 0x20, 0x20, 0x20, 0x1F, 0x20,
    0x00, 0x03, 0x04, 0x04, 0x04, 0x02,
    0x80, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x02, 0x02, 0x01, 0x00, 0x00, 0xC0,
    0x20, 0x20, 0xE0, 0x20, 0xC0, 0x30,
    0x00, 0x40, 0xFC, 0x42, 0x04, 0x00,
    0x10, 0xFF, 0x10, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x3F, 0x40, 0x40, 0x40, 0x41,
    0x3F, 0x00, 0x00, 0x10, 0x0F, 0x00,
    0x00, 0x00, 0x04, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x40, 0x40, 0x40, 0xC0, 0x40, 0x40,
    0x40, 0x40, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xC0, 0x20, 0x20,
    0x20, 0x40, 0x00, 0x00, 0xFF, 0x10,
    0x10, 0xEC, 0x00, 0x80, 0x00, 0x00,
    0x0F, 0x10, 0x00, 0x00, 0x04, 0xFF,
    0x04, 0x04, 0x00, 0x00, 0x3F, 0x40,
    0x40, 0x40, 0x3F, 0x40, 0x00, 0x07,
    0x08, 0x08, 0x08, 0x04, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x02, 0x04, 0x04, 0x04,
    0x02, 0x01, 0x00, 0x80, 0x40, 0x40,
    0xC0, 0x40, 0x80, 0x60, 0x00, 0x80,
    0xF8, 0x84, 0x08, 0x00, 0x20, 0xFE,
    0x21, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
    0x80, 0x80, 0x81, 0x82, 0x7F, 0x00,
    0x00, 0x20, 0x1F, 0x00, 0x00, 0x00,
    0x08, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(statement_logo, sizeof(statement_logo));
}

static void render_acrnym_logo(void) {
  static const char PROGMEM acrnym_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0xC0,
    0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0x7E,
    0x3E, 0x1E, 0x0E, 0x06, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xF8, 0xFC, 0xFE, 0xFF,
    0xFF, 0xDF, 0xCF, 0xC7, 0xC3, 0xC1,
    0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFE,
    0xFC, 0xF8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x03, 0xE7, 0xF7, 0xFF, 0xFF, 0xFF,
    0x87, 0x87, 0x87, 0x80, 0x80, 0x80,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F,
    0xFE, 0xFC, 0xF8, 0xF1, 0xE3, 0xC7,
    0x8F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x07, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0,
    0xE0, 0xF0, 0xF8, 0xFF, 0x7F, 0x3F,
    0x0F, 0x1F, 0x3F, 0x7F, 0xFE, 0xFC,
    0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0x07, 0x07, 0x07, 0x03, 0x01, 0x00,
    0x00, 0xE0, 0xF0, 0xF8, 0xF8, 0xF8,
    0x78, 0x79, 0x79, 0x79, 0x71, 0x61,
    0x41, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x03, 0x07,
    0x0F, 0x1F, 0x3F, 0x7E, 0xFC, 0xF8,
    0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x18, 0x9C, 0x9E, 0x9F, 0x9F, 0x9F,
    0x9F, 0x9F, 0x1F, 0x1F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x1F,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xFE, 0xFF, 0xFF, 0x0F, 0x07,
    0x07, 0x07, 0x0F, 0xFF, 0xFF, 0xFE,
    0x00, 0x00, 0xC0, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F, 0x1F, 0x1F,
    0x1C, 0x9C, 0x9C, 0x9C, 0x9E, 0x9F,
    0x8F, 0x0F, 0x00, 0x00, 0x0F, 0x9F,
    0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8,
    0xFC, 0x7E, 0x3F, 0x1F, 0x0F, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF8, 0xFC, 0x7E,
    0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0,
    0xC0, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F,
    0x00, 0x00, 0xF8, 0xF0, 0xE0, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x83, 0xC7,
    0xC7, 0x83, 0x00, 0x00, 0x00, 0x07,
    0x0F, 0x1F, 0x3F, 0x7F, 0xFF, 0xFB,
    0xF3, 0xE0, 0xE0, 0xE0, 0xFF, 0xFF,
    0xFF, 0xFF, 0xE0, 0xC0, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF1, 0xF1, 0xF9, 0xF9, 0xF9,
    0xF9, 0xF9, 0xF9, 0xF1, 0xE1, 0xC1,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x06, 0x0E, 0x1E, 0x3E, 0x7E,
    0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xFC,
    0xFC, 0xFC, 0x00, 0x00, 0x1F, 0x0F,
    0x07, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x03, 0x07,
    0x0F, 0x1F, 0x3F, 0x7F, 0x7E, 0x7C,
    0x78, 0x70, 0x60, 0x40, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(acrnym_logo, sizeof(acrnym_logo));
}

static void render_dead_logo(void) {
  static const char PROGMEM dead_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0x60,
    0x10, 0x08, 0x08, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x08,
    0x08, 0x10, 0x60, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFE,
    0x01, 0x00, 0x3C, 0x1E, 0x1F, 0x0F,
    0x00, 0xC0, 0xC0, 0x00, 0x0F, 0x1F,
    0x1E, 0x3C, 0x00, 0x00, 0x00, 0x01,
    0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1E,
    0x21, 0x11, 0x1E, 0x20, 0xC0, 0x00,
    0x00, 0x00, 0x07, 0x07, 0x07, 0x07,
    0x00, 0x00, 0x00, 0x1E, 0x21, 0x11,
    0x08, 0x00, 0x07, 0x04, 0x08, 0x10,
    0x90, 0x48, 0x3C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x02, 0x02, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x02, 0x02,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFE, 0xFE, 0x06, 0x0E,
    0xFC, 0xF8, 0x00, 0x00, 0xFE, 0xFE,
    0x66, 0x66, 0x66, 0x06, 0x00, 0x00,
    0xFC, 0xFE, 0xE6, 0xE6, 0xFE, 0xFC,
    0x00, 0x00, 0xFE, 0xFE, 0x06, 0x0E,
    0xFC, 0xF8, 0x00, 0x00, 0x07, 0x07,
    0x06, 0x07, 0x03, 0x01, 0x00, 0x00,
    0x07, 0x07, 0x06, 0x06, 0x06, 0x06,
    0x00, 0x00, 0x07, 0x07, 0x00, 0x00,
    0x07, 0x07, 0x00, 0x00, 0x07, 0x07,
    0x06, 0x07, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x40, 0x20, 0x10,
    0x10, 0x08, 0x88, 0xC4, 0xC4, 0x04,
    0x04, 0x84, 0xC4, 0xC4, 0xC4, 0xC4,
    0xC4, 0xC4, 0xC4, 0xCC, 0x10, 0x10,
    0x10, 0xCC, 0xC4, 0xC4, 0xFC, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x01, 0x00,
    0x0C, 0x0E, 0x0F, 0x0C, 0x0F, 0x0F,
    0x07, 0x80, 0xC0, 0xC1, 0x80, 0x00,
    0xE0, 0xF0, 0xF0, 0xE0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x04, 0x08, 0x10, 0x10, 0x20,
    0x20, 0x40, 0x40, 0x41, 0x43, 0x47,
    0x47, 0x46, 0x47, 0x43, 0x41, 0x40,
    0x40, 0x60, 0x10, 0x10, 0x10, 0x60,
    0x40, 0x40, 0x7F, 0x00, 0x00, 0x00,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x80,
    0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0x00, 0x00, 0x80, 0xC0,
    0xC0, 0xC0, 0xC0, 0x80, 0x00, 0x00,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0x00, 0x00, 0xFF, 0xFF, 0xCC, 0xCC,
    0xFF, 0x7B, 0x00, 0x00, 0xFF, 0xFF,
    0xCC, 0xCC, 0xCC, 0xC0, 0x00, 0x00,
    0xFF, 0xFF, 0x0C, 0x0C, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x80, 0x80, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x80, 0x80,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x78, 0x64, 0x44, 0x42, 0x22, 0xE1,
    0x19, 0x01, 0x01, 0x21, 0x20, 0x10,
    0x10, 0x00, 0x80, 0xA0, 0x80, 0x00,
    0x10, 0x10, 0x20, 0x21, 0x01, 0x01,
    0x19, 0xE1, 0x22, 0x42, 0x64, 0x7C,
    0x00, 0x00, 0x01, 0x02, 0x04, 0x08,
    0x1F, 0x60, 0x80, 0x06, 0x0F, 0x0F,
    0x06, 0x00, 0x00, 0x0F, 0x13, 0x13,
    0x1B, 0x27, 0x44, 0x84, 0x0E, 0x1F,
    0x2F, 0x06, 0x80, 0x60, 0x1F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x61, 0x92,
    0x0C, 0x00, 0xF8, 0x07, 0x00, 0x30,
    0xC0, 0x00, 0x0F, 0xF0, 0x00, 0x01,
    0x02, 0x02, 0x02, 0x01, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x07,
    0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x03,
    0x04, 0x68, 0x50, 0x51, 0x41, 0x60,
    0x20, 0x20, 0x10, 0x10, 0x10, 0x20,
    0x20, 0x60, 0x40, 0x50, 0x50, 0x68,
    0x04, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(dead_logo, sizeof(dead_logo));
}

static void render_media_logo(void) {
  static const char PROGMEM media_logo[] = {
    0x00, 0x00, 0x00, 0xF0, 0x30, 0x70,
    0xF0, 0xF0, 0xE0, 0x60, 0xE0, 0x40,
    0xC0, 0xC0, 0xC0, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0x80, 0xE0, 0xF0, 0xF9, 0x9F, 0xCF,
    0x76, 0x73, 0x7B, 0x7D, 0x7D, 0x7E,
    0x7E, 0x7E, 0x7E, 0xFE, 0xFE, 0xBC,
    0xFC, 0x78, 0xF0, 0xE0, 0xC0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0xFF, 0x7F, 0xCF, 0x3F, 0x78,
    0x80, 0x08, 0x04, 0x04, 0x04, 0x06,
    0x02, 0x02, 0x02, 0x03, 0x83, 0x80,
    0xC3, 0xEF, 0x7E, 0xFB, 0xFD, 0xB7,
    0x1F, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x70, 0x3E, 0x1F, 0x18, 0x17,
    0x2B, 0x76, 0xEE, 0xFE, 0x12, 0xF8,
    0xFC, 0x7C, 0x3E, 0x1E, 0x1F, 0x0F,
    0x0F, 0x07, 0x0F, 0x1C, 0x18, 0x1C,
    0x0D, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0xFE, 0x00, 0x30, 0x32, 0x33, 0x13,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
    0xFF, 0x0E, 0x1C, 0x1C, 0x0E, 0xFF,
    0xFF, 0x00, 0xF0, 0xF0, 0x30, 0x30,
    0x30, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7F, 0x1F, 0x00,
    0x00, 0x03, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x07, 0x00, 0xFF, 0x7F,
    0x03, 0x03, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0x00, 0x00, 0xC0,
    0x20, 0x20, 0xC0, 0x00, 0xE0, 0xA0,
    0x40, 0x00, 0xE0, 0x20, 0xC0, 0x00,
    0x00, 0x00, 0x00, 0xE0, 0x70, 0x10,
    0x10, 0x30, 0xF0, 0x80, 0x00, 0xE0,
    0xFC, 0x00, 0x00, 0x00, 0xE1, 0x01,
    0x00, 0x60, 0x31, 0x31, 0xF0, 0xC0,
    0x01, 0xC0, 0xE1, 0x20, 0x21, 0xE1,
    0xC0, 0x00, 0x00, 0x00, 0x1F, 0x3F,
    0x30, 0x30, 0x3C, 0x78, 0x6F, 0x6F,
    0x40, 0x47, 0x4F, 0x0C, 0x0C, 0x0F,
    0x07, 0x00, 0x1C, 0x3E, 0x33, 0x31,
    0x3F, 0x1F, 0x00, 0xE7, 0xCF, 0x8C,
    0x88, 0xFB, 0xF3, 0xE0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x80, 0xC0, 0xF0, 0xF8,
    0xCC, 0xEF, 0xFE, 0xFE, 0xDF, 0xF4,
    0x7C, 0xF8, 0x60, 0x00, 0xC0, 0xA0,
    0x40, 0xA1, 0x51, 0xA1, 0x01, 0x80,
    0x00, 0x00, 0x00, 0xA0, 0x40, 0x0E,
    0x1B, 0x1F, 0x1D, 0x1F, 0x8F, 0xCF,
    0x6F, 0x3F, 0x1F, 0x9F, 0xEB, 0xFF,
    0xFE, 0xFD, 0xBF, 0x6F, 0xFE, 0xBF,
    0xFF, 0xB6, 0x77, 0xEF, 0xDD, 0xBE,
    0x6D, 0x5A, 0x00, 0x00, 0x00, 0x31,
    0x7A, 0xEE, 0xC4, 0xC0, 0xE8, 0x7E,
    0x15, 0x00, 0x00, 0x00, 0xFE, 0x0B,
    0xFF, 0x3F, 0x1F, 0x0B, 0x07, 0x07,
    0x03, 0x07, 0x06, 0x40, 0x0F, 0x0F,
    0x5F, 0x1F, 0x8F, 0xD5, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0,
    0xB0, 0xF8, 0x18, 0xF0, 0xE0, 0x00,
    0x01, 0x7B, 0xFF, 0xF0, 0x60, 0x20,
    0x40, 0x04, 0x20, 0x60, 0xF2, 0xF0,
    0xB8, 0x7C, 0xAE, 0x55, 0xAF, 0x55,
    0x00, 0x00, 0x00, 0x7E, 0xB7, 0xD9,
    0xEA, 0x77, 0x2B, 0x1D, 0x0E, 0x05,
    0x03, 0x00, 0x00, 0x00, 0x80, 0xFC,
    0xF2, 0xE0, 0xC0, 0xC0, 0x88, 0x80,
    0xA0, 0x01, 0x11, 0x41, 0x01, 0x91,
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC,
    0xFF, 0xFF, 0x6F, 0xDF, 0xDF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xFE, 0xFE, 0x3E, 0x0E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x07, 0x0F, 0x0E,
    0x0F, 0x73, 0x7D, 0x7E, 0x5F, 0x7F,
    0x3F, 0x77, 0x5F, 0x7F, 0x3F, 0x6D,
    0x3F, 0x6B, 0x3F, 0x00, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(media_logo, sizeof(media_logo));
}

static void render_wow_logo(void) {
  static const char PROGMEM wow_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x40, 0x20,
    0xE0, 0x00, 0x80, 0x60, 0x20, 0xC0,
    0x20, 0x40, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xDC, 0x3F,
    0x1C, 0x9C, 0x5C, 0x17, 0x35, 0x2C,
    0x38, 0x3C, 0x2A, 0x28, 0xDF, 0x38,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x80, 0x80, 0x80, 0x00, 0x00, 0x18,
    0xF5, 0x96, 0x8C, 0x14, 0x16, 0x9A,
    0x8E, 0xCF, 0x8F, 0xD5, 0x96, 0x2F,
    0xED, 0xC5, 0xC0, 0x80, 0x00, 0x80,
    0x80, 0x80, 0x80, 0x00, 0x00, 0x00,
    0x7F, 0xF1, 0x7C, 0x3E, 0x8F, 0x9F,
    0x3E, 0x77, 0x73, 0x3B, 0x3D, 0x8F,
    0x1E, 0x7F, 0xE7, 0xF0, 0x3D, 0x0F,
    0xC3, 0x87, 0x1F, 0x3F, 0x7C, 0x39,
    0x9F, 0xC7, 0x8D, 0x3D, 0x7B, 0x3F,
    0x00, 0x00, 0xFE, 0x78, 0x9C, 0xCE,
    0xE7, 0xFF, 0xFF, 0x3C, 0x18, 0x7E,
    0xBF, 0xFF, 0xFE, 0xFC, 0xF0, 0xF8,
    0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    0xDC, 0x3F, 0xFF, 0xFD, 0xB3, 0x07,
    0x1E, 0xBF, 0x00, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xEF, 0xF1, 0x78,
    0xFE, 0xFF, 0xFF, 0x7F, 0x3F, 0xDF,
    0xFF, 0x7F, 0xFF, 0xEF, 0x1F, 0x3F,
    0x7D, 0xF3, 0xFF, 0xCE, 0x9C, 0xFB,
    0xFF, 0xFE, 0xFC, 0xF9, 0x00, 0x00,
    0x9F, 0xCF, 0xF7, 0xFF, 0xFF, 0x81,
    0xC0, 0x03, 0x0F, 0x1F, 0x08, 0x00,
    0x00, 0x81, 0xC0, 0xC0, 0xC0, 0x81,
    0x00, 0x00, 0x0C, 0x1F, 0x0F, 0x06,
    0xC1, 0x83, 0xFF, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0xFE, 0xFF, 0xFF, 0xFF,
    0xF3, 0xE1, 0xC0, 0x00, 0x00, 0x00,
    0x3C, 0xFE, 0xFF, 0xE7, 0x83, 0x01,
    0xC3, 0xE7, 0xFF, 0x7E, 0x1C, 0x00,
    0x00, 0x00, 0xC0, 0xF1, 0xFB, 0xFF,
    0xFE, 0xFA, 0x00, 0x00, 0x3F, 0xDF,
    0xFF, 0xFF, 0xF8, 0xF1, 0xE3, 0xC0,
    0xC0, 0xD8, 0xF8, 0xE0, 0x03, 0x07,
    0xFF, 0xFF, 0xFF, 0x03, 0x01, 0xF0,
    0xF8, 0xEC, 0xE0, 0xE0, 0xF1, 0xF8,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0x7F, 0xBF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF7, 0xC7, 0x87,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00,
    0x80, 0xC3, 0x83, 0x3B, 0x1F, 0x7F,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0xFF, 0xEF, 0xF0, 0xFC,
    0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF,
    0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, 0xF0, 0xFD, 0xFF, 0xFF, 0x7F,
    0xFE, 0xF1, 0x00, 0x00, 0xEF, 0x1F,
    0xFF, 0xFF, 0xFF, 0x1F, 0x0B, 0x01,
    0x1E, 0xFF, 0x7F, 0x1F, 0xEF, 0xFF,
    0xFF, 0x3F, 0x05, 0x07, 0xDF, 0xFF,
    0xFF, 0x7F, 0xBF, 0xFF, 0xF7, 0x6F,
    0x1F, 0x3F, 0xFE, 0xF9, 0x00, 0x00,
    0x00, 0x08, 0x07, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x01, 0xFE, 0x10, 0x0C,
    0x87, 0xE3, 0x00, 0x00, 0x32, 0x09,
    0x01, 0xE0, 0x3F, 0x00, 0x03, 0x07,
    0x0C, 0x00, 0x00, 0x03, 0x0F, 0x17,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC7,
    0x28, 0x30, 0x11, 0x20, 0x00, 0x00,
    0x00, 0xC0, 0x21, 0x02, 0xFC, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x3F, 0xC0, 0x00, 0x00, 0x2C,
    0x10, 0x00, 0x00, 0x00, 0x10, 0x18,
    0xE7, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7E, 0x02, 0x19,
    0x28, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x30, 0x5F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(wow_logo, sizeof(wow_logo));
}

static void render_akira_logo(void) {
  static const char PROGMEM akira_logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xC0, 0x00, 0xEC, 0xF6, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFE,
    0xFF, 0xFF, 0xFB, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x74, 0x74,
    0xF4, 0x64, 0x00, 0x00, 0x1C, 0xD0,
    0x14, 0x10, 0x00, 0x00, 0xF8, 0x98,
    0x00, 0x77, 0x77, 0x3D, 0x00, 0x00,
    0x00, 0x97, 0x4F, 0x27, 0x13, 0x01,
    0xBF, 0x9F, 0x00, 0x00, 0x00, 0x00,
    0x33, 0xBB, 0xFB, 0x42, 0x00, 0x00,
    0xCE, 0x7A, 0xCE, 0x02, 0x00, 0x00,
    0xF6, 0x6F, 0x00, 0xCD, 0xCD, 0xF5,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xA5, 0x96, 0x00, 0x00,
    0x00, 0x00, 0xFB, 0x97, 0xD3, 0x42,
    0x00, 0x20, 0x6D, 0x65, 0x6D, 0x21,
    0x00, 0x00, 0x0F, 0x09, 0x60, 0x4F,
    0x4F, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0xFC, 0x00, 0xFF, 0xFF,
    0x00, 0xFC, 0x00, 0x00, 0x06, 0x06,
    0x07, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x60, 0x30,
    0x18, 0xD8, 0xCC, 0xEC, 0xE6, 0x76,
    0x76, 0x32, 0x3B, 0x3B, 0x3B, 0x38,
    0x3F, 0x3F, 0x38, 0x3B, 0x3B, 0x33,
    0x72, 0x76, 0xE6, 0xEC, 0xCC, 0xDC,
    0x98, 0x30, 0x60, 0xE0, 0x00, 0x00,
    0x1E, 0x0F, 0x87, 0xE3, 0xF1, 0xF0,
    0xF8, 0xF8, 0xF8, 0xD8, 0x98, 0x30,
    0x70, 0xE0, 0xC0, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x10, 0x0C, 0x07,
    0x03, 0x01, 0x03, 0x07, 0x1E, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x1F,
    0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0x7E, 0x3C, 0x9D, 0xDF,
    0xCF, 0xEE, 0xA4, 0x30, 0x70, 0xE0,
    0xC0, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x01,
    0x03, 0x07, 0x07, 0x11, 0x3C, 0x7F,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, 0xFF, 0xF9, 0xF3, 0xE7, 0xFE,
    0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3C, 0x78, 0x7C, 0xE6, 0xC1, 0xC0,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x03, 0x07,
    0x07, 0x0F, 0x0F, 0x8F, 0x8F, 0xC7,
    0xC7, 0xE3, 0x70, 0x38, 0xBC, 0x9F,
    0x00, 0x00, 0x03, 0x07, 0x06, 0x0C,
    0x0D, 0x19, 0x1B, 0x1B, 0x33, 0x33,
    0x37, 0x66, 0xE6, 0x06, 0xFE, 0xFE,
    0x06, 0xE6, 0x77, 0x33, 0x33, 0x33,
    0x1B, 0x19, 0x1D, 0x0C, 0x06, 0x07,
    0x03, 0x01, 0x00, 0x00, 0xFC, 0x9E,
    0x52, 0xFE, 0xDA, 0xD2, 0xFC, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x0F, 0x00,
    0x7F, 0xFF, 0x00, 0x0F, 0x00, 0x00,
    0xB0, 0x90, 0xB0, 0x10, 0x00, 0x80,
    0x80, 0x80, 0x00, 0x80, 0x00, 0x00,
    0xFF, 0x25, 0x6B, 0xFF, 0x2D, 0x2F,
    0xFF, 0x00, 0x00, 0x38, 0x71, 0xE2,
    0xC4, 0x00, 0x5B, 0x96, 0x00, 0x00,
    0x00, 0x00, 0xB9, 0xBC, 0xBE, 0x30,
    0x00, 0xFF, 0x7D, 0xFD, 0x00, 0x84,
    0x00, 0x00, 0x7F, 0xAD, 0xE5, 0xFF,
    0xCD, 0xE9, 0x7F, 0x00, 0x00, 0xAE,
    0xFC, 0xF8, 0xF1, 0x00, 0xFE, 0xFE,
    0x00, 0x00, 0x00, 0x00, 0xB1, 0xBC,
    0xBC, 0x80, 0x00, 0x8F, 0xCF, 0xCF,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7D, 0xFF, 0xFF, 0xF7, 0x00,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x0B, 0x0F, 0x0F, 0x00, 0x00, 0x03,
    0x03, 0x03, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x03, 0x00, 0x59, 0x1B, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
    };
  oled_write_raw_P(akira_logo, sizeof(akira_logo));
}

// 5x3 Logos
void render_quas_logo(void) {
    static const char PROGMEM font_logo[16] = {0x80, 0x81, 0x82, 0x83, 0x84, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0};
    oled_write_P(font_logo, false);
}

void render_acr_logo(void) {
    static const char PROGMEM font_acr_logo[16] = {0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0};
    oled_write_P(font_acr_logo, false);
}

void render_media_controls(void) {
    static const char PROGMEM font_media_controls_logo[6] = {0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0};
    oled_write_P(font_media_controls_logo, false);
}

// 5x1 Layer indicator
void render_layer(void) {
    static const char PROGMEM font_layer[7][6] = {
        {0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0}, //base
        {0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0}, //mid
        {0x85, 0x86, 0x87, 0x88, 0x89, 0}, //raise
        {0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0}, //adjust
        {0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0}, //game
        {0xda, 0xdb, 0xdc, 0xdd, 0xde, 0}, //pshop
        {0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0}, //indes
    };
    uint8_t layer = 0;
    if (layer_state_is(_MID)) {
        layer = 1;
    } else if (layer_state_is(_RAISE)) {
        layer = 2;
    } else if (layer_state_is(_ADJUST)) {
        layer = 3;
    } else if (layer_state_is(_GAME)) {
        layer = 4;
    } else if (layer_state_is(_PHOTOSHOP)) {
        layer = 5;
    } else if (layer_state_is(_INDESIGN)) {
        layer = 6;
    }
    oled_write_P(font_layer[layer], false);
}

// 2x1 Audio, clicky and RGB status indicators
void render_audio_status(void) {
    static const char PROGMEM font_audio_on[3] = {0x8f, 0x90, 0};
    static const char PROGMEM font_audio_off[3]  = {0x91, 0x92, 0};
    oled_write_P(mute_enabled ? font_audio_on : font_audio_off, false);
}

void render_clicky_status(void) {
    static const char PROGMEM font_clicky_off[3] = {0xaf, 0xb0, 0};
    static const char PROGMEM font_clicky_on[3]  = {0xb1, 0xb2, 0};
    oled_write_P(encoder_enabled ? font_clicky_on : font_clicky_off, false);
}

#if defined(RGB_MATRIX_ENABLE) || defined(RGBLIGHT_ENABLE)
void render_rgb_status(void) {
    static const char PROGMEM font_rgb_off[3] = {0xcf, 0xd0, 0};
    static const char PROGMEM font_rgb_on[3]  = {0xd1, 0xd2, 0};
#ifdef RGBLIGHT_ENABLE
    bool rgb_enabled = rgblight_config.enable;
#elif RGB_MATRIX_ENABLE
    bool rgb_enabled = rgb_matrix_config.enable;
#endif
    oled_write_P(rgb_enabled ? font_rgb_on : font_rgb_off, false);
}
#endif

void render_feature_status(void) {
    render_audio_status();
    oled_write_P(PSTR(" "), false);
    #if defined(RGB_MATRIX_ENABLE) || defined(RGBLIGHT_ENABLE)
        render_rgb_status();
    #endif
}

// 2x1 Ctrl, Alt, Shift, GUI, Mouse
void render_mod_ctrl(void) {
    static const char PROGMEM font_ctrl[3] = {0x93, 0x94, 0};
    oled_write_P(font_ctrl, false);
}

void render_mod_alt(void) {
    static const char PROGMEM font_alt[3] = {0x95, 0x96, 0};
    oled_write_P(font_alt, false);
}

void render_mod_shift(void) {
    static const char PROGMEM font_shift[3] = {0xd3, 0xd4, 0};
    oled_write_P(font_shift, false);
}

void render_mod_gui(void) {
    static const char PROGMEM font_gui[3] = {0xb3, 0xb4, 0};
    oled_write_P(font_gui, false);
}

#ifdef MOUSEKEY_ENABLE
void render_mod_mouse(void) {
    static const char PROGMEM font_mouse[3] = {0x97, 0x98, 0};
    oled_write_P(font_mouse, false);
}
#endif

// 5x2 Mod and feature indicator clusters
void render_mod_status(void) {
#ifdef NO_ACTION_ONESHOT
    uint8_t modifiers = get_mods();
#else
    uint8_t modifiers = get_mods() | get_oneshot_mods();
#endif
    (modifiers & MOD_MASK_CTRL) ? render_mod_ctrl() : oled_write_P(PSTR("  "), false);
    oled_write_P(PSTR(" "), false);
    (modifiers & MOD_MASK_ALT) ? render_mod_alt() : oled_write_P(PSTR("  "), false);
    oled_write_ln_P(PSTR(""), false);

    (modifiers & MOD_MASK_SHIFT) ? render_mod_shift() : oled_write_P(PSTR("  "), false);
    oled_write_P(PSTR(" "), false);
    (modifiers & MOD_MASK_GUI) ? render_mod_gui() : oled_write_P(PSTR("  "), false);
    oled_write_ln("", false);
    oled_write_ln_P(PSTR(" "), false);
#ifdef MOUSEKEY_ENABLE
    render_mod_mouse();
    oled_write_P(PSTR(" "), false);
    render_clicky_status();
    oled_write_ln_P(PSTR(" "), false);
    oled_write_P(PSTR("_____"), false);
#else
    oled_write_P(PSTR("   "), false);
    render_clicky_status();
    oled_write_ln_P(PSTR(" "), false);
    oled_write_P(PSTR("_____"), false);
#endif
}

// title rendering
void render_prompt(void) {
    bool blink = (timer_read() % 1000) < 500;

    if (layer_state_is(_BASE)) {
        oled_write_ln_P(blink ? PSTR("> lo_") : PSTR("> lo "), false);
    } else if (layer_state_is(_MID)) {
        oled_write_ln_P(blink ? PSTR("> hi_") : PSTR("> hi "), false);
    } else if (layer_state_is(_ADJUST)) {
        oled_write_ln_P(blink ? PSTR("> mo_") : PSTR("> mo "), false);
    } else if (layer_state_is(_GAME)) {
        oled_write_ln_P(blink ? PSTR("> gg_") : PSTR("> gg "), false);
    } else if (layer_state_is(_PHOTOSHOP)) {
        oled_write_ln_P(blink ? PSTR("> ps_") : PSTR("> ps "), false);
    } else if (layer_state_is(_INDESIGN)) {
        oled_write_ln_P(blink ? PSTR("> in_") : PSTR("> in "), false);
    } else {
        oled_write_ln_P(blink ? PSTR("> _  ") : PSTR(">    "), false);
    }
}

void render_status_main(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_ln_P(PSTR(" "), false);
    render_keylogger_status();
    oled_write_ln_P(PSTR(" "), false);
    render_keylock_status(host_keyboard_leds());
    render_acr_logo();
}

void render_status_secondary(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    render_keylock_status(host_keyboard_leds());
    render_acr_logo();
}

void render_status_adjust(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_P(PSTR("_____"), false);
    oled_write_ln_P(PSTR(" "), false);
    render_mod_status();
    oled_write_ln_P(PSTR(" "), false);
    render_feature_status();
    #if defined(RGB_MATRIX_ENABLE) || defined(RGBLIGHT_ENABLE) || defined(AUDIO_ENABLE)
        layer_state_is(_ADJUST);
    #else
        render_mod_status();
    #endif
}

void render_status_media(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_ln_P(PSTR(" "), false);
    oled_write_P(PSTR("bully"), false);
    oled_write_P(PSTR("quasi"), false);
    oled_write_P(PSTR("_____"), false);
    oled_write_ln_P(PSTR(" "), false);
    render_media_controls();
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    oled_write_ln_P(PSTR(" "), false);
    render_quas_logo();
}

void render_status_games(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_P(PSTR("_____"), false);
    oled_write_ln("", false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    oled_write_ln_P(PSTR("m i h"), false);
    oled_write_ln_P(PSTR("j n x"), false);
    render_mod_status_text(get_mods()|get_oneshot_mods());
}

void render_status_pshop(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_P(PSTR("_____"), false);
    oled_write_ln("", false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    render_mod_status_text(get_mods()|get_oneshot_mods());
}

void render_status_indesign(void) {
    render_layer();
    oled_write_ln("", false);
    render_prompt();
    oled_write_P(PSTR("_____"), false);
    oled_write_ln("", false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    oled_write_ln_P(PSTR("_ _ _"), false);
    render_mod_status_text(get_mods()|get_oneshot_mods());
}

void oled_task_user(void) {
    // sleep if it has been long enough since we last got a char
    if (timer_elapsed32(oled_timer) > SLEEP_TIMEOUT) {
        oled_off();
        return;
    } else {
        oled_on();
    }

    // show screensaver after a few seconds
    if (timer_elapsed32(oled_timer) > SCREEN_TIMEOUT) {
        switch (get_highest_layer(layer_state)) {
            case _INDESIGN:
                render_akira_logo();
                break;
            case _PHOTOSHOP:
                render_akira_logo();
                break;
            case _GAME:
                render_wow_logo();
                break;
            case _ADJUST:
                render_dead_logo();
                break;
            case _RAISE:
                render_media_logo();
                break;
            case _MID:
                render_acrnym_logo();
                break;
            default:
                oled_scroll_left();
                render_statement_logo();
                break;
        }
        return;
    } else {
        switch (get_highest_layer(layer_state)) {
            case _SNAKE:
                break;
            case _INDESIGN:
                render_status_indesign();
                break;
            case _PHOTOSHOP:
                render_status_pshop();
                break;
            case _GAME:
                render_status_games();
                break;
            case _ADJUST:
                render_status_adjust();
                break;
            case _RAISE:
                render_status_media();
                break;
            case _MID:
                render_status_secondary();
                break;
            default:
                render_status_main();
                break;
        }
        oled_scroll_off();
        update_log();
    }
};
