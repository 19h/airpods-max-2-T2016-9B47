/*
 * src/phy/phy_rf_seq.c — W3 MAC<->PHY SEQ_RF_CTRL (0x4d154038) sequencer setup, IRQ completion, and BT clock math.
 *
 * Clean-room C11 reconstruction for Apple b515d (AirPods3,6) Bluetooth / ACI
 * firmware (`acib`), verified by Unicorn Cortex-M4 differential execution
 * and 3-stub mutation testing.
 */
#include "acib/acib.h"
#include "acib/bus.h"
#include "acib/cpu.h"
#include "acib/libc.h"
#include "acib/rtkit.h"
#include "acib/phy_ipc.h"
#include "acib/aci.h"
#include "acib/reconstructed_p2.h"

/* External pending_c link-boundary prototypes */
void sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg);
void sub_6CFF4(uint32_t halt_reason);
uint32_t sub_EA86E(uint32_t addr);
uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count);
uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val);
uint32_t sub_7E9F8(uint32_t end_ptr);
void sub_DF764(void);
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
void sub_740A8(uint32_t log_id, uint32_t line);
void sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0);
uint32_t sub_7513C(const char *msg, uint32_t code);
void sub_7835C(uint32_t delay_us);
void sub_7D820(uint32_t arg);
void sub_96458(uint32_t reg_id, uint32_t val);
void sub_964AC(uint32_t val);
void sub_96434(uint32_t reg_id, uint32_t val);
void sub_707F0(void);
void sub_DECFC(void);
void sub_2F78(void);
int  sub_71540(uint32_t arg);
uint32_t sub_71240(void);
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
void sub_2F8C(void);
void sub_59884(void);
void sub_5688(void);

/* @provenance RVA 0x07a918 sub_7A918 */
int32_t phy_aci_if_poll_and_ack(void)
{
    uint32_t if_base = acib_mem_read32(0x000F2A44u);
    acib_mem_write32(0x10020324u, 1u);
    if (acib_mem_read8(0x10021DC4u) != 0u) {
        if ((acib_mmio_read32(if_base + 0x420u) & 0x100u) != 0u) {
            acib_mem_write8(0x10021E33u, 1u);
            acib_mem_write32(0x10021DDCu, acib_mem_read32(0x10021DDCu) + 1u);
        }
        if ((acib_mmio_read32(if_base + 0x408u) & 0x100u) != 0u) {
            acib_mem_write32(0x10021DD4u, acib_mem_read32(0x10021DD4u) + 1u);
        }
        if ((acib_mmio_read32(if_base + 0x400u) & 0x100u) != 0u) {
            acib_mem_write32(0x10021DE0u, acib_mem_read32(0x10021DE0u) + 1u);
        }
        if ((acib_mmio_read32(if_base + 0x410u) & 0x100u) != 0u) {
            acib_mem_write32(0x10021DD8u, acib_mem_read32(0x10021DD8u) + 1u);
        }
    }
    return phy_mdm_fn_07a978(0x10020310, if_base);
}

/* @provenance RVA 0x084fd0 sub_84FD0 */
uint32_t phy_bt_clk_add_slots(uint32_t base_clk, int32_t delta_slots)
{
    if (delta_slots < 1) {
        uint32_t neg_slots = 0u - (uint32_t)delta_slots;
        if (neg_slots >= 0x10000000u) {
            (void)rtk_ep_fn_0efa60((int32_t)neg_slots);
            return base_clk & 0x7FFFFFFFu;
        }
    } else if ((uint32_t)delta_slots >= 0x10000000u) {
        (void)rtk_ep_fn_0efa54(delta_slots);
        return base_clk & 0x7FFFFFFFu;
    }
    return (base_clk + ((uint32_t)delta_slots << 3)) & 0x7FFFFFFFu;
}

/* @provenance RVA 0x096470 sub_96470 */
uint32_t phy_rf_read_status_158(void)
{
    uint32_t rf_base = acib_mem_read32(0x000F2A6Cu);
    return (uint16_t)acib_mmio_read32(rf_base + 0x118u);
}

/* @provenance RVA 0x096498 sub_96498 */
uint32_t phy_rf_read_seq_state(void)
{
    uint32_t rf_base = acib_mem_read32(0x000F2A6Cu);
    return (uint16_t)acib_mmio_read32(rf_base + 0x11Cu);
}

