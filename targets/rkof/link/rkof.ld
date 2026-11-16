/*
 * Linker script for rkof (Main RTKit Firmware, Apple T2016 Rev B0 / b515d)
 * Preserves exact ROM_VECTORS (0x0..0xC0), .rtkit.header (0xC0..0x108),
 * architectural handler VMAs (0x108, 0x1F8, 0x290, 0x2E0, 0x374, 0x16F70),
 * ROM_RODATA (0x1BFA00..0x1F389A), RAM_DATA (0x20000000..0x2000F138),
 * and pinned RAM_BSS state blocks (0x2000F138..0x200D46B8).
 */

ENTRY(rtkit_entry)

MEMORY
{
    ROM_VECTORS (rx)  : ORIGIN = 0x00000000, LENGTH = 0x000000C0
    ROM_TEXT    (rx)  : ORIGIN = 0x000000C0, LENGTH = 0x001BF940
    ROM_RODATA  (r)   : ORIGIN = 0x001BFA00, LENGTH = 0x00033E9A
    RAM_DATA    (rw)  : ORIGIN = 0x20000000, LENGTH = 0x0000F138
    RAM_BSS     (rw)  : ORIGIN = 0x2000F138, LENGTH = 0x000C5580
}

PHDRS
{
    rom  PT_LOAD FLAGS(5); /* PF_R | PF_X */
    sram PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
    .vectors 0x00000000 :
    {
        KEEP(*(.vectors))
    } > ROM_VECTORS :rom

    ASSERT(SIZEOF(.vectors) == 0x000000C0, ".vectors must be exactly 0xC0 bytes (48 entries)")

    .rtkit.header 0x000000C0 :
    {
        KEEP(*(.rtkit.header))
    } > ROM_TEXT :rom

    ASSERT(SIZEOF(.rtkit.header) == 0x00000048, ".rtkit.header must be 0x48 bytes")

    .text.entry 0x00000108 :
    {
        KEEP(*(.text.rtkit_entry))
    } > ROM_TEXT :rom

    ASSERT(ADDR(.text.entry) + SIZEOF(.text.entry) <= 0x000001F8,
           "rtkit_entry must fit before Reset_Handler at 0x1F8")

    .text.veneers 0x000001F8 :
    {
        KEEP(*(.text.entry_veneers))
    } > ROM_TEXT :rom

    .text 0x000003B0 :
    {
        *(EXCLUDE_FILE(*entry_veneers.o *binary_blobs.o *p2_*.o) .text)
        *(EXCLUDE_FILE(*entry_veneers.o *binary_blobs.o *p2_*.o) .text.*)
        *(EXCLUDE_FILE(*binary_blobs.o *p2_*.o) .rodata)
        *(EXCLUDE_FILE(*binary_blobs.o *p2_*.o) .rodata.*)
        . = ALIGN(4);
    } > ROM_TEXT :rom

    ASSERT(ADDR(.text) + SIZEOF(.text) <= 0x00016F70,
           ".text must fit before Default_Handler at 0x16F70")

    .text.default_handler 0x00016F70 :
    {
        KEEP(*(.text.default_handler))
    } > ROM_TEXT :rom

    .text.subsystems 0x00016F90 :
    {
        *p2_*.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(4);
    } > ROM_TEXT :rom

    ASSERT(ADDR(.text.subsystems) + SIZEOF(.text.subsystems) <= 0x001BFA00,
           ".text.subsystems must fit before .rodata.image at 0x1BFA00")

    .rodata.image 0x001BFA00 :
    {
        KEEP(*(.rodata.image))
    } > ROM_RODATA :rom

    ASSERT(SIZEOF(.rodata.image) == 0x00033E9A, ".rodata.image must match raw rkof ROM_RODATA")

    .data.image 0x20000000 :
    {
        KEEP(*(.data.image))
    } > RAM_DATA :sram

    ASSERT(SIZEOF(.data.image) == 0x0000F138, ".data.image must match raw rkof RAM_DATA")

    /* Pinned RAM_BSS state blocks in ascending VMA order */
    .state.bss_start       0x2000F138 (NOLOAD) : { KEEP(*(.state.bss_start)) } > RAM_BSS :sram
    .state.nmi_frame       0x20024180 (NOLOAD) : { KEEP(*(.state.nmi_frame)) } > RAM_BSS :sram
    .state.carveout        0x20024220 (NOLOAD) : { KEEP(*(.state.carveout)) } > RAM_BSS :sram
    .state.core_ptrs       0x20024398 (NOLOAD) : { KEEP(*(.state.core_ptrs)) } > RAM_BSS :sram
    .state.coproc_ref      0x20040648 (NOLOAD) : { KEEP(*(.state.coproc_ref)) } > RAM_BSS :sram
    .state.coproc_dyn      0x20040958 (NOLOAD) : { KEEP(*(.state.coproc_dyn)) } > RAM_BSS :sram
    .state.psp_stack       0x200409D8 (NOLOAD) : { KEEP(*(.state.psp_stack)) } > RAM_BSS :sram
    .state.gtb_ctx         0x20067F98 (NOLOAD) : { KEEP(*(.state.gtb_ctx)) } > RAM_BSS :sram
    .state.phy_cal_ptrs    0x2006A798 (NOLOAD) : { KEEP(*(.state.phy_cal_ptrs)) } > RAM_BSS :sram
    .state.sched_bitmap    0x200CE7E0 (NOLOAD) : { KEEP(*(.state.sched_bitmap)) } > RAM_BSS :sram
    .state.sched_pending   0x200CEBF0 (NOLOAD) : { KEEP(*(.state.sched_pending)) } > RAM_BSS :sram
    .state.sched_ctx       0x200CECF0 (NOLOAD) : { KEEP(*(.state.sched_ctx)) } > RAM_BSS :sram
    .state.exc_state       0x200CEDF0 (NOLOAD) : { KEEP(*(.state.exc_state)) } > RAM_BSS :sram
    .state.panic_hook      0x200CEE6C (NOLOAD) : { KEEP(*(.state.panic_hook)) } > RAM_BSS :sram
    .state.spicy_log       0x200CF630 (NOLOAD) : { KEEP(*(.state.spicy_log)) } > RAM_BSS :sram
    .state.heap_lock       0x200CFA00 (NOLOAD) : { KEEP(*(.state.heap_lock)) } > RAM_BSS :sram
    .state.crash_magic     0x200CFA10 (NOLOAD) : { KEEP(*(.state.crash_magic)) } > RAM_BSS :sram
    .state.fault_info      0x200D021C (NOLOAD) : { KEEP(*(.state.fault_info)) } > RAM_BSS :sram
    .state.bt_phy_otp      0x200D030C (NOLOAD) : { KEEP(*(.state.bt_phy_otp)) } > RAM_BSS :sram
    .state.mpu             0x200D32D0 (NOLOAD) : { KEEP(*(.state.mpu)) } > RAM_BSS :sram
    .state.irq_arg         0x200D3460 (NOLOAD) : { KEEP(*(.state.irq_arg)) } > RAM_BSS :sram
    .state.irq_fn          0x200D3588 (NOLOAD) : { KEEP(*(.state.irq_fn)) } > RAM_BSS :sram
    .state.msp_stack       0x200D36B0 (NOLOAD) : { KEEP(*(.state.msp_stack)) } > RAM_BSS :sram
    .state.irq_prio        0x200D46B0 (NOLOAD) : { KEEP(*(.state.irq_prio)) } > RAM_BSS :sram

    ASSERT(ADDR(.state.irq_prio) + SIZEOF(.state.irq_prio) == 0x200D46B8,
           "RAM_BSS must end at exact firmware boundary 0x200D46B8")

    /* Reject unpinned compiler globals */
    .bss.floating (NOLOAD) :
    {
        *(.bss .bss.* COMMON)
    } > RAM_BSS :sram

    ASSERT(SIZEOF(.bss.floating) == 0, "Unpinned floating .bss variables are forbidden")

    /DISCARD/ :
    {
        *(.ARM.exidx* .ARM.extab* .ARM.attributes .comment .note*)
    }
}
