# Makefile for rkof (Main RTKit Firmware — Apple T2016 Rev B0 / b515d)

CLANG ?= /Users/int/local/bin/clang
LLD ?= /Users/int/local/bin/ld.lld
OBJDUMP ?= /Users/int/local/bin/llvm-objdump
NM ?= /Users/int/local/bin/llvm-nm
HOST_CC ?= /usr/bin/cc
PYTHON ?= python3

ifneq ($(wildcard /tmp/rkof-venv/bin/python),)
SEMANTIC_PYTHON ?= /tmp/rkof-venv/bin/python
else
SEMANTIC_PYTHON ?= $(PYTHON)
endif

BUILD ?= build
RAW_IMAGE ?= ../../../rkof

WARN_FLAGS := \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wpointer-arith \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Werror

ARM_ARCH_FLAGS := \
	-target armv7em-none-eabi \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16

ARM_CFLAGS := \
	$(ARM_ARCH_FLAGS) \
	-std=c11 \
	-Os \
	-ffreestanding \
	-fno-builtin \
	-fno-common \
	-fno-stack-protector \
	-fomit-frame-pointer \
	-fdata-sections \
	-ffunction-sections \
	-MMD -MP \
	-Iinclude \
	$(WARN_FLAGS)

ARM_ASFLAGS := \
	$(ARM_ARCH_FLAGS) \
	-Iinclude

ARM_LDFLAGS := \
	-nostdlib \
	-Wl,-T,link/rkof.ld \
	-Wl,-Map,$(BUILD)/rkof.map \
	-Wl,--no-gc-sections \
	-fuse-ld=$(LLD)

HOST_CFLAGS := \
	-std=c11 \
	-O2 \
	-g \
	-DRKOF_HOST_BUILD=1 \
	-fno-builtin \
	-fno-common \
	-MMD -MP \
	-Iinclude \
	-Itests \
	$(WARN_FLAGS)

P2_C_SRCS := $(sort $(wildcard src/*/p2_*.c))

C_SRCS := \
	src/rtkit/vectors.c \
	src/rtkit/boot.c \
	src/rtkit/exception.c \
	src/rtkit/irq.c \
	src/rtkit/sched.c \
	src/rtkit/systick.c \
	src/rtkit/services.c \
	src/libc/string.c \
	src/libc/aeabi.c \
	src/coproc/supervisor.c \
	src/phy/modem_inits.c \
	src/phy/scanmedia.c \
	src/phy/wnvv.c \
	src/phy/bt_phy.c \
	$(P2_C_SRCS)

ASM_SRCS := \
	src/rtkit/binary_blobs.S \
	src/rtkit/entry_veneers.S \
	src/rtkit/pending_boundary.S

ARM_C_OBJS := $(patsubst src/%.c,$(BUILD)/arm/%.o,$(C_SRCS))
ARM_ASM_OBJS := $(patsubst src/%.S,$(BUILD)/arm/%.o,$(ASM_SRCS))
ARM_OBJS := $(ARM_C_OBJS) $(ARM_ASM_OBJS)

HOST_C_OBJS := $(patsubst src/%.c,$(BUILD)/host/%.o,$(C_SRCS))
HOST_TEST_SRCS := \
	tests/layout.c \
	tests/host_bus.c \
	tests/test_main.c
HOST_TEST_OBJS := $(patsubst tests/%.c,$(BUILD)/host/tests/%.o,$(HOST_TEST_SRCS))

ARM_ELF := $(BUILD)/rkof.elf
HOST_BIN := $(BUILD)/rkof_tests

.PHONY: all arm host check verify verify-semantics clean

all: arm host

arm: $(ARM_ELF) $(BUILD)/arm/layout.o

host: $(HOST_BIN)

$(BUILD)/arm/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CLANG) $(ARM_CFLAGS) -c $< -o $@

$(BUILD)/arm/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CLANG) $(ARM_ASFLAGS) -c $< -o $@

$(BUILD)/arm/layout.o: tests/layout.c
	@mkdir -p $(dir $@)
	$(CLANG) $(ARM_CFLAGS) -c $< -o $@

$(ARM_ELF): $(ARM_OBJS) link/rkof.ld
	@mkdir -p $(dir $@)
	$(CLANG) $(ARM_ARCH_FLAGS) $(ARM_LDFLAGS) $(ARM_OBJS) -o $@

$(BUILD)/host/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD)/host/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_BIN): $(HOST_C_OBJS) $(HOST_TEST_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_C_OBJS) $(HOST_TEST_OBJS) -o $@

check: $(BUILD)/arm/layout.o $(HOST_BIN)
	$(HOST_BIN) $(RAW_IMAGE)

verify: $(ARM_ELF)
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/verify_fidelity.py \
		--src src \
		--inventory verification/inventory.json \
		--ledger verification/ledger.json
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/verify_layout.py \
		--memmap include/rkof/memmap.h \
		--map $(BUILD)/rkof.map

verify-semantics: $(ARM_ELF)
	PYTHONDONTWRITEBYTECODE=1 $(SEMANTIC_PYTHON) tools/verify_load_image.py \
		--elf $(ARM_ELF) \
		--reference $(RAW_IMAGE) \
		--output $(BUILD)/load-image.json
	PYTHONDONTWRITEBYTECODE=1 $(SEMANTIC_PYTHON) tools/arm_semantics.py \
		--elf $(ARM_ELF) \
		--reference $(RAW_IMAGE) \
		--output $(BUILD)/arm-semantics.json
	PYTHONDONTWRITEBYTECODE=1 $(SEMANTIC_PYTHON) tools/verify_claims.py \
		--elf $(ARM_ELF) \
		--reference $(RAW_IMAGE) \
		--ledger verification/ledger.json \
		--semantics $(BUILD)/arm-semantics.json \
		--load-image $(BUILD)/load-image.json \
		--pending-asm src/rtkit/pending_boundary.S

clean:
	rm -rf $(BUILD)

-include $(ARM_C_OBJS:.o=.d) $(HOST_C_OBJS:.o=.d) $(HOST_TEST_OBJS:.o=.d) $(BUILD)/arm/layout.d
