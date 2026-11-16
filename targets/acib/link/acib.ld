/*
 * Linker script for Apple b515d Bluetooth / ACI Controller Firmware (acib).
 *
 * Segment layout verified from raw fwsg trailer (0x104480..0x1044e0) and acib.i64:
 *   ROM_VECTORS : 0x00000000 .. 0x000000c0 (0xc0 B, 48 words)
 *   RTKIT_HDR   : 0x000000c0 .. 0x00000108 (0x48 B, uuid v5 header)
 *   ROM_TEXT    : 0x00000108 .. 0x000efea8 (0xefda0 B)
 *   ROM_RODATA  : 0x000efea8 .. 0x000f984c (0x99a4 B, binary-preserved)
 *   RAM_DATA    : 0x10000000 .. 0x1000ac1e (0xac1e B, file 0xf9860..0x10447e)
 *   RAM_BSS     : 0x1000ac1e .. 0x1002d2c4 (0x226a6 B, NOLOAD)
 */

ENTRY(Reset_Handler)

MEMORY
{
    ROM_VECTORS (rx)  : ORIGIN = 0x00000000, LENGTH = 0x000000c0
    ROM_TEXT    (rx)  : ORIGIN = 0x000000c0, LENGTH = 0x000efde8
    ROM_RODATA  (r)   : ORIGIN = 0x000efea8, LENGTH = 0x000099a4
    RAM_DATA    (rw)  : ORIGIN = 0x10000000, LENGTH = 0x0000ac1e
    RAM_BSS     (rw)  : ORIGIN = 0x1000ac1e, LENGTH = 0x000226a6
}

