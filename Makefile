# Makefile -- Apple W3 Bluetooth PHY firmware (phyb) reconstruction
#
#   make arm      freestanding ARMv7E-M objects + linked ELF (build/phyb.elf)
#   make host     native regression binary (build/phyb_tests)
#   make check    compile-time layout proof + behavioural tests
#   make verify   layout of the linked image + provenance audit

CLANG        ?= clang
HOST_CC      ?= /usr/bin/cc
SEMANTIC_PYTHON ?= python3
SYMBOLIC_PYTHON ?= python3
TARGET       ?= armv7em-none-eabi
TARGET_FLAGS := -target $(TARGET) -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16

BUILD        := build
SRC          := $(sort $(wildcard src/w3/*.c src/rtkit/*.c src/libc/*.c))
OBJ_ARM      := $(patsubst src/%.c,$(BUILD)/arm/%.o,$(SRC))
OBJ_HOST     := $(patsubst src/%.c,$(BUILD)/host/%.o,$(SRC))
HOST_SRC     := tests/host_bus.c tests/host_image.c tests/test_main.c
OBJ_HOST     += $(patsubst tests/%.c,$(BUILD)/host/tests/%.o,$(HOST_SRC))

COMMON       := -std=c11 -ffreestanding -fno-builtin -fno-common -Iinclude -MMD -MP
WARN         := -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes \
                -Wmissing-prototypes -Wno-unused-parameter
ARM_CFLAGS   := $(COMMON) $(TARGET_FLAGS) -Oz -g -ffunction-sections -fdata-sections \
                $(WARN) -Werror
# The image defines its own __stack_chk_guard; keep the host compiler's stack
# protector out of the way of that symbol.
HOST_CFLAGS  := -std=c11 -fno-builtin -fno-common -fno-stack-protector -Iinclude -MMD -MP \
                -include tests/host_compat.h -DBTP_HOST -O1 -g $(WARN) -Werror \
                -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

LDFLAGS_ARM  := -target $(TARGET) -nostdlib -fuse-ld=lld -Wl,-T,link/phyb.ld \
                -Wl,--gc-sections -Wl,-Map=$(BUILD)/phyb.map

SUBTARGETS      := rkos aopf acib rkof
SUBTARGET_BUILD  = $(abspath $(BUILD))

.PHONY: all arm host check verify verify-layout verify-fidelity verify-archive verify-semantics verify-rom-observability verify-symbolic \
        $(SUBTARGETS) targets-all targets-check targets-verify targets-semantics verify-all clean

all: arm host

arm: $(BUILD)/phyb.elf

$(BUILD)/phyb.elf: $(OBJ_ARM) link/phyb.ld
	@mkdir -p $(dir $@)
	$(CLANG) $(OBJ_ARM) $(LDFLAGS_ARM) -o $@
	@echo "built $@"

$(BUILD)/arm/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CLANG) $(ARM_CFLAGS) -c $< -o $@

host: $(BUILD)/phyb_tests

$(BUILD)/phyb_tests: $(OBJ_HOST)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(OBJ_HOST) -o $@
	@echo "built $@"

$(BUILD)/host/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD)/host/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

check: host
	$(CLANG) $(ARM_CFLAGS) -MF $(BUILD)/layout.d -fsyntax-only tests/layout.c
	$(BUILD)/phyb_tests
	@echo "all checks passed"

verify: verify-layout verify-fidelity

# Every pinned state block must land at its documented address.
verify-layout: arm
	@python3 tools/verify_layout.py $(BUILD)/phyb.map

# Every recovered function is accounted for by a @provenance tag.
verify-fidelity:
	@python3 tools/verify_fidelity.py

# Check the retained evidence snapshot; this does not rebuild or prove semantics.
verify-archive:
	@python3 tools/verify_archive.py

# Finite instruction-level comparisons. Passing does not prove full firmware
# equivalence; unresolved functions and paths remain in verification/ledger.json.
verify-semantics: arm
	$(SEMANTIC_PYTHON) tools/verify_load_image.py --elf $(BUILD)/phyb.elf --output $(BUILD)/load-coverage.json --reference ../phyb
	$(SEMANTIC_PYTHON) tools/verify_frs_native.py --elf $(BUILD)/phyb.elf --output $(BUILD)/frs-native.json --mutation-controls
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite architecture --output $(BUILD)/architecture.json
	$(SEMANTIC_PYTHON) tools/probe_c_shifts.py --elf $(BUILD)/phyb.elf --output $(BUILD)/c-shifts.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite packets --output $(BUILD)/packets.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite boot --output $(BUILD)/boot.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite leaves --output $(BUILD)/leaves.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite mac --output $(BUILD)/mac.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite mac-alias --output $(BUILD)/mac-alias.json
	$(SEMANTIC_PYTHON) tools/probe_instruction_observer.py --elf $(BUILD)/phyb.elf --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite power --output $(BUILD)/power.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite tx-calibration --output $(BUILD)/tx-calibration.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite tx-volatile --output $(BUILD)/tx-volatile.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite tx-modes --output $(BUILD)/tx-modes.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite nvv --output $(BUILD)/nvv.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite loopback --output $(BUILD)/loopback.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite loopback-alias --output $(BUILD)/loopback-alias.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite status-bindings --output $(BUILD)/status-bindings.json
	$(SEMANTIC_PYTHON) tools/probe_diag_alias.py --elf $(BUILD)/phyb.elf --output $(BUILD)/diag-alias.json
	$(SEMANTIC_PYTHON) tools/verify_diag_sampling.py --elf $(BUILD)/phyb.elf --output $(BUILD)/diag-sampling.json
	$(SEMANTIC_PYTHON) tools/probe_diag_mac.py --elf $(BUILD)/phyb.elf --output $(BUILD)/diag-mac.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite trace --output $(BUILD)/trace.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite dpd --output $(BUILD)/dpd.json
	$(SEMANTIC_PYTHON) tools/arm_semantics.py --elf $(BUILD)/phyb.elf --suite dpd-workflow --output $(BUILD)/dpd-workflow.json
	$(SEMANTIC_PYTHON) tools/probe_frs.py --elf $(BUILD)/phyb.elf --output $(BUILD)/frs.json
	$(SEMANTIC_PYTHON) tools/qemu_formatter.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/qemu_loopback.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/qemu_mac_crash.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/qemu_diag_alias.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/qemu_dpd.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/qemu_frs_process.py --build $(BUILD)
	$(SEMANTIC_PYTHON) tools/probe_frs_scan.py --elf $(BUILD)/phyb.elf --output $(BUILD)/frs-scan.json
	$(SEMANTIC_PYTHON) tools/probe_clock_callback.py --elf $(BUILD)/phyb.elf --build $(BUILD) --output $(BUILD)/clock-callback.json
	$(SEMANTIC_PYTHON) tools/verify_semantic_domains.py --elf $(BUILD)/phyb.elf --build $(BUILD)

# Accepted NVV inputs can expose executable ROM as data. This gate currently
# fails: ordinary calibration-table matches do not establish ROM compatibility.
verify-rom-observability: arm
	$(SEMANTIC_PYTHON) tools/probe_rom_observability.py --elf $(BUILD)/phyb.elf --output $(BUILD)/rom-observability.json --require-equivalent

# Conditional universal checks for the selected scalar routines. This gate
# does not claim whole-firmware equivalence; see its explicit model assumptions.
verify-symbolic: arm
	$(SYMBOLIC_PYTHON) tools/symbolic_semantics.py --elf $(BUILD)/phyb.elf --output $(BUILD)/symbolic.json
	$(SYMBOLIC_PYTHON) tools/symbolic_diag_alias.py --elf $(BUILD)/phyb.elf \
	  --before-elf verification/evidence/diag-before-alias-fix.elf --output $(BUILD)/diag-symbolic.json

# Multi-target ftab reconstruction delegation (targets/{rkos,aopf,acib,rkof}).
# Root phyb targets above remain unchanged; sub-target outputs go to $(BUILD)/<target>.
$(SUBTARGETS):
	$(MAKE) -C targets/$@ all BUILD="$(SUBTARGET_BUILD)/$@"

targets-all:
	@for t in $(SUBTARGETS); do \
		$(MAKE) -C targets/$$t all BUILD="$(SUBTARGET_BUILD)/$$t" || exit $$?; \
	done

targets-check:
	@for t in $(SUBTARGETS); do \
		$(MAKE) -C targets/$$t check BUILD="$(SUBTARGET_BUILD)/$$t" || exit $$?; \
	done

targets-verify:
	@for t in $(SUBTARGETS); do \
		$(MAKE) -C targets/$$t verify BUILD="$(SUBTARGET_BUILD)/$$t" || exit $$?; \
	done

targets-semantics:
	@for t in $(SUBTARGETS); do \
		sem_py="$(SEMANTIC_PYTHON)"; \
		if ! "$$sem_py" -c "import capstone, elftools, unicorn" >/dev/null 2>&1; then \
			for cand in "/tmp/$$t-venv/bin/python" /tmp/phyb-verify-venv/bin/python /tmp/rkof-venv/bin/python /tmp/aopf-venv/bin/python /tmp/rkos-venv/bin/python /tmp/acib-venv/bin/python; do \
				if [ -x "$$cand" ] && "$$cand" -c "import capstone, elftools, unicorn" >/dev/null 2>&1; then \
					sem_py="$$cand"; \
					break; \
				fi; \
			done; \
		fi; \
		SEMANTIC_PYTHON="$$sem_py" $(MAKE) -C targets/$$t verify-semantics BUILD="$(SUBTARGET_BUILD)/$$t" || exit $$?; \
	done

verify-all:
	python3 tools/verify_all_targets.py --build "$(SUBTARGET_BUILD)"

clean:
	rm -rf $(BUILD) tools/__pycache__

-include $(OBJ_ARM:.o=.d) $(OBJ_HOST:.o=.d)
