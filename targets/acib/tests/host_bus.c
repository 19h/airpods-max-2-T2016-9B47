#include "host_bus.h"
#include "acib/aci.h"
#include "acib/memmap.h"
#include "acib/phy_ipc.h"
#include "acib/regs.h"
#include "acib/rtkit.h"
#include "acib/shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ACIB_BLOBS_DIR
#define ACIB_BLOBS_DIR "build/blobs"
#endif

/* Virtual address spaces for host behavioral tests */
static uint8_t s_vrom[0x100000];      /* 0x00000000 .. 0x000FFFFF */
static uint8_t s_vsram[0x40000];      /* 0x10000000 .. 0x1003FFFF */
static uint8_t s_vsocshm[0x10000];    /* 0x20000000 .. 0x2000FFFF */
static uint8_t s_vmbox[0x1000];       /* 0x4D280000 .. 0x4D280FFF */

/* Sparse MMIO register table */
#define HOST_MMIO_SLOTS 4096u
struct mmio_slot {
    uint32_t addr;
    uint32_t val;
    bool     used;
};
static struct mmio_slot s_mmio[HOST_MMIO_SLOTS];

struct acib_host_cpu_state   g_acib_host_cpu;
struct acib_host_panic_state g_host_panic;
struct acib_host_spy_state   g_host_spy;
struct acib_mmio_write_rec   g_host_mmio_writes[ACIB_HOST_MAX_MMIO_LOG];
uint32_t                     g_host_mmio_write_count;

/* Host definitions of firmware BSS globals (on ARM, defined in src/rtkit/image.c) */
acib_irq_handler_fn     g_rtk_irq_handlers[130];
uintptr_t               g_rtk_irq_contexts[130];
uint32_t                g_psp_init_stack[384];
uint32_t                g_msp_stack[512];
uint32_t                g_boot_mode_ctx[10];
uint32_t                g_bt_sys_threads_state[4];
struct acib_clock_ctx   g_acib_clock_ctx;
uint32_t                g_acib_panic_reason;
void                   *g_rtk_thread_ptr_table[16];
struct acib_board_props g_acib_board_props;
uintptr_t               g_rtk_nmi_saved_sp;
uint32_t                g_rtk_ready_mask;
uint32_t                g_rtk_ready_pad;
uintptr_t               g_rtk_ready_queues[92];
uint8_t                 g_rtk_sched_need_resched;
uint8_t                 g_rtk_sched_pad[3];
uint8_t                 g_rtk_sched_preempted;
uint32_t                g_rtk_irq_mask_basepri;
uint32_t                g_rtk_irq_prio_group;

struct acib_phy_ipc_state g_phy_ipc_state;
uint8_t                   g_phy_table_shadow[0x120];
uint32_t                  g_phy_crash_log_ctx;
uint8_t                   g_rf_tx_mode_ctx[0xB4];
uint8_t                   g_phy_ack_ctx[0x10];

struct acib_tm_buf_cb   g_tm_default_cb;
uint8_t                 g_bt_ctrl_ctx[0x6B0];
uint8_t                 g_osa_task_table[0x400];

static bool     s_blobs_cached;
static uint8_t  s_rom_rodata_cache[ACIB_ROM_RODATA_SIZE];
static uint8_t  s_ram_data_cache[ACIB_RAM_DATA_SIZE];

static void load_blob_once(const char *filename, uint8_t *dst, size_t expected_len)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", ACIB_BLOBS_DIR, filename);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "FATAL: cannot open blob %s\n", path);
        exit(2);
    }
    size_t got = fread(dst, 1, expected_len, fp);
    fclose(fp);
    if (got != expected_len) {
        fprintf(stderr, "FATAL: short read on %s (%zu != %zu)\n", path, got, expected_len);
        exit(2);
    }
}

static struct mmio_slot *find_or_alloc_mmio(uint32_t aligned_addr)
{
    for (uint32_t i = 0; i < HOST_MMIO_SLOTS; i++) {
        if (s_mmio[i].used && s_mmio[i].addr == aligned_addr) {
            return &s_mmio[i];
        }
    }
    for (uint32_t i = 0; i < HOST_MMIO_SLOTS; i++) {
        if (!s_mmio[i].used) {
            s_mmio[i].used = true;
            s_mmio[i].addr = aligned_addr;
            s_mmio[i].val  = 0u;
            return &s_mmio[i];
        }
    }
    fprintf(stderr, "FATAL:HOST_MMIO_SLOTS exhausted at 0x%08x\n", aligned_addr);
    exit(2);
}

