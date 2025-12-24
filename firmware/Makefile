
TARGET = DaisyMultiFX

LIBDAISY_DIR ?= ../lib/libDaisy
DAISYSP_DIR  ?= ../lib/DaisySP

# Daisy apps typically run from QSPI under the Daisy bootloader.
# BOOT_NONE uses the internal flash script (128KB) which is too small.
APP_TYPE ?= BOOT_QSPI

CPP_SOURCES = $(wildcard src/*.cpp)
C_INCLUDES += -Isrc
CFLAGS   += -O2
CXXFLAGS += -O2

# Core location, and generic Makefile for project builds
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

.PHONY: flash program clean_all
flash: program-dfu
program: program-dfu
clean_all: clean
	rm -f $(TARGET).bin $(TARGET).elf
