# MCU name
MCU = GD32F303  

# Bootloader selection
BOOTLOADER = ignition

# Build Options
#   change yes to no to disable
#
BOOTMAGIC_ENABLE = yes      # Enable Bootmagic Lite
MOUSEKEY_ENABLE = yes       # Mouse keys
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = no        # Console for debug
COMMAND_ENABLE = no        # Commands for debug and configuration
NKRO_ENABLE = yes           # Enable N-Key Rollover
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
RGBLIGHT_ENABLE = no        # Enable keyboard RGB underglow
AUDIO_ENABLE = no          # Audio output
CUSTOM_MATRIX = lite
SWAP_HANDS_ENABLE = yes
RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = IS31FL3731
EEPROM_DRIVER = vendor
MOUSE_SHARED_EP = no
LTO_ENABLE = no
DFU_SUFFIX_ARGS = -v 3297 -p 1791

#project specific files
SRC += matrix.c
QUANTUM_LIB_SRC += i2c_master.c

MOUSE_SHARED_EP = no