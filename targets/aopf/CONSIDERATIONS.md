# Binary Analysis & Engineering Considerations — `aopf`

## 1. Image Identity & Container Geometry

- **Product Banner (`0x2080C290`)**: `b515daop:MAIN:DEVELOPMENT:AppleSPUFirmwareBuilder-990.0.13~5644`
- **RTKit Runtime (`0x00073958`)**: `RTKit_release-3514.0.15.release`
- **Battery Component (`0x00070A10`)**: `BNI:AudioProductsBattery-900.106~38`
- **UUID (`0x000000D0`, `uuid` v5 header at `0xC0..0x108`)**: `61724c3e-0144-3e90-8882-7a4a637bd44a`
- **SoC & Core**: Apple `T2016` / `b515d` (`AirPods3,6` / Laguna; metadata TLVs at `0x000739BC..0x00073AEE` carry `STKG = 0x00000AFF`, `SOC_ = 0xFFFFFFFF`, `SOCR = 0xFFFFFFFF`, `RTSZ = 0x2089BED8`, `TXSZ = 0x00077B14`, `DSRL = 0x00`, `RTLL = 0x01`), ARMv7E-M Thumb-2 Cortex-M4F with single-precision FPU (`FPv4-SP-D16`, enabled in `rtk_start` via `SCB->CPACR = 0x00F00000` and lazy stacking `FPU->FPCCR = 0xC0000000`).
- **Boot vs Runtime Vector Tables**:
  - Boot `.vectors` at `0x00000000..0x000000C0` (`48` entries) has `vec[0] = 0x00000000` and `vec[1] = 0x0000693D` (`Reset_Handler`, which explicitly loads `MSP = 0x2089BA10` and `PSP = 0x2089A890`).
  - Runtime VTOR table at `0x00064000..0x00064178` (`94` entries, programmed into `SCB->VTOR`) has `VTOR[0] = 0x2089BA10` (`MSP_TOP`) and `93` Thumb handler pointers.
- **Segment Reload Trailer (`fwsg` v1 at `0x00091BE0..0x00091C40`)**:
  - `__TEXT`: `vmaddr = 0x00000000`, `fileoff = 0x00000000`, `filesize = 0x00077B14`, `vmsize = 0x00077B14`, `flags = 0x4` (`ROM_VECTORS` `0x0..0xC0` + `ROM_TEXT` `0xC0..0x64000` + `ROM_RODATA` `0x64000..0x77B14`; `0x77B14..0x77B20` is 12 zero alignment bytes in the file).
  - `__DATA`: `vmaddr = 0x20800000`, `fileoff = 0x00077B20`, `filesize = 0x0001A0B0`, `vmsize = 0x0009BED8`, `flags = 0x0` (`DSRL = 0`: `rkos` preloads `RAM_DATA` `0x20800000..0x2081A0B0` from `fileoff = 0x77B20`, followed by `0x81E28` bytes of zero-initialized `RAM_BSS` up to `0x2089BED8`).
  - Note: Unlike `rkos`, `aopf` executes in place from its load aperture and does **not** self-copy `RAM_DATA` during `Reset_Handler`; the stage-1 loader (`rkos`) materializes `__DATA` from `fileoff = 0x77B20` using the `fwsg` trailer. On warm resume (`*0x20819CA0 == 1`), `Reset_Handler` (`0x693C`) jumps via `*0x20819CA4`; on cold boot, `rtk_start` (`0x4E2A0`) zeroes `RAM_BSS` `[0x2081B000..0x2089BED8)` in three slices preserving the active PSP (`0x20899490..0x2089A890`) and MSP (`0x2089B610..0x2089BA10`) painted with `"RTKSTACK"`.

## 2. Mixed Embedded C++ (`AppleSPU`) and RTKit C Architecture

Disassembly and `.rodata` inspection establish that `aopf` links two distinct ABI/language layers into one monolithic Thumb-2 image:

1. **Embedded C++ (`-fno-rtti -fno-exceptions`) Application & SPU Framework**:
   - `ROM_RODATA` contains **160 Itanium C++ ABI vtables** (`offset-to-top = 0`, `typeinfo = NULL` confirming `-fno-rtti`) referencing **1,139** virtual method targets, plus **13 static global constructor pointers** in `.init_array` at `0x00077AE0..0x00077B14` invoked sequentially by `rtk_start` (`0x4E2A0`).
   - Uses `__cxa_guard_acquire` (`0x5DD58`), `__cxa_guard_release` (`0x5DE54`), `__cxa_pure_virtual` (`0x5DEAC`), `__cxa_atexit` (`0x6CE4`, no-op returning `0` in firmware), `operator new` (`0x5DEB8`), and `operator delete` (`0x5DEE0`).
2. **RTKit C Runtime & Platform Drivers**:
   - `ROM_RODATA` (`0x00073AF0..0x00074050`) contains **25 RTKit C driver vtables** (`218` method targets) where `+0x00` is `ioctl_fn(dev, cmd, arg0, arg1)` with no Itanium `[-2]`/`[-1]` header words.
   - Modeled in C11 (`include/aopf/shared.h`) using explicit typed vtable structs (`rtk_nvic_vtable_t`, `rtk_systick_vtable_t`, `rtk_timebase_vtable_t`, `rtk_wdt_vtable_t`, `aop_lock_vt_t`) and `_Static_assert`-verified struct layouts (`tests/layout.c`).

