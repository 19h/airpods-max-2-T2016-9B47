# Considerations — `phyb`

Supporting analysis for the `phyb` target. The source tree is at the workspace
root. The raw image is `../phyb`; the `phyb.asm`/`phyb.c` exports cited below
are not kept in the repository and can be regenerated from `../phyb.i64`.

This decompiled dump is a complete firmware image for the **Apple W3 Bluetooth/Wireless PHY coprocessor** (`WxBtPhy/firmware/w3/`, build `185{T:w3}{C:B0}.debug`, silicon stepping `B0`), running on top of **`RTKit_debug-3514.0.15.debug`**. It runs on an **ARMv7E-M (Cortex-M4F / Cortex-M7)** core with a single-precision hardware FPU and a lightweight **Apple RTKit** bare-metal runtime.

Below is a structured architectural review, memory/hardware map, subsystem breakdown, and a catalog of decompiler artifacts and ABI nuances required to reconstruct clean, idiomatic C source code. Sections marked **[asm]** were derived or verified directly from `phyb.asm` (IDA inventory, 232 functions: 219 in `phyb.c` plus 13 collapsed 2–4 byte IRQ/nop stubs; 30 compiler-outlined helpers).

---

## 1. System Architecture & Memory Map

### 1.1 Processor & Runtime Environment
* **Core:** ARM Cortex-M4F/M7 (`ARM_SCS` at `0xE000E000`), Little-Endian, Thumb-2 instruction set. **[asm]** The exact core is *not* determinable from the image: CPUID is never read, and the only M-profile evidence is special-register use (`PRIMASK`, `BASEPRI`, `FAULTMASK`, `CONTROL`, `IPSR`, `MSP`, `PSP`), CPACR/FPCCR/FPDSCR programming, SHCSR/SHPR writes, MPU_TYPE probing, and `S16-S31` lazy-context handling. Do not encode a specific core name in reconstructed source beyond `ARMv7E-M`.
* **Compiler (correcting IDA's "GNU C++" tag):** **[asm]** Apple Clang / LLVM, size-optimised (`-Oz`) with the ARM **machine outliner** enabled. Evidence: Apple ARMv7 frame-pointer convention (`PUSH {R7,LR}; MOV R7,SP` / `ADD R7,SP,#n` in every non-leaf function; IDA reports these as `bp-based frame`), 30 `_*_outlined_*` helpers with LLVM-outliner register conventions, LLVM-style `TBB/TBH` switch tables (18 jump tables), function-address hoisting into registers for repeated calls (`LDR R6,=(func+1); BLX R6`), and the `/Library/Caches/com.apple.xbs/...` build path baked into assertion strings.
* **FPU:** Single-precision FPU enabled in `CPACR` (`0xE000ED88 = 0x00F00000`) with lazy stacking enabled in `FPCCR` (`0xE000EF34 = 0xC0000000`) and `FPDSCR = 0`. **[asm]** FP use is sparse (about 50 VFP instructions total): `VCVT.F32.U32/S32`, `VMUL.F32`, `VDIV.F32`, `VMLA.F32`, `VSUB.F32`, one `VCMP/VMRS` pair (`btp_roundf_positive`), one `VPUSH {D4-D8}`/`VPOP` pair (`frs_dump_rssi_readings`). Used in TX frequency/kvco/slope calculations and RSSI fixed-point formatting.
* **Stacks:** Dual-stack model:
  * **MSP (Exception/Handler Stack):** `0x4D289EC8` – `0x4D28A2C8` (1024 bytes, `btp_msp_stack`).
  * **PSP (Thread/Process Stack):** `0x4D289AC8` – `0x4D289EC8` (1024 bytes, `btp_psp_stack`).
  * Both stacks are painted at reset with the 64-bit ASCII watermark `0x4B434154534B5452` (`"RTKSTACK"`, stored at ROM `0x7CD8`).
  * **[asm]** The vector table's initial-SP slot (`0x00000000`) is **zero**; stacks are established in software by `_rtk_reset_init_stacks`. Stack base/size descriptors live in `ROM_RODATA` at `0x6B4C` (`{&btp_msp_stack, 0x400}` for PSP top, `{&dword_4D28A2C8, 0x400}` for MSP top) and are read through literal-pool double indirection (`LDR R0,=off_6B58; LDR R0,[R0]`).
  * **[asm]** `R7` is seeded with the sentinel `0x77777700` before the first call so RTKit backtracers can detect the bottom of the frame-pointer chain.
* **Stack guard:** **[asm]** `_rtk_stack_chk_init` copies the value of the `STKG` metadata record (see §1.4, value `0x00000AFF`) into `__stack_chk_guard` at `0x4D289840`. `__stack_chk_fail` → `_rtk_panic_abort("stk_fail")`. Only `_vsnprintf_core` is instrumented with a canary check.

### 1.2 Address Space Layout

| Address Range | Segment / Region | Purpose & Key Contents |
| :--- | :--- | :--- |
| `0x00000000–0x000000BF` | `ROM_VECTORS` | 48-slot Cortex-M vector table (16 system + 25 external IRQs + 7 zero pads). See §1.3. |
| `0x000000C0–0x00000107` | `ROM_TEXT` (header) | **RTKit image header** (`'uuid'` record, UUID, metadata pointer/length, image size, build-string pointers). See §1.4. |
| `0x00000108–0x00006B4B` | `ROM_TEXT` | RTKit kernel, ISRs, PHY/Radio/Modem control logic, libc, and compiler-outlined thunks. 772 literal-pool words are interleaved with code (`LDR Rx,=imm` ×789). |
| `0x00006B4C–0x000088D1` | `ROM_RODATA` | Stack descriptors, build strings, exception-name table, FRS/RX/TX lookup tables (§3.7), format strings, hex-digit tables, source file paths, NVV tag names. |
| `0x4D000000–0x4D0003FF` | `MMIO_4D000` | **RF / Radio Front-End Registers:** Chip revision (`+0x000`), PA gain (`+0x0AC`/`+0x2F8`), Loopback/DCOC (`+0x0B8`–`+0x0C0`, `+0x304`–`+0x30C`), PLL status (`+0x100`/`+0x250`), LNA/Mixer/Filter BW (`+0x138`, `+0x1C8`, `+0x24C`). |
| `0x4D140000–0x4D1404FF` | `MMIO_4D140` | **TX Modem Registers:** TX control (`+0x000`), delays (`+0x014`, `+0x018`), DF1 filter (`+0x024`), IQ scale (`+0x02C`), KVCO/A4P (`+0x20C`), Dynamic HF coefficients (`+0x240`), band/mode select (`+0x480`). |
| `0x4D141000–0x4D1416FF` | `MMIO_4D141` | **DPD & Loopback Correlator Registers:** DPD mode (`+0x000`), IRQ clear (`+0x01C`), Correlator 0/1 windows & results (`+0x0E8`–`+0x1EC`), IQMM/DCOC cal (`+0x300`, `+0x328`), GTB counter (`+0x604`), error status (`+0x608`). |
| `0x4D148000–0x4D148FFF` | `HWBUF_4D148` | **DPD Hardware RAM:** Capture/test buffer (`0x4D148000`, `0xBC0` bytes) and actuation window (`0x4D148800`, `0x3C0` bytes). The two spans overlap; this is a hardware aperture, not two arrays. |
| `0x4D150000–0x4D151AFF` | `MMIO_4D150` | **RX Modem, AGC & FRS Registers:** AGC control/status (`+0x02C`, `+0x030`), RSSI/gain tables (`+0x090`–`+0x214`, `+0x500`–`+0x700`), RX packet status (`+0x1EC`), IQMM (`+0x84C`, `+0x940`), FRS correlator & energy bins (`+0xA8C`, `+0xAD0`–`+0xAF8`). |
| `0x4D154000–0x4D154CFF` | `MMIO_4D154` | **Sequencer & MAC IPC Registers:** Sequencer overrides (`+0x000`–`+0x008`), RFSeq pointer (`+0x01C`), Sequencer IRQ ack (`+0x020`), Packet Command 0/1 (`+0x030`, `+0x034`), Synth/TX mux (`+0x038`, `+0x03C`), Register write unlock (`+0x048`), 48-bit timestamps (`+0x118`, `+0x128`, `+0xC80`), PRBS generator (`+0xB00`–`+0xB74`), PHY block reset/enable (`+0xC08`–`+0xC10`, `+0xC70`–`+0xC78`), MAC IPC doorbells (`+0xC60`–`+0xC70`). |
| `0x4D157010` | `MMIO_4D157` | **Auxiliary Timer:** Aux timer IRQ status/clear register. |
| `0x4D159000–0x4D159014` | `MMIO_4D159` | **Clock Controller:** Clock enable, prescaler, mux divider (`+0x000`), hardware tick timer (`+0x004`), wakeup IRQ control (`+0x014`). |
| `0x4D280000–0x4D280FFF` | `RAM_SHARED_IPC` | **MAC-PHY Shared Mailbox:** Command (`+0x000`) / Response (`+0x400`) buffers, FRS parameters (`+0x1E4`) / results (`+0x4C0`), Coex state (`+0x1F9`), crash notification descriptor (`+0x5C1`, unaligned). |
| `0x4D281000–0x4D284FFF` | `RAM_NV_MDMI` | **NVV Calibration Region:** Pre-loaded `mdmi` NVV blob (`0x4D281000`, max `0x4000` bytes). **[asm]** `tx_power_init_and_interpolate_tables` writes the marker `0xCAFEBABA` to `0x4D284000` (offset `0x3000` within the stated `0x4000`-byte blob capacity). |
| `0x4D285000–0x4D288FFF` | `RAM_SHARED_STATUS`| **Shared Status & Telemetry:** Warm boot flag & register restore list (`0x4D285000`), packet telemetry (`0x4D287008`), version struct (`0x4D287034`). |
| `0x4D289000–0x4D2891FF` | `RAM_TRACE` | Trace ring (`btp_trace_buffer`, `0x1FC` bytes used). See §3.3. |
| `0x4D289200–0x4D28924B` | `RAM_CRASH` | Crash record (`btp_crash_record_ram`, `0x4C` bytes). See §3.4. |
| `0x4D289298–0x4D2898BC` | `RAM_INIT_DATA` | `.data`: pointer cells, default TX power tables, built-in `rfseq` NVV container, RTKit tagged metadata records (§1.4). |
| `0x4D2898BD–0x4D28D177` | `RAM_BSS` | `.bss` incl. both stacks, FRS/TX-power workspaces, NVV tag pointer cells, MPU descriptor table. |
| `0x4D28D178–0x4D28D2E0` | (NOLOAD) | Trace site token anchor region (never written; addresses only). See §4.3. |

### 1.3 Vector Table & Interrupt Map **[asm]**

The vector table at `0x0` has 48 slots. All addresses carry the Thumb bit. **Every** unused system slot (reserved 7–10, DebugMon 12, reserved 13) points at the fault entry, not at zero:

| Slot | Exception | Target | Resolved handler |
| :--- | :--- | :--- | :--- |
| 0 | Initial SP | `0x00000000` | *(none – stacks set up in software)* |
| 1 | Reset | `0x201` | `Reset_Handler` → `_rtk_reset_init_stacks` |
| 2–10, 12, 13 | NMI, HardFault, MemManage, BusFault, UsageFault, reserved, DebugMon | `0x205` | `NMI_Handler` → `_rtk_nmi_and_kernel_fault_entry` (one shared fault entry; IPSR is used to classify) |
| 11, 14 | SVCall, PendSV | `0x209` | `Fault_Handler` (misnamed) → `_rtk_svc_pendsv_entry` |
| 15 | SysTick | `0x435` | `SysTick_Handler` (increments `btp_systick_count` at `0x4D289904`; **never fires** – see §2.2) |
| IRQ0 | MAC IPC doorbell | `0xC81` | `mac_ipc_irq_handler` |
| IRQ1 | TX packet setup | `0xC85` | `tx_setup_isr` |
| IRQ2 | TX packet done | `0xC89` | `tx_done_isr` |
| IRQ3 | RX packet setup | `0xC8D` | `rx_setup_isr` |
| IRQ4 | RX packet done | `0xC91` | `rx_done_isr` |
| IRQ5 | Clock wakeup | `0x28D` | `IRQ5_Handler` (clock.c): `0x4D159014 = (v \| 1) & ~4`, disables IRQ5, then calls and clears a one-shot callback stored at `0x4D289900` |
| IRQ6 | Aux timer | `0xCD9` | `btp_aux_timer_isr` (ack `0x4D157010 = 0x10` if bit 4 set) |
| IRQ12 | Loopback correlator 0 | `0xCB5` | `loopback_corr0_done_isr` |
| IRQ15 | Loopback correlator 1 | `0xCB9` | `loopback_corr1_done_isr` |
| IRQ16 | `AFE_EL_CORR_DONE` | `0xCBD` | `IRQ16_Handler`: debug prints only (`>> ISR: AFE_EL_CORR_DONE` / `<< ...`) |
| IRQ19 | DPD capture done | `0xC95` | `dpd_capture_done_isr` |
| IRQ20 | `DPD_RAM_WR_AVAIL` | `0xC99` | `IRQ20_Handler`: debug prints only |
| IRQ7–11, 13, 14, 17, 18, 21–24 | unused | `0xD63`–`0xD7B` | 2-byte `BX LR` stubs (`IRQ7_Handler` sits 2 bytes inside `btp_nop_return_stub_D60`) |
| 41–47 | padding | `0` | – |

`btp_irq_init_and_enable` enables exactly NVIC IRQs `0, 1, 2, 3, 4, 12, 15, 16` and then executes `CPSIE I`. IRQ5, IRQ6, IRQ19 and IRQ20 are **not** enabled at init (IRQ19 is additionally disabled from `dpd_capture_done_isr`; IRQ5 disables itself). NVIC priority registers (IPR) are never written, so every IRQ runs at priority 0 and `BASEPRI = 0x80` never masks any of them.

### 1.4 RTKit Image Header & Tagged Metadata Records **[asm]**

Immediately after the vector table (ROM `0xC0`–`0x107`) sits an RTKit image header, byte-exact:

| Offset | Value | Meaning |
| :--- | :--- | :--- |
| `0xC0` | `"uuid"` | Record tag |
| `0xC4` | `0x00000005` | Header/record version |
| `0xC8` | `0x00007D00` | → `rtkit_build_string` (`"RTKit_debug-3514.0.15.debug"`) |
| `0xCC` | `0x00006B60` | → `btp_build_string` (`"185{T:w3}{C:B0}.debug"`) |
| `0xD0` | 16 bytes `5F A9 E1 61 3A 11 37 3E 96 8A F4 BE 63 F5 41 83` | Image UUID |
| `0xE0` | `0, 0` | – |
| `0xE8` | `0x4D289844` | → tagged metadata records in `.data` |
| `0xEC` | `0x79` | Length of that record region (ends exactly at `.data` end `0x4D2898BD`) |
| `0xF0` | `0x88D4` | Image (text + rodata) size |
| `0xF4` | `0, 0, 0` | – |
| `0x100` | `0x7D00`, `0x6B60` | Build-string pointers again (RTKit + product) |

`btp_assert_or_exception_panic` copies ROM `0xC0..0xDF` (`btp_image_identity[32]`) into the crash record so the host can match a crash to a build.

The metadata region at `0x4D289844` is a sequence of `{char tag[4]; uint32_t len; uint8_t value[len];}` records. Tags appear byte-reversed in the listing because IDA prints them as little-endian `DCD`s; the natural FourCC names are:

| Address | FourCC | Len | Value | Interpretation |
| :--- | :--- | :--- | :--- | :--- |
| `0x4D289844` | `SOC_` | 4 | `0xFFFFFFFF` | Target SoC (unset) |
| `0x4D289850` | `SOCR` | 4 | `0xFFFFFFFF` | SoC revision (unset) |
| `0x4D28985C` | `fpga` | 4 | `0xFFFFFFFF` | FPGA build flag (unset) |
| `0x4D289868` | `STKG` | 4 | `0x00000AFF` | `__stack_chk_guard` seed (copied to `0x4D289840`) |
| `0x4D289874` | `RTSZ` | 4 | `0x4D28D178` | RAM end (`.bss` end) |
| `0x4D289880` | `RRST` | 4 | `0` | – |
| `0x4D28988C` | `RRED` | 4 | `0` | – |
| `0x4D289898` | `TXSZ` | 4 | `0x000088D4` | Text size (matches header `0xF0`) |
| `0x4D2898A4` | `DSRL` | 1 | `0` | **Data-segment reload flag** – read by `_rtk_cold_boot_init` (see §4.4) |
| `0x4D2898AD` | `SLID` | 8 | *(truncated at `.data` end)* | Slide/ASLR placeholder |

Reconstruct these as a linker-emitted `__rtkit_meta` section; they are not firmware logic.

---

## 2. Source File & Subsystem Breakdown

From embedded assertion strings, debug prints, and address locality, the firmware's original source tree can be mapped into **11 logical modules**:

### 2.1 RTKit Core & Startup (`rtkit`, `threading.c`, `exception.c`)
* **Functions:** `0x108`–`0x260`, `0x4968`–`0x4A4B`, `0x5B50`–`0x5C67`, `0x605C`–`0x6443`, `0x6B22`.
* **Role:**
  * `_rtk_reset_init_stacks` initializes `MSP` and `PSP`, fills both with `"RTKSTACK"`, switches to `PSP` (`CONTROL |= 2`), and calls `_rtk_boot_router`.
  * `_rtk_boot_router` checks `btp_is_warm_boot()`:
    * **Cold Boot (`_rtk_cold_boot_init`):** Zeroes `.bss` (in three pieces around the two stacks, §4.4), optionally reloads the RAM image (`DSRL` flag), initializes `__stack_chk_guard`, configures SCB/FPU, probes the MPU, configures exception priorities, runs the (empty) `.init_array`, and jumps to `btp_main()`. If `btp_main` returns: `_rtk_platform_halt(2)`.
    * **Warm Boot (`_rtk_warm_boot_init`):** Re-initializes SCB/FPU/priorities and `BASEPRI` without wiping `.bss` or recalibrating tables, then jumps to `btp_main()`. If `btp_main` returns: halt code `6`.
  * **[asm] Identical SCB programming in both paths** (reconstruct once as `rtk_arch_init()`):
    * `CCR |= 0x210` (`STKALIGN | DIV_0_TRP`), then `CCR &= ~0x8` (**`UNALIGN_TRP` cleared** – required because the IPC mailbox is accessed with unaligned 32-bit stores, e.g. the crash descriptor at `base+0x5C1` and the coex word at `0x4D2801F9`).
    * `CPACR = 0` → `FPCCR = 0xC0000000` → `FPDSCR = 0` → `CPACR = 0x00F00000`, followed by `DSB; ISB` (cold path only).
    * `SHPR1 = 0` (MemManage/BusFault/UsageFault priority 0), `SHPR2 = 0x80000000` (SVCall `0x80`), `SHPR3 = 0x00800000` (PendSV `0x80`, SysTick `0`), `SHCSR = 0x70000` (enable UsageFault/BusFault/MemManage).
    * `PRIMASK = 1` and `BASEPRI = btp_rtkit_basepri` (`0x80`) **before** calling `btp_main`; interrupts stay globally masked until `btp_irq_init_and_enable` executes `CPSIE I`.
  * **[asm] BASEPRI policy:** `rtk_irq_set_basepri_level(n)` accepts `n` with `(n >> 2) - 1 ∈ {0,1,2}`, stores `n` in `0x4D28D174`, and sets `btp_rtkit_basepri = 0x80 - 0x20 * ((n >> 2) - 1)`; otherwise it returns `0x80000001` and the wrapper `rtk_irq_configure_basepri` returns RTKit error `0xB`. The warm path calls it with `n = 4` (→ `0x80`).
  * **[asm] MPU probing (`_rtk_cold_boot_init`):** reads `MPU_TYPE`; stores `DREGION` to `0x4D28CFE0`. If `SEPARATE == 0` and `DREGION != 0`: writes `MPU_RBAR = 0xFFFFFFE0`, reads it back to derive the supported address-mask (`0x4D28CFE4`) and a region-index mask (`0x4D28CFE8`), zeroes a 16-entry × 24-byte descriptor table at `0x4D28CFF0` and seeds each entry with `{0xFFFFFFE0, 0}`. **`MPU_CTRL` is never written** (no xref) – the MPU is probed but never enabled.
  * **[asm] Thread/SVC path (`_rtk_svc_pendsv_entry`):** if `EXC_RETURN.SPSEL == 0` (exception taken from handler mode) the event is treated as a kernel fault and branches to `_rtk_nmi_and_kernel_fault_entry`. Otherwise it pushes `{EXC_RETURN, pad, [S16-S31 if FP active], R4-R11}` onto PSP, calls `_rtk_thread_context_switch(frame)` and restores whatever frame it returns. The scheduler is a single-thread skeleton: it only acts when bit 0 of `byte_4D289A48` (“switch pending”) is set, which nothing in this image ever sets, so SVC/PendSV are effectively no-ops. Its three `_rtk_assert_fail` sites carry `threading.c` lines `0x49`, `0x52`, `0x53` with the original expression strings.
  * **Crash Handling:** Hardware faults go through `_rtk_nmi_and_kernel_fault_entry` → `_rtk_capture_fault_and_panic` → `btp_fault_exception_bridge` → `btp_report_hardware_exception` (`exception.c:0x6C`) → `btp_assert_or_exception_panic`. Software assertions call `btp_assert_or_exception_panic` directly. Both end in `mac_ipc_send_crash_notification` then `btp_system_halt_on_code5(5)` (always halts; see §4.9). See §3.4 and §3.5 for frame/record layouts.
  * **[asm] Stack-sanity spin traps:** `_rtk_nmi_and_kernel_fault_entry` begins with `CMP SP, &btp_msp_stack; BLT .` and `CMP SP, msp_top; BGT .`; `_rtk_check_active_stack_lower_bound` is `CMP SP, #0x4D289EC8; BLT .`. These are deliberate infinite loops (“hang here for the debugger if the handler stack is corrupt”). Hex-Rays drops or mangles them; reconstruct as `while (__get_MSP() < MSP_BASE) {}`.
  * **[asm] `btp_main` sequence:** `btp_is_warm_boot()` → `btp_early_hw_init()` (SysTick init, trace, radio revision, clock enable, prescaler 7, mux divider 7) → print `[BTP]: (version: %s)` → `phy_hw_init(warm)` → `btp_irq_init_and_enable()` → `btp_late_init_noop()` → `rx_init()` (takes no parameters; the caller still leaves `warm` in `R0`) → `frs_correlator_hw_init()` → `tx_init(warm)` → print `PHY init done rad_rev=%d` → warm ? `mac_ipc_send_warm_boot_ack()` : `btp_set_warm_boot_state(1)` → `for(;;) rtk_cpu_idle();` (`WFI`). Everything afterwards is interrupt-driven.
  * **[asm] `phy_hw_init(warm)`:** cold path runs `modem_init_load_nvv_and_module()`, `syscfg_init()`, `tx_power_init_and_interpolate_tables()`; both paths then pulse the PHY block reset (`0x4D154C70 |= 0xF`, delay 1 tick, `0x4D154C74 |= 0xF`, delay, `0x4D154C78 &= ~0xF`, delay – the last two steps are the outlined `_phy_outlined_assert_reset/_deassert_reset`), the cold path clears radio bits `0x4D000254/26C/104/11C` and calls `phy_reset_tx_and_radio_state()`, and both finish with `0x4D15403C[15:9] = 0x40, |= 4`, `0x4D154C10 |= 3`, `0x4D154C0C |= 0x10000`, `0x4D154C08 |= 0x10000`.
  * **[asm] Warm-boot detection:** `btp_is_warm_boot()` = `(btp_warm_boot_magic[0] == 0x4DBCC725 && btp_warm_boot_magic[1] == 0xE91D98A5) && btp_warm_boot_flag != 0` (`.bss` magic at `0x4D2899AC`, flag in shared status RAM `0x4D285000`). `btp_set_warm_boot_state(1)` writes both magic words and the flag.
  * **[asm] Radio revision:** `btp_detect_radio_revision()` reads `0x4D000000`: `maj = bits[19:12]`, `min = bits[11:8]`; `'A',0 → 2`; `'A',1 → 3`; `'B',0 → 4`; else `0`. Stored at `btp_radio_revision` (`0x4D28CF44`).

### 2.2 Clocks, Timers & Interrupts (`clock.c`, `systick.c`, `irq.c`, `timer.c`)
* **Functions:** `0x264`–`0x313`, `0x420`–`0x4C7`, `0xC80`–`0xD7B`, `0x4AE0`–`0x4B53`.
* **Role:**
  * Manages two timebases:
    1. **ARM SysTick (`0xE000E010`):** 24-bit down-counter initialized to `0xFFFFFF`, used for short busy-waits (`systick_delay_cycles`) and trace entry timestamps. **[asm]** `systick_init(0xFFFFFF, 0, 0)` writes `CTRL = 1` (`ENABLE` only; `TICKINT` and `CLKSOURCE` clear), so `SysTick_Handler` never executes and `btp_systick_count` stays 0. `systick_wait_cycles` handles wrap by adding the stored reload value (`0x4D289908`).
    2. **48-bit Sequencer Timer (`0x4D154C80`):** High-resolution radio timer with hardware-latched TX (`0x4D154128`) and RX (`0x4D154118`) trigger timestamps.
    3. **[asm] Clock-controller tick (`0x4D159004`):** `btp_clock_delay_ticks(n)` spins until the free-running counter passes `now + n + 1`.
  * **[asm] Clock controller (`0x4D159000`):** bit 0 = enable; bit 2 = mux switch request (set, wait 0 ticks, then `bits[6:4] = divider & 7` with bit 2 cleared); `bits[14:12]` = prescaler; `btp_clock_set_panic_mode` = `(v & ~0x10002) | 0x2` (switch to a safe clock before halting).
  * `btp_irq_init_and_enable` enables NVIC IRQs `0, 1, 2, 3, 4, 12, 15, 16` (see §1.3). `nvic_clear_pending_irq` masks the IRQ number to 5 bits; `nvic_enable_irq/disable_irq` do not.
  * **[asm] `irq_save_and_disable()`** returns `PRIMASK == 0` as a boolean (`MRS R0,PRIMASK; CPSID I; CLZ R0,R0; LSRS R0,#5`) and `irq_restore(flag)` executes `CPSIE I` only if the flag is set – i.e. a classic `irq_save/irq_restore` pair; the decompiler shows `CLZ`/shift arithmetic.

### 2.3 MAC-PHY IPC Interface (`WxBtPhy/firmware/w3/mac.c`)
* **Functions:** `0x4C8`–`0xC7F`.
* **Role:** Handles `IRQ0_Handler` (`mac_ipc_irq_handler`).
  * Reads `mmio_ipc_rx_status` (`0x4D154C68`) and acknowledges bits via `mmio_ipc_rx_ack` (`0x4D154C6C`). **[asm]** The ack is written as `(ack & ~0xFFFF) + status_low16`; all doorbell writes use `BFI` into the low halfword, preserving the upper 16 bits.
  * **Status Bit `0x0001`:** Fast RSSI Scan (FRS) request using parameters at `0x4D2801E4`.
  * **Status Bit `0x0002`:** No-op / wake ping.
  * **Status Bit `0x0004`:** Coex RX gain backoff toggle based on `*(uint32_t *)0x4D2801F9`.
  * **Status Bit `0x8000`:** Command packet in shared memory (`btp_mac_shared_base` at `0x4D280000`). Command id = `u16 @ +0x002`:
    * **Cmd `0`:** Handshake / capability query.
    * **Cmd `1`:** Subcommand multiplexer on `u16 @ +0x008` (24-entry `TBH` table over `0x0A..0x21`; implemented: `0x0A, 0x0F, 0x10, 0x12–0x1D, 0x20, 0x21`; `0x0B–0x0E, 0x11, 0x1E, 0x1F` fall to default). Subcommand `0x15` dispatches on a request byte: `switch` over `1..8`, range `9..16` → `tx_power_set_band_workspace`, range `0x11..0x14` → `rx_set_iqmm_override(…, idx-0x11, …)`, `switch` over `0x15..0x1A`; everything else is an error.
    * **Cmd `2`:** Full TX power table update (`update_tx_power_tables`, 288 bytes at `+0xC4`).
    * **Cmd `3`:** Query whether `mmio_ipc_rx_status` is idle.
    * **Cmd `4`:** Warm-boot / sleep preparation (`byte_4D28990C = 1`, sets `dword_4D154C10 |= 0xC`).
    * **Cmd `5`:** Per-band TX power table update (`update_tx_power_tables_per_band`).
    * Unknown status bits trace error `(7, status, 1)` and assert at `mac.c:0x51C`; command ids ≥ 6 trace error `(7, cmd, 2)` and assert at `mac.c:0x437` (both error `0xB`).
  * **[asm] Response header at `base + 0x400`:** `u16 flags` (`+0x400`, bit 7 = response ready), `u16 cmd_echo` (`+0x402`), `u8 status` (`+0x404`), `u8 count` (`+0x405`), payload (`+0x408`, up to `0xB4` bytes; `_mac_outlined_setup_readback_buf` returns `{base+0x408, 0xB4}`). `_mac_outlined_fill_cmd2_response` writes `{cmd=2, status, count=1, payload[0]=status}`.
  * Replies by setting bit 7 (`0x80`) in the response header at `base + 0x400` and ringing `mmio_ipc_tx_doorbell` (`0x4D154C60`) with `0x8000` (command ack), `0x0004` (FRS done), `0x0002` (warm boot ack, only if `byte_4D28990C == 1`), or `0x0001` (crash notification).

### 2.4 Fast RSSI Scan (`frs.c`)
* **Functions:** `0xD7C`–`0x1BCB`.
* **Role:** Performs rapid multi-channel energy/RSSI scanning across up to 8 bands and 3 frequency-offset passes (`0`, `-1`, `+1`):
  * `frs_unpack_channel_bitmap`: Expands a 16-byte (128-bit) channel bitmap into a channel list array (with fast-path switch cases for single-bit bytes `0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80`; 9-case `TBB` at `0x1094`).
  * `frs_save_state_and_enter` / `frs_restore_state_and_exit`: Context-switches 22 PHY/Radio MMIO registers into `btp_frs_saved_registers` (`0x58` bytes).
  * `frs_scan_channel_pass`: Tunes the synth per channel, runs AGC (or fixed gain override, 4-case `TBB` at `0xE8E`), triggers the FRS correlator, and reads 17 10-bit signed RSSI bins from `0x4D150AD8`–`0x4D150AF8` into a 17 × `uint16_t` local array (`SP+0x28..0x4A`; bins 9–16 are filled via half-word-offset word stores, bin 8 is the centre bin).
  * `frs_process_and_calibrate_results`: Combines multi-pass readings, applies `syscfg_rssi_cal` and low-gain linearity corrections, and writes calibrated 16-bit Q14.2 fixed-point values to `0x4D2804C0`. Uses the per-band default channel lists pointed to by `dword_4D2892A0[8]` (a `.data` pointer array into `ROM_RODATA` `0x6BA4..0x6BDC`) and the constant tables at `0x6B9C`, `0x6BDD`, `0x6BE8`, `0x6BF8`, `0x6C38`, `0x6C60` (see §3.7).
  * `frs_dump_rssi_readings`: debug table printer (`"RSSI readings:"`, `%4s %15s %15s`, `Ch/#1/#2/#3`), formatting values with `frs_format_rssi_fixed_point` (`value * 0.25f` → `"%d.%02d"`, the `0.25f` scale held in `S16` across the loop, output into an 8-slot rotating string ring – see §4.14). Timeouts print `"FRS timeout!"`, `"PLL NOT OK!"`, `"AGC NOT OK! %x"`.

### 2.5 RX Path & Coex (`rx.c`, `coex.c`)
* **Functions:** `0x1BCC`–`0x27AB`, `0x28DC`–`0x296F`.
* **Role:**
  * `rx_setup_isr` (`IRQ3`): Triggered at packet RX start. Reads packet configuration from `mmio_packet_command0` (`0x4D154030`) and `mmio_packet_command1` (`0x4D154034`), selects filter bandwidth (8-case `TBB` at `0x2196`), configures LNA/Mixer IQ mismatch (`rxiqmm_ab_*`), applies per-band maxgain (`byte_4D28AA40`), MXDC offsets (`0x1182`), and RSSI calibration offsets (`0x117F` + `syscfg`), and acknowledges the sequencer (`0x4D154020 |= 0x40000`). `btp_saved_rssi_baselines` (4 bytes) holds register-field baselines captured at NVV load time, not module IDs.
  * `rx_done_isr` (`IRQ4`): Triggered at packet RX completion. Extracts packet RSSI and 10-bit signed CFO from `mmio_rx_packet_status` (`0x4D1501EC`), updates `btp_rssi_histogram[4]` and CFO min/max/sum, restores `0x4D150094`/`98`, and acknowledges the sequencer (`0x4D154020 |= 0x80000`).

### 2.6 TX Path, TX Power & RF Test (`tx.c`, `tx_power.c`, `dpd.c`, `rf_test.c`)
* **Functions:** `0x27AC`–`0x28CB`, `0x2970`–`0x412B`.
* **Role:**
  * `tx_setup_isr` (`IRQ1`): Configures radio PA/driver settings, power mode (LP vs HP), modulation-dependent A4P slope and KVCO scaling (`0x1006`, `0x115B`, `0x117A`, `0x118C`), DF1 mantissa/exponent (`0x1169`, `0x1170`, `0x117D`, `0x1180`, `0x118D`, `0x1190`), dynamic AM/IF/HF delays (`0x1171`–`0x1173`), and 6-word HF filter coefficients (`btp_selected_hf_table_ptr`). Packet-type dispatch is an 8-case `TBH` at `0x2DE4`. Also handles synchronous DPD actuation/capture when enabled (debug prints `Act GTB=%d:`, `GTB DPD test %d (%d)`, `>>>>>> (err=%d gtb=%d)`, `TS=%d %d %d`). **[asm]** Float constants used (exact IEEE-754 bit patterns from the ROM bytes): `4194304.0f` = `0x4A800000` (2²²; IDA's `DCFS` display rounds it to `4194300.0`), `-0.03125f` = `0xBD000000` (−1/32), `2048.0f` = `0x45000000`, `0.0078125f` = `0x3C000000` (1/128). Write them as hex-float (`0x1p22f`, `-0x1p-5f`, `0x1p11f`, `0x1p-7f`) to guarantee bit-exact reassembly.
  * `tx_done_isr` (`IRQ2`): Resets PA stage bits via `tx_power_complete_packet` and acknowledges the sequencer (`0x4D154020 |= 0x20000`).
  * `dpd_capture_done_isr` (`IRQ19`): disables IRQ19, copies the `0xBC0`-byte hardware capture buffer to `btp_dpd_capture_buffer` (`0x4D28AAB0`) via the outlined `memmove` wrapper, logs `%u,` words and `<<<<<< (dpd_cpu_access_err=%d)`.
  * `dpd_handle_command`: 4-case `TBB` (`0x369E`): `DPD mode %d>%d set up @%d`, `DPD actuation %d (GTB=%d)`, `DPD test %d, GTB=%d`, `DPD get: %x %x %d %d`.
  * `tx_power_*`: Interpolates sparse per-channel max power calibration points from NVV tag `0x117C` into a dense 8-band × 128-channel × 4-byte lookup table at `0x4D28B900` (`dword_4D2895A8`). Computes target power per packet (6-case `TBB` at `0x3E68`), enforces band-edge regulatory limits (`btp_tx_channel_limits`, `0x120` bytes) and coex floor (`dword_4D2895B0`, default `-120`), and programs PA gain stage (`1..3`) and attenuation index (`0..120`). Default tables `btp_default_tx_power_table` (pairs of `{index, code}`) and `btp_tx_power_code_table` live in `.data`. Debug: `phymaxpwr_p=%08x CAL %08x`, `No Tx PWR TBL found!!`, `band: %d packet type: %d:%08x:%08x`, `tx_pwr_mode=%d`.

### 2.7 SysCfg & NVV Calibration Loader (`syscfg.c`, `nvv.c`, `phy.c`)
* **Functions:** `0x412C`–`0x4287`, `0x4EA8`–`0x5B4F`.
* **Role:**
  * `modem_init_load_nvv_and_module`: parses the built-in `rfseq` container (`btp_rfseq_header` at `0x4D2895D0`: `{len=0x22C, magic=0x4E56, meta=0xECCF0000}` containing a single long tag `0x080E`, `0x228` bytes) and the host-provided `mdmi` container at `0x4D281000` (capacity `0x4000`; `"Blob too big! (%x + %x > %x)"`). Prints `loading rfseq` / `loading mdmi`, `Blob @ %x..`, `NVV %s (%x) %d bytes = %d`, `Modem inits:`, `Done @ %04x!`, `## BAD ADDR %08x!!` (via `nvv_validate_mmio_address`). After both containers are parsed, if `btp_get_radio_revision() == 2` the `0x1170`/`0x1169` pointers are forced to the `.data` defaults `unk_4D289818` (8 bytes) / `unk_4D289820` (16 bytes). Module identity comes from tag `0x857` (`mod_info`), compared with `nvv_module_id_compare4` against `DAUG`, `PLTY`, `PLTE`, `YNGR`, `ELDR` (`found %s module`, `missing mod info, defaulting to YNGR`, `Error! Unexpected module id!` + `btp_trace_log_error(7, id, 0x26)`). The result selects the HF-coefficient table: **`DAUG`** keeps `btp_selected_hf_table_ptr` as set by tag `0x1168` (`dynamic_hf_coeff`); **`PLTY`, `YNGR`, missing or unknown** → `btp_nvv_tag_116A_ptr` (`dynamic_hf_delay_pty`); **`PLTE`, `ELDR`** → `btp_nvv_tag_116B_ptr` (`dynamic_hf_delay_pte`).
  * `nvv_parse_blob`: 69-case `TBH` switch on `tag - 0x115B` (`0x5168`) plus explicit compares for `0x80B`, `0x80E`, `0x821`, `0x836`, `0x857`, `0x1006`, `0x11D8`, `0x11D9`. Each recognised tag stores its payload pointer into a `btp_nvv_tag_XXXX_ptr` cell and sets a presence bit in `btp_nvv_presence0/1`. Unknown tags print `"??"` as the name. `0x80B` is a list of `{addr, value}` pairs written directly (after `nvv_validate_mmio_address`); `0x821`/`0x836` are lists of 12-byte `{addr, mask, value}` entries applied as `*addr = (*addr & ~mask) | (value & mask)`. See §3.6 for the full table.
  * `syscfg_*`: Receives host-provided calibration chunks over IPC subcommand `0x0A`, validates the XOR checksum (`struct btp_syscfg_buffer`: header `{version, device, payload_length, unknown_06, xor_checksum}` + payload; prints `appkying syscfg ver %d, dev %04x len=%d checksum=%08x`, `checksum ERROR got %08x expect %08x`), and overrides TX power (TLV `tag 1` → `btp_nvv_tag_117C_ptr`, re-runs `tx_power_init_and_interpolate_tables`, sets `btp_nvv_presence0 |= 0x200000`) and RSSI (TLV `tag 2` → `btp_syscfg_rssi_cal`, `syscfg_rssicalp=%08x`, sets `btp_nvv_presence0 |= 0x400000` if non-NULL) calibration pointers.
  * `modem_set_register_write_unlock(on)` (5 call sites) writes `on` into `0x4D154048` bits 4 and 6 and into `0x4D140000` bits 5 and 16 around protected register writes; `modem_warm_boot_restore_regs` replays the register list saved in shared status RAM.

### 2.8 Loopback Subsystem (`loopback.c`)
* **Functions:** `0x4288`–`0x4967`.
* **Role:** Configures internal TX-to-RX loopback (`loopback_configure_mode`), applies LP/HP loopback DCOC and IQMM calibration (`0x1191`, `0x1192`, `0x119B`–`0x119F`), arms two hardware correlator windows per TX packet (`loopback_setup_tx_measurement`), and collects I/Q statistics in `IRQ12_Handler` (`corr0`) and `IRQ15_Handler` (`corr1`) into `btp_loopback_stats` (`uint8_t sample_count` + pad, then `int32_t {i0,q0,i1,q1}_sum`, `{…}_max`, `{…}_min`, reached through the `.data` pointer cell `btp_loopback_stats_ptr`).

### 2.9 RTKit libc & formatting (`0x5C68`–`0x6B5F`) **[asm]**
* `btp_memcpy` is a 4-byte thunk (`B.W memmove`); `memmove`/`_memmove_core`, `btp_memset`, `strlen`, `strncmp`, `strlcat`, `strlcpy`, `strnlen`, `strrchr` are hand-aligned library routines (loop heads padded with `NOP` runs; the 16-byte `btp_unresolved_region_5E74` is alignment padding, not code). `__udivdi3`, `__aeabi_llsl`, `__aeabi_llsr` are compiler-rt builtins (64-bit shift/divide). **Reconstruct all of these as standard libc / compiler builtins, not from the decompiled bodies.**
* `_vsnprintf_core` is a sink-based formatter. Verified layout (`_snprintf_putc` at `0x6A38` starts with `LDR R2,[R0,#8]` for the buffer):
  ```c
  struct btp_format_sink {
      void  (*emit)(struct btp_format_sink *, int);  /* +0x00, the BLX R2 target */
      size_t  capacity;                              /* +0x04 */
      char   *buffer;                                /* +0x08 */
      size_t  stored_count;                          /* +0x0C */
      size_t  total_count;                           /* +0x10 */
  };
  ```
  88-case `TBH` on the conversion character (`'#'..'z'`). Supported: `%s` (`<null>` for NULL), `%c`, `%d/%i/%u/%x/%X/%p`, `%#x` (`0x` prefix), `*` width, `.` precision, `-`/`+`/`0` flags, `h`/`l`/`z` length modifiers, 64-bit decimal via `_format_uint64_decimal`. Floating-point conversions (`%a %e %f %g %A %E %F %G`) emit a literal `"@"`. Unknown specifiers are echoed as `%` followed by the character. Digit tables `btp_format_digits_lower/upper` at `0x7CE0/0x7CF0`.

---

## 3. Data Structures & Protocol Formats

### 3.1 NVV Blob & Tag Encoding (`nvv_parse_blob`)
Each NVV blob starts with `struct btp_nvv_header` (8 bytes: `uint16_t payload_length`, `uint16_t magic == 0x4E56` (`"NV"`), `uint32_t metadata`). Multiple blobs can be concatenated back-to-back until a non-`0x4E56` magic is encountered. Inside the payload, tags use a 3-tier variable-length header:
1. **1-Byte Header (`(b0 & 0x80) == 0`):**
   * `tag = b0 & 0x7F`, `length = 1`, data follows at `+1`. (Note: The decompiler reads `*(__int16 *)v7` and checks `v11 < 0`, which tests bit 15 of the 16-bit little-endian load, i.e., bit 7 of `v7[1]`).
   * *Important Decompiler Precision:* Because ARM is little-endian, `int16_t v11 = *(int16_t *)v7` places `v7[0]` in bits `0..7` and `v7[1]` in bits `8..15`:
     * If `(v11 & 0x8000) == 0`: `tag = v11 & 0xFF`, `len = 1`, `data = v7 + 1`.
     * If `(v11 & 0xC000) == 0x8000`: `tag = v11 & 0x7FF`, `len = (v11 >> 11) & 7`, `data = v7 + 2`.
     * If `(v11 & 0xC000) == 0xC000`: `tag = v11 & 0x3FFF`, `len = *(uint16_t *)(v7 + 2)`, `data = v7 + 4` (`struct btp_nvv_long_tag {uint16_t tag_hdr; uint16_t len;}`, e.g. `0xC80E, 0x228` for the built-in `rf_seq`).

### 3.2 Sequencer Packet Command Registers
In `rx_setup_isr`, `tx_setup_isr`, and `tx_power_setup_from_mac_command`, the hardware sequencer passes per-packet metadata via two 32-bit registers:
* **`mmio_packet_command0` (`0x4D154030`):**
  * `bits [6:0]`: Channel index (`0..127`)
  * `bit [8]`: Packet mode flag (`0` = Bluetooth/standard packet type table, `1` = alternate/proprietary rate table)
  * `bits [16:14]`: Band index (`0..7`, where `0` = 2.4 GHz, `1..7` = 5 GHz / sub-bands)
  * `bits [19:17]`: Proprietary rate index (`0..7`, used when `bit [8] == 1`)
* **`mmio_packet_command1` (`0x4D154034`):**
  * `bits [11:8]`: TX power backoff step count (`0..15`)
  * `bit [15]`: TX power table column select (`0` vs `1`)
  * `bits [21:18]`: Packet type ID (`0..15`, e.g., `0xC`/`0xD` = LE 1M/2M, `6..8` = EDR 2-DH/3-DH, `9` = LE Coded, etc.)
* **[asm]** `_tx_outlined_set_band_and_channel` writes `band` into `0x4D154008[21:19]` and `channel` into `[6:0]`, then ORs `0x10 | 0x4` into `0x4D154000` (sequencer override kick).

### 3.3 Trace Ring (`btp_trace_log_entry`, `0x4D289000`) **[asm]**
```c
struct btp_trace_record {            /* 28 bytes */
    uint32_t type;                   /* 1 = init phase, 2 = IPC, 4 = error, ... */
    uint32_t subtype;                /* e.g. 1 = irq init, 5 = IPC irq, error code for type 4 */
    uint32_t start_ts;               /* SysTick VAL at entry (0 on "end" records) */
    uint32_t end_ts;                 /* SysTick VAL at exit  (0 on "begin" records) */
    uint32_t arg0, arg1;
    uint32_t site;                   /* site_token - 0x4D28D178 */
};
struct btp_trace_ring {              /* 0x1FC bytes used of the 0x200 buffer */
    uint32_t next_index;             /* advanced as (i + 1) % 18 (UDIV by 18) */
    struct btp_trace_record entries[18];
    uint32_t padding;
};
```
* Callers build a `struct btp_trace_payload {type, subtype, start_ts, end_ts, arg0, arg1}` on the stack (`0x20` bytes incl. padding) and pass `(site_token, &payload)`. Begin/end pairs are logged as two records (`start_ts=now,end_ts=0` then `start_ts=0,end_ts=now`).
* A `NULL` payload logs `{type=4, subtype=7, 0, 0, 0, 0, site}`.
* Logging is suppressed while bit 0 of `dword_4D28A2C8` (the “trace freeze” word, set by `btp_set_trace_freeze_bit`, which also doubles as the MSP top marker) is set.
* `btp_trace_log_error(code, a, b)` logs `{type=4, subtype=code, start_ts=now, end_ts=now, arg0=a, arg1=b, site=0x148}` (two `STRD` pairs: `{code, now}` → `subtype/start_ts`, `{now, a}` → `end_ts/arg0`).
* `btp_trace_get_buffer_size()` returns `0x1FC`; the crash descriptor exports `(buffer, size)` to the host.

### 3.4 Exception Register Frame (`_rtk_nmi_and_kernel_fault_entry` / `_rtk_capture_fault_and_panic`) **[asm]**
The fault entry picks `MSP` or `PSP` from `EXC_RETURN.SPSEL`, pushes `R4-R11`, conditionally `S16-S31` (`EXC_RETURN` bit 4 clear ⇒ FP context active), stores `EXC_RETURN`, then reserves a 0x18-byte header – producing this frame, which is also what `btp_fault_exception_bridge` hands to the host as `detail0` (pointer) / `detail1` (size):

| Offset | Field | Source |
| :--- | :--- | :--- |
| `+0x00` | `cfsr` | `CFSR` on HardFault only; otherwise 0 |
| `+0x04` | `control` | `CONTROL` |
| `+0x08` | `masks` | `(BASEPRI << 8) \| (FAULTMASK << 1) \| PRIMASK` |
| `+0x0C` | `exception_number` | `IPSR` |
| `+0x10` | `status` | HardFault: `HFSR`; MemManage/BusFault/UsageFault: `CFSR`; DebugMonitor: `DFSR`; otherwise 0 |
| `+0x14` | `fault_address` | HardFault: `MMFAR` if `(CFSR & 0xFF00) == 0` else `BFAR`; MemManage: `MMFAR`; BusFault: `BFAR`; otherwise 0 |
| `+0x18` | `exc_return` | `LR` at entry |
| `+0x1C` | `original_sp` | frame end `+ 4` if stacked `xPSR` bit 9 (alignment pad) |
| `+0x20` | `s16_s31[16]` | only when FP context active |
| `+0x20`/`+0x60` | `r4_r11[8]` | software-stacked |
| then | hardware frame | `r0-r3, r12, lr, pc, xpsr[, s0-s15, fpscr, pad]` |

Frame size = `0x60` (no FP) or `0xE8` (FP), as returned by `_rtk_exc_get_frame_size(frame)` (which tests `frame->exc_return & 0x10`; `NULL` ⇒ `0xE8`). Globals: crash counter `dword_4D289A4C`, class `dword_4D289A54`, frame pointer `dword_4D289A58`, frame size `dword_4D289A5C`.

**Exception class mapping** (RTKit's ARMv7-A vocabulary reused on M-profile): `IPSR → class` via bitmask `0x243F` over `(IPSR-1)` and the table at `0x7CA0`: Reset→0 `RESET`, NMI→7 `NMI`, HardFault→8 `HARDFAULT`, MemManage→4 `DATA_ABORT`, BusFault→4 `DATA_ABORT`, UsageFault→1 `UNDEF`, SVCall→2 `SVC`, PendSV→2 `SVC`; other system exceptions→9; external IRQs→5 `IRQ`. `btp_fault_exception_bridge` maps class `0..8` to the name table `const char *rtk_exception_names[9]` at `0x6B78` (`RESET, UNDEF, SVC, PREFETCH_ABORT, DATA_ABORT, IRQ, FIQ, NMI, HARDFAULT`; IDA mis-typed the first entry as the string `"!}"`), and to `error_code = class + 0x1E` (else `0x27`).

### 3.5 Crash Record & Crash Notification Descriptor **[asm]**
```c
struct btp_crash_record {            /* 0x4C bytes at 0x4D289200 */
    char     message[32];            /* "assert at line %d in %s" | "Assert at line %d in %s" | "Exception: %s" */
    uint8_t  image_identity[32];     /* copy of ROM 0xC0..0xDF */
    uint32_t error_code;
    uint32_t detail0;                /* exception: frame pointer; assert: 0 */
    uint32_t detail1;                /* exception: frame size;    assert: 0 */
};
struct btp_crash_descriptor {        /* built on the panic stack, copied to IPC base+0x5C1 (unaligned!) */
    uint32_t error_code;
    uint32_t trace_buffer;           /* btp_trace_get_buffer() */
    uint32_t trace_size;             /* 0x1FC */
    uint32_t detail0;
    uint32_t detail1;
};
```
`btp_assert_or_exception_panic(error_code, detail0, detail1, line, file, exception_msg)` – the last two arguments arrive on the stack (`arg_0`, `arg_4`) – first calls `btp_clock_set_panic_mode()` and `irq_save_and_disable()`, then fills the record, rings doorbell `0x0001`, and halts (code 5).

### 3.6 NVV Tag Catalogue (`nvv_parse_blob`) **[asm]+C**

| Tag | Name string | Presence bit | Pointer cell / consumer |
| :--- | :--- | :--- | :--- |
| `0x080B` | `global_phy_inits` | p0 `0x2` | `{addr, value}` register init list (`Modem inits:`) |
| `0x080E` | `rf_seq` | p0 `0x1` | `btp_nvv_tag_080E_ptr` → `0x4D15401C` RFSeq pointer |
| `0x0821` | `global_phy_msk_inits` | p0 `0x4` | `{addr, mask, value}` masked register init list |
| `0x0836` | `global_radio_msk_inits` | p0 `0x8` | `{addr, mask, value}` masked radio init list |
| `0x0857` | `mod_info` | p0 `0x40000` | `btp_module_info_ptr` (`DAUG/PLTY/PLTE/YNGR/ELDR`) |
| `0x1006` | `a4p_cal` | p0 `0x4000` | `tx_setup_isr` |
| `0x115B` | `a4p_cal_slopes` | p0 `0x8000` | `tx_setup_isr` |
| `0x1168` | `dynamic_hf_coeff` | p0 `0x10` | `btp_selected_hf_table_ptr` (kept only for module `DAUG`) |
| `0x1169` | `dynamic_mant` | p0 `0x80` | `tx_setup_isr` (replaced by `unk_4D289820` on radio rev 2) |
| `0x116A` | `dynamic_hf_delay_pty` | p0 `0x20` | → `btp_selected_hf_table_ptr` for `PLTY`, `YNGR`, missing/unknown module |
| `0x116B` | `dynamic_hf_delay_pte` | p0 `0x40` | → `btp_selected_hf_table_ptr` for `PLTE`, `ELDR` |
| `0x1170` | `dynamic_exp` | p0 `0x100` | `tx_setup_isr` (replaced by `unk_4D289818` on radio rev 2) |
| `0x1171` | `dynamic_if_delay` | p0 `0x200` | `tx_setup_isr` |
| `0x1172` | `dynamic_hf_delay` | p0 `0x400` | `tx_setup_isr` |
| `0x1173` | `dynamic_am_delay` | p0 `0x800` | `tx_setup_isr` |
| `0x1176` | `dpd_actuation_table` | p0 `0x1000` | len `0x3C0` → window A; `0x780` → window B; `0xB40` → A and B (`+0x3C0`); else `## DPD bad len %d!` |
| `0x117A` | `kvco_cal` | p0 `0x10000` | `tx_setup_isr` |
| `0x117C` | `tx_maxpwr_cal_v2` | p0 `0x2000` | `tx_power_init_and_interpolate_tables` |
| `0x117D` | `df1_mant_cal` | p0 `0x20000` | `tx_setup_isr` |
| `0x117E` | `rx_maxgain_cal` | p1 `0x200` | `rx_update_maxgain_tables` / FRS |
| `0x117F` | *(printed as `NVV_TAG_A0_RSSI_CAL`)* | p0 `0x100000` | `rx_setup_isr` RSSI offsets |
| `0x1180` | `df1_cal_v2` | p0 `0x1000000` | `tx_setup_isr` |
| `0x1182` | *(printed as `NVV_TAG_MXDC_CAL`)* | p0 `0x80000` | `rx_setup_isr` MXDC |
| `0x118C` | `kvco_cal_lp` | p0 `0x800000` | `tx_setup_isr` |
| `0x118D` | `df1_cal_v3` | p0 `0x2000000` | `tx_setup_isr` |
| `0x1190` | `df1_cal_v4` | p0 `0x4000000` | `tx_setup_isr` |
| `0x1191` | `lb_dcoc_cal_lp` | p0 `0x8000000` | `loopback_apply_dcoc_cal` |
| `0x1192` | `lb_dcoc_cal_hp` | p0 `0x10000000` | `loopback_apply_dcoc_cal` |
| `0x1193`–`0x119A` | `rxiqmm_ab_{lna1st,mix1st}_bw{1p5,2,2p5,3}` | p1 `0x2..0x100` | `rx_setup_isr` |
| `0x119B` / `0x119C` | `lbiqmm_ab_lp` / `_hp` | p0 `0x80000000` / p1 `0x1` | `loopback_apply_iqmm_ab_cal` |
| `0x119D` / `0x119F` | `lbiqmm_dcoc_lp` / `_hp` | p0 `0x20000000` / `0x40000000` | `loopback_apply_iqmm_dcoc_cal` |
| `0x11D8` / `0x11D9` | `frs_iqmm_ab_lna1st_bw3` / `_mix1st_bw3` | p1 `0x400` / `0x800` | `frs_save_state_and_enter` |
| SysCfg TLV `1` | *(host TX power table)* | p0 `0x200000` | `syscfg_apply` → overrides `btp_nvv_tag_117C_ptr` |
| SysCfg TLV `2` | *(host RSSI calibration)* | p0 `0x400000` | `syscfg_apply` → `btp_syscfg_rssi_cal` |

Tags `0x115C–0x1167, 0x116C–0x116F, 0x1174, 0x1175, 0x1177–0x1179, 0x117B, 0x1181, 0x1183–0x118B, 0x118E, 0x118F, 0x119E` are explicitly accepted but ignored. All 32 bits of `btp_nvv_presence0` are assigned (30 NVV tags + 2 SysCfg overrides); `btp_nvv_presence1` uses bits 0–11 only. Reconstruct as an `enum nvv_tag` plus a table-driven parser; the four `_nvv_outlined_*` helpers are just “store pointer + load u16 + touch presence word” fragments.

### 3.7 `ROM_RODATA` Lookup Tables (`0x6B9C`–`0x6C9C`) **[asm]**
All offsets below were checked against the raw ROM bytes and the indexing code that reads them:

| Address | Bytes | Shape / contents | Reader |
| :--- | :--- | :--- | :--- |
| `0x6B9C` | 8 | `uint8_t frs_band_channel_span[8] = {79,101,126,76,126,125,125,125}` | `frs_process_and_calibrate_results`, `frs_dump_rssi_readings` |
| `0x6BA4`–`0x6BDC` | 57 | 8 default FRS channel lists (`0x08,0x18,0x28,…` step 16), lengths `5,7,8,5,8,8,8,8`; targets of `dword_4D2892A0[8]` | `frs_run_multipass_scan` (when called with a NULL list) |
| `0x6BDD` | 8 | `uint8_t frs_default_channel_count[8] = {5,7,8,5,8,8,8,8}` (IDA: `dword_6B9C+0x41`) | `frs_run_multipass_scan` |
| `0x6BE8` | 16 | `const char *frs_status_names[4] = {"inv","clp","pll","agc"}` (`0x7E73..0x7E7F`) | `frs_dump_rssi_readings` |
| `0x6BF8` | 64 | 8 × 8-byte default max-gain calibration entries, indexed `[entry*8]`; used only when `btp_nvv_tag_117E_ptr == NULL` | `frs_save_state_and_enter` |
| `0x6C38` | 13 + pad | `uint8_t frs_fixed_gain_idx[13] = {65,60,55,…,5}` | `frs_save_state_and_enter` |
| `0x6C46` | 26 | `uint16_t frs_fixed_gain_code[13] = {0x00FE,0x00EA,0x00CE,0x00CD,0x008D,0x0094,0x40C0,0x401C,0x4008,0x500C,0x5800,0x6808,0x7318}` (IDA: `dword_6C38+0xE`) | `frs_save_state_and_enter` |
| `0x6C60` | 8 | `uint16_t frs_error_sentinel[4] = {0x7FFF,0x7FFE,0x7FFD,0x7FFC}` | `frs_process_and_calibrate_results` |
| `0x6C68` | 7 | `uint8_t rx_alt_rate_to_bw[7] = {0,0,1,0,0,0,0}` (selected when cmd0 bit 8 = 1) | `rx_setup_isr` |
| `0x6C6F` | 16 | `uint8_t rx_pkt_type_to_bw[16] = {0,0,0,0,0,0,0,0,2,2,3,2,2,2,0,0}` (IDA: `dword_6C68+7`) | `rx_setup_isr` |
| `0x6C7D` | ≥6 | `uint8_t rx_maxgain_band_offset[8] = {0,0,1,1,1,2,2,2}`, indexed by the calibration row index shifted right by one (IDA: `dword_6C74+9`) | `rx_update_maxgain_tables` |
| `0x6C85` | 8 | `uint8_t tx_alt_rate_to_pwr_type[8] = {3,3,4,3,3,3,3,3}` (IDA: `dword_6C74+0x11`) | `tx_power_apply_for_packet` |
| `0x6C8D` | 16 | `uint8_t tx_pkt_type_to_pwr_type[16] = {2,2,2,2,2,2,0,1,5,6,2,2,7,8,2,2}` (IDA: `dword_6C74+0x19`) | `tx_power_apply_for_packet` |

The `rx_maxgain_band_offset` base overlaps the tail of `rx_pkt_type_to_bw` by two bytes, so the two were probably separate arrays whose declared lengths differ from the slices above; keep them as distinct `const` arrays and let the linker place them.

---

## 4. Key Decompiler Artifacts & Reconstruction Considerations

When translating this pseudocode into clean, compilable C, several compiler optimizations and Hex-Rays decompiler quirks must be reversed:

### 4.1 Misidentified `MRS` / `MSR` Instructions (`__get_CPSR()`)
Hex-Rays maps almost all ARMv7-M `MRS Rx, <spec_reg>` instructions to `__get_CPSR()` (24 occurrences in `phyb.c`). **[asm]** The complete census of special-register and barrier instructions in the image is:

| Function | Actual instructions |
| :--- | :--- |
| `irq_save_and_disable` (`0x1E8`) | `MRS R0,PRIMASK; CPSID I` → `uint32_t primask = __get_PRIMASK(); __disable_irq(); return (primask & 1) == 0;` |
| `irq_restore` / `irq_enable` | `CPSIE I` |
| `_rtk_reset_init_stacks` (`0x5B50`) | `MSR MSP,R0; MSR PSP,R1; MRS R0,CONTROL; ORR #2; MSR CONTROL,R0` (switch Thread mode to PSP) |
| `_rtk_warm_boot_init` (`0x124`) / `_rtk_cold_boot_init` (`0x605C`) | `MSR PRIMASK,#1; MSR BASEPRI,R0`; cold path also `DSB SY; ISB` after CPACR |
| `_rtk_nmi_and_kernel_fault_entry` (`0x5BB4`) | `TST LR,#4; ITE EQ; MRSEQ R0,MSP; MRSNE R0,PSP; ... MSREQ MSP,R0; MRS R1,IPSR` |
| `_rtk_svc_pendsv_entry` (`0x5C04`) | `MRS R0,PSP ... MSR PSP,R0` (saves `r4-r11` and `s16-s31` onto PSP) |
| `_rtk_thread_context_switch` (`0x624C`) | `MRS R5,BASEPRI; MSR BASEPRI,R0; ... MSR BASEPRI,R5` (raise to `btp_rtkit_basepri` only if current BASEPRI is 0 or lower-priority) |
| `_rtk_cpu_idle_wfi` (`0x62D0`) | `MRS R0,PRIMASK`; if enabled: `MSR PRIMASK,#1; DSB; ISB; WFI; ISB; MSR PRIMASK,#0`; else `DSB; ISB; WFI; ISB` |
| `_rtk_cpu_halt_wfi` (`0x6B22`) | `MRS/MSR PRIMASK; DSB; ISB; WFI; ISB` (loop forever) |
| `_rtk_capture_fault_and_panic` (`0x6314`) | `MRS R4,BASEPRI; MRS R3,CONTROL; MRS R6,FAULTMASK; MRS R5,PRIMASK` plus SCB `CFSR/HFSR/DFSR/MMFAR/BFAR` reads |
| `btp_roundf_positive` (`0x4B54`) | `VMRS APSR_nzcv, FPSCR` after `VCMP.F32` – a float compare, not a system register read |

No `SVC`, `BKPT`, `UDF`, `LDREX/STREX`, `DMB`, `SEV` or `WFE` instructions exist in the image.

### 4.2 Compiler Machine Outlining (`_*_outlined_*`)
The firmware was compiled with Apple Clang using `-Oz` and **machine outlining**. The 30 functions with `_outlined_` in their names use non-standard register assignments (`@<R8>`, `@<R9:R0>`, `@<R10>`, `@<S16>`, `@<ZF>`) and artificial return structs for multi-register results. The reconstruction inlines these bodies; no separate C functions are emitted.
* **Action:** Do **not** keep these as separate functions in C. Inline their bodies directly back into their call sites. **[asm]** Semantics for the non-obvious ones:
  * `_btp_trace_outlined_current_record_base(base)` → `&ring->entries[ring->next_index]`; `_indexed_record_base(base, i)` → `&ring->entries[i]` (both are `base + 28*i`).
  * `_mac_outlined_set_resp_ready_flag` → `resp->flags |= 0x80`; `_mac_outlined_ring_frs_doorbell` → `doorbell = (doorbell & ~0xFFFF) | 4`; `_mac_outlined_log_cmd_error_18(v)` → `btp_trace_log_error(7, v, 0x12)`; `_mac_outlined_strrchr_slash*` → the `BTP_FILENAME` macro (§4.5).
  * `_frs_outlined_elapsed_systick(now, start)` → `start >= now ? start - now : start - now + reload` (SysTick counts down; wrap-corrected delta); `_frs_outlined_clear_agc_bit/_clear_trigger_bit` → single RMW bit clears.
  * `_tx_outlined_scale_and_convert(divisor@S0, numerator@S2)` → `btp_roundf_positive(numerator / (float)divisor)`; `_tx_outlined_uint32_to_float` → `(float)u32`.
  * `_dpd_outlined_copy_hw_buffer_save_r0(saved, dst, src)` → `memmove(dst, src, 0xBC0)` while preserving a live value in `R9`; `_dpd_outlined_log_buffer_word/_log_timestamps` → `btp_printf_stub(fmt, ...)` calls.
  * `_tx_power_outlined_select_radio_bank` → the `band == 0 ? 0x4D0002xx : 0x4D0000xx` selection (§4.7); `_tx_power_outlined_merge_pa_gain/_commit_pa_gain/_write_seq_code` → PA-gain RMW helpers.
  * `_phy_outlined_assert_reset(base@R4, delay_fn@R5)` → `base[0x6C/4] |= 0xF; btp_clock_delay_ticks(1);` (the function pointer is just the hoisted call target, see §4.8).
  * `_nvv_outlined_store_ptr_*` → `*cell = payload; presence = btp_nvv_presence0/1; len = *(uint16_t*)...` fragments; `_nvv_outlined_load_u8x4` → load four bytes for `nvv_module_id_compare4`.
  * `_vsnprintf_outlined_emit_char(sink@R10, ch)` → `sink->emit(sink, ch)`.

### 4.3 Trace Site Token Arithmetic (`site_token - 0x4D28D178`)
In `btp_trace_log_entry (0x314)`:
```c
v10 = site_token - 0x4D28D178;
```
Every call to `btp_trace_log_entry` passes an address in the range `0x4D28D178`–`0x4D28D2E0` (which lies just past the end of `.bss` at `0x4D28D174`). **[asm]** The tokens are loaded from literal pools as plain constants (`LDR R0,=0x4D28D1B6`), confirming they are link-time addresses of zero-size objects, and `RTSZ` in the metadata (§1.4) ends the RAM image exactly at `0x4D28D178`, i.e. the trace-site section is `NOLOAD` and sits after `.bss`.
* **Mechanism:** In the original source, a macro placed a dummy `char` or metadata struct into a dedicated `NOLOAD` linker section (e.g., `__attribute__((section("__DATA,__trace_sites")))`), and passed its address `&__trace_site_N` as the token. Subtracting the section base `0x4D28D178` yields a compact byte offset (`0x00` mac irq, `0x3E`/`0x55` irq init begin/end, `0x148` error, `0x154`/`0x168` early hw init, …) identifying the exact trace point without storing strings in ROM.

### 4.4 Cold Boot `.bss` Zeroing, RAM Image Reload and `.init_array` (`_rtk_cold_boot_init`) **[asm – corrected]**
The decompiler produced:
```c
btp_memset(a1: (int *)btp_msp_stack, a2: 0, a3: 0);
...
dst = 0;
btp_memcpy(&dst, src: &unk_4D2898AC, len: dword_4D2898A8);
```
* **What the code actually does:**
  1. **Three-piece `.bss` clear that skips the stacks.** RTKit sorts the two stack ranges and clears `[bss_start, lower_stack_base)`, `[lower_stack_top, upper_stack_base)` and `[upper_stack_top, bss_end)`. Here PSP (`0x4D289AC8–0x4D289EC8`) and MSP (`0x4D289EC8–0x4D28A2C8`) are adjacent, so the middle piece has length 0 – that is the `btp_memset(btp_msp_stack, 0, 0)` call. The other two calls are `memset(0x4D289900, 0, 0x1C8)` and `memset(0x4D28A2C8, 0, 0x2EB0)`. The stacks were already painted with `"RTKSTACK"` in `_rtk_reset_init_stacks`, which is why they are excluded.
  2. **`DSRL` metadata record.** `btp_memcpy(&dst, 0x4D2898AC, 1)` copies the 1-byte value of the `DSRL` (“data segment reload”) record (§1.4) into a local. If it is non-zero **and** `0x4D280000 > 0x88D2`, the firmware does `memmove(0x4D280000, (void*)0x88D2, 0x9900)` – i.e. it would copy a RAM image appended to the ROM (`[0x88D2, 0x88D2+0x9900)`) over `0x4D280000..0x4D289900` (shared IPC, NVV, status, trace, crash and `.data`). In this build the flag is `0`, so the host loader is responsible for populating RAM. Reconstruct as `if (rtk_meta_dsrl) memmove(__ram_start, __rom_end, __data_end - __ram_start);`.
  3. **Two empty constructor loops.** The first iterates `[0x88D2, 0x88D2)` (ROM-resident `.init_array`, empty), the second `[0x4D2898BD, 0x4D2898BD)` (`btp_init_array_boundary`, RAM-resident, empty). Both are `for (fn = __init_array_start; fn < __init_array_end; ++fn) (*fn)();` – the `BLX R0` sites are the only genuine indirect calls in the kernel.

### 4.5 Assertion Filename Extraction Macro
Throughout `mac_ipc_irq_handler` and `btp_report_hardware_exception`, you see:
```c
v3 = "/Library/Caches/.../mac.c";
if (strrchr(v3, '/') != nullptr)
    v3 = strrchr(v3, '/') + 1;
btp_assert_or_exception_panic(err, 0, 0, __LINE__, v3, nullptr);
```
* **Action:** Reconstruct this as a single macro:
  ```c
  #define BTP_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
  #define BTP_PANIC(err) btp_assert_or_exception_panic((err), 0, 0, __LINE__, BTP_FILENAME, NULL)
  ```
  **[asm]** Only two source paths survive in ROM (`.../w3/mac.c` and `.../w3/exception.c`); RTKit's own asserts use the bare `"threading.c"` with `_rtk_assert_fail(file, line, expr)`. The full path is `"/Library/Caches/com.apple.xbs/E06151E4-0C9A-48B8-A88C-98275C13C50B/TemporaryDirectory.vX0z3Y/Sources/WxBtPhy/firmware/w3/"`.

### 4.6 Consecutive `volatile` Bitfield Writes
Many functions perform multiple consecutive read-modify-writes to the same MMIO address, which Hex-Rays simplifies into algebraic expressions like:
```c
v1 = mmio_clock_4D159000;
mmio_clock_4D159000 = (v1 & 0xFFFEFFFD) + 2;
```
or:
```c
v0 = arm_scb_ccr;
arm_scb_ccr = v0 | 0x210;
arm_scb_ccr = v0 & 0xFFFFFDE7 | 0x210;
```
* **Action:** These originate from separate `volatile` bitfield assignments or register field macros executed in sequence (e.g., setting `STKALIGN` and `DIV_0_TRP`, then clearing `UNALIGN_TRP`). **[asm]** The listing shows the true shape: `LDR; ORR; STR; LDR; BIC; STR` – two independent RMW cycles, and `BFI`/`UBFX` (101 + 45 occurrences) for field inserts/extracts. Reconstruct them using clean bitfield structs or `REG_RMW(reg, mask, val)` macros, and keep each store as a separate statement so the access count matches.

### 4.7 Radio Bank Dual-Addressing (`0x4D0000xx` vs `0x4D0002xx`)
Notice that many radio registers exist in two mirrored banks separated by an offset of **`0x24C`** (or `0x150` depending on the sub-block):
* **Band 0 (2.4 GHz) vs. Band 1–7 (5 GHz / High Band):**
  * PA Gain: `0x4D0002F8` (Band 0) vs `0x4D0000AC` (Band ≠ 0) — delta `0x24C`
  * PRBS IQ: `0x4D0002FC` (Band 0) vs `0x4D0000B0` (Band ≠ 0) — delta `0x24C`
  * Loopback Ctrl 1: `0x4D000304` (Band 0) vs `0x4D0000B8` (Band ≠ 0) — delta `0x24C`
  * Loopback DCOC: `0x4D000308` (Band 0) vs `0x4D0000BC` (Band ≠ 0) — delta `0x24C`
  * Loopback Ctrl 2: `0x4D00030C` (Band 0) vs `0x4D0000C0` (Band ≠ 0) — delta `0x24C`
  * PLL Status: `0x4D000250` (Band < 2) vs `0x4D000100` (Band ≥ 2) — delta `0x150`
  * Radio Init: `0x4D000254`/`0x26C` vs `0x4D000104`/`0x11C` — delta `0x150`
  * RX Filter/LNA: `0x4D0002AC`/`0x2BC` vs `0x4D00015C`/`0x16C` — delta `0x150`
* **Action:** Define a unified C `struct radio_bank_regs` or parameterized bank base macro rather than hardcoding duplicate register addresses.

### 4.8 Register-Hoisted Call Targets (`BLX Rn`) **[asm]**
`-Oz` loads a function address once and calls it repeatedly through a register: `nvic_enable_irq` ×8 (`btp_irq_init_and_enable`), `btp_printf_stub` ×3+3+3, `systick_delay_cycles` ×5, `btp_memset` ×3 (`_rtk_cold_boot_init`), `btp_clock_delay_ticks` ×2 (`phy_hw_init`, via `BX R5` inside the outlined reset helpers). IDA marks most of these `DEOBF: Indirect call (unresolved)`; the decompiler may show function-pointer locals or drop the calls. **They are ordinary direct calls** – the `+1` in `LDR R6,=(nvic_enable_irq+1)` is just the Thumb bit. The only true function pointers are the `.init_array` loop (§4.4), the `emit` callback in `_vsnprintf_core` (§2.9), and the one-shot IRQ5 callback cell at `0x4D289900`.

### 4.9 Fall-Through Functions Split by IDA **[asm]**
Several “functions” are tail fragments reached by fall-through and should be merged in C:
* `btp_system_halt_on_code5(code)`: `CMP R0,#5; IT NE; BXNE LR` then falls into `btp_system_halt` → `_rtk_platform_halt(4)`. Reconstruct as `if (code == 5) btp_system_halt();`. Every caller passes 5, so the panic path always halts.
* `systick_delay_cycles(n)` ends in an `IT HI` whose conditional `LDRHI R0,[R1]` is the first instruction of `systick_wait_cycles` – one function: `if (n > reload) n = reload; wait(n);`.
* `_rtk_warm_boot_init`'s trailing `MOVS R0,#6; POP {R7,LR}; B btp_system_halt_on_code5` is its own (unreachable) epilogue for “`btp_main` returned”, not a separate routine; the cold path uses `_rtk_platform_halt(2)`.
* `IRQ7_Handler` is the `BX LR` inside `btp_nop_return_stub_D60` (`NOP; BX LR`); IRQ8–11, 13, 14, 17, 18, 21–24 are each a bare `BX LR`.
* `btp_report_hardware_exception` (`0x4A0C–0x4A48`) is one function despite IDA's earlier split at `0x4A26`.
* `frs_format_rssi_fixed_point` (`0x1B98`) is a 4-instruction prologue that `B.W`s into a code chunk at `0x4A84` which then falls into `btp_format_fixed_point_slot` (`0x4AB6`); IDA shows the chunk as belonging to the FRS function. In C it is one helper: `btp_format_fixed_point(float v)` (see §4.14).
* Halt codes passed to `_rtk_platform_halt`: `2` = main returned (cold), `3` = after fault capture, `4` = panic/abort/system halt, `6` = main returned (warm).

### 4.10 ROM Addresses Encoded as Small Immediates **[asm]**
Because ROM starts at address 0, the compiler encodes some pointers as 8-bit immediates: `MOVS R1, #btp_image_identity` (`#0xC0`) in `btp_assert_or_exception_panic`. The decompiler renders this as the integer `0xC0` passed to `memcpy`; it is `&btp_image_identity`. Likewise `MOV.W R0,#0x4D000000` in `btp_detect_radio_revision` is the radio base, not a magic constant.

### 4.11 Mis-typed Data in RODATA **[asm]**
* `asc_6B78 "!}"` is `const char *rtk_exception_names[9]` (pointers `0x7D21…0x7D5D` whose low bytes happen to be printable).
* `dword_4D2892A0[8]` in `.data` is `const uint8_t *frs_band_tables[8]` pointing into RODATA `0x6BA4..0x6BD5`.
* `dword_7C74` (12 zero words) + the class table at `0x7CA0` is one `uint8_t/uint32_t` exception-class lookup (§3.4).
* `btp_mac_shared_base`, `btp_trace_buffer_ptr`, `btp_loopback_stats_ptr`, `btp_loopback_control_ptr`, `ptr_btp_crash_record_ram`, and `btp_status_shared_base` are **initialized pointer cells in `.data`**. Their source-level `const` qualification is unknown. NVV register-write records can modify RAM cells (§4.18); replacing an observed load from such a cell with an absolute base address requires a stated invariant that the accepted NVV inputs do not establish. The TX power workspace and loopback consumers now preserve these cell loads; the other consumers still require a complete mutation/alias audit.
* `dword_4D28A2C8` serves both as the MSP top limit (exclusive) and as the trace-freeze flag word.

### 4.12 Unaligned Shared-Memory Accesses **[asm]**
`mac_ipc_send_crash_notification` stores five 32-bit words at `base+0x5C1, +0x5C5, +0x5C9, +0x5CD, +0x5D1`, and the coex handler reads a 32-bit word at `0x4D2801F9`. These compile to plain `STR.W/LDR` because the firmware clears `CCR.UNALIGN_TRP` (§2.1). In C, declare the mailbox structures `__attribute__((packed))` so the compiler does not “fix” the layout, and keep `UNALIGN_TRP` cleared.

### 4.13 Conditional Execution (`IT` blocks) **[asm]**
140 `IT` blocks (e.g. `IT NE; MOVNE R0,#0xB`, `ITT MI; MOVMI R1,#0x10; STRMI R1,[R0]`) become ternaries or `if` statements in the decompiler. They are benign, but note two patterns: `CMP x,#5; IT NE; BXNE LR` (early return, §4.9) and `ITE EQ; MRSEQ/MRSNE` (SP selection in the fault entry, §4.1).

### 4.14 Floating-Point ABI Oddities **[asm]**
* `frs_format_rssi_fixed_point(value@R0, scale@S16)` reads its scale from `S16`, a callee-saved VFP register that `frs_dump_rssi_readings` preloads with `VMOV.F32 S16,#0.25` after `VPUSH {D4-D8}` (Q14.2 quarter-dB → dBm). This is loop-invariant hoisting, not a real calling convention. The code then branches into the shared chunk at `0x4A84`: `ip = (int)x; frac = (int)((x - ip) * 100.0f)` (`flt_4AD0`), `frac = abs(frac)`, `slot = dword_4D289A34; dword_4D289A34 = (slot + 1) & 7;` and `btp_format_fixed_point_slot(slot, frac)` does `snprintf(&unk_4D2899B4[slot * 16], 15, "%d.%02d", ip, frac)` and returns that buffer. The 8-slot rotating ring exists so one `btp_printf_stub("%15s|%3d %15s|%3d %15s|%3d\n", …)` call can take three formatted strings at once.
* `btp_roundf_positive(x)` = `i = (int)x; if (x - (float)i >= 0.5f) i++; return i;` (`VCVT.S32.F32`, `VCVT.F32.S32`, `VSUB`, `VMOV.F32 S2,#0.5`, `VCMP`, `VMRS`).
* `VSTMDBEQ {S16-S31}` / `VLDMEQ {S16-S31}` in the exception entries are the RTKit lazy-FP context save, keyed on `EXC_RETURN` bit 4.
* `HIDWORD`/`__PAIR64__`/`LODWORD` (35 occurrences) come from the 48-bit timer reads (`btp_timer_get_*_timestamp`, two `LDR` + mask) and `__udivdi3`/`__aeabi_llsl/llsr` – use `uint64_t` directly.

### 4.15 Debug Print Stub **[asm]**
`btp_printf_stub` is `MOVS R0,#0; BX LR` (52 direct `BL` sites plus register-hoisted calls). All `"..."` debug strings in ROM are its arguments; argument expressions (e.g. `snprintf` into temporaries in `frs_dump_rssi_readings`, timestamp reads) are still evaluated. Reconstruct as `BTP_LOG(fmt, ...)` → `btp_printf(fmt, ...)` with an empty body, keeping the format strings so the ROM string table and the `"NVV %s (%x) %d bytes = %d"` tag names survive.

### 4.16 Switch Statement Inventory **[asm]**
| Location | Function | Cases | Selector |
| :--- | :--- | :--- | :--- |
| `0x5C8` | `mac_ipc_irq_handler` | 6 | IPC command id `0..5` |
| `0x77A` | `mac_ipc_irq_handler` | 24 | subcommand `0x0A..0x21` |
| `0x99A` / `0xBB6` | `mac_ipc_irq_handler` | 8 / 6 | request byte `1..8` / `0x15..0x1A` (subcmd `0x15`) |
| `0xE8E` | `frs_scan_channel_pass` | 4 | AGC/gain selector |
| `0x1094` | `frs_unpack_channel_bitmap` | 9 | bitmap byte fast path |
| `0x2196` | `rx_setup_isr` | 8 | band index (`cmd0[16:14]`), gated on `btp_nvv_tag_117E_ptr != NULL` |
| `0x2550` | `rx_set_iqmm_override` | 4 | IQMM parameter index `0..3` (`0xB` returned for `> 3`) |
| `0x2DE4` | `tx_setup_isr` | 8 | band index (`UBFX cmd0, #14, #3`) |
| `0x369E` | `dpd_handle_command` | 4 | DPD opcode |
| `0x3E68` | `tx_power_apply_for_packet` | 6 | power-mode selector |
| `0x5168` | `nvv_parse_blob` | 69 | `tag - 0x115B` |
| `0x5AA0` | `phy_configure_synth_band_mux` | 4 | band group (`0,3` / `1` / `2` / default) |
| `0x63E0` | `_rtk_capture_fault_and_panic` | 10 | `IPSR - 3` (fault class) |
| `0x64AE` | `_vsnprintf_core` | 88 | conversion char `- '#'` |

Hex-Rays recovers all of these as `switch`; keep them as `switch` statements (the `TBB/TBH` encoding implies dense case ranges in the original source).

### 4.17 Cross-Checking `phyb.c` Against `phyb.asm`
* `phyb.c` lists 219 functions; the IDA inventory has 232 (the 13 additional entries are 2–4 byte IRQ/nop stubs such as `IRQ8/9/10/11_Handler`, listed as `[empty]` in the C header and collapsed in `phyb.asm`).
* Prefer the asm for: exact register/field widths (`BFI/UBFX` operands), store ordering in MMIO sequences, `IT`-block early returns, literal-pool constants (site tokens; for floats read the raw bytes – IDA's `DCFS` text rounds to 6 significant digits), and anything involving `__get_CPSR`, `_R0 =`/`_R5 =` pseudo-assignments (7 sites) or `__asm` blocks (15 sites).
* Prefer the C for: control-flow shape of large `switch`es and the NVV tag bookkeeping, which the asm spreads across the outlined helpers.


### 4.18 Additional reconstruction checks against instruction bytes

* IQMM writes in RX setup and FRS entry use `PKHBT` to combine both calibration
  halfwords. The upper halfword comes from mixer table `band + 8`, or from IQMM
  override parameter 1. It must not be discarded when reconstructing the C.
* Cold boot uses one CCR RMW at `0x60E4`; warm boot uses two at `0x012C`.
  DSRL-enabled cold boot copies `0x9900` bytes from runtime address `0x88D2` to
  `0x4D280000`. The raw file's initialized-RAM payload starts at file offset
  `0x88E0`; file padding is not part of the runtime source mapping.
* Copy/fill routines group 32-bit reads and writes. Byte-loop adapters match
  final ordinary RAM contents but do not preserve those MMIO transactions.
* Override setters preserve the observed byte or word flag widths. A C `bool`
  argument can collapse host-supplied values such as 2 or 255 before storing.
  `coex_is_active` returns its full 32-bit stored word at `0x2964`.
* The formatter ignores precision for integer and string conversions. `%n`
  neither consumes an argument nor stores a result. Floating conversions emit
  `@`; the core leaves the sink's word at offset `0x10` unchanged. These are
  firmware behaviors, not claims of ISO C `printf` conformance.

* HF coefficient selection reads `SEQ_RF_CTRL` only on the stage-dependent
  paths. For a nonzero band and packet type 9 it tests stage 2 twice: the first
  match selects set 4, a match on the fresh fallback read selects set 3, and two
  nonmatches select set 1 (`0x3198..0x3220`, `0x34B8`).
* `tx_power_init_and_interpolate_tables` leaves `R0=0` when calibration exists,
  and `R0=1` when it is absent (`0x3BD8..0x3BE4`).
* `nvv_parse_blob` has a numeric raw-register result. A completed container leaves
  `R0=container_offset+8`, invalid magic returns the debug stub's zero, and size
  zero retains `0x835D`, the original `"Blob @ %x..\n"` address. A payload-pointer
  return type does not describe these instructions (`0x50C4`, `0x5724..0x5744`).
* `btp_roundf_positive` uses architectural `VCVT.S32.F32`, including its exception
  effects. A C cast alone does not specify its NaN or overflow behavior. The
  target reconstruction retains the 34 instruction bytes at `0x4B54..0x4B76`.

* NVV address validation is a pair of bit-mask exclusions, not a peripheral
  allowlist. It accepts RAM addresses such as `0x4D2895A8`. A valid `0x080B`
  register-write record can replace the TX power workspace cell there, or the
  loopback statistics/control cells at `0x4D2895B4` and `0x4D2895B8`. Consumers
  must follow those cells; their original initialized values are not invariants.

* Target `strlen` (`0x5E00..0x5E70`) uses aligned word loads, including bytes
  before the supplied pointer and after its first NUL. Its `0x01010101` literal
  is at `0x5E70`; the long-count `strncmp` (`0x5E88..0x5F54`) uses the same
  constant at `0x5E84`. Byte-loop replacements do not preserve these accesses.
* `strrchr` (`0x6AFE..0x6B22`) first locates the NUL, then searches backward.
  `strlcpy`/`strlcat` (`0x6A60..0x6AEA`) copy forward byte by byte, write the
  terminator, then call `strlen` on the remaining source. Precomputing source
  length or substituting `memmove` changes overlapping-input behavior. These
  statements describe the firmware instructions, not a portable library contract.
* TX setup activates the power-mode override only when the byte at
  `0x4D289938` equals exactly `1` (`0x2C6A..0x2C78`). Other byte values use
  the initialized/latched mode, with band 1 forced to mode 1. An accepted NVV
  register-write record can set that byte to `2` or `255`; a nonzero C test
  changes the resulting RF mode and dynamic calibration accesses.

* Loopback statistics may be redirected by the cells in §4.18. Correlator 0
  loads its Q sum and I maximum before incrementing the byte count, then loads
  its I sum (`0x4544..0x454E`). Correlator 1 preloads both sums and its I maximum
  (`0x45D6..0x45DA`); it does not increment the count. Sums use wrapping 32-bit
  addition, including when NVV has initialized them near the signed limits.
* Statistics reset writes the four minima in descending address order, then
  the four maxima in descending order (`0x4802..0x4814`). Readback loads all four
  sums before its first mean store (`0x4910..0x4930`). These access orders are
  observable when the accepted NVV records redirect the cells to a peripheral.
* Loopback configuration uses a byte mode read and fresh attenuation/mode/gain
  byte reads interleaved with radio RMWs (`0x471A..0x47EE`). Tone enable is read
  before the tone register; frequency is read after the enable writes
  (`0x4858..0x487C`). Caching one request word changes these transactions.

* The status pointer cell at `0x4D2895C4` is used by TX/RX latency updates and
  modem register restore (`0x5836..0x584C`). Warm-boot flag access
  (`0x4D58..0x4DA6`), telemetry readback (`0x4BC4..0x4C6A`) and version readback
  (`0x4C9C..0x4CD8`) use fixed RAM addresses. Their literal pools explicitly
  contain `0x4D285000`, `0x4D287008`, `0x4D287010` and `0x4D287034`; they do not
  dereference that cell. Register restore caches the pointer, rereads the count
  each iteration, and loads an entry's value before its destination address.
* Trace logging samples four payload words before its first record store,
  reloads the cursor before each remaining store, then reads it again for the
  wrapping increment modulo 18 (`0x0336..0x03E0`). A cached destination record changes overlap and wrapped-index
  behavior. The target uses 32-bit wrapped address arithmetic, including for
  NVV-supplied cursor values outside `0..17`; invalid physical mappings and
  resulting fault behavior remain outside the current modeled comparisons.
* Telemetry computes the mean with `SDIV` (`0x4C26`). Signed minimum divided by
  `-1` must not become an undefined C division expression. The target retains
  `SDIV`, including the processor's configured divide-by-zero behavior.

* Crash notification (`0x04EC..0x051A`) samples the first descriptor word before
  loading the mailbox cell, then alternates five 32-bit reads and five unaligned
  32-bit stores at mailbox offsets `0x5C1..0x5D1`. Aggregate assignment/memmove
  changes overlap propagation and transaction widths.
* MAC responses set ready with a halfword RMW, then write command as a halfword
  and status/length as bytes. The version payload uses four halfword stores.
  Nonvolatile C stores can be combined or reordered. Override selector reads
  are selector-specific (`0x0B60..0x0C06`): unused enable/value fields must not
  be fetched, and IQMM parameters are read as bytes after the enable byte.
* Unicorn 2.1.4 peripheral memory hooks can leave stale IT state across the
  `ITTEE LS` block emitted for the IQMM selector. The same ELF/input completes
  without the peripheral logging hooks (the isolated ACK response model remains).
  MAC comparisons now use an executed-instruction observer
  with no memory hooks; supported transfers are decoded and unsupported
  peripheral instructions fail closed. This observer has explicit oracle tests
  and does not establish full emulator or physical-peripheral correctness.

* DPD command entry (`0x368E..0x3698`) sets `result_armed=1` before reading the
  selector, including invalid selectors. SET_MODE preloads words 2/3, stores
  target mode/trigger, reads DPD_CTRL, then reads word 1; its debug argument
  evaluation rereads trigger, target mode and word 1 (`0x36A6..0x36CE`).
  ACTUATE and TEST preload words 1/2 before either state store. GET_RESULT
  samples capacity once (`0x36EC`), then reloads result pointer/count around
  the output stores; capacity and output aliases can change these results.
* DPD capture copies `0xBC0 = 752 * 4` bytes; test copies `0x780 = 480 * 4`
  bytes; actuation copies `0x3C0 = 240 * 4` bytes. Published result lengths
  `0x2F0` and `0x1E0` count 32-bit words. GET_RESULT writes two words and
  reports a response size of 8 bytes. The capture-done ISR copies samples but
  does not publish a new result pointer/count or clear capture_gtb.
* TX setup executes actuation, then test, then capture. The test request flag
  is cleared before its wait; synchronous capture clears capture_gtb after
  the copy/dump. Actuation reporting clears actuation_gtb only when capture_gtb
  is zero; actuation_enable remains set. These transitions are tested with
  static and scripted-progress GTB responses; device progress/timing and
  concurrent aperture writes remain unverified.

* Accepted NVV `0x080B` records can store a readable ROM address in the DPD
  actuation-pointer cell `0x4D28AA94`. TX setup then copies `0x3C0` bytes from
  that address to `0x4D148800` (`0x3122..0x314E`). Executable bytes are therefore
  data-observable under the scalar model; relocating/recompiling code changes
  this output. The current rebuilt ELF diverges for pointers `0x108`, `0x10C`
  and `0x200`, including completely unhooked execution. The RAM control matches.
  Physical ROM data-read attributes remain unknown. The reproducible input,
  complete output bytes, assumptions and failing gate are documented in
  `verification/rom-observability.md`; no input-domain restriction was added.
* FRS entry saves `SEQ_RF_CTRL` once and uses that sampled value for its first
  mode write (`0x15F2..0x1600`). A second RMW read changes the transaction trace
  and consumes an extra changing-register response. Later channel-latch writes
  use fresh reads; the first saved-value write must not introduce another read.
* FRS channel tuning clears the channel latch, then rereads `SEQ_RF_CTRL` before
  setting it (`0x0DD0..0x0DDE`). PLL failure bits 18 and 17 come from separate
  status loads (`0x0DF4`, `0x0DF6`). The AGC table read precedes the gain-force
  register read (`0x0ED2`, `0x0ED6`). An enabled center bin rereads
  `0x4D150AF8` after its earlier clipping check (`0x0FAC..0x0FCA`).
* AGC selector 0 jumps directly to `0x0ECC` without initializing stack slots
  `SP+8`/`SP+0xC` on the first channel. Those slots hold the gain-table index
  base and address after selectors 1..3. A zeroed entry stack and gain index 45
  produce an unmapped modeled read at `0xFFFFFF4C`, PC `0x0ED2`; the former C
  fallback read `0x4D15044C` and returned. The target now retains the native
  scan frame/instructions and address `0x0D7C`; matching failed-access, register
  and frame observations plus seeded QEMU cases verify this correction. Physical
  exception delivery and selector-zero reachability remain unknown. Reproduce with
  `tools/probe_frs_scan.py --selector-zero` and the ELF/output arguments.
* ELF section bytes are not sufficient evidence of a loaded image. Earlier
  backwards ROM section ordering left 28 build-string and 43 FRS-message bytes
  outside every PT_LOAD segment; actual modeled memory contained zeros. ROM
  sections are now ordered by address, and allocated file-backed sections are
  checked against PT_LOAD bytes. The entire readonly bank `0x6B4C..0x88D2`
  is restored and checked in loaded memory. Executable/vector bytes still differ.
* NVV can set the wakeup callback cell `0x4D289900` to the original stub address
  `0x41D`. The reference stub at `0x41C` is `MOVS R0,#0; BX LR`; the earlier
  rebuilt layout faulted at that address. The C stub now retains this address
  and its four bytes. Scalar callback and controlled register-capture cases
  verify the correction; physical IRQ delivery remains outside that fixture.

### Diagnostic response/descriptor overlap (`0x4DBC`) — sampling order

The descriptor checksum (`+0xC`) and 16-bit length (`+8`) are both loaded before
any response-header store. The header stores occur as STRH at `dst+1`, STRB at
`dst+0`, STR at `dst+6`, STRH at `dst+3`, and STRB at `dst+5`. The descriptor's
base pointer (`+4`) is loaded after those stores. Thus a response overlapping its
own descriptor uses the old checksum/length and the new base pointer. Ordinary
structure assignments and a nonvolatile descriptor let the C compiler sample
these fields at different times. The C reconstruction now preserves the
preloads, late base load, store order and widths with volatile accesses.

`tools/probe_diag_alias.py` covers all three descriptor slots, zero-copy overlap
positions, length/checksum overlaps, count truncation boundaries and a mapped
base-pointer substitution caused by the response header. Its 2100 cases run with
all Unicorn hooks removed. `tools/qemu_diag_alias.py` runs the corresponding
matrix independently on Cortex-A15 Normal memory, without patching reference
instructions: each engine reports 1409 divergences for the previous C object and
zero after the correction. These results establish the recorded mapped-memory
cases; arbitrary stack aliases, faults, interrupts and physical execution remain
unverified. A separate reassembly result is not evidence that these C
implementations are correct and is excluded from the C-only deliverable.

The same mismatch is reached through a real NVV `0x080B` mailbox-cell write
followed by MAC subcommand `0x1C` (op 1, nonzero offset, zero requested length).
Across 216 scalar workflows, the retained old ELF diverges in 144 and the
corrected ELF in zero; instruction observation and fully unhooked execution
agree. The source-access sequence is also checked directly in a disjoint RAM
case by `verify_diag_sampling.py`, which detects the old ordering even when
final response bytes happen to agree. `symbolic_diag_alias.py` proves 27 fixed
zero-copy geometries for arbitrary 32-bit offsets and symbolic descriptor bytes
under its explicit entry-state/model assumptions; it retains 212 SMT queries
and detects both the old bug and an altered copied-count store. These are
conditional results, not arbitrary-stack or physical-device proofs.


### C recovery of the 64-bit shift helpers (`0x5F54`, `0x5F76`)

Both `__aeabi_llsl` and `__aeabi_llsr` now use the same C bodies on host and target.
The effective amount is the low eight bits of the 32-bit argument; amounts
64..255 produce zero. In particular, argument 256 leaves the value unchanged,
-255 shifts by one and -1 produces zero. Every individual C word shift is
bounded to 0..31, avoiding undefined out-of-range shifts. This replaces the
retained Thumb bodies and corrects the previous host-only amount handling.

`probe_c_shifts.py` compares 16384 concrete cases against unmodified reference
instructions, with memory hooks removed, including numerical results, SP and
callee-saved registers. The symbolic checks cover all 64-bit values, all 32-bit
argument bit patterns and initial NZCV under their declared model. Neither
check establishes caller-clobbered register/flag results, execution timing,
arbitrary stack aliases or whole-firmware equivalence. The retained division
body is now checked with decoded BL relocations to the C helpers rather than
requiring their former code sizes and displacement geometry.
