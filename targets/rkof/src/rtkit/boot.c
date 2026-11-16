#include "rkof/cpu.h"
#include "rkof/libc.h"
#include "rkof/memmap.h"
#include "rkof/regs.h"
#include "rkof/rtkit.h"
#include "rkof/shared.h"

extern void rtkit_app_init(void);

static inline uint32_t read_tlv_u32_payload(uintptr_t tlv_vma)
{
    const uint32_t *words = RKOF_PTR(const uint32_t *, tlv_vma);
    return words[2];
}

static inline void wait_remap_ack(void)
{
    while (MMIO32_READ(RKOF_REMAP_ACK_ADDR) != 1u) {
    }
}

/* @provenance RVA 0x108 rtkit_entry */
#if !defined(RKOF_HOST_BUILD)
__attribute__((section(".text.rtkit_entry")))
#endif
void rtkit_entry(void)
{
    if ((MMIO32_READ(RKOF_REMAP_STATUS_ADDR) & RKOF_REMAP_STATUS_DONE_BIT) == 0u) {
        mmio32_clear_bits(RKOF_REMAP_CTRL_ADDR, RKOF_REMAP_CTRL_ENABLE_BIT);

        MMIO32_WRITE(RKOF_REMAP_DATA_RMD1_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMD1_ADDR));
        wait_remap_ack();
        MMIO32_WRITE(RKOF_REMAP_DATA_RMD2_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMD2_ADDR));
        wait_remap_ack();
        MMIO32_WRITE(RKOF_REMAP_DATA_RMD3_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMD3_ADDR));
        wait_remap_ack();

        MMIO32_WRITE(RKOF_REMAP_CODE_RMC1_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMC1_ADDR));
        wait_remap_ack();
        MMIO32_WRITE(RKOF_REMAP_CODE_RMC2_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMC2_ADDR));
        wait_remap_ack();
        MMIO32_WRITE(RKOF_REMAP_CODE_RMC3_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMC3_ADDR));
        wait_remap_ack();

        MMIO32_WRITE(RKOF_REMAP_RMFI_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RMFI_ADDR));
        mmio32_set_bits(RKOF_REMAP_CTRL_ADDR, RKOF_REMAP_CTRL_ENABLE_BIT);
        MMIO32_WRITE(RKOF_SCRATCH_RTSI_ADDR, read_tlv_u32_payload(RKOF_RODATA_TLV_RTSI_ADDR));
    }

    Reset_Handler();
}

/* @provenance RVA 0xca40 __stack_chk_init */
void __stack_chk_init(void)
{
    const uint8_t *tlv_stkg = RKOF_PTR(const uint8_t *, RKOF_RODATA_TLV_STKG_ADDR);
    uint32_t guard_len = *(const uint32_t *)(const void *)(tlv_stkg + 4u);
    void *guard_dst = RKOF_PTR(void *, RKOF_DATA_STACK_CHK_GUARD_ADDR);
    memcpy_thunk(guard_dst, tlv_stkg + 8u, guard_len);
}

/* @provenance RVA 0xca58 __stack_chk_fail */
RKOF_NORETURN void __stack_chk_fail(void)
{
    rtkit_panic("stk_fail");
}

