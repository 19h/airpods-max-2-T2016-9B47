# `rkof` — Main RTKit Application & W3 Coprocessor Supervisor Firmware

Freestanding ARMv7E-M (`Cortex-M4F`, `fpv4-sp-d16`) + host-testable C11 reconstruction of **`rkof`**, the main RTKit application and multi-coprocessor supervisor image on **Apple T2016 Rev B0 (`b515d` / `AirPods3,6`)**.

---

## Binary Identity & Metadata

| Property | Value |
| --- | --- |
| Raw image | `../../../rkof` (`2,159,008` bytes = `0x20F1A0`) |
| SHA-256 | `2b40a04f29ea859ba750fb954ebda9c2eefe1c3dcc7593d19115f55ad1edaa98` |
| CPU / FPU | ARMv7E-M (`cortex-m4`, Thumb-2, hard-float `fpv4-sp-d16`, 48 vectors = 16 core + 32 ext IRQ) |
| RTKit `uuid` header | `@ 0x000000C0`, version `5`, UUID `ff8569c1-ff1f-31fd-9dc5-24a5c9791ecf` |
| RTKit build tag | `@ 0x001E0868`: `RTKit_release-3514.0.15.release` |
| Product build tag | `@ 0x20008298` (file `0x208298`): `RTKitOSPlatform-3431.0.25~004286` (`b515d`, T2016 Rev B0) |
| Metadata TLV stream | `@ 0x001E1AC0..0x001E1C9A` (`0x1DA` = `474` B, `36` FourCC TLVs: `STKG`, `RMC1..3`, `RMD1..3`, `RMFI`, `RTSI`, `RTSZ`, `TXSZ`, `DSRL`, etc.) |
| `fwsg` v1 trailer | `@ EOF - 0x60` (`0x20F140..0x20F1A0`): `__TEXT` (`vma=0x0`, `filesz=0x1F389A`, `vmsz=0x1F389A`, `flags=0x4`) + `__DATA` (`vma=0x20000000`, `fileoff=0x200000`, `filesz=0xF138`, `vmsz=0xD46B8`, `flags=0x0`) |
| IDB inventory | `17,479` functions, `275` surviving source file paths, `148` assert/panic strings |

---

## Memory Map & Linker Fidelity

| Region | VMA Range | Size | File Offset | Reconstruction Backing |
| --- | --- | --- | --- | --- |
| `ROM_VECTORS` | `0x00000000..0x000000BF` | `0x0000C0` (`192 B`) | `0x000000` | Typed C table (`src/rtkit/vectors.c`) — **100% byte-identical** to `../../../rkof` |
| `.rtkit.header` | `0x000000C0..0x00000107` | `0x000048` (`72 B`) | `0x0000C0` | Binary-backed `uuid` v5 header (`src/rtkit/binary_blobs.S`) — **100% byte-identical** |
| `ROM_TEXT` | `0x00000108..0x001BF9FF` | `0x1BF8F8` (`1.79 MB`) | `0x000108` | `rtkit_entry` (`0x108`), 5 naked veneers (`0x1F8, 0x290, 0x2E0, 0x374, 0x16F70`), reconstructed C11 (`.text`) |
| `ROM_RODATA` | `0x001BFA00..0x001F3899` | `0x033E9A` (`212,634 B`) | `0x1BFA00` | Binary-backed `.rodata.image` — **100% byte-identical** |
| `RAM_DATA` | `0x20000000..0x2000F137` | `0x00F138` (`61,752 B`) | `0x200000` | Binary-backed `.data.image` (`DSRL=0` preloaded by `rkos`) — **100% byte-identical** |
| `RAM_BSS` | `0x2000F138..0x200D46B7` | `0x0C5580` (`808,320 B`) | `NOLOAD` | 24 pinned `.state.*` blocks ending at `0x200D46B8` (`SIZEOF(.bss.floating) == 0`) |

---

## Cross-Core W3 PHY (`phyb`) & Coprocessor Architecture Recovered in `rkof`

`rkof` does **not** drive live Bluetooth packet mailbox traffic at `0x4D280000`/`0x4D280400` during normal radio operation (that runtime Dekker-locked IPC channel belongs to `acib` <-> `phyb`). Instead, `rkof` is the **SoC-wide supervisor, RF/OTP calibration synthesizer, reset sequencer, and post-mortem crash harvester** for W3 (`phyb` + `acib`) and peer RTKit cores (`aop`, `dsp0`, `dsp1`):

1. **Multi-Core RTKit Supervision (`src/coproc/supervisor.c`)**:
   - `sub_17F5C` (`0x17F5C`) maps coprocessor bitmasks (`0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80`) to static/BSS RTKit supervisor descriptors (`0x200008F8..0x20040998`), preserving the `uxth` (`(uint16_t)mask == 2`) quirk on mask `0x02`.
   - `sub_15B20..sub_15C4C` manage reference-counted `btp` shared-memory carveout grants (`0x200009B0`, `0x2000C988`) and lifecycle IPC hooks (`sub_1BA50` / `sub_1BA72`).
   - `sub_F1B90` (`0xF1B90`) coordinates cross-core Global Time Base (`GTB`) snapshots via `0x4D000800` and `0x001C0004`.
