
TARGET = DaisyMultiFX

LIBDAISY_DIR ?= ../lib/libDaisy
DAISYSP_DIR  ?= ../lib/DaisySP

# Daisy apps typically run from QSPI under the Daisy bootloader.
# BOOT_NONE uses the internal flash script (128KB) which is too small.
APP_TYPE ?= BOOT_QSPI

# Build-time debug: compile a pure passthrough patch at boot.
# Usage: `make clean && make -j DEBUG_PATCH=1 && make flash DEBUG_PATCH=1`
DEBUG_PATCH ?= 0
ifeq ($(DEBUG_PATCH),1)
C_DEFS += -DDEBUG_PATCH_PASSTHROUGH
endif

# Build-time debug: compile with Neural Amp only patch at boot.
# Usage: `make clean && make -j DEBUG_NEURALAMP=1 && make flash`
DEBUG_NEURALAMP ?= 0
ifeq ($(DEBUG_NEURALAMP),1)
C_DEFS += -DDEBUG_PATCH_NEURALAMP
endif

CPP_SOURCES = src/main.cpp \
              ../core/audio/audio_processor.cpp \
              $(wildcard src/effects/*.cpp) \
              $(wildcard src/midi/*.cpp) \
              $(wildcard src/patch/*.cpp) \
              src/audio/tempo_control.cpp
C_INCLUDES += -Isrc -I../core

# Define DAISY_SEED_BUILD for platform-specific code paths in core/
C_DEFS += -DDAISY_SEED_BUILD

# Use standard libDaisy linker script (no ITCMRAM for now)
# LDSCRIPT = STM32H750IB_custom.lds

# Aggressive optimizations for real-time audio DSP on BOOT_QSPI.
# -O3: Maximum optimization (inlining, loop opts) - important because code
#      runs from QSPI flash; well-optimized code runs faster even with cache.
# -ffast-math: Unsafe FP optimizations (no NaN/Inf checks, reordering)
# -funroll-loops: Unroll small loops for speed (critical for DSP tight loops)
# Note: Mars uses -Os but runs BOOT_SRAM (zero-wait-state). BOOT_QSPI needs -O3.
OPT = -O3 -ffast-math -funroll-loops
CFLAGS   += $(OPT)
CXXFLAGS += $(OPT)

# Use C++17 for better constexpr and if-constexpr support
CPP_STANDARD = -std=gnu++17

# Core location, and generic Makefile for project builds
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Link-time optimization (whole program optimization)
LDFLAGS += -flto

.PHONY: flash program dfu-list clean_all
# Ensure we rebuild before flashing (libDaisy's `program-dfu` target does not depend on the binary).
flash: $(BUILD_DIR)/$(TARGET_BIN) program-dfu
program: program-dfu
dfu-list:
	dfu-util -l
clean_all: clean
	rm -f $(TARGET).bin $(TARGET).elf