## 3. Peripheral MMIO Map & Cross-Core Shared Hardware

| Base Address | Size | Driver Global (`RAM_DATA`) | Role & Cross-Target Sharing |
|---|---:|---|---|
| `0x4002C000` | `0x4000` | `0x2080CD20` | Shared AKF hardware mailbox aperture |
| `0x42004000` | `0x4000` | `0x2080C900` | AOP SCM / DMA engine for SPI0 |
| `0x42008000` | `0x1000` | `0x2080C8B8` | AOP SPI0 controller (inertial / acoustic sensors) |
| `0x4200C000` | `0x4000` | `0x2080C998` | AOP SCM / DMA engine for I2CM0 |
| `0x42010000` | `0x1000` | `0x2080C820` | AOP I2CM0 master controller |
| `0x42015000` | `0x1000` | `0x2080C728` | Auxiliary SPI sensor controller |
| `0x42016000` | `0x1000` | `0x2080CBE8` | 1 MHz 48-bit Always-On Timebase (`lo=+0x08`, `hi=+0x0C`) |
| `0x42018000` | `0x1000` | `0x2080CC40` | AOP Always-On Timer / Wake block |
| `0x42019000` | `0x1000` | `0x2080D530` | Always-On GPIO controller |
| `0x4201C000` | `0x1000` | `0x2080CB10` | Fuse / OTP controller (`STATUS=+0x00`, `CMD=+0x10`, `DATA=+0x14`; OTP word `0x3E41`) |
| `0x42020000` | `0x1000` | `0x2080C7F8` | Multi-channel AOP Timer Bank (`0x20` bytes/channel) |
| `0x42038000` | `0x4000` | `0x2080CA30` | SPMI PMIC bus & arbiter (`0x2080C7A0`, `0x2080CA48`) |
| `0x42200000` | `0x1000` | `0x2080CCD8` | Always-On Watchdog (`AOP_WDT`, 125 ms bark/bite unit) |
| `0x42218000` | `0x4000` | `0x2080CE00` | Secondary AKF mailbox aperture |
| `0x42220000` | `0x8000` | `0x2080C5D0` | Primary `ap` (`rkof`) AKF 128-bit FIFO mailbox (IRQ `0x37`) |
| `0x42440000` | `0xF000` | `0x2080D888` | Shared SoC PMGR power/clock gate registers (shared with `rkos`/`rkof`/`acib`) |
| `0x440C4000` | `0x8000` | `0x2080C4F0` | Dedicated `rtp_motion` peer AKF 128-bit FIFO mailbox |
| `0xE0001000` | `0x1000` | — | ARMv7E-M DWT cycle counter |
| `0xE000E000` | `0x1000` | `0x20819808` | ARMv7E-M SCS (SysTick, NVIC, SCB, MPU, FPU) |

## 4. RTKit IPC Peers & Application Endpoints (`0x2080C4CC` & `0x20819978`)

`aopf` communicates with two hardware mailbox peers (`inventory.json["ipc_peers"]`):
- **Peer 0 (`ap`, main RTKit OS `rkof`)** via `0x42220000` (`g_mbox_ap_42220000_dev` @ `0x2080C5D0`, ep table `0x20819978`): hosts RTKit system endpoints (`0x00 management`, `0x01 crashlog`, `0x05 remotefs`, `0x08 oslog`) plus AOP application endpoints (`0x20 SPUApp`, `0x21 sensors`, `0x22 pmu`, `0x23 aop-voicetrigger`, `0x24 aop-audio`, `0x25 misc`, `0x26 cma_misc`, `0x27 awd`).
- **Peer 1 (`rtp_motion`)** via `0x440C4000` (`g_mbox_440c4000_dev` @ `0x2080C4F0`, ep table `0x2080C6B0`): hosts realtime motion-coprocessor endpoint `0x20 rtp_motion`.

## 5. Known Link-Time & Relocation Limitations (`known_deviations`)

Because `.rodata.rom_bank` (`0x00064000..0x00077B14`) and `.data.ram_bank` (`0x20800000..0x2081A0B0`) are preserved byte-for-byte from `../../../aopf` while `3,824` functions remain `pending_c`:
- **Bridged pointers (`235` slots across `52` exact-RVA sections)**: All `93/93` non-zero Thumb handler pointers in the runtime VTOR table (`0x00064000..0x00064178`) and `142` RTKit C driver vtable slots (`46` `.vt.*` sections in `src/rtkit/vt_trampolines.c`) land at their exact original RVAs in `aopf.elf` and dispatch into compiled C11 functions.
- **Unrelocated raw pointers (`2,630` slots)**: `2,550` slots in C++/C `.rodata` vtables/tables (`0x00064178..0x00077AE0`), `13` slots in `.init_array` (`0x00077AE0..0x00077B14`), and `67` initialized callback pointers in `RAM_DATA` (`0x20800000..0x2081A0B0`) still hold raw `ROM_TEXT` addresses of `pending_c` functions. These counts are recorded in `verification/ledger.json["known_deviations"]` and mechanically verified on every `make verify-semantics` run by `tools/verify_claims.py`.
