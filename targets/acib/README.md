# Apple `acib` (ACI / Bluetooth MAC Controller Firmware) — C11 Reconstruction

Recompileable freestanding ARMv7E-M (soft-float) + native host C11 reconstruction and differential verification suite for `acib`, the Apple Bluetooth / ACI MAC controller firmware (`b515d` / `AirPods3,6`, product `3.91.1.0`, running on `RTKit_release-3514.0.15.release`).

## Quickstart

All build, layout, anti-idiom, load-image, Unicorn differential execution, and stub-mutation checks run reproducibly out-of-tree:

```sh
make -C targets/acib all check verify verify-semantics BUILD=/tmp/acib-build
```

Individual targets:

- `make -C targets/acib arm BUILD=/tmp/acib-build` — Cross-compiles freestanding ARMv7E-M (`clang -target armv7em-none-eabi -mcpu=cortex-m4 -mfloat-abi=soft -mthumb -std=c11 -Oz`) into `/tmp/acib-build/acib.elf` and `/tmp/acib-build/libacib_reconstructed.a`.
- `make -C targets/acib host BUILD=/tmp/acib-build` — Compiles the reconstructed C11 modules natively (`HOST_CC`) against the 64-bit host MMIO/shared-RAM bus harness into `/tmp/acib-build/acib_tests`.
- `make -C targets/acib check BUILD=/tmp/acib-build` — Verifies `_Static_assert` ABI/register/mailbox layouts on both 32-bit ARM and 64-bit host, and runs 128 host behavioral assertions.
- `make -C targets/acib verify BUILD=/tmp/acib-build` — Runs `tools/verify_fidelity.py` (anti-idiom + `@provenance` RVA vs `verification/inventory.json` + 9,917-function ledger audit) and `tools/verify_layout.py` (ELF section & 35 pinned symbol VMAs vs linker map).
- `make -C targets/acib verify-semantics BUILD=/tmp/acib-build` — Runs `tools/verify_load_image.py` (6 binary-backed sections = `84,362` bytes byte-exact vs raw `acib`, plus `12/12` `SHF_ALLOC` sections verified against `PT_LOAD`) and `tools/arm_semantics.py` (`200` Unicorn `UC_CPU_ARM_CORTEX_M4` Thumb-2 differential execution test cases across all `120 / 120` reconstructed C functions vs raw `acib` including `pending_c` callee/argument/varargs verification, `0` divergences + `0 / 357` 3-stub mutation survivors).

---

## Firmware Identity

| Property | Value |
| :--- | :--- |
| **Raw Image** | `../../../acib` (`1,066,208` bytes = `0x1044E0`) |
| **SHA-256** | `dda0b224409bb5997dd95374312f13a207591e857edf9f2cff234ae86b66795e` |
| **IDA Database** | `../../../acib.i64` (`9,917` functions; `469` surviving strings) |
| **RTKit Runtime** | `RTKit_release-3514.0.15.release` (ROM `0x0F8860`) |
| **Product / Build Tag** | `3.91.1.0` (ROM `0x0F5E40`; `Built on 17-Aug-2026 at 17:44` @ `0x0F5360`, `SDK version 9A347` @ `0x0F5380`, `b515d` / `AirPods3,6`) |
| **Image UUID** | `e8ded9ec-b944-30a3-9c80-1069bd1d094b` (ROM `0x0000D0`) |
| **CPU / ABI** | ARMv7E-M Thumb-2, **soft-float** (`0` hardware VFP instructions; software IEEE-754 helpers in `0x0E50B8..0x0E564B`) |
| **RTKit Metadata** | `0x0F9640..0x0F9756` (`0x116` B, 23 TLVs: `STKG=0x0AFF`, `RTSZ=0x1002D2C4`, `TXSZ=0x0F984C`, `DSRL=1`) + 61 constructors (`0x0F9758..0x0F984C`) |

---

## Memory Map & Binary-to-Memory Layout

### File & Runtime Segments (`fwsg` v1 Segment Trailer at `0x104480..0x1044E0`)

