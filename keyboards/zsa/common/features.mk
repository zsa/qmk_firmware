VPATH += keyboards/zsa/common

ifeq ($(strip $(ORYX_ENABLE)), yes)
    SRC += keyboards/zsa/common/oryx.c
    OPT_DEFS += -DORYX_ENABLE -DORYX_CONFIGURATOR
    POST_CONFIG_H += keyboards/zsa/common/config.h
    RAW_ENABLE := yes
    VIA_ENABLE := no
    RGB_MATRIX_CUSTOM_KB = yes
else
endif
