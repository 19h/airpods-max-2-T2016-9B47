# Apple b515d Bluetooth / ACI Controller Firmware (acib) — C11 Reconstruction
#
# Freestanding ARMv7E-M (soft-float) cross-build + native host verification.

BUILD      ?= build
RAW_IMAGE  ?= $(abspath $(CURDIR)/../../../acib)

TOOLCHAIN_BIN := /Users/int/local/bin
export PATH := $(TOOLCHAIN_BIN):$(PATH)

ARM_CC   ?= $(shell command -v clang)
ARM_LD   ?= $(shell command -v ld.lld)
ARM_AR   ?= $(shell command -v llvm-ar)
HOST_CC  ?= cc

PYTHON ?= $(shell \
	for p in "$(SEMANTIC_PYTHON)" /tmp/acib-venv/bin/python /tmp/rkof-venv/bin/python /tmp/phyb-verify-venv/bin/python python3; do \
		if [ -n "$$p" ] && command -v "$$p" >/dev/null 2>&1 && "$$p" -c "import unicorn, capstone, elftools" >/dev/null 2>&1; then \
			echo "$$p"; break; \
		fi; \
	done)
ifeq ($(strip $(PYTHON)),)
PYTHON := python3
endif

WARN_FLAGS := \
	-std=c11 \
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
	-mfloat-abi=soft \
	-mthumb

ARM_CFLAGS := \
	$(ARM_ARCH_FLAGS) \
	$(WARN_FLAGS) \
	-Oz \
	-ffreestanding \
	-fno-builtin \
	-fno-common \
	-fno-stack-protector \
	-fdata-sections \
	-ffunction-sections \
	-Iinclude

HOST_CFLAGS := \
	$(WARN_FLAGS) \
	-O2 \
	-g \
	-fno-common \
	-DACIB_HOST_TEST=1 \
	-Iinclude \
	-Itests

RECONSTRUCTED_SRCS := \
	src/vectors.c \
	src/rtkit/image.c \
	src/rtkit/boot.c \
	src/rtkit/exception.c \
	src/rtkit/irq.c \
	src/libc/string.c \
	src/phy/phy_ipc.c \
	src/aci/trace.c \
	src/aci/osa.c \
	src/aci/bt_ctrl.c

ARM_ONLY_SRCS := \
	src/pending_links.c

HOST_TEST_SRCS := \
	src/rtkit/boot.c \
	src/rtkit/exception.c \
	src/rtkit/irq.c \
	src/libc/string.c \
	src/phy/phy_ipc.c \
	src/aci/trace.c \
	src/aci/osa.c \
	src/aci/bt_ctrl.c \
	tests/host_bus.c \
	tests/test_main.c

ARM_OBJS  := $(patsubst src/%.c,$(BUILD)/arm/%.o,$(RECONSTRUCTED_SRCS) $(ARM_ONLY_SRCS)) $(BUILD)/arm/image_blobs.o
HOST_OBJS := $(patsubst %.c,$(BUILD)/host/%.o,$(HOST_TEST_SRCS))

ARM_ELF  := $(BUILD)/acib.elf
ARM_MAP  := $(BUILD)/acib.map
ARM_LIB  := $(BUILD)/libacib_reconstructed.a
HOST_BIN := $(BUILD)/acib_tests

BLOBS_STAMP := $(BUILD)/blobs/.stamp

.PHONY: all arm host check verify verify-semantics clean

all: arm host

arm: $(ARM_ELF) $(ARM_LIB) $(BUILD)/arm/tests_layout.o

host: $(HOST_BIN)

$(BLOBS_STAMP): $(RAW_IMAGE) tools/extract_blobs.py
	@mkdir -p $(BUILD)/blobs
	$(PYTHON) tools/extract_blobs.py --raw "$(RAW_IMAGE)" --out-dir "$(BUILD)/blobs"
	@touch $@

$(BUILD)/arm/image_blobs.o: src/rtkit/image_blobs.S $(BLOBS_STAMP)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_ARCH_FLAGS) \
		-DBLOB_RTKIT_HDR=\"$(abspath $(BUILD)/blobs/rtkit_header.bin)\" \
		-DBLOB_SEC_VECTORS=\"$(abspath $(BUILD)/blobs/secondary_vectors.bin)\" \
		-DBLOB_ROM_RODATA=\"$(abspath $(BUILD)/blobs/rom_rodata.bin)\" \
		-DBLOB_RAM_DATA=\"$(abspath $(BUILD)/blobs/ram_data.bin)\" \
		-c $< -o $@

$(BUILD)/arm/%.o: src/%.c $(BLOBS_STAMP)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(BUILD)/arm/tests_layout.o: tests/layout.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(ARM_LIB): $(ARM_OBJS)
	@rm -f $@
	$(ARM_AR) rcs $@ $(ARM_OBJS)

$(ARM_ELF): $(ARM_OBJS) link/acib.ld
	$(ARM_LD) -T link/acib.ld -Map=$(ARM_MAP) -o $@ $(ARM_OBJS)

$(BUILD)/host/%.o: %.c $(BLOBS_STAMP)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -DACIB_BLOBS_DIR=\"$(abspath $(BUILD)/blobs)\" -c $< -o $@

$(BUILD)/host/tests_layout.o: tests/layout.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_BIN): $(HOST_OBJS) $(BUILD)/host/tests_layout.o
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_OBJS)

check: $(BUILD)/arm/tests_layout.o $(BUILD)/host/tests_layout.o $(HOST_BIN)
	$(HOST_BIN)

verify: arm
	$(PYTHON) tools/verify_fidelity.py \
		--src-dir src \
		--include-dir include \
		--inventory verification/inventory.json \
		--ledger verification/ledger.json
	$(PYTHON) tools/verify_layout.py \
		--elf $(ARM_ELF) \
		--map $(ARM_MAP) \
		--inventory verification/inventory.json

verify-semantics: arm
	$(PYTHON) tools/verify_load_image.py \
		--elf $(ARM_ELF) \
		--raw "$(RAW_IMAGE)" \
		--output $(BUILD)/load-coverage.json
	$(PYTHON) tools/arm_semantics.py \
		--elf $(ARM_ELF) \
		--raw "$(RAW_IMAGE)" \
		--ledger verification/ledger.json \
		--output $(BUILD)/arm-semantics.json

clean:
	rm -rf $(BUILD)