/* @provenance RVA 0x0976bc sub_976BC */
uint32_t phy_rf_seq_setup_op(uint32_t slot_idx, uint32_t desc_vma)
{
    uint32_t ctx_base = 0x10027958u;
    uint32_t slot_cfg = acib_mem_read32(ctx_base + (slot_idx << 4) + 8u);
    sub_96458(0x1Cu, ((slot_cfg & 0x3Fu) << 4) | 2u);

    for (uint32_t i = 0u; i < 8u; i++) {
        acib_mem_write32(ctx_base + 0x90u + (i << 2), acib_mem_read32(desc_vma + (i << 2)));
    }
    if (acib_mem_read8(ctx_base + 0xB6u) == 1u) {
        uint16_t ovr_ch = (uint16_t)acib_mem_read8(ctx_base + 0xB7u)
                        | (uint16_t)((uint16_t)acib_mem_read8(ctx_base + 0xB8u) << 8);
        acib_mem_write16(ctx_base + 0xA8u, ovr_ch);
    }

    uint32_t phy_ctl = acib_mmio_read32(0x4D154038u);
    uint32_t mode = acib_mem_read32(ctx_base + 0xA4u);
    uint32_t reg4_val = mode;
    uint32_t mode_flags = mode;
    uint32_t reg7_val = mode;

    switch (mode) {
    case 0u:
        break;
    case 1u:
    case 2u:
        mode_flags = 0u;
        reg7_val = 0u;
        break;
    case 3u:
        reg7_val = 0u;
        mode_flags = 0x20000000u;
        reg4_val = 0u;
        break;
    case 4u:
        reg4_val = 1u;
        reg7_val = 0u;
        mode_flags = 0x20000000u;
        break;
    case 5u:
        acib_mmio_write32(0x520E4A48u, (acib_mmio_read32(0x520E4A48u) & 0xFFFFFFFCu) | 1u);
        reg7_val = 0u;
        mode_flags = 0x04000000u;
        reg4_val = 0u;
        break;
    case 6u:
        acib_mmio_write32(0x520E4A48u, (acib_mmio_read32(0x520E4A48u) & 0xFFFFFFFCu) | 1u);
        reg4_val = 0u;
        reg7_val = 2u;
        mode_flags = 0x04000000u;
        break;
    case 7u:
        acib_mmio_write32(0x520E4A48u, (acib_mmio_read32(0x520E4A48u) & 0xFFFFFFFCu) | 2u);
        reg4_val = 0u;
        reg7_val = 6u;
        mode_flags = 0x04000000u;
        break;
    case 8u:
        acib_mmio_write32(0x520E4A48u, (acib_mmio_read32(0x520E4A48u) & 0xFFFFFFFCu) | 2u);
        reg4_val = 0u;
        reg7_val = 4u;
        mode_flags = 0x04000000u;
        break;
    case 9u:
        phy_ctl = (phy_ctl & 0x1FFFFFFFu) | 0xA0000000u;
        reg7_val = 0u;
        mode_flags = 0x30000000u;
        reg4_val = 0u;
        break;
    case 10u:
        reg4_val = 1u;
        reg7_val = 0u;
        mode_flags = 0x30000000u;
        break;
    case 13u:
        acib_mmio_write32(0x520E4A48u, acib_mmio_read32(0x520E4A48u) | 3u);
        reg7_val = 0u;
        mode_flags = 0x04000000u;
        reg4_val = 0u;
        break;
    default:
        sub_740A8(0x00096006u, 0x8375u);
        mode_flags = 0u;
        reg7_val = 0u;
        reg4_val = 0u;
        break;
    }

    uint32_t band = acib_mem_read8(ctx_base + 0xA8u);
    uint32_t chan = acib_mem_read8(ctx_base + 0xA9u);
    uint32_t chan_limit = acib_mem_read8(0x10000640u + 6u * band + 2u);
    if (chan >= chan_limit) {
        phy_rf_log_chan_oob();
    }

    uint32_t mode_reloaded = acib_mem_read32(ctx_base + 0xA4u);
    uint32_t reg3_val = mode_flags | ((chan & 0x7Fu) << 8);
    uint32_t reg8_val = 0u;
    if (mode_reloaded != 11u) {
        uint32_t band_now = acib_mem_read8(ctx_base + 0xA8u);
        uint32_t ch_term = (chan < 0x28u) ? (chan << 1) : ((chan << 1) + 0xFFFFFFB1u);
        if (band_now != 0u) {
            ch_term = chan;
        }
        reg8_val = (((ch_term << 16) | (band_now << 23)) & 0x03FF0000u) + 1u;
    }

    uint32_t reg11_val = ((mode_flags & 0x04000000u) != 0u) ? 0xFFFu : 0x800u;
    sub_96434(3u, reg3_val);
    sub_96434(4u, reg4_val);
    sub_96434(11u, reg11_val);
    sub_96434(5u, acib_mem_read32(ctx_base + 0x98u));
    sub_96434(6u, acib_mem_read32(ctx_base + 0x9Cu));
    sub_96434(7u, reg7_val);
    sub_96434(8u, reg8_val);
    sub_96434(35u, 0u);
    sub_96434(9u, phy_ctl);
    return 0u;
}

