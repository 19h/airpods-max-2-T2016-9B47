/*
 * boot.c — ARMv7E-M reset/exception veneers (tracked as retained_native) and
 * C11 boot, stack-canary, MPU permission, VM region allocator, and MPU shadow
 * non-overlap verification routines reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

#if defined(__arm__) || defined(__thumb__)

/* @shim RVA 0x693c Reset_Handler */
__attribute__((naked, section(".text.reset"), used))
void Reset_Handler(void)
{
    __asm__ volatile(
        "ldr r0, =0x20819ca0\n\t"
        "ldr r1, [r0]\n\t"
        "ldr r2, =0xbaadf1ac\n\t"
        "cmp r1, r2\n\t"
        "beq rtk_boot_corrupt_halt\n\t"
        "ldr r3, =0xcafe4b0b\n\t"
        "cmp r1, r3\n\t"
        "bne 1f\n\t"
        "ldr r1, [r0, #4]\n\t"
        "bx  r1\n\t"
        "1:\n\t"
        "str r2, [r0]\n\t"
        "mov r7, #0x7700\n\t"
        "movt r7, #0x7777\n\t"
        "ldr r0, =0x00072e7c\n\t"
        "ldr r0, [r0]\n\t"
        "msr msp, r0\n\t"
        "ldr r1, =0x00072e78\n\t"
        "ldr r1, [r1]\n\t"
        "msr psp, r1\n\t"
        "mrs r2, control\n\t"
        "orr r2, r2, #2\n\t"
        "msr control, r2\n\t"
        "isb sy\n\t"
        "b   rtk_start\n\t"
    );
}

/* @shim RVA 0x69d4 HardFault_Default_Handler */
__attribute__((naked, section(".text.hardfault"), used))
void HardFault_Default_Handler(void)
{
    __asm__ volatile(
        "tst lr, #4\n\t"
        "ite eq\n\t"
        "mrseq r0, msp\n\t"
        "mrsne r0, psp\n\t"
        "mov r1, lr\n\t"
        "b   rtk_fault_entry\n\t"
    );
}

/* @shim RVA 0x6a24 NMI_Handler */
__attribute__((naked, section(".text.nmi"), used))
void NMI_Handler(void)
{
    __asm__ volatile(
        "b   SysTick_IRQ_Handler\n\t"
    );
}

/* @shim RVA 0x6ab8 SVCall_PendSV_Handler */
__attribute__((naked, section(".text.svc_pendsv"), used))
void SVCall_PendSV_Handler(void)
{
    __asm__ volatile(
        "tst lr, #4\n\t"
        "ite eq\n\t"
        "mrseq r0, msp\n\t"
        "mrsne r0, psp\n\t"
        "mov r1, lr\n\t"
        "b   rtk_svc_pendsv_dispatch\n\t"
    );
}

#define AOPF_CORRUPT_SEC __attribute__((section(".text.corrupt_halt"), used))
#else
void Reset_Handler(void) {}
void HardFault_Default_Handler(void) {}
void NMI_Handler(void) {}
void SVCall_PendSV_Handler(void) {}
#define AOPF_CORRUPT_SEC
#endif

/* @shim RVA 0x69d0 rtk_boot_corrupt_halt */
AOPF_CORRUPT_SEC
void rtk_boot_corrupt_halt(void)
{
#if defined(__arm__) || defined(__thumb__)
    for (;;) {
        aopf_wfi();
    }
#endif
}

/* @provenance RVA 0x6cbc __stack_chk_guard_init */
void rtk_stack_chk_guard_init(void)
{
    const rtk_tlv_hdr_t *stkg_tlv =
        AOPF_VMA_PTR(const rtk_tlv_hdr_t *, AOPF_METADATA_TLV_BASE);
    void *guard_slot = AOPF_VMA_PTR(void *, AOPF_STACK_CHK_GUARD_VMA);
    rtk_memcpy_wrap(guard_slot, (const void *)(stkg_tlv + 1), stkg_tlv->length);
}

/* @shim RVA 0x6cd4 __stack_chk_fail */
void rtk_stack_chk_fail(void)
{
    rtk_panic("stk_fail");
}

