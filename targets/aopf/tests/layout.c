/*
 * layout.c — Compile-time _Static_assert structural proofs for wire/header
 * formats (checked on both ARMv7E-M and 64-bit host) and exact ARMv7E-M
 * device/vtable byte offsets matching raw aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/aopf.h"

/* 1. Architecture-independent binary image & wire structs */
_Static_assert(sizeof(aopf_uuid_header_t) == 0x48u, "uuid v5 header must be 0x48 bytes");
_Static_assert(offsetof(aopf_uuid_header_t, uuid_bytes) == 0x10u, "uuid offset");
_Static_assert(offsetof(aopf_uuid_header_t, metadata_tlv_vma) == 0x28u, "tlv vma offset");
_Static_assert(offsetof(aopf_uuid_header_t, text_end_vma) == 0x30u, "text_end offset");

_Static_assert(sizeof(aopf_fwsg_segment_t) == 0x20u, "fwsg segment entry must be 32 bytes");
_Static_assert(offsetof(aopf_fwsg_segment_t, fileoff) == 0x08u, "fwsg fileoff");
_Static_assert(offsetof(aopf_fwsg_segment_t, filesize) == 0x0cu, "fwsg filesize");
_Static_assert(offsetof(aopf_fwsg_segment_t, vmsize) == 0x10u, "fwsg vmsize");
_Static_assert(offsetof(aopf_fwsg_segment_t, segname) == 0x18u, "fwsg segname");

_Static_assert(sizeof(aopf_fwsg_trailer_t) == 0x20u, "fwsg trailer must be 32 bytes");
_Static_assert(sizeof(rtk_tlv_hdr_t) == 8u, "TLV header must be 8 bytes");
_Static_assert(sizeof(rtk_mbox_msg128_t) == 16u, "AKF mailbox FIFO msg must be 128 bits");
_Static_assert(sizeof(rtk_mpu_shadow_entry_t) == 16u, "MPU shadow entry must be 16 bytes");
_Static_assert(sizeof(rtk_pmgr_entry_t) == 16u, "PMGR lookup entry must be 16 bytes");
_Static_assert(sizeof(rtk_once_t) == 8u, "rtk_once_t must be 8 bytes");

/* 2. Exact 32-bit ARMv7E-M device & vtable offsets verified against disassembly */
#if __SIZEOF_POINTER__ == 4

_Static_assert(sizeof(rtk_dev_hdr_t) == 0x18u, "rtk_dev_hdr_t must be 0x18 bytes on ARM");
_Static_assert(offsetof(rtk_dev_hdr_t, mmio_base) == 0x08u, "mmio_base @ +0x08");
_Static_assert(offsetof(rtk_dev_hdr_t, mmio_hi) == 0x0cu, "mmio_hi @ +0x0c");
_Static_assert(offsetof(rtk_dev_hdr_t, claimed_flags) == 0x14u, "claimed_flags @ +0x14");

_Static_assert(offsetof(rtk_nvic_vtable_t, dispatch_irq) == 0x0cu, "nvic vt +0x0c");
_Static_assert(offsetof(rtk_nvic_vtable_t, get_cpsr) == 0x10u, "nvic vt +0x10");
_Static_assert(offsetof(rtk_nvic_vtable_t, set_irq_enabled) == 0x14u, "nvic vt +0x14");
_Static_assert(offsetof(rtk_nvic_vtable_t, trigger_irq) == 0x18u, "nvic vt +0x18");
_Static_assert(offsetof(rtk_nvic_vtable_t, register_irq) == 0x1cu, "nvic vt +0x1c");
_Static_assert(offsetof(rtk_nvic_vtable_t, set_irq_priority) == 0x20u, "nvic vt +0x20");

_Static_assert(offsetof(rtk_systick_dev_t, ext_clk_hz) == 0x18u, "systick +0x18");
_Static_assert(offsetof(rtk_systick_dev_t, use_ext_clk) == 0x1cu, "systick +0x1c");
_Static_assert(offsetof(rtk_systick_dev_t, callback_fn) == 0x20u, "systick +0x20");
_Static_assert(offsetof(rtk_systick_dev_t, callback_ctx) == 0x24u, "systick +0x24");
_Static_assert(offsetof(rtk_systick_dev_t, ten_khz_ticks) == 0x28u, "systick +0x28");
_Static_assert(offsetof(rtk_systick_dev_t, cpu_clk_hz) == 0x2cu, "systick +0x2c");
_Static_assert(offsetof(rtk_systick_dev_t, rem_ticks_lo) == 0x30u, "systick +0x30");
_Static_assert(offsetof(rtk_systick_dev_t, rem_ticks_hi) == 0x34u, "systick +0x34");
_Static_assert(offsetof(rtk_systick_dev_t, periodic_mode) == 0x38u, "systick +0x38");