void acib_host_set_mmio32(uint32_t addr, uint32_t raw_val)
{
    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    slot->val = raw_val;
}

uint32_t acib_host_get_mmio32(uint32_t addr)
{
    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    return slot->val;
}

uint8_t *acib_host_mbox_ptr(uint32_t offset)
{
    return &s_vmbox[offset & 0xFFFu];
}

void acib_host_reset(void)
{
    if (!s_blobs_cached) {
        load_blob_once("rom_rodata.bin", s_rom_rodata_cache, ACIB_ROM_RODATA_SIZE);
        load_blob_once("ram_data.bin", s_ram_data_cache, ACIB_RAM_DATA_SIZE);
        s_blobs_cached = true;
    }

    memset(s_vrom, 0, sizeof(s_vrom));
    memset(s_vsram, 0, sizeof(s_vsram));
    memset(s_vsocshm, 0, sizeof(s_vsocshm));
    memset(s_vmbox, 0, sizeof(s_vmbox));
    memset(s_mmio, 0, sizeof(s_mmio));

    memcpy(&s_vrom[ACIB_ROM_RODATA_BASE], s_rom_rodata_cache, ACIB_ROM_RODATA_SIZE);
    memcpy(&s_vsram[0], s_ram_data_cache, ACIB_RAM_DATA_SIZE);

    memset(&g_acib_host_cpu, 0, sizeof(g_acib_host_cpu));
    memset(&g_host_panic, 0, sizeof(g_host_panic));
    memset(&g_host_spy, 0, sizeof(g_host_spy));
    memset(g_host_mmio_writes, 0, sizeof(g_host_mmio_writes));
    g_host_mmio_write_count = 0u;

    memset(g_rtk_irq_handlers, 0, sizeof(g_rtk_irq_handlers));
    memset(g_rtk_irq_contexts, 0, sizeof(g_rtk_irq_contexts));
    memset(g_psp_init_stack, 0, sizeof(g_psp_init_stack));
    memset(g_msp_stack, 0, sizeof(g_msp_stack));
    memset(g_boot_mode_ctx, 0, sizeof(g_boot_mode_ctx));
    memset(g_bt_sys_threads_state, 0, sizeof(g_bt_sys_threads_state));
    memset(&g_acib_clock_ctx, 0, sizeof(g_acib_clock_ctx));
    g_acib_panic_reason = 0u;
    memset(g_rtk_thread_ptr_table, 0, sizeof(g_rtk_thread_ptr_table));
    memset(&g_acib_board_props, 0, sizeof(g_acib_board_props));
    g_rtk_nmi_saved_sp = 0u;
    g_rtk_ready_mask = 0u;
    g_rtk_ready_pad = 0u;
    memset(g_rtk_ready_queues, 0, sizeof(g_rtk_ready_queues));
    g_rtk_sched_need_resched = 0u;
    memset(g_rtk_sched_pad, 0, sizeof(g_rtk_sched_pad));
    g_rtk_sched_preempted = 0u;
    g_rtk_irq_mask_basepri = 0x20u;
    g_rtk_irq_prio_group = 5u;

    memset(&g_phy_ipc_state, 0, sizeof(g_phy_ipc_state));
    memset(g_phy_table_shadow, 0, sizeof(g_phy_table_shadow));
    g_phy_crash_log_ctx = 0u;
    memset(g_rf_tx_mode_ctx, 0, sizeof(g_rf_tx_mode_ctx));
    memset(g_phy_ack_ctx, 0, sizeof(g_phy_ack_ctx));

    memset(&g_tm_default_cb, 0, sizeof(g_tm_default_cb));
    memset(g_bt_ctrl_ctx, 0, sizeof(g_bt_ctrl_ctx));
    memset(g_osa_task_table, 0, sizeof(g_osa_task_table));

    /* Sensible hardware reset defaults: Cortex-M4 MPU with 8 regions */
    acib_host_set_mmio32(ACIB_MPU_TYPE_ADDR, 0x00000800u);
}

