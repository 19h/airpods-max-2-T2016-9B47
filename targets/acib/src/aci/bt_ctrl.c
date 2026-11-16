#include "acib/aci.h"
#include "acib/bus.h"
#include "acib/regs.h"
#include "acib/rtkit.h"

/*
 * Core ACI / Bluetooth Controller entry points, HCI BER test-mode lookup,
 * BD_ADDR byte-pair swapper, hardware timestamp ring recorder, and RTKit
 * thread bootstrap sequence reconstructed from acib ROM.
 */

#define ACIB_ROM_HCI_BER_OPCODES_ADDR  0x000F1D98u
#define ACIB_ROM_BUILD_DATE_STR_ADDR   0x000F5360u
#define ACIB_ROM_SDK_VER_STR_ADDR      0x000F5380u
#define ACIB_ROM_EXTRA_BANNER_STR_ADDR 0x000F53A0u

#define ACIB_RAM_TS_RING_CAP_ADDR      0x10001644u
#define ACIB_RAM_TS_RING_BUF_ADDR      0x10001648u
#define ACIB_RAM_TS_RING_IDX_ADDR      0x1000164Cu
#define ACIB_RAM_TS_RING_LAST_ADDR     0x10001650u

/* External pending_c subsystem initializers called by main_entry / thread init */
extern void     sub_707F0(void);
extern void     sub_DECFC(void);
extern void     sub_2F78(void);
extern int      sub_71540(uint32_t arg);
extern void     sub_E696A(const char *fmt, const char *arg);
extern uint32_t sub_71240(void);
extern const char *sub_EBA64(uint32_t id);
extern void     sub_7076C(void);
extern uint32_t sub_77A44(void);
extern uint32_t sub_6C850(uint32_t arg);
extern void     sub_73F20(uint32_t arg);
extern void     sub_7B1C4(uint32_t arg);
extern void     sub_74F10(uint32_t arg);
extern void     sub_6AFB0(void);
extern void     sub_776E0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);
extern int      sub_74240(uint32_t log_id);
extern void     sub_69978(uint32_t arg);
extern uint32_t sub_598E8(uint32_t arg);
extern uint32_t sub_133BC(uint32_t arg);
extern uint32_t sub_188B4(uint32_t arg);
extern void     sub_4B5C2(uint32_t arg);
extern void     sub_2774(void);
extern uint32_t sub_2584(void);
extern void     sub_4B2B0(uint32_t arg);
extern uintptr_t sub_E713C(const char *name, uintptr_t entry, uint32_t id,
                           uint32_t flags, uint32_t stack_sz, uint32_t prio);
extern void    *sub_E7C20(uintptr_t handle);
extern void     sub_2F8C(void);
extern void     sub_59884(void);
extern void     sub_5688(void);

/* @provenance RVA 0x020a58 hci_test_mode_get_opcode_from_type */
uint32_t hci_test_mode_get_opcode_from_type(uint32_t ber_type)
{
    if (ber_type < 5u) {
        const uint32_t *opcodes = (const uint32_t *)acib_addr_ptr(ACIB_ROM_HCI_BER_OPCODES_ADDR);
        return opcodes[ber_type];
    }
    rtk_abort_panic("hci_test_mode_get_opcode_from_type: invalid ber meter type",
                    0u, 0u, 0u);
    return 0u;
}

/* @provenance RVA 0x0aef9e bt_bdaddr_swap */
void bt_bdaddr_swap(const uint8_t src[6], uint8_t dst[6])
{
    uint8_t b0 = src[0];
    uint8_t b1 = src[1];
    uint8_t b2 = src[2];
    uint8_t b3 = src[3];
    uint8_t b4 = src[4];
    uint8_t b5 = src[5];

    dst[1] = b0;
    dst[0] = b1;
    dst[3] = b2;
    dst[2] = b3;
    dst[5] = b4;
    dst[4] = b5;
}

static inline void bt_ts_ring_write_common(uint32_t tag10)
{
    uint32_t now_us = acib_mmio_read32(ACIB_BT_TIMER_US_ADDR);
    uint32_t cap = *(uint32_t *)acib_addr_ptr(ACIB_RAM_TS_RING_CAP_ADDR);
    uint32_t buf_addr = *(uint32_t *)acib_addr_ptr(ACIB_RAM_TS_RING_BUF_ADDR);
    uint32_t *idx_ptr = (uint32_t *)acib_addr_ptr(ACIB_RAM_TS_RING_IDX_ADDR);
    uint32_t *last_ptr = (uint32_t *)acib_addr_ptr(ACIB_RAM_TS_RING_LAST_ADDR);

    uint32_t idx = *idx_ptr;
    uint32_t next = idx + 1u;
    if (next >= cap) {
        next = 0u;
    }
    *idx_ptr = next;

    uint32_t ts22 = (now_us >> 2) & 0x003FFFFFu;
    uint32_t entry = ts22 | (tag10 << 22);
    uint32_t *ring = (uint32_t *)acib_addr_ptr(buf_addr);
    ring[idx] = entry;
    *last_ptr = now_us;
}

