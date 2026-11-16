# Freestanding ARMv7E-M + Host C11 Reconstruction Makefile for `rkos`
# Target: Apple Silicon T2016 Rev B0 (`b515d` / `AirPods3,6`) Stage-1 RTKit OS (`RTKit-3514.0.15`)

BUILD ?= build
ifneq ($(wildcard ../../../rkos),)
  RAW_FW ?= $(abspath ../../../rkos)
else
  RAW_FW ?= $(abspath ../../rkos)
endif

ifneq ($(wildcard /Users/int/local/bin/clang),)
  ARM_CC ?= /Users/int/local/bin/clang
  export PATH := /Users/int/local/bin:$(PATH)
else
  ARM_CC ?= clang
endif

HOST_CC ?= cc
PYTHON ?= python3

ifneq ($(wildcard /tmp/rkos-venv/bin/python),)
  DEFAULT_SEMANTIC_PYTHON := /tmp/rkos-venv/bin/python
else ifneq ($(wildcard /tmp/phyb-verify-venv/bin/python),)
  DEFAULT_SEMANTIC_PYTHON := /tmp/phyb-verify-venv/bin/python
else
  DEFAULT_SEMANTIC_PYTHON := $(PYTHON)
endif
SEMANTIC_PYTHON ?= $(DEFAULT_SEMANTIC_PYTHON)

WARN_FLAGS := \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wpointer-arith \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Werror

ARM_CFLAGS := \
	-target armv7em-none-eabi \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-std=c11 \
	-Oz \
	-ffreestanding \
	-fno-builtin \
	-fno-common \
	-fno-stack-protector \
	-fdata-sections \
	-ffunction-sections \
	-Iinclude \
	-Isrc \
	$(WARN_FLAGS)

ARM_LDFLAGS := \
	-target armv7em-none-eabi \
	-fuse-ld=lld \
	-nostdlib \
	-Wl,-T,link/rkos.ld \
	-Wl,-Map,$(BUILD)/rkos.map

HOST_CFLAGS := \
	-std=c11 \
	-O2 \
	-g \
	-DRKOS_HOST=1 \
	-fno-common \
	-Iinclude \
	-Isrc \
	$(WARN_FLAGS)

ARM_SRCS := \
	src/vectors.c \
	src/image.c \
	src/state.c \
	src/shims/arch_entry.c \
	src/rtkit/vtable_dispatch.c \
	src/libc/string.c \
	src/libc/printf_stdlib.c \
	src/img4/img4_der.c \
	src/crypto/aes_ccn_drbg.c \
	src/kernel/heap.c \
	src/kernel/crypto_der.c \
	src/kernel/sched_sync.c \
	src/rtkit/irq.c \
	src/rtkit/boot.c \
	src/platform/ftab_crashlog.c

HOST_SRCS := \
	$(ARM_SRCS) \
	tests/layout.c \
	tests/host_bus.c \
	tests/test_main.c

ARM_OBJS := $(patsubst %.c,$(BUILD)/arm/%.o,$(ARM_SRCS))
HOST_OBJS := $(patsubst %.c,$(BUILD)/host/%.o,$(HOST_SRCS))

ELF := $(BUILD)/rkos.elf
MAP := $(BUILD)/rkos.map
HOST_BIN := $(BUILD)/test_rkos

HEADERS := \
	include/rkos/memmap.h \
	include/rkos/regs.h \
	include/rkos/shared.h \
	include/rkos/rkos.h \
	src/image_rtkit_hdr.inc \
	src/image_rodata.inc \
	src/image_data.inc

.PHONY: all arm host check verify verify-semantics clean

all: arm host

arm: $(ELF)

host: $(HOST_BIN)

$(BUILD)/arm/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(ELF): $(ARM_OBJS) link/rkos.ld
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_LDFLAGS) $(ARM_OBJS) -o $@

$(BUILD)/host/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_BIN): $(HOST_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_OBJS) -o $@

check: $(HOST_BIN)
	$(HOST_BIN)

verify: $(ELF)
	@mkdir -p $(BUILD)/reports
	$(PYTHON) tools/verify_fidelity.py --root . --output $(BUILD)/reports/fidelity-report.json
	$(PYTHON) tools/verify_layout.py --map $(MAP) --elf $(ELF) --output $(BUILD)/reports/layout-report.json

verify-semantics: $(ELF)
	$(SEMANTIC_PYTHON) tools/verify_load_image.py --elf $(ELF) --reference $(RAW_FW) --output $(BUILD)/load-coverage.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(ELF) --reference $(RAW_FW) --output $(BUILD)/arm-semantics.json
	PYTHONDONTWRITEBYTECODE=1 $(SEMANTIC_PYTHON) tools/verify_claims.py --root . --semantics $(BUILD)/arm-semantics.json --elf $(ELF) --reference $(RAW_FW) --output $(BUILD)/reports/claims-report.json

clean:
	rm -rf $(BUILD) tools/__pycache__
