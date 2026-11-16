# Makefile — Freestanding ARMv7E-M + Native Host C11 build & verification
# harness for targets/aopf/ (Always-On Processor Firmware).

BUILD ?= build

ARM_CC ?= $(shell if [ -x /Users/int/local/bin/clang ]; then echo /Users/int/local/bin/clang; else echo clang; fi)
HOST_CC ?= cc
PYTHON ?= $(if $(SEMANTIC_PYTHON),$(SEMANTIC_PYTHON),$(shell if [ -x /tmp/aopf-venv/bin/python ]; then echo /tmp/aopf-venv/bin/python; else echo python3; fi))
RAW_IMAGE ?= $(shell if [ -f ../../../aopf ]; then echo ../../../aopf; elif [ -f ../../aopf ]; then echo ../../aopf; else echo ../aopf; fi)

WARN_FLAGS := -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Werror

ARM_CFLAGS := \
	-target armv7em-none-eabi \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-mthumb \
	-Oz \
	-std=c11 \
	-ffreestanding \
	-fno-builtin \
	-fno-common \
	-fno-stack-protector \
	-fdata-sections \
	-ffunction-sections \
	-Iinclude \
	$(WARN_FLAGS)

ARM_LDFLAGS := \
	-target armv7em-none-eabi \
	-fuse-ld=lld \
	-nostdlib \
	-Wl,-T,link/aopf.ld \
	-Wl,-Map,$(BUILD)/aopf.map

HOST_CFLAGS := \
	-O2 \
	-std=c11 \
	-fno-common \
	-Iinclude \
	$(WARN_FLAGS)

ARM_SRCS := \
	src/vectors.c \
	src/libc/string.c \
	src/libc/printf.c \
	src/libc/cxxabi.c \
	src/rtkit/image_blobs.c \
	src/rtkit/boot.c \
	src/rtkit/exception.c \
	src/rtkit/irq.c \
	src/rtkit/threading.c \
	src/rtkit/ipc.c \
	src/rtkit/vt_trampolines.c \
	src/drivers/systick.c \
	src/drivers/timers.c \
	src/drivers/platform.c \
	src/aop/app.c

HOST_LIB_SRCS := \
	src/vectors.c \
	src/libc/string.c \
	src/libc/printf.c \
	src/libc/cxxabi.c \
	src/rtkit/image_blobs.c \
	src/rtkit/boot.c \
	src/rtkit/exception.c \
	src/rtkit/irq.c \
	src/rtkit/threading.c \
	src/rtkit/ipc.c \
	src/drivers/systick.c \
	src/drivers/timers.c \
	src/drivers/platform.c \
	src/aop/app.c

HOST_TEST_SRCS := \
	tests/host_bus.c \
	tests/layout.c \
	tests/test_main.c

ARM_OBJS := $(patsubst src/%.c,$(BUILD)/arm/%.o,$(ARM_SRCS))
HOST_OBJS := $(patsubst src/%.c,$(BUILD)/host/src/%.o,$(HOST_LIB_SRCS)) \
             $(patsubst tests/%.c,$(BUILD)/host/tests/%.o,$(HOST_TEST_SRCS))

ELF := $(BUILD)/aopf.elf
HOST_BIN := $(BUILD)/host_tests
LAYOUT_ARM_OBJ := $(BUILD)/arm/tests/layout.o

.PHONY: all arm host check verify verify-semantics clean

all: arm host

arm: $(ELF) $(LAYOUT_ARM_OBJ)

host: $(HOST_BIN)

HDRS := $(wildcard include/aopf/*.h)

$(BUILD)/arm/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(LAYOUT_ARM_OBJ): tests/layout.c $(HDRS)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(ELF): $(ARM_OBJS) link/aopf.ld
	@mkdir -p $(dir $@)
	$(ARM_CC) $(ARM_LDFLAGS) $(ARM_OBJS) -o $@

$(BUILD)/host/src/%.o: src/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD)/host/tests/%.o: tests/%.c $(HDRS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(HOST_BIN): $(HOST_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_OBJS) -o $@

check: $(LAYOUT_ARM_OBJ) $(HOST_BIN)
	$(HOST_BIN)

verify: $(ELF)
	$(PYTHON) tools/verify_fidelity.py --target-dir . --elf $(ELF)
	$(PYTHON) tools/verify_layout.py --target-dir . --elf $(ELF) --map $(BUILD)/aopf.map

verify-semantics: $(ELF)
	$(PYTHON) tools/verify_load_image.py --target-dir . --elf $(ELF) --raw $(RAW_IMAGE) --report $(BUILD)/load-coverage.json
	$(PYTHON) tools/arm_semantics.py --target-dir . --elf $(ELF) --raw $(RAW_IMAGE) --report $(BUILD)/arm-semantics.json
	$(PYTHON) tools/verify_claims.py --target-dir . --elf $(ELF) --raw $(RAW_IMAGE) --semantics-report $(BUILD)/arm-semantics.json

clean:
	rm -rf $(BUILD)