| Runtime VMA | Size | File Offset | Segment / Section | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x00000000..0x000000BF` | `0x000C0` (`192` B) | `0x000000` | `ROM_VECTORS` (`.vectors`) | Primary 48-slot Cortex-M vector table (`Initial MSP = 0x1001BD80`, `Reset = 0x109`, `NMI = 0xE4A95`, `Fault = 0xE4A51`, `SVC/PendSV = 0xE4B15`, `SysTick = 0xE58F1`; byte-exact C in `src/vectors.c`). |
| `0x000000C0..0x00000107` | `0x00048` (`72` B) | `0x0000C0` | `RTKIT_UUID_HEADER` (`.rtkit.header`) | RTKit `'uuid'` v5 image header (`0xC0..0x108`). |
| `0x00000108..0x000003FF` | `0x002F8` (`760` B) | `0x000108` | `ROM_TEXT` | `Reset_Handler` (`0x108`: warm-resume check `0x10000074 == 0xAA11CE55`, `.bss` zero `0x1000AC40..0x1002D2C4`, `STKG` seed, static constructors `0xF9758..0xF984C`, `R7 = 0x77777700`, `main_entry` @ `0x71A50`). |
| `0x00000400..0x0000063F` | `0x00240` (`576` B) | `0x000400` | `ROM_TEXT` (`.vectors.secondary`) | Secondary 144-slot runtime VTOR table (`SCB->VTOR = 0x400` set in `sub_707F0`, routing SysTick and all 128 external IRQs `[15..143]` through `0x0E58F1` -> `rtk_nvic_dispatch_irq` @ `0x0E5B0C`). |
| `0x00000640..0x000EFEA7` | `0xEF868` (`981,096` B) | `0x000640` | `ROM_TEXT` (`.text`, `.text.nmi`) | ACI / Bluetooth MAC stack, W3 PHY IPC bridge, OSA, TraceManager, RTKit kernel, libc (`NMI_Handler` pinned byte-exact at `0x0E4A94..0x0E4B14`, `SysTick_Handler` pinned at `0x0E58F0`). |
| `0x000EFEA8..0x000F984B` | `0x099A4` (`39,332` B) | `0x0EFEA8` | `ROM_RODATA` (`.rom.rodata`) | Stack descriptors (`0xF52D4`/`0xF52E0`), build tags, lookup tables, RTKit TLVs (`0xF9640..0xF9756`), 61 constructors (`0xF9758..0xF984C`); followed by `0x14` B file pad (`0x0F984C..0x0F985F`). |
| `0x10000000..0x1000AC1D` | `0x0AC1E` (`44,062` B) | `0x0F9860` | `RAM_DATA` (`.ram.data`) | Initialized SRAM globals (`0x0F9860..0x10447E`); followed by `0x2` B pad + `0x60` B `fwsg` v1 segment trailer (`0x104480..0x1044E0`). |
| `0x1000AC1E..0x1002D2C3` | `0x226A6` (`140,966` B) | *(NOLOAD)* | `RAM_BSS` (`.ram.bss`) | Zero-init SRAM (`.bss` zeroed `0x1000AC40..0x1002D2C4`), including PSP init stack (`0x1001AF80..0x1001B580`, 1.5 KB), MSP stack (`0x1001B580..0x1001BD80`, 2 KB), NVIC/scheduler/BT/OSA/PHY state. |

### MPU Protection Regions (`rtk_early_hw_init` @ `0x07DEB8`, 7 Regions)

| Region | Base VMA | Size (`order`) | Attributes (`TEX/SCB`, `XN`, `AP`) | Role |
| :---: | :--- | :--- | :--- | :--- |
| 0 | `0x40000000` | `512 MB` (`0x1C`) | Device (`0x00100000`), `XN=1`, `AP=1` (Priv RW) | Peripheral MMIO window (`0x42440000` ACI/PMGR, `0x4D154000` W3 PHY Seq, `0x520DC000..0x52205000` BT/RF) |
| 1 | `0x00000000` | `1 MB` (`0x13`) | Normal (`0x00080000`), `XN=0`, `AP=5` (Priv RO) | Controller Flash / ROM (`0x00000000..0x000FFFFF`) |
| 2 | `0x10000000` | `256 KB` (`0x11`) | Normal (`0x000C0000`), `XN=1`, `AP=3` (Full RW) | Controller TCM / SRAM (`0x10000000..0x1003FFFF`) |
| 3 | `0x521C0000` | `64 KB` (`0x0F`) | Normal Non-Cacheable (`0x00200000`), `XN=1`, `AP=1` | BT Hardware / Event-Trigger Shared Window (`0x521C0000..0x521CFFFF`) |
| 4 | `0x20000000` | `16 MB` (`0x17`) | Normal (`0x000C0000`), `XN=1`, `AP=1` (Priv RW) | SoC Shared SRAM Aperture (`0x20000000..0x20FFFFFF`) |
| 5 | `0x4D280000` | `4 KB` (`0x0B`) | Normal Non-Cacheable (`0x00200000`), `XN=1`, `AP=1` | **Shared MAC (`acib`) <-> W3 PHY (`phyb`) Mailbox SRAM** (`0x4D280000..0x4D280FFF`) |
| 6 | `0x000F0000` | `64 KB` (`0x0F`) | Normal (`0x00080000`), `XN=0`, `AP=1` (Priv RWX) | High ROM / `.rodata` + Constructor Table Window (`0x000F0000..0x000FFFFF`) |

---

## MAC (`acib`) <-> W3 PHY (`phyb`) Shared Interface Contract

`acib` (the Bluetooth MAC / ACI controller) is the **master** of the shared W3 PHY mailbox (`0x4D280000`) and sequencer doorbell block (`0x4D154000`), directly pairing with `phyb`'s slave-side handlers (`mac_ipc_irq_handler`, `frs_*`, `btp_crash_notify_mac`).

### 1. Shared Doorbell & Sequencer MMIO (`0x4D154000`)

| Register Address | Name | Access in `acib` | Bit / Protocol Semantics |
| :--- | :--- | :--- | :--- |
| `0x4D154038` | `PHY_SEQ_RF_CTRL` | R/M | Read by `phy_seq_rf_ctrl_apply_tx_mode` (`0x097920`) to coordinate RF TX/RX mode bits `[29:27]` before updating RF control registers. |
| `0x4D154C64` | `PHY_IPC_TO_MAC_STATUS` | R / W1C (`phy_ipc_irq_handler` @ `0x07CD5C`) | **Bit 0 (`0x0001`)**: PHY fatal crash notification -> reads 20B unaligned crash record at `0x4D2805C1..0x4D2805D4`, sets `g_acib_panic_reason = 0x5D`, panics.<br>**Bit 1 (`0x0002`)**: PHY warm-boot ack (`phy_ipc_wait_warm_boot_ack` @ `0x07D264`).<br>**Bit 2 (`0x0004`)**: FRS scan complete.<br>**Bit 15 (`0x8000`)**: Synchronous/asynchronous command ack (`phy_ipc_wait_and_ack_cmd` @ `0x07CABC`, `phy_ipc_on_cmd_ack_clear_flags` @ `0x084E74`). |
| `0x4D154C68` | `PHY_IPC_FROM_MAC_DB` | W (`0x07A0DC`, `0x07A104`, `0x07CB4C`, `0x07DB44`) | **Bit 0 (`0x0001`)**: Start Fast RSSI Scan (FRS) — triggered via SW or HW timer DMA (`phy_hw_trig_frs_start_arm` @ `0x07A0DC`).<br>**Bit 1 (`0x0002`)**: Abort timed-out FRS scan (`phy_hw_trig_frs_abort_arm` @ `0x07A104`, >15,000 us -> error `0x18`).<br>**Bit 2 (`0x0004`)**: Coexistence grant update (`phy_ipc_coex_ring_doorbell` @ `0x07DB44`).<br>**Bit 15 (`0x8000`)**: Command request (`phy_ipc_ring_cmd_doorbell` @ `0x07CB4C`). |

### 2. Shared Mailbox SRAM (`0x4D280000..0x4D280FFF`, MPU Region 5)

`phy_ipc_init` (`0x07C820`) sets `g_phy_ipc_state.req_base = 0x4D280000` (`0x1002268C`) and negotiates protocol `{3, 0x5B, 1, 0}` (`3.91.1.0`): on W3 B0 (`phyb`), `resp_base` (`0x10022690`) is `0x4D280400` (legacy fallback: `0x4D2801E6`).

| Address (`0x4D280000 + off`) | Window | Size | Master / Slave Direction & Structure |
| :--- | :--- | :--- | :--- |
| `0x4D280000` (`req + 0x000`) | Command Header | `4 B` | `u16 flags` (`+0x000`), `u16 opcode` (`+0x002`: `0` handshake, `2` mode, `3` ready check, `4` async sleep, `6..13` RF/cal/modem). |
| `0x4D280004` (`req + 0x004`) | Command Payload | `188 B` | Command arguments (`u16[4]` handshake `{3, 0x5B, 1, 0}`, `u8`/`u16`/`u32` RF parameters, or payload copied via `phy_ipc_clear_cmd_status_flag` @ `0x07CB70`). |
| `0x4D2800C0` (`req + 0x0C0`) | Dekker Spinlock | `4 B` | `u16 mac_want` (`+0x0C0`), `u16 phy_want` (`+0x0C2`) — acquired/released by `phy_ipc_sync_shared_table_dekker` (`0x084EB8`) with `DMB SY`. |
| `0x4D2800C4` (`req + 0x0C4`) | Link/Chan Table | `288 B` | Active MAC link/channel table copied from `0x10021EA9` (`0x120` bytes) under Dekker lock (`0x084EB8`). |
| `0x4D2801E4` (`req + 0x1E4`) | FRS Request | `20 B` | Fast RSSI Scan descriptor written by `phy_ipc_frs_prepare_scan` (`0x07D35C`): mode, timing, channel count, and 16-byte channel bitmap (`+0x1E8..+0x1F7`). |
| `0x4D2801F9` (`req + 0x1F9`) | Coex Grant | `4 B` | **Unaligned** `uint32_t` coexistence grant word written by `phy_ipc_coex_ring_doorbell` (`0x07DB44`) before ringing bit 2. |
| `0x4D280408` (`resp + 0x008`) | PHY FW Version / Ack | `8 B` | Response status / four `uint16_t` PHY version fields verified during `phy_ipc_init` (`0x07C820`). |
| `0x4D2804C0` (`resp + 0x0C0`) | FRS RSSI Array | `252 B` | Up to 126 signed `int16_t` RSSI values (`phy_ipc_frs_run_scan` @ `0x07D9D4` packs 126 x 12-bit samples into 189B 24-bit LE pairs). |
| `0x4D2805C0` (`resp + 0x1C0`) | FRS Status | `1 B` | Pre-filled to `0xFF` by MAC before ringing FRS doorbell; cleared to `0x00` by `phyb` when RSSI results are ready (`0x07D470`). |
| `0x4D2805C1` (`resp + 0x1C1`) | PHY Crash Copy | `20 B` | **Unaligned** 5 x `uint32_t` crash summary written by `phyb` (`btp_crash_notify_mac`) and unpacked by `acib` (`0x07CD5C`). |
| `0x4D2805D5` (`resp + 0x1D5`) | PHY Cal Status | `41 B` | Calibration/telemetry status block read by `phy_ipc_cmd_set_op6_mode` (`0x07CA90`), `phy_ipc_cmd_op9` (`0x07D2EC`), and `phy_ipc_cmd_op10` (`0x07D300`). |
| `0x4D2805FE` (`resp + 0x1FE`) | FRS Aux Metrics | `126 B` | Per-channel auxiliary AGC/gain bytes copied into `0x100017E7` (`phy_ipc_frs_copy_results` @ `0x07DB60`, `phy_ipc_frs_read_window` @ `0x07DBBC`). |

---

## Verification Summary

| Check | Tool / Target | Result |
| :--- | :--- | :--- |
| **Function Inventory & Ledger** | `tools/verify_fidelity.py` | **9,917 / 9,917** IDB functions accounted (`120 reconstructed_c`, `1 retained_native`, `9,796 pending_c`), **0** anti-idioms |
| **Linker & Section Layout** | `tools/verify_layout.py` | **12** output sections & **35** pinned VMAs exact, `unpinned_bss = 0` |
| **Load-Image & `PT_LOAD` Fidelity** | `tools/verify_load_image.py` | **6** binary-backed sections (**84,362** bytes) **100% byte-identical** to raw `acib` + **12 / 12** `SHF_ALLOC` sections byte-verified against `PT_LOAD` |
| **Host Behavioral & Layout Suite** | `tests/layout.c` + `acib_tests` | **128 / 128** host checks passed across boot/MPU, exceptions, NVIC/IRQ, libc, PHY IPC, ACI trace/OSA/BT |
| **ARMv7E-M Differential & Stub Audit** | `tools/arm_semantics.py` | **200 / 200** Unicorn `Cortex-M4` differential cases (incl. `pending_c` callee/arg/varargs trace) across **120 / 120 (`100%`)** `reconstructed_c` functions, **0 divergences**, **0 / 357** stub-mutation survivors |