/*
 * Helper modelling MPU permission code checks used by rtk_mpu_addr_check_perm6
 * (0x5AC3A: (perm & ~1) == 6) and rtk_mpu_addr_check_mask (0x5AC58: bitmask 0x135).
 */
int rtk_mpu_perm_is_rw(uint32_t perm_code)
{
    return ((perm_code & ~1u) == 6u) ? 1 : 0;
}

int rtk_mpu_perm_is_readable(uint32_t perm_code)
{
    if (perm_code > 8u) {
        return 0;
    }
    return (int)((0x0135u >> perm_code) & 1u);
}

/* @shim RVA 0x5ac7c sub_5AC7C */
rtk_vm_entry_t *rtk_vm_entry_alloc(uint32_t base_addr, uint32_t size_bytes)
{
    rtk_vm_entry_t *entry = (rtk_vm_entry_t *)rtk_malloc(sizeof(rtk_vm_entry_t));
    if (entry != NULL) {
        entry->base_addr = base_addr;
        entry->size_perm = (size_bytes & 0x0fffffffu) | 0x80000000u;
        entry->reserved_08 = 0u;
        entry->flags_0c = 0u;
        entry->next = NULL;
    }
    return entry;
}

int rtk_mpu_verify_non_overlap(const rtk_mpu_shadow_entry_t *table, uint32_t count)
{
    for (uint32_t outer = 0u; outer < count; outer++) {
        uint32_t sz_outer = table[outer].size_bytes;
        if (sz_outer == 0u) {
            continue;
        }
        uint32_t start_outer = table[outer].base_addr;
        uint32_t end_outer = start_outer + sz_outer;

        for (uint32_t inner = outer + 1u; inner < count; inner++) {
            uint32_t sz_inner = table[inner].size_bytes;
            if (sz_inner == 0u) {
                continue;
            }
            uint32_t start_inner = table[inner].base_addr;
            uint32_t end_inner = start_inner + sz_inner;
            if (end_outer > start_inner && start_outer < end_inner) {
                return -1;
            }
        }
    }
    return 0;
}

/*
 * Link-boundary reconstruction of Vm_initAndVerifyNonOverlap (RVA 0x5aca0;
 * tracked as pending_c in ledger.json until callee sub_5AD7C is reconstructed).
 */
int Vm_initAndVerifyNonOverlap(void)
{
    uint32_t *state_ptr = AOPF_VMA_PTR(uint32_t *, AOPF_VM_INIT_STATE_VMA);
    uint32_t cur_state = *state_ptr;
    if (cur_state == 1u || cur_state == 2u) {
        return (cur_state == 2u) ? 0 : -1;
    }
    *state_ptr = 1u;

    const rtk_mpu_shadow_entry_t *mpu_tbl =
        AOPF_VMA_PTR(const rtk_mpu_shadow_entry_t *, AOPF_MPU_SHADOW_TABLE_VMA);
    for (uint32_t outer = 0u; outer < 8u; outer++) {
        uint32_t sz_outer = mpu_tbl[outer].size_bytes;
        if (sz_outer == 0u) {
            continue;
        }
        uint32_t start_outer = mpu_tbl[outer].base_addr;
        uint32_t end_outer = start_outer + sz_outer;
        for (uint32_t inner = outer + 1u; inner < 8u; inner++) {
            uint32_t sz_inner = mpu_tbl[inner].size_bytes;
            if (sz_inner == 0u) {
                continue;
            }
            uint32_t start_inner = mpu_tbl[inner].base_addr;
            uint32_t end_inner = start_inner + sz_inner;
            if (end_outer > start_inner && start_outer < end_inner) {
                rtk_panic("VmOv %d %d", (int)outer, (int)inner);
            }
        }
    }
    *state_ptr = 2u;
    return 0;
}

/* @shim RVA 0x4e5fc sub_4E5FC */
void rtk_idle_wfi_loop(void)
{
#if defined(__arm__) || defined(__thumb__)
    for (;;) {
        aopf_dsb();
        aopf_isb();
        aopf_wfi();
        aopf_isb();
    }
#else
    aopf_dsb();
    aopf_isb();
    aopf_wfi();
    aopf_isb();
#endif
}