void *acib_host_translate_ptr(uint32_t addr)
{
    if (addr < sizeof(s_vrom)) {
        return &s_vrom[addr];
    }
    if (addr >= ACIB_RAM_DATA_BASE && addr < ACIB_RAM_DATA_BASE + sizeof(s_vsram)) {
        return &s_vsram[addr - ACIB_RAM_DATA_BASE];
    }
    if (addr >= ACIB_MPU_R4_SOC_SHM_BASE && addr < ACIB_MPU_R4_SOC_SHM_BASE + sizeof(s_vsocshm)) {
        return &s_vsocshm[addr - ACIB_MPU_R4_SOC_SHM_BASE];
    }
    if (addr >= ACIB_PHY_MBOX_BASE && addr < ACIB_PHY_MBOX_BASE + sizeof(s_vmbox)) {
        return &s_vmbox[addr - ACIB_PHY_MBOX_BASE];
    }
    fprintf(stderr, "FATAL: unmapped acib_addr_ptr(0x%08x)\n", addr);
    exit(2);
}

static void record_mmio_write(uint32_t addr, uint32_t val, uint8_t width)
{
    if (g_host_mmio_write_count < ACIB_HOST_MAX_MMIO_LOG) {
        g_host_mmio_writes[g_host_mmio_write_count].addr  = addr;
        g_host_mmio_writes[g_host_mmio_write_count].val   = val;
        g_host_mmio_writes[g_host_mmio_write_count].width = width;
        g_host_mmio_write_count++;
    }
}

uint32_t acib_host_mmio_read32(uint32_t addr)
{
    if (addr >= ACIB_PHY_MBOX_BASE && addr < ACIB_PHY_MBOX_BASE + sizeof(s_vmbox)) {
        return acib_read_unaligned_le32(&s_vmbox[addr - ACIB_PHY_MBOX_BASE]);
    }
    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    uint32_t cur = slot->val;
    if (addr == ACIB_DWT_CYCCNT_ADDR && g_host_spy.cyccnt_step_per_read != 0u) {
        slot->val = cur + g_host_spy.cyccnt_step_per_read;
    } else if (addr == ACIB_BT_TIMER_US_ADDR && g_host_spy.timer_step_per_read != 0u) {
        slot->val = cur + g_host_spy.timer_step_per_read;
    }
    return cur;
}

void acib_host_mmio_write32(uint32_t addr, uint32_t val)
{
    record_mmio_write(addr, val, 4u);

    if (addr >= ACIB_PHY_MBOX_BASE && addr < ACIB_PHY_MBOX_BASE + sizeof(s_vmbox)) {
        acib_write_unaligned_le32(&s_vmbox[addr - ACIB_PHY_MBOX_BASE], val);
        return;
    }

    if (addr == ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) {
        /* W1C hardware semantics on 0x4D154C64 */
        struct mmio_slot *sts = find_or_alloc_mmio(addr);
        sts->val &= ~val;
        return;
    }

    /* NVIC ISER / ICER / ISPR / ICPR pair semantics */
    if (addr >= ACIB_NVIC_ISER0_ADDR && addr < ACIB_NVIC_ISER0_ADDR + 0x20u) {
        struct mmio_slot *iser = find_or_alloc_mmio(addr);
        iser->val |= val;
        return;
    }
    if (addr >= ACIB_NVIC_ICER0_ADDR && addr < ACIB_NVIC_ICER0_ADDR + 0x20u) {
        struct mmio_slot *iser = find_or_alloc_mmio(addr - 0x80u);
        iser->val &= ~val;
        struct mmio_slot *icer = find_or_alloc_mmio(addr);
        icer->val = val;
        return;
    }

    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    slot->val = val;

    /* Simulated W3 PHY coprocessor reactions to MAC->PHY doorbell 0x4D154C68 */
    if (addr == ACIB_PHY_IPC_FROM_MAC_DB_ADDR) {
        struct mmio_slot *sts = find_or_alloc_mmio(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
        if ((val & ACIB_PHY_DB_CMD_REQ) != 0u && g_host_spy.auto_ack_sync_cmd) {
            if (g_host_spy.mutate_legacy_probe_on_cmd) {
                /* Simulate legacy PHY writing 4x u16 version into 0x4D2801EE..0x4D2801F5 */
                acib_write_unaligned_le16(&s_vmbox[0x1EEu], 0x0102u);
                acib_write_unaligned_le16(&s_vmbox[0x1F0u], 0x0304u);
                acib_write_unaligned_le16(&s_vmbox[0x1F2u], 0x0506u);
                acib_write_unaligned_le16(&s_vmbox[0x1F4u], 0x0708u);
            }
            sts->val |= ACIB_PHY_STATUS_CMD_ACK;
        }
        if ((val & ACIB_PHY_DB_FRS_START) != 0u && !g_host_spy.frs_timeout_mode) {
            uint32_t resp_off = (g_phy_ipc_state.resp_addr - ACIB_PHY_MBOX_BASE) & 0xFFFu;
            s_vmbox[resp_off + 0x1C0u] = g_host_spy.frs_auto_complete_status;
            sts->val |= ACIB_PHY_STATUS_FRS_DONE;
        }
    }
}

uint16_t acib_host_mmio_read16(uint32_t addr)
{
    uint32_t w = acib_host_mmio_read32(addr & ~3u);
    uint32_t shift = (addr & 2u) * 8u;
    return (uint16_t)((w >> shift) & 0xFFFFu);
}

void acib_host_mmio_write16(uint32_t addr, uint16_t val)
{
    record_mmio_write(addr, val, 2u);
    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    uint32_t shift = (addr & 2u) * 8u;
    slot->val = (slot->val & ~(0xFFFFu << shift)) | ((uint32_t)val << shift);
}

uint8_t acib_host_mmio_read8(uint32_t addr)
{
    uint32_t w = acib_host_mmio_read32(addr & ~3u);
    uint32_t shift = (addr & 3u) * 8u;
    return (uint8_t)((w >> shift) & 0xFFu);
}

void acib_host_mmio_write8(uint32_t addr, uint8_t val)
{
    record_mmio_write(addr, val, 1u);
    struct mmio_slot *slot = find_or_alloc_mmio(addr & ~3u);
    uint32_t shift = (addr & 3u) * 8u;
    slot->val = (slot->val & ~(0xFFu << shift)) | ((uint32_t)val << shift);
}

/* =========================================================================
 * Controllable Host Spies & Link Boundaries for External pending_c Symbols
 * ========================================================================= */

void sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg);
void sub_EAC84(uint32_t code, uint32_t sub, const char *fmt, uint32_t *args_ptr, uint32_t *sp_arg);
void sub_6CFF4(uint32_t halt_reason);
uint32_t sub_EA86E(uint32_t addr);
void sub_E5E78(void *exc_frame, uint32_t ipsr);

uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count);
uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val);
uint32_t sub_7E9F8(uint32_t end_ptr);
void sub_DF764(void);
void sub_7E084(uint16_t ver_major, uint16_t ver_minor, uint16_t ver_patch, uint16_t ver_build);
void sub_7EE58(void);
int  sub_74240(uint32_t log_id);
void sub_74256(uint32_t log_id, uint32_t val0);
void sub_74270(uint32_t log_id, uint32_t val0, uint32_t val1);
void sub_74290(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2);
void sub_742B2(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2, uint32_t val3);
void sub_7DC92(uint32_t err_id);
void sub_6DF3C(uint32_t ext_irq);
void sub_6DC88(void);
void sub_6DD04(void);
void sub_6DD70(uint32_t lock_arg);
void sub_6DDB8(void);
void sub_6DE10(void);
int  sub_7CC08(uint32_t flags, uint32_t slot);
int  sub_7CC8C(uint32_t flags, uint32_t slot);
void sub_740A8(uint32_t log_id, uint32_t line);
void sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0);
void sub_35354(uint8_t *out_buf, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4);
void sub_EBA60(uint32_t ctx_addr, uint32_t tag_addr, const uint8_t *buf);
uint32_t sub_7513C(const char *msg, uint32_t code);
void sub_7835C(uint32_t delay_us);
void sub_7D820(uint32_t arg);
int  sub_CF6B4(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3,
               uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4);
void sub_96458(uint32_t reg_id, uint32_t val);
void sub_964AC(uint32_t val);
void sub_96434(uint32_t reg_id, uint32_t val);

void sub_707F0(void);
void sub_DECFC(void);
void sub_2F78(void);
int  sub_71540(uint32_t arg);
void sub_E696A(const char *fmt, const char *arg);
uint32_t sub_71240(void);
const char *sub_EBA64(uint32_t id);
void sub_7076C(void);
uint32_t sub_77A44(void);
uint32_t sub_6C850(uint32_t arg);
void sub_73F20(uint32_t arg);
void sub_7B1C4(uint32_t arg);
void sub_74F10(uint32_t arg);
void sub_6AFB0(void);
void sub_776E0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);
void sub_69978(uint32_t arg);
uint32_t sub_598E8(uint32_t arg);
uint32_t sub_133BC(uint32_t arg);
uint32_t sub_188B4(uint32_t arg);
void sub_4B5C2(uint32_t arg);
void sub_2774(void);
uint32_t sub_2584(void);
void sub_4B2B0(uint32_t arg);
uintptr_t sub_E713C(const char *name, uintptr_t entry, uint32_t id,
                    uint32_t flags, uint32_t stack_sz, uint32_t prio);