_Static_assert(offsetof(rtk_timebase_dev_t, freq_hz) == 0x18u, "timebase +0x18");
_Static_assert(offsetof(rtk_timebase_dev_t, lo_reg_off) == 0x1cu, "timebase +0x1c");
_Static_assert(offsetof(rtk_timebase_dev_t, hi_reg_off) == 0x20u, "timebase +0x20");
_Static_assert(offsetof(rtk_timebase_dev_t, offset_lo) == 0x30u, "timebase +0x30");
_Static_assert(offsetof(rtk_timebase_dev_t, offset_hi) == 0x34u, "timebase +0x34");
_Static_assert(offsetof(rtk_timebase_dev_t, sync_target_lo) == 0x40u, "timebase +0x40");
_Static_assert(offsetof(rtk_timebase_dev_t, sync_raw_lo) == 0x48u, "timebase +0x48");
_Static_assert(offsetof(rtk_timebase_dev_t, fast_1mhz_flag) == 0x50u, "timebase +0x50");
_Static_assert(offsetof(rtk_timebase_dev_t, offset_neg) == 0x54u, "timebase +0x54");
_Static_assert(offsetof(rtk_timebase_dev_t, slew_active) == 0x55u, "timebase +0x55");

_Static_assert(sizeof(rtk_timer_chan_state_t) == 0x10u, "timer chan state 16B");
_Static_assert(offsetof(rtk_timer_bank_dev_t, num_channels) == 0x1cu, "timer_bank +0x1c");
_Static_assert(offsetof(rtk_timer_bank_dev_t, channels) == 0x20u, "timer_bank +0x20");

_Static_assert(offsetof(rtk_fuse_dev_t, lock_word) == 0x20u, "fuse lock_word @ +0x20");
_Static_assert(offsetof(rtk_pmgr_dev_t, entries) == 0x18u, "pmgr entries @ +0x18");
_Static_assert(offsetof(rtk_pmgr_dev_t, locked_flag) == 0x24u, "pmgr locked_flag @ +0x24");

_Static_assert(offsetof(rtk_wdt_dev_t, bark_irq) == 0x18u, "wdt bark_irq @ +0x18");
_Static_assert(offsetof(rtk_wdt_dev_t, allow_bite_no_cb) == 0x1cu, "wdt +0x1c");
_Static_assert(offsetof(rtk_wdt_dev_t, bite_prescaler) == 0x20u, "wdt +0x20");
_Static_assert(offsetof(rtk_wdt_dev_t, bark_cb) == 0x24u, "wdt +0x24");
_Static_assert(offsetof(rtk_wdt_dev_t, bark_ctx) == 0x28u, "wdt +0x28");

_Static_assert(offsetof(rtk_mbox_dev_t, irq_ctrl) == 0x30u, "mbox +0x30");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_irq) == 0x38u, "mbox +0x38");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_defer_count) == 0x48u, "mbox +0x48");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_int_set_mmio) == 0x58u, "mbox +0x58");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_int_en_flags) == 0x64u, "mbox +0x64");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_int_bit_shift) == 0x68u, "mbox +0x68");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_empty_arm_en) == 0x90u, "mbox +0x90");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_drained_flag) == 0x95u, "mbox +0x95");
_Static_assert(offsetof(rtk_mbox_dev_t, err_reg_off) == 0xb0u, "mbox +0xb0");
_Static_assert(offsetof(rtk_mbox_dev_t, rx_fifo_off) == 0xb8u, "mbox +0xb8");

_Static_assert(sizeof(rtk_heap_block_t) == 12u, "heap block header 12B");
_Static_assert(offsetof(rtk_heap_ctx_t, region_count) == 0x40u, "heap +0x40");
_Static_assert(offsetof(rtk_heap_ctx_t, total_bytes) == 0x44u, "heap +0x44");
_Static_assert(offsetof(rtk_heap_ctx_t, free_head) == 0x48u, "heap +0x48");
_Static_assert(offsetof(rtk_heap_ctx_t, initialized) == 0x4cu, "heap +0x4c");
_Static_assert(offsetof(rtk_heap_ctx_t, stats[4]) == 0x60u, "heap allocated_bytes @ +0x60");

_Static_assert(sizeof(rtk_vm_entry_t) == 0x14u, "rtk_vm_entry_t 20B");
_Static_assert(sizeof(rtk_fmt_sink_t) == 0x14u, "rtk_fmt_sink_t 20B");
_Static_assert(sizeof(aop_aud_log_buf_t) == 0x0cu, "aop_aud_log_buf_t 12B");
_Static_assert(sizeof(aop_prop_node_t) == 0x10u, "aop_prop_node_t 16B");
_Static_assert(sizeof(aop_prop_table_t) == 0x24u, "aop_prop_table_t 36B");

#endif

int aopf_layout_checks_compiled(void);
int aopf_layout_checks_compiled(void)
{
    return 1;
}
