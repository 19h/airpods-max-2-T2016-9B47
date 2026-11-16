#include "rkof/cpu.h"
#include "rkof/memmap.h"
#include "rkof/phy.h"
#include "rkof/regs.h"
#include "rkof/rtkit.h"
#include "rkof/shared.h"

/* @provenance RVA 0xfdd74 sub_FDD74 */
uint16_t sub_FDD74(uint16_t seed, const uint8_t *data, uint32_t len)
{
    uint32_t acc = (uint32_t)seed;
    for (uint32_t idx = 0u; idx < len; idx++) {
        uint32_t rotated = cpu_ror16_1((uint16_t)acc);
        acc = (rotated + (uint32_t)data[idx]) & 0xFFFFu;
    }
    return (uint16_t)acc;
}

/* @provenance RVA 0xfdd90 sub_FDD90 */
void sub_FDD90(int assert_reset)
{
    uint32_t bit = (uint32_t)assert_reset & 1u;

    uint32_t modem_rst = MMIO32_READ(RKOF_W3_MODEM_RESET_ADDR);
    modem_rst = (modem_rst & ~RKOF_W3_MODEM_RESET_MASK45) | (bit << 4u);
    MMIO32_WRITE(RKOF_W3_MODEM_RESET_ADDR, modem_rst);

    modem_rst = MMIO32_READ(RKOF_W3_MODEM_RESET_ADDR);
    modem_rst = (modem_rst & ~RKOF_W3_MODEM_RESET_BIT6) | (bit << 6u);
    MMIO32_WRITE(RKOF_W3_MODEM_RESET_ADDR, modem_rst);

    uint32_t top_ctrl = MMIO32_READ(RKOF_W3_PHY_TOP_CTRL_ADDR);
    top_ctrl = (top_ctrl & ~RKOF_W3_PHY_TOP_RST_BIT5) | (bit << 5u);
    MMIO32_WRITE(RKOF_W3_PHY_TOP_CTRL_ADDR, top_ctrl);

    top_ctrl = MMIO32_READ(RKOF_W3_PHY_TOP_CTRL_ADDR);
    top_ctrl = (top_ctrl & ~RKOF_W3_PHY_TOP_RST_BIT16) | (bit << 16u);
    MMIO32_WRITE(RKOF_W3_PHY_TOP_CTRL_ADDR, top_ctrl);

    top_ctrl = MMIO32_READ(RKOF_W3_PHY_TOP_CTRL_ADDR);
    top_ctrl = (top_ctrl & ~RKOF_W3_PHY_TOP_RST_BIT19) | (bit << 19u);
    MMIO32_WRITE(RKOF_W3_PHY_TOP_CTRL_ADDR, top_ctrl);
}

/* @provenance RVA 0xfde74 sub_FDE74 */
void sub_FDE74(void)
{
    rtkit_assert_fail("modem_inits.c", 157u, "write_warm_restore");
}

/* @provenance RVA 0xfde48 sub_FDE48 */
void sub_FDE48(uint32_t reg_addr, uint32_t reg_val)
{
    if ((reg_addr - RKOF_W3_MODEM_WARM_MIN_ADDR) > 0x1000u) {
        return;
    }

    struct phy_status_region *smem = RKOF_PTR(
        struct phy_status_region *,
        RKOF_PHY_STATUS_SRAM_BASE
    );
    uint32_t slot = smem->slpr_reg_num;
    if (slot >= RKOF_PHY_WARM_RESTORE_MAX_REGS) {
        sub_FDE74();
    }

    smem->warm_regs[slot].reg_addr = reg_addr;
    smem->warm_regs[slot].reg_val = reg_val;
    smem->slpr_reg_num = slot + 1u;
}

/* @provenance RVA 0xfdddc sub_FDDDC */
void sub_FDDDC(const uint32_t *pairs, uint32_t byte_len)
{
    while (byte_len != 0u) {
        uint32_t reg_addr = pairs[0];
        uint32_t reg_val = pairs[1];
        MMIO32_WRITE(reg_addr, reg_val);
        sub_FDE48(reg_addr, reg_val);
        pairs += 2;
        byte_len -= 8u;
    }
}

/* @provenance RVA 0xfddf8 sub_FDDF8 */
void sub_FDDF8(const uint32_t *triples, uint32_t byte_len)
{
    if (byte_len == 0u) {
        return;
    }

    uint32_t pending_addr = 0u;
    uint32_t pending_val = 0u;
    uint32_t have_pending = 0u;

    while (byte_len != 0u) {
        uint32_t reg_addr = triples[0];
        uint32_t mask = triples[1];
        uint32_t set_bits = triples[2];

        if (have_pending != 0u && reg_addr != pending_addr) {
            sub_FDE48(pending_addr, pending_val);
        }

        uint32_t cur = MMIO32_READ(reg_addr);
        uint32_t updated = (cur & ~mask) | set_bits;
        MMIO32_WRITE(reg_addr, updated);

        pending_addr = reg_addr;
        pending_val = updated;
        have_pending = 1u;

        triples += 3;
        byte_len -= 12u;
    }

    if (have_pending != 0u) {
        sub_FDE48(pending_addr, pending_val);
    }
}

/* @provenance RVA 0x1068a0 bt_phy_mmio_init_seq */
void bt_phy_mmio_init_seq(void)
{
    mmio32_set_bits(RKOF_RF_PMGR_BIAS_ADDR, 0x00000008u);
    mmio32_set_bits(RKOF_RF_PMGR_LDO_ADDR, 0x80000000u);
    mmio32_set_bits(RKOF_RF_PMGR_BASE, 0x00000100u);
    mmio32_set_bits(RKOF_RF_PMGR_BASE, 0x00008000u);

    sub_18A134(5ULL);

    mmio32_clear_bits(RKOF_RF_PMGR_BASE, 0x00008000u);
    mmio32_clear_bits(RKOF_RF_PMGR_ANA_EN_ADDR, 0x00000200u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00020000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00040000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00002000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00004000u);

    sub_18A134(5ULL);

    mmio32_clear_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00020000u);
    mmio32_clear_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00002000u);

    sub_18A134(50ULL);

    mmio32_clear_bits(RKOF_RF_PMGR_ANA_EN_ADDR, 0x00000100u);
    mmio32_set_bits(RKOF_RF_PMGR_DIG_EN_ADDR, 0x00800000u);
    mmio32_set_bits(RKOF_RF_PMGR_DIG_EN_ADDR, 0x01000000u);
    mmio32_clear_bits(RKOF_RF_ISOLATION_ADDR, 0x00000020u);
}

/* @provenance RVA 0x10694c bt_phy_mmio_pre_otp */
void bt_phy_mmio_pre_otp(void)
{
    mmio32_set_bits(RKOF_RF_PMGR_ANA_EN_ADDR, 0x00000100u);
    sub_18A134(2ULL);

    mmio32_clear_bits(RKOF_RF_PMGR_DIG_EN_ADDR, 0x00800000u);
    mmio32_set_bits(RKOF_RF_PMGR_DIG_EN_ADDR, 0x01000000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00020000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00040000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00002000u);
    mmio32_set_bits(RKOF_RF_PMGR_CLK_GATE_ADDR, 0x00004000u);
    mmio32_clear_bits(RKOF_RF_PMGR_BASE, 0x00000100u);
    mmio32_set_bits(RKOF_RF_PMGR_ANA_EN_ADDR, 0x00000200u);
    mmio32_clear_bits(RKOF_RF_ISOLATION_ADDR, 0x00000020u);
}