2. **W3 PHY Reset, RF PMGR Sequencing & Warm-Restore Table (`src/phy/modem_inits.c`)**:
   - `sub_FDD90` (`0xFDD90`) asserts/releases W3 modem reset bits (`0x4D154048` bits `4, 6`) and W3 PHY top reset bits (`0x4D140000` bits `5, 16, 19`).
   - `bt_phy_mmio_pre_otp` (`0x10694C`) and `bt_phy_mmio_init_seq` (`0x1068A0`) sequence RF PMGR LDOs, bias, clock gates (`0x4246E004..0x4246E074`), and analog isolation (`0x4247A004`).
   - `sub_FDDDC` (`0xFDDDC`), `sub_FDDF8` (`0xFDDF8`), and `sub_FDE48` (`0xFDE48`) replay OTP/WNVV register pairs & RMW triples while logging every write inside `[0x4D154000..0x4D155000]` into `phyb`'s warm-restore table at `0x4D291008[slpr_reg_num++]` (`0x4D291004`), coalescing consecutive RMWs to the same register.
3. **WNVV OTP Parsing & `0x4D28D000` Calibration Container Synthesis (`src/phy/scanmedia.c`, `src/phy/wnvv.c`, `src/phy/bt_phy.c`)**:
   - `bt_phy_otp_init` (`0xFD7C0`) scans SRAM (`0x20003328`, `media_id=0`) and hardware OTP (`sub_FDEC0` -> `sub_18E930`, `media_id=1`) through `sub_FDFD0` (`scanmedia.c`) and `sub_FE2B0` (`wnvv.c`), building a sorted tag list at `0x200D033C`.
   - Replays tag `0x080C` (direct block copy), 16 pair/triple tags (`0x080B..0x116E`), and `0x0C02`, then synthesizes a packed `'NV'` (`0x4E56`) container from the 29-entry `RKOF_RODATA_NVV_MDMI_TABLE` (`0x001D36C8`) into shared calibration SRAM at **`0x4D28D000`** (`<= 0x3FFB` bytes) before releasing `phyb` reset and clearing `slpr_state` (`0x4D291000 = 0`).
4. **Coredump Harvesting & `phy` Diagnostic Shell (`src/phy/bt_phy.c`)**:
   - Registers `'BPHY'` (`0x42504859`, `sub_FDC38` @ `0xFDC38`) to harvest up to `0x1FC` bytes of `phyb`'s trace ring (`0x4D289000`) + `0x98`-byte crash record (`0x4D289200`) + variable fault detail (`*0x4D289244`), temporarily waking power domain `0x6E` (`sub_FDD1C` / `sub_FDD48`).
   - Registers `'MgrC'` (`0x4372674D`, `sub_FDCB4` @ `0xFDCB4`) to capture `error_code` (`0x4D289240`) + detail payload, and registers the `"phy"` RTKit shell command (`phy_shell_cmd` @ `0xFE120`: `sleep`, `up`, `uart`, `wake`, `crash`).

---

## Ledger Accounting & Verification Summary

- **Total IDB functions (`verification/inventory.json` & `verification/ledger.json`)**: `17,479` (`100.0%` accounted)
  - `reconstructed_c`: **`108`** functions across 14 C11 translation units (`0` banned Hex-Rays idioms; **100% (`108/108`) Unicorn-verified & stub-mutation-sensitive**)
  - `retained_native`: **`5`** naked ARMv7E-M architectural entry/exception veneers (`src/rtkit/entry_veneers.S`, pinned at exact raw RVAs & byte-verified `442/442` non-relocated bytes)
  - `pending_c`: **`17,366`** remaining application/DSP/audio/codec functions
- **Build & Verification Commands**:

```sh
make -C targets/rkof all check verify verify-semantics BUILD=/tmp/rkof-build
```

| Gate | Tool / Binary | Result |
| --- | --- | --- |
| ARMv7E-M ELF + compile-time layout | `clang -target armv7em-none-eabi` + `ld.lld` | `rkof.elf` + `arm/layout.o` (`0` warnings under `-Werror`) |
| Host C11 behavioral test runner | `/tmp/rkof-build/rkof_tests` | **`123 / 123` checks passed** |
| Source fidelity & ledger audit | `tools/verify_fidelity.py` | **`0` banned idioms**, `108` `@provenance` tags, `17,479 / 17,479` ledger |
| Pinned linker map audit | `tools/verify_layout.py` | `8` fixed sections + `23` `STATE_*_BASE` blocks + `6` handler VMAs |
| Binary load-image & veneer byte proof | `tools/verify_load_image.py` | **`8 / 8` `SHF_ALLOC` in `PT_LOAD`, `4 / 4` slices + `5 / 5` veneers (`275,092` B)** |
| Unicorn Cortex-M4F differential | `tools/arm_semantics.py` | **`175 / 175` cases (`108 / 108` symbols) passed across 6 suites (`0` divergences)** |
| Claim, stub-mutation & deviation gate | `tools/verify_claims.py` | **`108 / 108` defeat `bx lr` / `ret0` / `ret1` stubs; `known_deviations` exact** |
