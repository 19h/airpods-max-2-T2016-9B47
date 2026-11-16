/*
 * aopf.ld — Exact memory-map linker script for Always-On Processor Firmware (aopf)
 * matching raw ../../aopf & aopf.i64 segment boundaries:
 *   ROM_VECTORS : 0x00000000 .. 0x000000C0 (0xC0 B)
 *   ROM_TEXT    : 0x000000C0 .. 0x00064000 (0x63F40 B; uuid v5 @ 0xC0..0x108)
 *   ROM_RODATA  : 0x00064000 .. 0x00077B14 (0x13B14 B)
 *   RAM_DATA    : 0x20800000 .. 0x2081A0B0 (0x1A0B0 B, LMA 0x00077B20)
 *   RAM_BSS     : 0x2081A0B0 .. 0x2089BED8 (0x81E28 B)
 */

MEMORY
{
    ROM_VECTORS (rx)  : ORIGIN = 0x00000000, LENGTH = 0x000000c0
    ROM_TEXT    (rx)  : ORIGIN = 0x000000c0, LENGTH = 0x00063f40
    ROM_RODATA  (r)   : ORIGIN = 0x00064000, LENGTH = 0x00013b14
    ROM_LMA_PAD (r)   : ORIGIN = 0x00077b20, LENGTH = 0x0001a0b0
    RAM_DATA    (rwx) : ORIGIN = 0x20800000, LENGTH = 0x0001a0b0
    RAM_BSS     (rw)  : ORIGIN = 0x2081a0b0, LENGTH = 0x00081e28
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* 1. Boot vector table (48 entries = 0xC0 bytes) */
    .vectors 0x00000000 :
    {
        KEEP(*(.vectors))
    } > ROM_VECTORS

    ASSERT(SIZEOF(.vectors) == 0x000000c0, ".vectors must be exactly 0xC0 bytes")

    /* 2. RTKit 'uuid' v5 image header (0x48 bytes at 0xC0..0x108) */
    .rtkit.header 0x000000c0 :
    {
        KEEP(*(.rtkit.header))
    } > ROM_TEXT

    ASSERT(SIZEOF(.rtkit.header) == 0x00000048, ".rtkit.header must be 0x48 bytes")

    /* 3. Exact RVA boot/exception veneers referenced by .vectors */
    .text.reset 0x0000693c : { KEEP(*(.text.reset)) } > ROM_TEXT
    .text.corrupt_halt 0x000069d0 : { KEEP(*(.text.corrupt_halt)) } > ROM_TEXT
    .text.hardfault 0x000069d4 : { KEEP(*(.text.hardfault)) } > ROM_TEXT
    .text.nmi 0x00006a24 : { KEEP(*(.text.nmi)) } > ROM_TEXT
    .text.svc_pendsv 0x00006ab8 : { KEEP(*(.text.svc_pendsv)) } > ROM_TEXT

    /* 4. Reconstructed C11 code & compiler constants inside ROM_TEXT */
    .text 0x00007000 : ALIGN(4)
    {
        *(.text .text.*)
        *(EXCLUDE_FILE(*image_blobs.o) .rodata .rodata.*)
    } > ROM_TEXT

    ASSERT(. <= 0x0004e510, ".text overflowed into 0x4E518 fixed-RVA region")

    /* 5. SysTick_IRQ_Handler @ 0x4E518 (referenced by boot vectors [15..47]) */
    .rva_4e518 0x0004e518 : { KEEP(*(.rva_4e518)) } > ROM_TEXT

    /* 6. Exact-RVA vtable dispatch veneers for ROM_RODATA RTKit C driver vtables */
    .vt.4e614 0x0004e614 : { KEEP(*(.vt.4e614)) } > ROM_TEXT
    .vt.4e61c 0x0004e61c : { KEEP(*(.vt.4e61c)) } > ROM_TEXT
    .vt.4e69c 0x0004e69c : { KEEP(*(.vt.4e69c)) } > ROM_TEXT
    .vt.4e6e0 0x0004e6e0 : { KEEP(*(.vt.4e6e0)) } > ROM_TEXT
    .vt.4e774 0x0004e774 : { KEEP(*(.vt.4e774)) } > ROM_TEXT
    .vt.4e850 0x0004e850 : { KEEP(*(.vt.4e850)) } > ROM_TEXT
    .vt.4e8b4 0x0004e8b4 : { KEEP(*(.vt.4e8b4)) } > ROM_TEXT
    .vt.4e8b8 0x0004e8b8 : { KEEP(*(.vt.4e8b8)) } > ROM_TEXT
    .vt.514f8 0x000514f8 : { KEEP(*(.vt.514f8)) } > ROM_TEXT
    .vt.51574 0x00051574 : { KEEP(*(.vt.51574)) } > ROM_TEXT
    .vt.51640 0x00051640 : { KEEP(*(.vt.51640)) } > ROM_TEXT
    .vt.51648 0x00051648 : { KEEP(*(.vt.51648)) } > ROM_TEXT
    .vt.51684 0x00051684 : { KEEP(*(.vt.51684)) } > ROM_TEXT
    .vt.516c4 0x000516c4 : { KEEP(*(.vt.516c4)) } > ROM_TEXT
    .vt.5171c 0x0005171c : { KEEP(*(.vt.5171c)) } > ROM_TEXT
    .vt.51748 0x00051748 : { KEEP(*(.vt.51748)) } > ROM_TEXT
    .vt.520a4 0x000520a4 : { KEEP(*(.vt.520a4)) } > ROM_TEXT
    .vt.5223c 0x0005223c : { KEEP(*(.vt.5223c)) } > ROM_TEXT
    .vt.52244 0x00052244 : { KEEP(*(.vt.52244)) } > ROM_TEXT
    .vt.523b4 0x000523b4 : { KEEP(*(.vt.523b4)) } > ROM_TEXT
    .vt.52da0 0x00052da0 : { KEEP(*(.vt.52da0)) } > ROM_TEXT
    .vt.52e40 0x00052e40 : { KEEP(*(.vt.52e40)) } > ROM_TEXT
    .vt.52ef8 0x00052ef8 : { KEEP(*(.vt.52ef8)) } > ROM_TEXT
    .vt.52f3c 0x00052f3c : { KEEP(*(.vt.52f3c)) } > ROM_TEXT
    .vt.55914 0x00055914 : { KEEP(*(.vt.55914)) } > ROM_TEXT
    .vt.55984 0x00055984 : { KEEP(*(.vt.55984)) } > ROM_TEXT
    .vt.559bc 0x000559bc : { KEEP(*(.vt.559bc)) } > ROM_TEXT
    .vt.55a08 0x00055a08 : { KEEP(*(.vt.55a08)) } > ROM_TEXT
    .vt.55a54 0x00055a54 : { KEEP(*(.vt.55a54)) } > ROM_TEXT
    .vt.55a74 0x00055a74 : { KEEP(*(.vt.55a74)) } > ROM_TEXT
    .vt.55a7c 0x00055a7c : { KEEP(*(.vt.55a7c)) } > ROM_TEXT
    .vt.5a540 0x0005a540 : { KEEP(*(.vt.5a540)) } > ROM_TEXT
    .vt.5a630 0x0005a630 : { KEEP(*(.vt.5a630)) } > ROM_TEXT
    .vt.5a654 0x0005a654 : { KEEP(*(.vt.5a654)) } > ROM_TEXT
    .vt.5a678 0x0005a678 : { KEEP(*(.vt.5a678)) } > ROM_TEXT
    .vt.5a67e 0x0005a67e : { KEEP(*(.vt.5a67e)) } > ROM_TEXT
    .vt.5a6ae 0x0005a6ae : { KEEP(*(.vt.5a6ae)) } > ROM_TEXT
    .vt.5a928 0x0005a928 : { KEEP(*(.vt.5a928)) } > ROM_TEXT
    .vt.5a9e4 0x0005a9e4 : { KEEP(*(.vt.5a9e4)) } > ROM_TEXT
    .vt.5aa02 0x0005aa02 : { KEEP(*(.vt.5aa02)) } > ROM_TEXT
    .vt.5aa10 0x0005aa10 : { KEEP(*(.vt.5aa10)) } > ROM_TEXT
    .vt.5aaa8 0x0005aaa8 : { KEEP(*(.vt.5aaa8)) } > ROM_TEXT
    .vt.5aac0 0x0005aac0 : { KEEP(*(.vt.5aac0)) } > ROM_TEXT
    .vt.5aaf8 0x0005aaf8 : { KEEP(*(.vt.5aaf8)) } > ROM_TEXT
    .vt.5ab8c 0x0005ab8c : { KEEP(*(.vt.5ab8c)) } > ROM_TEXT
    .vt.5aba0 0x0005aba0 : { KEEP(*(.vt.5aba0)) } > ROM_TEXT

    /* 7. Verbatim binary-backed ROM_RODATA (0x00064000..0x00077B14) */
    .rodata.rom_bank 0x00064000 :
    {
        KEEP(*(.rodata.rom_bank))
    } > ROM_RODATA

    ASSERT(SIZEOF(.rodata.rom_bank) == 0x00013b14, ".rodata.rom_bank size mismatch")

    /* 8. Verbatim binary-backed RAM_DATA (0x20800000..0x2081A0B0, fileoff 0x77B20) */
    .data.ram_bank 0x20800000 : AT(0x00077b20)
    {
        __ram_data_start = .;
        KEEP(*(.data.ram_bank))
        __ram_data_end = .;
    } > RAM_DATA

    ASSERT(SIZEOF(.data.ram_bank) == 0x0001a0b0, ".data.ram_bank size mismatch")

    /* 9. Uninitialized RAM_BSS (0x2081A0B0..0x2089BED8) */
    .bss 0x2081a0b0 (NOLOAD) :
    {
        __bss_pad_start = .;
        . = . + 0x00000f50;
        __bss_clear_start = .;
        *(.bss .bss.*)
        *(COMMON)
    } > RAM_BSS

    ASSERT(SIZEOF(.bss) == 0x00000f50, "unpinned floating .bss is forbidden; pin state VMAs explicitly")

    .bss.psp_stack 0x20899490 (NOLOAD) :
    {
        __psp_stack_bottom = .;
        . = . + 0x1400;
        __psp_stack_top = .;
    } > RAM_BSS

    .bss.msp_stack 0x2089b610 (NOLOAD) :
    {
        __msp_stack_bottom = .;
        . = . + 0x0400;
        __msp_stack_top = .;
    } > RAM_BSS

    .bss.end_sentinel 0x2089bed8 (NOLOAD) :
    {
        __bss_end = .;
    } > RAM_BSS

    /DISCARD/ :
    {
        *(.ARM.attributes)
        *(.ARM.exidx*)
        *(.comment)
        *(.note*)
    }
}