/* @provenance RVA 0x16d60 rtkit_start */
void rtkit_start(void)
{
    uintptr_t bss_start = RKOF_RAM_BSS_BASE;
    uintptr_t bss_end = RKOF_RAM_BSS_END;

    if (bss_end > bss_start) {
        uintptr_t psp_lo = STATE_PSP_STACK_BASE;
        uintptr_t psp_hi = STATE_PSP_STACK_BASE + 0x800u;
        uintptr_t msp_lo = STATE_MSP_STACK_BASE;
        uintptr_t msp_hi = STATE_MSP_STACK_BASE + 0x1000u;

        uintptr_t first_lo = (psp_lo < msp_lo) ? psp_lo : msp_lo;
        uintptr_t first_hi = (psp_lo < msp_lo) ? psp_hi : msp_hi;
        uintptr_t second_lo = (psp_lo < msp_lo) ? msp_lo : psp_lo;
        uintptr_t second_hi = (psp_lo < msp_lo) ? msp_hi : psp_hi;

        rkof_memset(RKOF_PTR(void *, bss_start), 0, first_lo - bss_start);
        rkof_memset(RKOF_PTR(void *, first_hi), 0, second_lo - first_hi);
        rkof_memset(RKOF_PTR(void *, second_hi), 0, bss_end - second_hi);
    }

    cpu_dsb();

    uint8_t dsrl_flag = 0u;
    const uint8_t *dsrl_tlv = RKOF_PTR(const uint8_t *, RKOF_RODATA_TLV_DSRL_ADDR);
    uint32_t dsrl_len = *(const uint32_t *)(const void *)(dsrl_tlv + 4u);
    memcpy_thunk(&dsrl_flag, dsrl_tlv + 8u, dsrl_len);

    if (RKOF_RAM_DATA_END > RKOF_RAM_DATA_BASE && dsrl_flag != 0u) {
        size_t reload_len = (size_t)(RKOF_RAM_DATA_END - RKOF_RAM_DATA_BASE);
        rkof_memcpy(
            RKOF_PTR(void *, RKOF_RAM_DATA_BASE),
            RKOF_PTR(const void *, RKOF_ROM_RODATA_END),
            reload_len
        );
    }

    __stack_chk_init();

    uint32_t ccr = MMIO32_READ(SCB_CCR_ADDR);
    ccr = (ccr | 0x210u) & ~0x8u;
    MMIO32_WRITE(SCB_CCR_ADDR, ccr);

    MMIO32_WRITE(SCB_CPACR_ADDR, 0u);
    MMIO32_WRITE(FPU_FPCCR_ADDR, 0xC0000000u);
    MMIO32_WRITE(FPU_FPDSCR_ADDR, 0u);
    MMIO32_WRITE(SCB_CPACR_ADDR, 0x00F00000u);
    cpu_dsb();
    cpu_isb();

    uint32_t mpu_type = MMIO32_READ(MPU_TYPE_ADDR);
    uint32_t dregion = (mpu_type >> 8u) & 0xFFu;
    uint32_t *mpu_state = RKOF_PTR(uint32_t *, STATE_MPU_BASE);
    mpu_state[0] = dregion;

    if ((mpu_type & 1u) == 0u && dregion != 0u) {
        MMIO32_WRITE(MPU_RBAR_ADDR, 0xFFFFFFE0u);
        uint32_t rbar_probe = MMIO32_READ(MPU_RBAR_ADDR);
        mpu_state[2] = ~(0xFFFFFFFFu << dregion);
        uint32_t mask_low = rbar_probe & ~0x1Fu;
        mpu_state[1] = (0u - mask_low) & rbar_probe;
        rkof_memset(&mpu_state[4], 0, 0x180u);
        uint32_t count = mpu_state[0];
        uint8_t *entry = (uint8_t *)&mpu_state[4];
        while (count != 0u) {
            *(uint32_t *)(void *)(entry + 4u) = 0u;
            *(uint32_t *)(void *)entry = 0xFFFFFFE0u;
            entry += 0x18u;
            count--;
        }
    }

    MMIO32_WRITE(SCB_SHPR1_ADDR, 0u);
    MMIO32_WRITE(SCB_SHPR2_ADDR, 0x80000000u);
    MMIO32_WRITE(SCB_SHPR3_ADDR, 0x00800000u);

    uint32_t *irq_prio = RKOF_PTR(uint32_t *, STATE_IRQ_PRIO_BASE);
    irq_prio[0] = 0x80u;
    irq_prio[1] = 0x04u;

    MMIO32_WRITE(SCB_SHCSR_ADDR, 0x00070000u);
    cpu_write_primask(1u);
    cpu_write_basepri(irq_prio[0]);

    typedef void (*ctor_fn)(void);
    const ctor_fn *ctor_it = RKOF_PTR(const ctor_fn *, RKOF_RODATA_CTOR_EARLY_START);
    const ctor_fn *ctor_end = RKOF_PTR(const ctor_fn *, RKOF_RODATA_CTOR_EARLY_END);
    while (ctor_it < ctor_end) {
        ctor_fn fn = *ctor_it++;
        fn();
    }

    const ctor_fn *late_it = RKOF_PTR(const ctor_fn *, RKOF_RODATA_CTOR_LATE_START);
    const ctor_fn *late_end = RKOF_PTR(const ctor_fn *, RKOF_RODATA_CTOR_LATE_END);
    while (late_it < late_end) {
        ctor_fn fn = *late_it++;
        fn();
    }

    rtkit_app_init();
    rtkit_halt_forever(2u);
}

/* @provenance RVA 0x175c8 sub_175C8 */
uint32_t sub_175C8(void)
{
    return *RKOF_PTR(const uint32_t *, RKOF_DATA_BOOT_MODE_ADDR);
}

/* @provenance RVA 0x175d4 sub_175D4 */
void sub_175D4(void *stack_base, uint32_t byte_len)
{
    uint32_t *cursor = (uint32_t *)stack_base;
    while (byte_len >= 8u) {
        cursor[1] = 0x4B434154u; /* "TACK" */
        cursor[0] = 0x534B5452u; /* "RTKS" */
        cursor += 2;
        byte_len -= 8u;
    }
}