void *sub_E7C20(uintptr_t handle);
void sub_2F8C(void);
void sub_59884(void);
void sub_5688(void);

void sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg)
{
    (void)arg2;
    (void)arg3;
    (void)sp_arg;
    g_host_panic.fmt  = fmt;
    g_host_panic.arg0 = args_ptr ? args_ptr[0] : 0u;
    g_host_panic.arg1 = args_ptr ? args_ptr[1] : 0u;
    g_host_panic.arg2 = args_ptr ? args_ptr[2] : 0u;
}

void sub_EAC84(uint32_t code, uint32_t sub, const char *fmt, uint32_t *args_ptr, uint32_t *sp_arg)
{
    (void)code;
    (void)sub;
    (void)fmt;
    (void)args_ptr;
    (void)sp_arg;
}

void sub_6CFF4(uint32_t halt_reason)
{
    g_host_panic.triggered   = true;
    g_host_panic.halt_reason = halt_reason;
    if (g_host_panic.armed) {
        longjmp(g_host_panic.env, 1);
    }
    fprintf(stderr, "FATAL: unexpected firmware panic (fmt=%s, arg0=0x%x, arg1=0x%x)\n",
            g_host_panic.fmt ? g_host_panic.fmt : "<null>",
            g_host_panic.arg0, g_host_panic.arg1);
    exit(3);
}

uint32_t sub_EA86E(uint32_t addr)
{
    return (addr >= 0x108u && addr < ACIB_ROM_RODATA_BASE) ? 1u : 0u;
}

void sub_E5E78(void *exc_frame, uint32_t ipsr)
{
    (void)exc_frame;
    (void)ipsr;
}

uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count)
{
    (void)ev_base;
    return count;
}

uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val)
{
    (void)slot_ptr;
    g_host_spy.last_rf_reg_id  = reg_addr;
    g_host_spy.last_rf_reg_val = val;
    return 2u;
}

uint32_t sub_7E9F8(uint32_t end_ptr)
{
    return end_ptr;
}

void sub_DF764(void) {}

void sub_7E084(uint16_t ver_major, uint16_t ver_minor, uint16_t ver_patch, uint16_t ver_build)
{
    g_host_spy.phy_ver[0] = ver_major;
    g_host_spy.phy_ver[1] = ver_minor;
    g_host_spy.phy_ver[2] = ver_patch;
    g_host_spy.phy_ver[3] = ver_build;
    g_host_spy.phy_ver_calls++;
}

void sub_7EE58(void) {}

int sub_74240(uint32_t log_id)
{
    g_host_spy.last_log_id = log_id;
    g_host_spy.log_calls++;
    return 0;
}

void sub_74256(uint32_t log_id, uint32_t val0)
{
    (void)val0;
    g_host_spy.last_log_id = log_id;
    g_host_spy.log_calls++;
}

void sub_74270(uint32_t log_id, uint32_t val0, uint32_t val1)
{
    (void)val0;
    (void)val1;
    g_host_spy.last_log_id = log_id;
    g_host_spy.log_calls++;
}

void sub_74290(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2)
{
    (void)val0;
    (void)val1;
    (void)val2;
    g_host_spy.last_log_id = log_id;
    g_host_spy.log_calls++;
}

void sub_742B2(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2, uint32_t val3)
{
    (void)val0;
    (void)val1;
    (void)val2;
    (void)val3;
    g_host_spy.last_log_id = log_id;
    g_host_spy.log_calls++;
}

void sub_7DC92(uint32_t err_id)
{
    g_host_spy.last_log_id = err_id;
}