/*
 * Early C11 reset initialization sequence from rtk_start (RVA 0x4e2a0;
 * tracked as pending_c in ledger.json until C++ __init_array targets and
 * kernel entry 0x17f84 / 0x206f8 are reconstructed):
 *   1. 3-slice RAM_BSS zeroing around live PSP (0x20899490..0x2089A890) and
 *      MSP (0x2089B610..0x2089BA10) stacks.
 *   2. "RTKSTACK" 64-bit watermark paint on PSP and MSP stacks.
 *   3. STKG TLV copy into __stack_chk_guard (0x2080C2D0).
 *   4. SCB->CCR (STKALIGN|DIV_0_TRP), VTOR (0x64000), CPACR (FPv4-SP-D16),
 *      SHPR2/SHPR3 (SVCall/PendSV prio 0x80), SHCSR fault enables.
 */
void rtk_start(void)
{
    /* 1. Clear RAM_BSS [0x2081B000 .. 0x2089BED8) in 3 stack-preserving slices */
    rtk_memset(AOPF_VMA_PTR(void *, AOPF_RAM_BSS_CLEAR_BASE), 0,
               AOPF_PSP_STACK_BOTTOM - AOPF_RAM_BSS_CLEAR_BASE);
    rtk_memset(AOPF_VMA_PTR(void *, AOPF_PSP_STACK_TOP), 0,
               AOPF_MSP_STACK_BOTTOM - AOPF_PSP_STACK_TOP);
    rtk_memset(AOPF_VMA_PTR(void *, AOPF_MSP_STACK_TOP), 0,
               AOPF_RAM_END - AOPF_MSP_STACK_TOP);
    aopf_dsb();

    /* 2. Paint PSP and MSP stack regions with "RTKSTACK" watermark */
    uint32_t *psp_words = AOPF_VMA_PTR(uint32_t *, AOPF_PSP_STACK_BOTTOM);
    for (size_t idx = 0u; idx < (AOPF_PSP_STACK_SIZE / 8u); idx++) {
        psp_words[idx * 2u + 0u] = RTK_STACK_PAINT_LO;
        psp_words[idx * 2u + 1u] = RTK_STACK_PAINT_HI;
    }
    uint32_t *msp_words = AOPF_VMA_PTR(uint32_t *, AOPF_MSP_STACK_BOTTOM);
    for (size_t idx = 0u; idx < (AOPF_MSP_STACK_SIZE / 8u); idx++) {
        msp_words[idx * 2u + 0u] = RTK_STACK_PAINT_LO;
        msp_words[idx * 2u + 1u] = RTK_STACK_PAINT_HI;
    }

    /* 3. Copy STKG TLV into __stack_chk_guard @ 0x2080C2D0 */
    rtk_stack_chk_guard_init();

    /* 4. Configure SCB->CCR, VTOR, FPU CPACR, SHPR, SHCSR, and g_crit_basepri */
    uint32_t ccr = (bus_read32(SCB_CCR_ADDR) | 0x00000210u) & ~0x00000008u;
    bus_write32(SCB_CCR_ADDR, ccr);
    bus_write32(SCB_VTOR_ADDR, AOPF_VTOR_TABLE_BASE);
    bus_set32(SCB_CPACR_ADDR, 0x00f00000u);
    aopf_dsb();
    aopf_isb();

    bus_write32(SCB_SHPR1_ADDR, 0u);
    bus_write32(SCB_SHPR2_ADDR, 0x80000000u);
    bus_write32(SCB_SHPR3_ADDR, 0x00800000u);
    bus_write32(SCB_SHCSR_ADDR, 0x00070000u);
    *AOPF_VMA_PTR(uint32_t *, 0x2089bed0u) = 0x80u;
    *AOPF_VMA_PTR(uint32_t *, 0x2089bed4u) = 4u;

    (void)Vm_initAndVerifyNonOverlap();
}