/* @provenance RVA 0x078d44 bt_ts_ring_record */
void bt_ts_ring_record(uint32_t tag10)
{
    bt_ts_ring_write_common(tag10);
}

/* @provenance RVA 0x078d84 sub_78D84 */
void bt_ts_ring_record_alt(uint32_t tag10)
{
    bt_ts_ring_write_common(tag10);
}

/* @provenance RVA 0x071a74 sub_71A74 */
void acib_set_bt_ctrl_mode(uint8_t mode)
{
    g_bt_ctrl_ctx[0x6ACu] = mode;
}

/* @provenance RVA 0x071a84 sub_71A84 */
uint8_t acib_get_bt_ctrl_mode(void)
{
    return g_bt_ctrl_ctx[0x6ACu];
}

/* @provenance RVA 0x071a94 sub_71A94 */
uint8_t acib_get_conn_slot_state(uint32_t slot_idx)
{
    if (slot_idx > 0x3Cu) {
        return 0u;
    }
    return g_bt_ctrl_ctx[(28u * slot_idx) + 0x18u];
}

/* @provenance RVA 0x070bf0 sub_70BF0 */
int acib_log_build_banner_and_start(void)
{
    const char *build_date = (const char *)acib_addr_ptr(ACIB_ROM_BUILD_DATE_STR_ADDR);
    const char *sdk_ver = (const char *)acib_addr_ptr(ACIB_ROM_SDK_VER_STR_ADDR);
    const char *extra_banner = (const char *)acib_addr_ptr(ACIB_ROM_EXTRA_BANNER_STR_ADDR);

    if (build_date[0] != '\0') {
        sub_E696A("%s\n", build_date);
        if (sdk_ver[0] != '\0') {
            sub_E696A("%s\n", sdk_ver);
        }
        if (extra_banner[0] != '\0') {
            sub_E696A("%s\n", extra_banner);
        }
    } else {
        sub_74240(0x02014005u);
    }

    uint32_t tag_id = sub_71240();
    const char *tag_str = sub_EBA64(tag_id);
    sub_E696A("%s\n", tag_str);
    sub_7076C();

    if (g_bt_sys_threads_state[3] >= 0x25u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x25u;

    uint32_t r0 = sub_77A44();
    uint32_t r1 = sub_6C850(r0);
    sub_73F20(r1);
    sub_7B1C4(1u);
    sub_74F10(1u);
    if (acib_soc_is_fpga_emulation() == 0u) {
        sub_6AFB0();
    }

    if (g_bt_sys_threads_state[3] >= 0x27u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x27u;
    sub_776E0(7u, 0u, 0u, 0u);
    return sub_74240(0x01014006u);
}

/* @provenance RVA 0x002ea8 bt_system_thread_init */
void bt_system_thread_init(void)
{
    uint32_t saved_pm = rtk_primask_disable_save();
    sub_69978(saved_pm);
    uint32_t s1 = sub_598E8(1u);
    uint32_t s2 = sub_133BC(s1);
    uint32_t s3 = sub_188B4(s2);
    sub_4B5C2(s3);
    sub_2774();
    uint32_t s4 = sub_2584();
    sub_4B2B0(s4);

    if (g_bt_sys_threads_state[3] >= 0x21u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x21u;
    rtk_irq_enable_all();

    uintptr_t hci_h = sub_E713C("hci_thread", 0x0006A515u, 3u, 0u, 0x400u, 0x15u);
    g_rtk_thread_ptr_table[3] = sub_E7C20(hci_h);
    sub_2F8C();

    uintptr_t lm_h = sub_E713C("link_manager_thread", 0x00059CADu, 1u, 0u, 0xA00u, 0x11u);
    g_rtk_thread_ptr_table[1] = sub_E7C20(lm_h);
    sub_2F8C();

    if (g_bt_sys_threads_state[3] >= 0x22u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x22u;
    sub_59884();

    if (g_bt_sys_threads_state[3] >= 0x23u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x23u;
    sub_5688();

    if (g_bt_sys_threads_state[3] >= 0x24u) {
        rtk_abort_panic("tcm_debug_information: init_state must be incremented consecutively!",
                        0u, 0u, 0u);
    }
    g_bt_sys_threads_state[3] = 0x24u;
    (void)sub_74240(0x0213A000u);
}

/* @provenance RVA 0x071a50 main_entry */
int main_entry(void)
{
    sub_707F0();
    sub_DECFC();
    bt_system_thread_init();
    (void)acib_log_build_banner_and_start();
    sub_2F78();
    return sub_71540(0u);
}
