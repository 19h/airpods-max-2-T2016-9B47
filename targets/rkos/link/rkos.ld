/*
 * Exact memory-layout linker script for `rkos` (Apple H2 Stage-1 RTKitOS).
 *
 * Matches `../../rkos` (`fwsg` wire segments) and `rkos.i64`:
 *   ROM_VECTORS : 0x00000000 .. 0x000000C0 (48-entry boot VTOR)
 *   ROM_TEXT    : 0x000000C0 .. 0x00013090 (RTKit 'uuid' hdr + Thumb-2 text)
 *   ROM_RODATA  : 0x00013090 .. 0x00017590 (runtime VTOR 0x13200, strings, TLVs, init_array)
 *   RAM_DATA    : 0x20000000 .. 0x20000BA4 (initialized data + 0x200B SRAM trampoline)
 *   RAM_BSS     : 0x20000BA4 .. 0x2001F068 (pinned `.state.*` blocks; zero wildcard BSS)
 *
 * Vector handler entry points are pinned at their exact original RVAs
 * (0x108, 0x1F8, 0x290, 0x2E0, 0x374, 0x34D8) so BOTH `ROM_VECTORS` (0x0..0xC0)
 * AND the relocated runtime VTOR inside `ROM_RODATA` (0x13200..0x13360) resolve
 * to valid Thumb-2 entry points while matching `../../rkos` 100% byte-for-byte.
 */

MEMORY
{
    ROM_VECTORS (rx)  : ORIGIN = 0x00000000, LENGTH = 0x000000C0
    ROM_TEXT    (rx)  : ORIGIN = 0x000000C0, LENGTH = 0x00012FD0
    ROM_RODATA  (r)   : ORIGIN = 0x00013090, LENGTH = 0x00004500
    RAM_DATA    (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00000BA4
    RAM_BSS     (rw)  : ORIGIN = 0x20000BA4, LENGTH = 0x0001E4C4
}

ENTRY(Reset_Handler)

SECTIONS
{
    .vectors 0x00000000 :
    {
        KEEP(*(.vectors))
    } > ROM_VECTORS

    ASSERT(SIZEOF(.vectors) == 0xC0, ".vectors must be exactly 0xC0 bytes")

    .text 0x000000C0 :
    {
        KEEP(*(.rtkit.header))
        ASSERT(. == 0x00000108, "Reset_Handler must start at RVA 0x108");
        KEEP(*(.text.entry.reset))
        ASSERT(. <= 0x000001F8, "entry.reset overflowed past 0x1F8");
        . = 0x000001F8;
        KEEP(*(.text.entry.reset_main))
        ASSERT(. <= 0x00000290, "entry.reset_main overflowed past 0x290");
        . = 0x00000290;
        KEEP(*(.text.entry.hardfault))
        ASSERT(. <= 0x000002E0, "entry.hardfault overflowed past 0x2E0");
        . = 0x000002E0;
        KEEP(*(.text.entry.nmi))
        ASSERT(. <= 0x00000374, "entry.nmi overflowed past 0x374");
        . = 0x00000374;
        KEEP(*(.text.entry.svcall))
        ASSERT(. <= 0x000003B0, "entry.svcall overflowed past 0x3B0");
        . = 0x000003B0;

        /* Early text slot before pinned SysTick/IRQ common entry at 0x34D8 */
        *(.text.early .text.early.*)
        ASSERT(. <= 0x000034D8, "early .text overflowed past 0x34D8");
        . = 0x000034D8;
        KEEP(*(.text.entry.systick))

        /* Main reconstructed C11 text & local constants */
        *(.text .text.*)
        *(EXCLUDE_FILE(*image.o) .rodata .rodata.*)
        . = ALIGN(4);
    } > ROM_TEXT

    ASSERT(. <= 0x00013090, ".text overflowed ROM_TEXT end 0x13090")

    .rodata 0x00013090 :
    {
        __rodata_start = .;
        KEEP(*(.rodata.firmware))
        __rodata_end = .;
    } > ROM_RODATA

    ASSERT(SIZEOF(.rodata) == 0x4500, ".rodata must be exactly 0x4500 bytes")

    .data 0x20000000 :
    {
        __data_start = .;
        KEEP(*(.data.firmware))
        __data_end = .;
    } > RAM_DATA

    ASSERT(SIZEOF(.data) == 0x0BA4, ".data must be exactly 0x0BA4 bytes")

    .bss 0x20000BA4 (NOLOAD) :
    {
        __state_start = .;
        KEEP(*(.state.warm))
        ASSERT(. == 0x20000BC0, ".state.warm end mismatch");
        __bss_zero_start = .;
        KEEP(*(.state.qspi))
        ASSERT(. == 0x20000D40, ".state.qspi end mismatch");
        KEEP(*(.state.irq))
        ASSERT(. == 0x20001078, ".state.irq end mismatch");
        KEEP(*(.state.meta))
        ASSERT(. == 0x20001140, ".state.meta end mismatch");
        KEEP(*(.state.arena))
        ASSERT(. == 0x2001D140, ".state.arena end mismatch");
        KEEP(*(.state.psp))
        ASSERT(. == 0x2001D940, ".state.psp end mismatch");
        KEEP(*(.state.plat))
        ASSERT(. == 0x2001DCD0, ".state.plat end mismatch");
        KEEP(*(.state.sched))
        ASSERT(. == 0x2001DE00, ".state.sched end mismatch");
        KEEP(*(.state.fault))
        ASSERT(. == 0x2001E060, ".state.fault end mismatch");
        KEEP(*(.state.msp))
        ASSERT(. == 0x2001F068, ".state.msp end mismatch");
        __state_end = .;

        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    } > RAM_BSS

    ASSERT(__bss_end == __bss_start, "Unpinned wildcard .bss symbols are forbidden")
    ASSERT(__state_end == 0x2001F068, "RAM_BSS must end at 0x2001F068")

    /DISCARD/ :
    {
        *(.ARM.exidx* .ARM.extab* .comment .note*)
    }
}