/* @provenance RVA 0x097d94 sub_97D94 */
uint32_t phy_rf_seq_complete_irq(uint32_t slot_idx, uint32_t out_clk_vma)
{
    uint32_t ctx_base = 0x10027958u;
    uint32_t seq_state = phy_rf_read_seq_state();

    switch (seq_state) {
    case 1u: {
        uint32_t slot_cfg = acib_mem_read32(ctx_base + (slot_idx << 4) + 8u);
        uint32_t cmd = ((slot_cfg & 0x3Fu) << 4) | 1u;
        sub_96458(0x1Bu, cmd);
        (void)phy_rf_fn_096444(0x1B, 0);
        sub_964AC(cmd);
        uint32_t tbl_idx = acib_mem_read32(ctx_base + 0x88u);
        (void)phy_rf_fn_0964c0(0x19u);
        if (out_clk_vma != 0u) {
            uint32_t raw_clk = acib_mmio_read32(0x520D51F8u) & 0x7FFFFFFFu;
            int32_t slots = (int32_t)acib_mem_read8(0x10001D5Cu + tbl_idx);
            acib_mem_write32(out_clk_vma, phy_bt_clk_add_slots(raw_clk, slots));
        }
        break;
    }
    case 2u:
        if (out_clk_vma != 0u) {
            uint32_t tbl_idx = acib_mem_read32(ctx_base + 0x88u);
            int32_t slots = (int32_t)acib_mem_read8(0x10001D5Cu + tbl_idx);
            uint32_t clk_lo = (uint32_t)phy_rf_fn_096484(0x3C);
            uint32_t clk_hi = (uint32_t)phy_rf_fn_096484(0x3D);
            acib_mem_write32(out_clk_vma, phy_bt_clk_add_slots(clk_lo | (clk_hi << 16), slots));
        }
        break;
    case 3u:
    case 5u:
    case 15u: {
        acib_mem_write32(ctx_base + 0xB0u, 0xCu);
        uint32_t slot_cfg = acib_mem_read32(ctx_base + (slot_idx << 4) + 8u);
        uint32_t cmd = ((slot_cfg & 0x3Fu) << 4) | 3u;
        sub_96458(0x1Du, cmd);
        sub_964AC(cmd);
        sub_96434(0x24u, 0u);
        uint32_t phy_ctl = acib_mmio_read32(0x4D154038u) & 0xDFFFFFFFu;
        sub_96434(0x0Au, phy_ctl);
        uint32_t mode_idx = acib_mem_read32(ctx_base + 0xA4u);
        (void)phy_rf_fn_0964c0(0x1Bu);
        if (out_clk_vma != 0u) {
            if (mode_idx == 0xDu) {
                mode_idx = 8u;
            }
            uint32_t raw_clk = acib_mmio_read32(0x520D51F8u) & 0x7FFFFFFFu;
            int32_t slots = (int32_t)acib_mem_read8(0x10027948u + mode_idx);
            acib_mem_write32(out_clk_vma, phy_bt_clk_add_slots(raw_clk, slots));
        }
        break;
    }
    case 4u:
        if (out_clk_vma != 0u) {
            uint32_t mode_idx = acib_mem_read32(ctx_base + 0xA4u);
            int32_t slots = (int32_t)acib_mem_read8(0x10027948u + mode_idx);
            uint32_t clk_lo = (uint32_t)phy_rf_fn_096484(0x3C);
            uint32_t clk_hi = (uint32_t)phy_rf_fn_096484(0x3D);
            acib_mem_write32(out_clk_vma, phy_bt_clk_add_slots(clk_lo | (clk_hi << 16), slots));
        }
        break;
    default:
        if (out_clk_vma != 0u) {
            uint32_t raw_clk = acib_mmio_read32(0x520D51F8u) & 0x7FFFFFFFu;
            acib_mem_write32(out_clk_vma, raw_clk);
        }
        break;
    }

    return (uint32_t)phy_rf_fn_095fbc(1);
}

/* @provenance RVA 0x0efb78 sub_EFB78 */
void phy_rf_log_chan_oob(void)
{
    sub_740A8(0x00096007u, 0x838Eu);
}