void sub_6DF3C(uint32_t ext_irq) { (void)ext_irq; }
void sub_6DC88(void) {}
void sub_6DD04(void) {}
void sub_6DD70(uint32_t lock_arg) { (void)lock_arg; }
void sub_6DDB8(void) {}
void sub_6DE10(void) {}
int  sub_7CC08(uint32_t flags, uint32_t slot) { (void)flags; (void)slot; return 1; }
int  sub_7CC8C(uint32_t flags, uint32_t slot) { (void)flags; (void)slot; return 1; }
void sub_740A8(uint32_t log_id, uint32_t line) { (void)line; g_host_spy.last_log_id = log_id; }
void sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0) { (void)line; (void)val0; g_host_spy.last_log_id = log_id; }

void sub_35354(uint8_t *out_buf, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4)
{
    g_host_spy.crash_dump_words[0] = w0;
    g_host_spy.crash_dump_words[1] = w1;
    g_host_spy.crash_dump_words[2] = w2;
    g_host_spy.crash_dump_words[3] = w3;
    g_host_spy.crash_dump_words[4] = w4;
    g_host_spy.crash_dump_calls++;
    acib_write_unaligned_le32(out_buf + 0,  w0);
    acib_write_unaligned_le32(out_buf + 4,  w1);
    acib_write_unaligned_le32(out_buf + 8,  w2);
    acib_write_unaligned_le32(out_buf + 12, w3);
    acib_write_unaligned_le32(out_buf + 16, w4);
}

void sub_EBA60(uint32_t ctx_addr, uint32_t tag_addr, const uint8_t *buf)
{
    (void)ctx_addr;
    (void)tag_addr;
    (void)buf;
}

uint32_t sub_7513C(const char *msg, uint32_t code)
{
    (void)msg;
    (void)code;
    return 1u;
}

void sub_7835C(uint32_t delay_us) { (void)delay_us; }
void sub_7D820(uint32_t arg) { (void)arg; }

int sub_CF6B4(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3,
              uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    (void)p0; (void)p1; (void)p2; (void)p3;
    (void)s0; (void)s1; (void)s2; (void)s3; (void)s4;
    return 0;
}

void sub_96458(uint32_t reg_id, uint32_t val)
{
    g_host_spy.rf_reg_writes++;
    g_host_spy.last_rf_reg_id  = reg_id;
    g_host_spy.last_rf_reg_val = val;
}

void sub_964AC(uint32_t val)
{
    g_host_spy.rf_reg_writes++;
    g_host_spy.last_rf_reg_val = val;
}

void sub_96434(uint32_t reg_id, uint32_t val)
{
    g_host_spy.rf_reg_writes++;
    g_host_spy.last_rf_reg_id  = reg_id;
    g_host_spy.last_rf_reg_val = val;
}

void sub_707F0(void) {}
void sub_DECFC(void) {}
void sub_2F78(void) {}
int  sub_71540(uint32_t arg) { (void)arg; return 0; }
void sub_E696A(const char *fmt, const char *arg) { (void)fmt; (void)arg; }
uint32_t sub_71240(void) { return 1u; }
const char *sub_EBA64(uint32_t id) { (void)id; return "AirPods3,6-b515d"; }
void sub_7076C(void) {}
uint32_t sub_77A44(void) { return 0x10u; }
uint32_t sub_6C850(uint32_t arg) { return arg + 1u; }
void sub_73F20(uint32_t arg) { (void)arg; }
void sub_7B1C4(uint32_t arg) { (void)arg; }
void sub_74F10(uint32_t arg) { (void)arg; }
void sub_6AFB0(void) {}
void sub_776E0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{ (void)p0; (void)p1; (void)p2; (void)p3; }
void sub_69978(uint32_t arg) { (void)arg; }
uint32_t sub_598E8(uint32_t arg) { return arg; }
uint32_t sub_133BC(uint32_t arg) { return arg; }
uint32_t sub_188B4(uint32_t arg) { return arg; }
void sub_4B5C2(uint32_t arg) { (void)arg; }
void sub_2774(void) {}
uint32_t sub_2584(void) { return 0u; }
void sub_4B2B0(uint32_t arg) { (void)arg; }

uintptr_t sub_E713C(const char *name, uintptr_t entry, uint32_t id,
                    uint32_t flags, uint32_t stack_sz, uint32_t prio)
{
    (void)entry;
    (void)flags;
    (void)stack_sz;
    (void)prio;
    g_host_spy.spawned_threads++;
    g_host_spy.last_thread_name = name;
    return (uintptr_t)(0x1000u + id);
}

void *sub_E7C20(uintptr_t handle) { return (void *)handle; }
void sub_2F8C(void) {}
void sub_59884(void) {}
void sub_5688(void) {}
