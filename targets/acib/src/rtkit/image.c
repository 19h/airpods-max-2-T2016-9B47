#include "acib/acib.h"
#include "acib/aci.h"
#include "acib/phy_ipc.h"
#include "acib/rtkit.h"

/*
 * Pinned RAM_BSS definitions covering 0x1000AC1E .. 0x1002D2C4 (0x226A6 bytes).
 * Every reconstructed subsystem accesses its state at the exact firmware VMA.
 */

ACIB_SECTION(".state.pre_zero") ACIB_USED
uint8_t g_bss_pre_zero[0x22];

ACIB_SECTION(".state.pad_ac40") ACIB_USED
uint8_t g_bss_pad_ac40[0x4274];

ACIB_SECTION(".state.irq_handlers") ACIB_USED
acib_irq_handler_fn g_rtk_irq_handlers[130];

ACIB_SECTION(".state.irq_contexts") ACIB_USED
uintptr_t g_rtk_irq_contexts[130];

ACIB_SECTION(".state.pad_f2c4") ACIB_USED
uint8_t g_bss_pad_f2c4[0xBCBC];

ACIB_SECTION(".state.psp_init_stack") ACIB_USED
uint32_t g_psp_init_stack[384];

ACIB_SECTION(".state.msp_stack") ACIB_USED
uint32_t g_msp_stack[512];

ACIB_SECTION(".state.pad_1bd80") ACIB_USED
uint8_t g_bss_pad_1bd80[0x42EC];

ACIB_SECTION(".state.boot_mode_2006c") ACIB_USED
uint32_t g_boot_mode_ctx[10];

ACIB_SECTION(".state.pad_20094") ACIB_USED
uint8_t g_bss_pad_20094[0x27C];

ACIB_SECTION(".state.sys_threads_20310") ACIB_USED
uint32_t g_bt_sys_threads_state[4];

ACIB_SECTION(".state.pad_20320") ACIB_USED
uint8_t g_bss_pad_20320[0x648];

ACIB_SECTION(".state.clock_ctx_20968") ACIB_USED
struct acib_clock_ctx g_acib_clock_ctx;

ACIB_SECTION(".state.pad_209ac") ACIB_USED
uint8_t g_bss_pad_209ac[0x1B4];

ACIB_SECTION(".state.panic_reason_20b60") ACIB_USED
uint32_t g_acib_panic_reason;

ACIB_SECTION(".state.pad_20b64") ACIB_USED
uint8_t g_bss_pad_20b64[0x14];

ACIB_SECTION(".state.bt_ctrl_ctx_20b78") ACIB_USED
uint8_t g_bt_ctrl_ctx[0x6B0];

ACIB_SECTION(".state.osa_tasks_21228") ACIB_USED
uint8_t g_osa_task_table[0x400];

ACIB_SECTION(".state.pad_21628") ACIB_USED
uint8_t g_bss_pad_21628[0x1A4];

ACIB_SECTION(".state.phy_crash_ctx_217cc") ACIB_USED
uint32_t g_phy_crash_log_ctx;

ACIB_SECTION(".state.pad_217d0") ACIB_USED
uint8_t g_bss_pad_217d0[0x434];

ACIB_SECTION(".state.rtk_threads_21c04") ACIB_USED
void *g_rtk_thread_ptr_table[16];

ACIB_SECTION(".state.pad_21c44") ACIB_USED
uint8_t g_bss_pad_21c44[0x23C];

ACIB_SECTION(".state.psp_reset_top_21e80") ACIB_USED
uint32_t g_psp_reset_marker;

ACIB_SECTION(".state.pad_21e84") ACIB_USED
uint8_t g_bss_pad_21e84[0x25];

ACIB_SECTION(".state.phy_table_shadow_21ea9") ACIB_USED
uint8_t g_phy_table_shadow[0x120];

ACIB_SECTION(".state.pad_21fc9") ACIB_USED
uint8_t g_bss_pad_21fc9[0x6C3];

ACIB_SECTION(".state.phy_ipc_2268c") ACIB_USED
struct acib_phy_ipc_state g_phy_ipc_state;

ACIB_SECTION(".state.pad_226c8") ACIB_USED
uint8_t g_bss_pad_226c8[0x2BC];

ACIB_SECTION(".state.board_props_22984") ACIB_USED
struct acib_board_props g_acib_board_props;

ACIB_SECTION(".state.pad_22994") ACIB_USED
uint8_t g_bss_pad_22994[0x4FC4];

ACIB_SECTION(".state.rf_tx_ctx_27958") ACIB_USED
uint8_t g_rf_tx_mode_ctx[0xB4];

ACIB_SECTION(".state.pad_27a0c") ACIB_USED
uint8_t g_bss_pad_27a0c[0x275C];

ACIB_SECTION(".state.phy_ack_ctx_2a168") ACIB_USED
uint8_t g_phy_ack_ctx[0x10];

ACIB_SECTION(".state.pad_2a178") ACIB_USED
uint8_t g_bss_pad_2a178[0x2DF0];

ACIB_SECTION(".state.tm_cb_2cf68") ACIB_USED
struct acib_tm_buf_cb g_tm_default_cb;

ACIB_SECTION(".state.pad_2cf70") ACIB_USED
uint8_t g_bss_pad_2cf70[0x44];

ACIB_SECTION(".state.nmi_sp_2cfb4") ACIB_USED
uintptr_t g_rtk_nmi_saved_sp;

ACIB_SECTION(".state.heap_ctx_2cfb8") ACIB_USED
uint8_t g_rtk_heap_ctx_pad[0x110];

ACIB_SECTION(".state.sched_ready_2d0c8") ACIB_USED
uint32_t g_rtk_ready_mask;

ACIB_SECTION(".state.sched_ready_2d0c8") ACIB_USED
uint32_t g_rtk_ready_pad;

ACIB_SECTION(".state.sched_ready_2d0c8") ACIB_USED
uintptr_t g_rtk_ready_queues[92];

ACIB_SECTION(".state.sched_flags_2d240") ACIB_USED
uint8_t g_rtk_sched_need_resched;

ACIB_SECTION(".state.sched_flags_2d240") ACIB_USED
uint8_t g_rtk_sched_pad[3];

ACIB_SECTION(".state.sched_flags_2d240") ACIB_USED
uint8_t g_rtk_sched_preempted;

ACIB_SECTION(".state.sched_flags_2d240") ACIB_USED
uint8_t g_rtk_sched_pad2[3];

ACIB_SECTION(".state.pad_2d248") ACIB_USED
uint8_t g_bss_pad_2d248[0x74];

ACIB_SECTION(".state.irq_mask_cfg_2d2bc") ACIB_USED
uint32_t g_rtk_irq_mask_basepri;

ACIB_SECTION(".state.irq_mask_cfg_2d2bc") ACIB_USED
uint32_t g_rtk_irq_prio_group;
