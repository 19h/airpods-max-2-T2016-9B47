/*
 * phyb.ld -- link map for the W3 PHY image.
 *
 * ROM holds the vector table, code and read-only tables.  RAM starts with the
 * initialised data and is followed by the module state blocks, which are
 * pinned to the addresses of the recovered image so crash dumps can be read
 * against this source.  Memory shared with the host (mailbox, status page,
 * trace ring, crash record) is addressed absolutely from memmap.h and is not
 * allocated here.
 */
ENTRY(Reset_Handler)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x000088D4
    RAM (rwx) : ORIGIN = 0x4D280000, LENGTH = 0x0000D178
}

__psp_stack_base = 0x4D289AC8;
__psp_stack_top  = 0x4D289EC8;
__msp_stack_base = 0x4D289EC8;
__msp_stack_top  = 0x4D28A2C8;

SECTIONS
{
    .vectors ORIGIN(ROM) : { KEEP(*(.vectors)) } > ROM
    .rtkit.header 0xC0 : { KEEP(*(.rtkit.header)) } > ROM
    ASSERT(SIZEOF(.rtkit.header) == 0x48, "RTKit image header size")

    /* The scan's stack-dependent fault PC is observable. Keep its native
     * address and use the preceding space for the small PHY modules and FRS
     * helpers. The entry routine remains after the scan to bound this bucket. */
    .text.clock_prefix 0x108 :
    {
        *clock.o(.text .text.*)
    } > ROM
    ASSERT(. <= 0x41C, "clock prefix overlaps reference debug callback")
    .text.debug_stub 0x41C : { KEEP(*(.text.dbg_printf)) } > ROM
    ASSERT(SIZEOF(.text.debug_stub) == 4, "debug callback stub instruction size")
    .text.before_scan 0x420 :
    {
        *dpd.o(.text .text.*)
        *frs.o(.text.frs_elapsed_systick_arm .text.frs_clear_agc_arm .text.frs_clear_trigger_arm)
        *frs.o(.text.frs_unpack_channel_bitmap .text.frs_run_scan .text.frs_process_results)
        *frs.o(.text.frs_calibrate_sample .text.frs_dump_readings .text.frs_sample_name)
        *frs.o(.text.frs_format_rssi .text.frs_configure .text.frs_exit .text.frs_correlator_init)
    } > ROM
    ASSERT(. <= 0xD7C, "early text overlaps native FRS scan")
    .text.frs_scan 0xD7C : { KEEP(*(.text.frs_scan_pass)) } > ROM
    ASSERT(SIZEOF(.text.frs_scan) == 0x2E8, "native FRS scan instruction/literal size")

    .text 0x1064 :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > ROM

    .ARM.exidx : { *(.ARM.exidx*) } > ROM
    ASSERT(. <= 0x6B4C, "code overlaps original readonly data bank")
    .rom.rodata_6b4c 0x6B4C : { KEEP(*(.image.rom.rom_rodata_6b4c)) } > ROM
    .rtkit.product 0x6B60 : { KEEP(*(.rtkit.product)) } > ROM
    .rom.rodata_6b76 0x6B76 : { KEEP(*(.image.rom.rom_rodata_6b76)) } > ROM

    .frs.channels_5 0x6BA4 : { KEEP(*(.image.rom.frs_channels_5)) } > ROM
    .frs.channels_7 0x6BA9 : { KEEP(*(.image.rom.frs_channels_7)) } > ROM
    .frs.channels_8a 0x6BB0 : { KEEP(*(.image.rom.frs_channels_8a)) } > ROM
    .frs.channels_5b 0x6BB8 : { KEEP(*(.image.rom.frs_channels_5b)) } > ROM
    .frs.channels_8b 0x6BBD : { KEEP(*(.image.rom.frs_channels_8b)) } > ROM
    .frs.channels_8c 0x6BC5 : { KEEP(*(.image.rom.frs_channels_8c)) } > ROM
    .frs.channels_8d 0x6BCD : { KEEP(*(.image.rom.frs_channels_8d)) } > ROM
    .frs.channels_8e 0x6BD5 : { KEEP(*(.image.rom.frs_channels_8e)) } > ROM
    .rom.rodata_6bdd 0x6BDD : { KEEP(*(.image.rom.rom_rodata_6bdd)) } > ROM
    /* Keep ROM output sections in address order: backwards placement can
     * leave file-backed data outside every PT_LOAD despite valid section data. */
    .rtkit.build 0x7D00 : { KEEP(*(.rtkit.build)) } > ROM
    .rom.rodata_7d1c 0x7D1C : { KEEP(*(.image.rom.rom_rodata_7d1c)) } > ROM
    .frs.scan_log 0x7E48 : { KEEP(*(.image.rom.frs_scan_log)) } > ROM
    ASSERT(SIZEOF(.frs.scan_log) == 43, "FRS scan log wire size")
    .rom.rodata_7e73 0x7E73 : { KEEP(*(.image.rom.rom_rodata_7e73)) } > ROM
    ASSERT(. == 0x88D2, "reference readonly data bank end")
    /* Runtime reload image follows ROM at the unpadded address 0x88D2. */
    .image.prefix 0x4D280000 : AT(0x88D2) { KEEP(*(.image.prefix)) } > RAM
    .data 0x4D289298 : AT(0x88D2 + 0x9298)
    {
        . = 0x4D289298; KEEP(*(.image.data.trace_ring))
        . = 0x4D28929C; KEEP(*(.image.data.mac_mbox))
        . = 0x4D2892A0; KEEP(*(.image.data.frs_default_channel_lists))
        . = 0x4D2892C0; KEEP(*(.image.data.tx_tone_mode_code))
        . = 0x4D2892C4; KEEP(*(.image.data.tx_power_mode_override_value))
        . = 0x4D2892C8; KEEP(*(.image.data.txpwr_pa_codes))
        . = 0x4D28959E; KEEP(*(.image.data.txpwr_pa_gain_code))
        . = 0x4D2895A2; KEEP(*(.image.data.txpwr_pa_seq_code))
        . = 0x4D2895A8; KEEP(*(.image.data.txpwr_workspace_cell))
        . = 0x4D2895AC; KEEP(*(.image.data.txpwr_iq_scale))
        . = 0x4D2895B0; KEEP(*(.image.data.txpwr_coex_floor))
        . = 0x4D2895B4; KEEP(*(.image.data.loopback_stats_cell))
        . = 0x4D2895B8; KEEP(*(.image.data.loopback_control_cell))
        . = 0x4D2895BC; KEEP(*(.image.data.crash_record))
        . = 0x4D2895C0; KEEP(*(.image.data.txpwr_debug_flags))
        . = 0x4D2895C4; KEEP(*(.image.data.phy_status))
        . = 0x4D2895C8; KEEP(*(.image.data.btp_build_id))
        . = 0x4D2895CC; KEEP(*(.image.data.btp_version_format))
        . = 0x4D2895D0; KEEP(*(.image.data.nvv_rfseq_blob))
        . = 0x4D289804; KEEP(*(.image.data.nvv_rfseq_fallback))
        . = 0x4D289818; KEEP(*(.image.data.nvv_dynamic_exp_rev2))
        . = 0x4D289820; KEEP(*(.image.data.nvv_dynamic_mant_rev2))
        . = 0x4D289840;
    } > RAM
    .unplaced.data : { *(.data .data.*) *(.image.data.*) } > RAM
    ASSERT(SIZEOF(.unplaced.data) == 0, "unplaced initialized object")
    .rtkit.guard 0x4D289840 : AT(0x88D2 + 0x9840) { KEEP(*(.rtkit.guard)) } > RAM
    .rtkit.meta 0x4D289844 : AT(0x88D2 + 0x9844) { KEEP(*(.rtkit.meta)) } > RAM
    ASSERT(SIZEOF(.rtkit.meta) == 0x79, "RTKit metadata wire size")

    /* Early .bss: small per-module state ahead of the stacks. */
    .state.clock        0x4D289900 (NOLOAD) : { KEEP(*(.state.clock)) }        > RAM
    .state.mac          0x4D28990C (NOLOAD) : { KEEP(*(.state.mac)) }          > RAM
    .state.rx_ovr       0x4D289910 (NOLOAD) : { KEEP(*(.state.rx_ovr)) }       > RAM
    .state.tx_ovr       0x4D289930 (NOLOAD) : { KEEP(*(.state.tx_ovr)) }       > RAM
    .state.txpwr_mode   0x4D289948 (NOLOAD) : { KEEP(*(.state.txpwr_mode)) }   > RAM
    .state.loopback     0x4D28996C (NOLOAD) : { KEEP(*(.state.loopback)) }     > RAM
    .state.warm_boot    0x4D2899AC (NOLOAD) : { KEEP(*(.state.warm_boot)) }    > RAM
    .state.format       0x4D2899B4 (NOLOAD) : { KEEP(*(.state.format)) }       > RAM
    .state.nvv_presence 0x4D289A38 (NOLOAD) : { KEEP(*(.state.nvv_presence)) } > RAM
    .state.rtk          0x4D289A40 (NOLOAD) : { KEEP(*(.state.rtk)) }          > RAM

    .stacks 0x4D289AC8 (NOLOAD) : { . += 0x800; } > RAM

    /* Late .bss. */
    .state.diag         0x4D28A2C8 (NOLOAD) : { KEEP(*(.state.diag)) }         > RAM
    .state.frs          0x4D28A300 (NOLOAD) : { KEEP(*(.state.frs)) }          > RAM
    .state.rx           0x4D28A9C8 (NOLOAD) : { KEEP(*(.state.rx)) }           > RAM
    .state.dpd          0x4D28AA88 (NOLOAD) : { KEEP(*(.state.dpd)) }          > RAM
    .state.tx           0x4D28B674 (NOLOAD) : { KEEP(*(.state.tx)) }           > RAM
    .state.txpwr        0x4D28B700 (NOLOAD) : { KEEP(*(.state.txpwr)) }        > RAM
    .state.syscfg       0x4D28CB24 (NOLOAD) : { KEEP(*(.state.syscfg)) }       > RAM
    .state.lb_saved     0x4D28CF1C (NOLOAD) : { KEEP(*(.state.lb_saved)) }     > RAM
    .state.radio_rev    0x4D28CF44 (NOLOAD) : { KEEP(*(.state.radio_rev)) }    > RAM
    .state.nvv_tables   0x4D28CF48 (NOLOAD) : { KEEP(*(.state.nvv_tables)) }   > RAM
    .state.mpu          0x4D28CFE0 (NOLOAD) : { KEEP(*(.state.mpu)) }          > RAM
    .state.rtk_irq      0x4D28D170 (NOLOAD) : { KEEP(*(.state.rtk_irq)) }      > RAM

    /* Anything not pinned above is a reconstruction error. */
    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM
    ASSERT(__bss_end == __bss_start, "unplaced .bss object; add it to a state block")
    ASSERT(. <= 0x4D28D178, "RAM image overflows RTSZ (0x4D28D178)")
}
