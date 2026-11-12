SDK_DEMO_PATH ?= $(abspath .)
BL_SDK_BASE ?= $(abspath  ./../../../..)

export BL_SDK_BASE

CROSS_COMPILE ?= riscv64-unknown-elf-


ifneq ($(OTA_VERSION),)
cmake_definition += -DOTA_VERSION=$(OTA_VERSION)
endif

include $(BL_SDK_BASE)/project.build
