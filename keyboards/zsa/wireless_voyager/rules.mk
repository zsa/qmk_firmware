MCU_LDSCRIPT = voyager2

CUSTOM_MATRIX = lite
PROGRAM_CMD = $(call EXEC_DFU)
DFU_ARGS = -d 3297:0792 -a 0 -s 0x08004000:leave
DFU_SUFFIX_ARGS = -v 3297 -p 0792

VPATH += drivers/gpio
SRC += matrix.c mcp23018.c
I2C_DRIVER_REQUIRED = yes