SECTIONS
{
    /* Primary Cortex-M vector table at VTOR = 0x00000000 */
    .vectors 0x00000000 :
    {
        __vectors_start = .;
        KEEP(*(.vectors))
        __vectors_end = .;
    } > ROM_VECTORS

    ASSERT(__vectors_end - __vectors_start == 0xc0,
           ".vectors must be exactly 0xc0 bytes (48 entries)")

    /* RTKit v5 UUID image header at 0x000000c0 .. 0x00000108 */
    .rtkit.header 0x000000c0 :
    {
        __rtkit_header_start = .;
        KEEP(*(.rtkit.header))
        __rtkit_header_end = .;
    } > ROM_TEXT

    ASSERT(__rtkit_header_end - __rtkit_header_start == 0x48,
           ".rtkit.header must be exactly 0x48 bytes")

    /* Reset_Handler pinned at exact firmware entry 0x00000108 */
    .text.reset 0x00000108 :
    {
        KEEP(*(.pinned.reset))
    } > ROM_TEXT

    /* Secondary 144-entry VTOR table pinned at exact firmware offset 0x00000400 */
    .vectors.secondary 0x00000400 :
    {
        __secondary_vectors_start = .;
        KEEP(*(.vectors.secondary))
        __secondary_vectors_end = .;
    } > ROM_TEXT

    ASSERT(__secondary_vectors_end - __secondary_vectors_start == 0x240,
           ".vectors.secondary must be exactly 0x240 bytes (144 entries)")

    /* Reconstructed C text + compiler jump tables / rodata literals */
    .text 0x00000680 : ALIGN(4)
    {
        __text_reconstructed_start = .;
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __text_reconstructed_end = .;
    } > ROM_TEXT

    ASSERT(__text_reconstructed_end <= 0x000e4a50,
           "reconstructed .text overflowed into pinned exception handlers at 0xe4a50")

    /* Pinned core exception handlers referenced by ROM_VECTORS */
    .text.fault 0x000e4a50 :
    {
        KEEP(*(.pinned.fault))
    } > ROM_TEXT

    .text.nmi 0x000e4a94 :
    {
        KEEP(*(.pinned.nmi))
    } > ROM_TEXT

    .text.svcall 0x000e4b14 :
    {
        KEEP(*(.pinned.svcall))
    } > ROM_TEXT

    .text.sleep_restore 0x000e4bb0 :
    {
        KEEP(*(.pinned.sleep_restore))
    } > ROM_TEXT

    .text.systick 0x000e58f0 :
    {
        KEEP(*(.pinned.systick))
    } > ROM_TEXT

    /* Binary-preserved ROM_RODATA (0x000efea8 .. 0x000f984c) */
    .rom.rodata 0x000efea8 :
    {
        __rom_rodata_start = .;
        KEEP(*(.rom.rodata))
        __rom_rodata_end = .;
    } > ROM_RODATA

    ASSERT(__rom_rodata_end - __rom_rodata_start == 0x99a4,
           ".rom.rodata must match exact firmware size 0x99a4")

    /* Binary-preserved initialized RAM_DATA (0x10000000 .. 0x1000ac1e) */
    .ram.data 0x10000000 :
    {
        __data_start = .;
        KEEP(*(.ram.data))
        *(.data .data.*)
        __data_end = .;
    } > RAM_DATA

    ASSERT(__data_end - __data_start == 0xac1e,
           ".ram.data must match exact firmware size 0xac1e")

    /*
     * Pinned RAM_BSS layout (0x1000ac1e .. 0x1002d2c4).
     * Every global referenced by reconstructed C is placed at its exact VMA.
     */
    .bss.pre_zero 0x1000ac1e (NOLOAD) : SUBALIGN(1)
    {
        __bss_start = .;
        KEEP(*(.state.pre_zero))
    } > RAM_BSS

    .bss.zero_region 0x1000ac40 (NOLOAD) : SUBALIGN(1)
    {
        __bss_zero_start = .;
        KEEP(*(.state.pad_ac40))
    } > RAM_BSS

    .bss.irq_handlers 0x1000eeb4 (NOLOAD) : SUBALIGN(1)
    {
        KEEP(*(.state.irq_handlers))
        KEEP(*(.state.irq_contexts))
        KEEP(*(.state.pad_f2c4))
    } > RAM_BSS

    .bss.stacks 0x1001af80 (NOLOAD) : SUBALIGN(1)
    {
        __psp_init_stack_base = .;
        KEEP(*(.state.psp_init_stack))
        __msp_stack_base = .;
        KEEP(*(.state.msp_stack))
        __msp_stack_top = .;
        KEEP(*(.state.pad_1bd80))
    } > RAM_BSS

    .bss.sys_state 0x1002006c (NOLOAD) : SUBALIGN(1)
    {
        KEEP(*(.state.boot_mode_2006c))
        KEEP(*(.state.pad_20094))
        KEEP(*(.state.sys_threads_20310))
        KEEP(*(.state.pad_20320))
        KEEP(*(.state.clock_ctx_20968))
        KEEP(*(.state.pad_209ac))
        KEEP(*(.state.panic_reason_20b60))
        KEEP(*(.state.pad_20b64))
        KEEP(*(.state.bt_ctrl_ctx_20b78))
        KEEP(*(.state.osa_tasks_21228))
        KEEP(*(.state.pad_21628))
        KEEP(*(.state.phy_crash_ctx_217cc))
        KEEP(*(.state.pad_217d0))
        KEEP(*(.state.rtk_threads_21c04))
        KEEP(*(.state.pad_21c44))
        KEEP(*(.state.psp_reset_top_21e80))
        KEEP(*(.state.pad_21e84))
        KEEP(*(.state.phy_table_shadow_21ea9))
        KEEP(*(.state.pad_21fc9))
    } > RAM_BSS

    .bss.phy_ipc 0x1002268c (NOLOAD) : SUBALIGN(1)
    {
        KEEP(*(.state.phy_ipc_2268c))
        KEEP(*(.state.pad_226c8))
        KEEP(*(.state.board_props_22984))
        KEEP(*(.state.pad_22994))
        KEEP(*(.state.rf_tx_ctx_27958))
        KEEP(*(.state.pad_27a0c))
        KEEP(*(.state.phy_ack_ctx_2a168))
        KEEP(*(.state.pad_2a178))
    } > RAM_BSS

    .bss.rtkit_tail 0x1002cf68 (NOLOAD) : SUBALIGN(1)
    {
        KEEP(*(.state.tm_cb_2cf68))
        KEEP(*(.state.pad_2cf70))
        KEEP(*(.state.nmi_sp_2cfb4))
        KEEP(*(.state.heap_ctx_2cfb8))
        KEEP(*(.state.sched_ready_2d0c8))
        KEEP(*(.state.sched_flags_2d240))
        KEEP(*(.state.pad_2d248))
        KEEP(*(.state.irq_mask_cfg_2d2bc))
        __bss_zero_end = .;
    } > RAM_BSS

    ASSERT(__bss_zero_end == 0x1002d2c4,
           "Pinned RAM_BSS tail must end at exact firmware boundary 0x1002d2c4")

    /* Forbid any unpinned .bss / COMMON symbols */
    .bss (NOLOAD) :
    {
        __unpinned_bss_start = .;
        *(.bss .bss.* COMMON)
        __unpinned_bss_end = .;
    } > RAM_BSS

    ASSERT(__unpinned_bss_end == __unpinned_bss_start,
           "Unpinned .bss variables are forbidden; pin every global in link/acib.ld")

    /DISCARD/ :
    {
        *(.ARM.exidx* .ARM.extab* .comment .note*)
    }
}
