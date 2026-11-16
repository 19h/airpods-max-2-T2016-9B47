#include "rkos/rkos.h"

/*
 * Reconstructed RTKitOS stage-1 boot, stack canary/watermark, exception
 * reporting, panic/halt, static constructors (`.init_array` at `0x17580`),
 * and stage-2 `rkof` SRAM trampoline handoff (`0x4110`).
 */

#ifdef RKOS_HOST
#define RKOS_BOOT_EARLY
void rkos_host_on_panic(const char *msg);
void rkos_host_on_halt(uint32_t code);
#else
#define RKOS_BOOT_EARLY __attribute__((section(".text.early")))
#endif

/* @shim RVA 0x1140 sub_1140 (pending_c: not shown equivalent to raw) */
RKOS_BOOT_EARLY void rkos_stack_guard_init(void)
{
    uint32_t guard_len = *RKOS_VMA_PTR(const uint32_t *, 0x00016404u);
    const void *src = RKOS_VMA_RAW(0x00016408u, guard_len);
    void *dst = RKOS_VMA_RAW(RKOS_STACK_GUARD_VA, guard_len);
    (void)memcpy_thunk(dst, src, guard_len);
}

/* @shim RVA 0x1158 __stack_chk_fail (pending_c: not shown equivalent to raw) */
RKOS_BOOT_EARLY void __stack_chk_fail(void)
{
    rtkit_panic("stk fail");
}

/* @provenance RVA 0x3b20 sub_3B20 */
uint32_t rkos_stack_paint_words(uint32_t base_va, uint32_t byte_len)
{
    uint32_t *words = RKOS_VMA_PTR(uint32_t *, base_va);
    while (byte_len >= 8u) {
        words[0] = 0x534B5452u; /* "RTKS" */
        words[1] = 0x4B434154u; /* "TACK" */
        words += 2;
        byte_len -= 8u;
        base_va += 8u;
    }
    return base_va;
}

/* @shim RVA 0x3b58 sub_3B58 (pending_c: not shown equivalent to raw) */
uint32_t rkos_unwind_frame(uint32_t fp, uint32_t sp_low, uint32_t sp_high,
                           uint32_t *out_pcs, uint32_t max_frames)
{
    uint32_t count = 0u;
    while (count < max_frames) {
        if ((fp & 3u) != 0u || fp < sp_low || fp + 8u > sp_high) {
            break;
        }
        const uint32_t *frame = RKOS_VMA_PTR(const uint32_t *, fp);
        uint32_t next_fp = frame[0];
        uint32_t saved_lr = frame[1];
        if (saved_lr == 0u) {
            break;
        }
        if (out_pcs != NULL) {
            out_pcs[count] = saved_lr & ~1u;
        }
        count++;
        if (next_fp <= fp) {
            break;
        }
        fp = next_fp;
    }
    return count;
}

/* @shim RVA 0x3eac sub_3EAC (pending_c: not shown equivalent to raw) */
uint32_t rkos_unwind_current(uint32_t *out_pcs, uint32_t max_frames)
{
    uint32_t sp_low = RKOS_STATE_PSP_BASE;
    uint32_t sp_high = RKOS_STATE_PLAT_BASE;
    uint32_t fp = rkos_arch_get_psp();
    return rkos_unwind_frame(fp, sp_low, sp_high, out_pcs, max_frames);
}

/* @shim RVA 0x10334 rtkit_reboot_halt (pending_c: not shown equivalent to raw) */
void rtkit_reboot_halt(void)
{
    if (rkos_arch_get_primask() == 0u) {
        rkos_arch_set_primask(1u);
    }
#ifdef RKOS_HOST
    rkos_host_on_halt(0xFFu);
#else
    for (;;) {
        rkos_arch_dsb();
        rkos_arch_isb();
        rkos_arch_wfi();
        rkos_arch_isb();
    }
#endif
}

/* @shim RVA 0x3f4c rtkit_halt (pending_c: not shown equivalent to raw) */
void rtkit_halt(uint32_t action)
{
#ifdef RKOS_HOST
    rkos_host_on_halt(action);
#else
    (void)action;
    rtkit_reboot_halt();
#endif
}

/* @shim RVA 0x3f04 sub_3F04 (pending_c: not shown equivalent to raw) */
int rkos_vpanic_report(const char *fmt, va_list ap)
{
#ifdef RKOS_HOST
    char msg[256];
    va_list copy;
    va_copy(copy, ap);
    (void)rkos_vsnprintf(msg, sizeof(msg), fmt, copy);
    va_end(copy);
    rkos_host_on_panic(msg);
    return 0;
#else
    (void)rkos_vprintf(fmt, ap);
    (void)rkos_printf("\n");
    crashlog_dump(0x20u, 0u);
    return 0;
#endif
}

/* @shim RVA 0x3ee0 rtkit_panic (pending_c: not shown equivalent to raw) */
void rtkit_panic(const char *fmt, ...)
{
    (void)rkos_primask_disable_if_enabled();
    va_list ap;
    va_start(ap, fmt);
    (void)rkos_vpanic_report(fmt, ap);
    va_end(ap);
    rtkit_halt(4u);
}

/* @shim RVA 0x39f0 rtkit_exception_halt (pending_c: not shown equivalent to raw) */
void rtkit_exception_halt(uint32_t frame_va, uint32_t exc_num)
{
    uint32_t *fault_depth = RKOS_VMA_PTR(uint32_t *, 0x2001DE00u);
    (*fault_depth)++;

    uint32_t reason = 9u;
    uint32_t idx = exc_num - 1u;
    if (idx <= 13u && (((0x243Fu >> idx) & 1u) != 0u)) {
        const uint32_t *reason_tbl = RKOS_VMA_PTR(const uint32_t *, 0x000142A8u);
        reason = reason_tbl[idx];
    } else if (exc_num > 15u) {
        reason = 5u;
    }

    uint32_t frame_sz = 0xE8u;
    if (frame_va != 0u) {
        uint8_t exc_ret_lo = *RKOS_VMA_PTR(const uint8_t *, frame_va + 0x18u);
        if ((exc_ret_lo & 0x10u) != 0u) {
            frame_sz = 0x60u;
        }
    }

    *RKOS_VMA_PTR(uint32_t *, 0x2001DE58u) = reason;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE5Cu) = frame_va;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE60u) = frame_sz;

    if (frame_va != 0u) {
        uint32_t *frame = RKOS_VMA_PTR(uint32_t *, frame_va);
        uint32_t cfsr  = RKOS_REG32_READ(RKOS_SCB_CFSR);
        uint32_t hfsr  = RKOS_REG32_READ(RKOS_SCB_HFSR);
        uint32_t dfsr  = RKOS_REG32_READ(RKOS_SCB_DFSR);
        uint32_t mmfar = RKOS_REG32_READ(RKOS_SCB_MMFAR);
        uint32_t bfar  = RKOS_REG32_READ(RKOS_SCB_BFAR);

        frame[0] = 0u;
        frame[1] = rkos_arch_get_ipsr();
        frame[2] = (rkos_arch_get_basepri() << 8) | (rkos_arch_get_primask() & 1u);
        frame[3] = exc_num;
        frame[4] = 0u;
        frame[5] = 0u;

        switch (exc_num) {
        case 3u:
            frame[0] = cfsr;
            frame[4] = hfsr;
            frame[5] = ((cfsr & 0xFF00u) != 0u) ? bfar : mmfar;
            break;
        case 4u:
            frame[4] = cfsr;
            frame[5] = mmfar;
            break;
        case 5u:
            frame[4] = cfsr;
            frame[5] = bfar;
            break;
        case 6u:
            frame[4] = cfsr;
            break;
        case 12u:
            frame[4] = dfsr;
            break;
        default:
            break;
        }
    }

    crashlog_dump(reason, frame_va);
    rtkit_halt(3u);
}

/* @shim RVA 0x42e8 rtkit_bad_register (pending_c: not shown equivalent to raw) */
int rtkit_bad_register(uint32_t obj_va)
{
    if (obj_va == 0u) {
        return -1;
    }
    uint8_t flag = *RKOS_VMA_PTR(const uint8_t *, obj_va + 0x0Cu);
    if (flag == 0xFFu) {
        rtkit_panic("bad register: %x", obj_va);
    }
    uint32_t log_va = *RKOS_VMA_PTR(const uint32_t *, 0x2001DE34u);
    uint8_t large_log = *RKOS_VMA_PTR(const uint8_t *, 0x2001DDF8u);
    *RKOS_VMA_PTR(uint32_t *, 0x2000049Cu) = log_va;
    *RKOS_VMA_PTR(uint32_t *, 0x200004A0u) = (large_log != 0u) ? 0x8000u : 0x400u;
    return 0;
}

/* Static constructor 0 (`g_init_array[0]` at 0x17580) */
/* @provenance RVA 0x12b28 sub_12B28 */
uint32_t rkos_init_qspi_objects(void)
{
    *RKOS_VMA_PTR(uint32_t *, 0x20000BC0u) = 0x000168C4u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BC8u) = RKOS_MMIO_QSPI_BASE;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BCCu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BD0u) = 0x1000u;
    *RKOS_VMA_PTR(uint8_t *,  0x20000BD4u) &= 0xFCu;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BD8u) = 0x18u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BDCu) = 0x03938700u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BE0u) = 8u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BE4u) = 8u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BE8u) = 0x200001A0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BECu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BF0u) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BF4u) = 0xFFFFFFFFu;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BF8u) = 0x10u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000BFCu) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C00u) = 0x80u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C04u) = 0x20000FC0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C08u) = 0x0Au;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C0Cu) = 0x20u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C10u) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C14u) = 4u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C18u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C1Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C20u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C24u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C28u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C2Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C30u) = 0u;
    *RKOS_VMA_PTR(uint16_t *, 0x20000C34u) = 0u;

    rkos_memset(RKOS_VMA_RAW(0x20000C38u, 0x108u), 0, 0x108u);
    *RKOS_VMA_PTR(uint32_t *, 0x20000C38u) = 0x000168F0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C50u) = 0x20000BC0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C54u) = 0x03938700u;
    *RKOS_VMA_PTR(uint8_t *,  0x20000C5Au) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C70u) = 0x00015CE0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000C74u) = 0x1Du;
    *RKOS_VMA_PTR(uint8_t *,  0x20000CA8u) = 0x25u;
    return 0x000168F0u;
}

/* Static constructor 1 (`g_init_array[1]` at 0x17584) */
/* @provenance RVA 0x12bf8 sub_12BF8 */
uint32_t rkos_init_crashlog_plugin(void)
{
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE08u) = RKOS_RTBUDDY_DRV_VA;
    return 0x2001DDE8u;
}

/* Static constructor 2 (`g_init_array[2]` at 0x17588) */
/* @provenance RVA 0x12c24 sub_12C24 */
uint32_t rkos_init_aux_heap(void)
{
    return rkos_heap_init_struct(0x2001DAA0u);
}

/* Static constructor 3 (`g_init_array[3]` at 0x1758C) */
/* @provenance RVA 0x12c30 sub_12C30 */
uint32_t rkos_init_platform_drivers(void)
{
    /* RTBuddy / TRNG / Fuse platform driver objects at 0x2001DB08..0x2001DC38 */
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB08u) = 0x0001670Cu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB10u) = RKOS_MMIO_RTBUDDY_BASE;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB14u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB18u) = 0x4000u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DB1Cu) &= 0xFCu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB20u) = 0x20000240u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB24u) = 0x20000928u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB28u) = 0x200006D0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB2Cu) = 0x200003F0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB30u) = 0x20000FC0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB34u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB38u) = 0x800Cu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB3Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB40u) = 0x6000u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB44u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB48u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB4Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB50u) = 0x6000u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB54u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB58u) = 0x607Cu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB5Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB60u) = 0xFFFFFFFFu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB64u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB68u) = 0x6078u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB6Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB70u) = 0xFFFFFFFFu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB74u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB78u) = 0x605Cu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB7Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB80u) = 0xFFFFFFFFu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB84u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB88u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB8Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB90u) = 0x6080u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB94u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB98u) = 0x03F8u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DB9Cu) = 3u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DBA0u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBA4u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBA8u) = 0u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DBACu) = 0u;

    *RKOS_VMA_PTR(uint32_t *, 0x2001DBB0u) = 0x000168D8u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBB8u) = RKOS_MMIO_TRNG_BASE;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBBCu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBC0u) = 0x001C0000u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DBC4u) &= 0xFCu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBC8u) = 0x2001DB08u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBCCu) = 0x20000928u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBD0u) = 0x200006D0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBD4u) = 0x20000958u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBD8u) = 0x20000FC0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBDCu) = 0x20000240u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBE0u) = 0x7Fu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBE4u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBE8u) = 0x120u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBECu) = 0x20000690u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBF0u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBF4u) = 0x20000290u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBF8u) = 0xAFu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DBFCu) = 0xADu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC00u) = 0xB0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC04u) = 0xAEu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC08u) = 0xB1u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC0Cu) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC10u) = 3u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC14u) = 3u;
    *RKOS_VMA_PTR(uint16_t *, 0x2001DC18u) = 0u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DC1Au) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC1Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC20u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC24u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC28u) = 0u;
    *RKOS_VMA_PTR(uint16_t *, 0x2001DC2Cu) = 0u;
    *RKOS_VMA_PTR(uint8_t *,  0x2001DC2Eu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC30u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DC34u) = 0u;

    /* SPI-DMA & PMGR/WDT driver state at 0x20000FC0..0x20001074 */
    *RKOS_VMA_PTR(uint32_t *, 0x20000FC0u) = 0x000168B8u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FC8u) = RKOS_MMIO_SPIDMA_BASE;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FCCu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FD0u) = 0x4000u;
    *RKOS_VMA_PTR(uint8_t *,  0x20000FD4u) &= 0xFCu;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FD8u) = 0x39u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FDCu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FE0u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FE4u) = 0x200001A0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FE8u) = 2u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FECu) = 0x0Bu;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FF0u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FF4u) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FF8u) = 2u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FFCu) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001000u) = 2u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001004u) = 0x50u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001008u) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001010u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001018u) = RKOS_MMIO_PMGR_WDT_BASE;
    *RKOS_VMA_PTR(uint32_t *, 0x2000101Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001020u) = 0x1000u;
    *RKOS_VMA_PTR(uint8_t *,  0x20001024u) &= 0xFCu;
    *RKOS_VMA_PTR(uint32_t *, 0x20001028u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2000102Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001030u) = 0u;
    *RKOS_VMA_PTR(uint8_t *,  0x20001034u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001038u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2000103Cu) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001040u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001044u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20001048u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x2000104Cu) = 0u;
    *RKOS_VMA_PTR(uint8_t *,  0x20001050u) = 0u;
    rkos_memset(RKOS_VMA_RAW(0x20001054u, 0x20u), 0, 0x20u);
    return 0x20001054u;
}

/* @shim RVA 0x4110 rtkit_boot_stage2 (pending_c: not shown equivalent to raw) */
void rtkit_boot_stage2(void)
{
    uint32_t status = rkos_remap_build_table(0x40000u, 0x490000u,
                                             *RKOS_VMA_PTR(const uint32_t *, 0x2001DE18u),
                                             *RKOS_VMA_PTR(const uint32_t *, 0x2001DE1Cu));
    if ((status & 0x90000000u) != 0u) {
        rtkit_panic("!memamp");
    }

    rtkit_crashlog_init(RKOS_CRASHLOG_SHM_BASE);
    (void)rkos_irq_save_and_raise_basepri();

    void *tramp_dst = RKOS_VMA_RAW(RKOS_SRAM_TRAMP_BASE, RKOS_SRAM_TRAMP_SIZE);
    const void *tramp_src = RKOS_VMA_RAW(RKOS_TRAMPOLINE_SRC_VA, RKOS_SRAM_TRAMP_SIZE);
    (void)memcpy_thunk(tramp_dst, tramp_src, RKOS_SRAM_TRAMP_SIZE);
    rkos_arch_dsb();
    rkos_arch_isb();

#ifndef RKOS_HOST
    void (*stage2_tramp)(uint32_t) = (void (*)(uint32_t))(uintptr_t)(RKOS_SRAM_TRAMP_BASE | 1u);
    stage2_tramp(0x2001DE94u);
#endif
}

/* @shim RVA 0x279c rtkit_main (pending_c: not shown equivalent to raw) */
void rtkit_main(void)
{
    uint32_t status = rkos_remap_build_table(0u, 0x490000u, RKOS_REMAP_CFG_VA, 9u);
    rkos_assert_status_ok(status);

    uint32_t log_obj = cxx_operator_new(0x10u);
    uint32_t *obj_words = RKOS_VMA_PTR(uint32_t *, log_obj);
    obj_words[0] = 0x000148F8u;
    obj_words[1] = 0u;
    obj_words[2] = 0x2001DA74u;
    *RKOS_VMA_PTR(uint8_t *, log_obj + 0x0Cu) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE04u) = log_obj;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE20u) = log_obj;

    uint32_t log_buf = rtkit_malloc(0x8000u);
    if (log_buf != 0u) {
        *RKOS_VMA_PTR(uint32_t *, 0x2001DE34u) = log_buf;
        *RKOS_VMA_PTR(uint8_t *,  0x2001DDF8u) = 1u;
    }

    (void)rkos_irq_enable_all();
    rtkit_boot_stage2();
}

/* @shim RVA 0x32cc rtkit_start (pending_c: not shown equivalent to raw) */
RKOS_BOOT_EARLY void rtkit_start(void)
{
    /* Enable single-precision FPU (CP10/CP11 full access) */
    RKOS_REG32_WRITE(RKOS_SCB_CPACR, 0x00F00000u);
    rkos_arch_dsb();
    rkos_arch_isb();

    /* Probe ARMv7E-M MPU region count and initialize MPU shadow state */
    uint32_t mpu_type = RKOS_REG32_READ(RKOS_MPU_TYPE);
    uint32_t num_regions = (mpu_type >> 8) & 0xFFu;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DED0u) = num_regions;

    if ((mpu_type & 1u) == 0u && num_regions != 0u) {
        RKOS_REG32_WRITE(RKOS_MPU_RBAR, 0xFFFFFFE0u);
        uint32_t rbar_probe = RKOS_REG32_READ(RKOS_MPU_RBAR);
        uint32_t mask = (num_regions >= 32u) ? 0xFFFFFFFFu : ((1u << num_regions) - 1u);
        uint32_t rbar_masked = rbar_probe & 0xFFFFFFE0u;
        uint32_t min_align = (0u - rbar_masked) & rbar_probe;

        *RKOS_VMA_PTR(uint32_t *, 0x2001DED8u) = mask;
        *RKOS_VMA_PTR(uint32_t *, 0x2001DED4u) = min_align;

        uint32_t *shadow = RKOS_VMA_PTR(uint32_t *, 0x2001DEE0u);
        rkos_memset(shadow, 0, 96u * sizeof(uint32_t));
        for (uint32_t i = 0u; i < num_regions && i < 16u; i++) {
            shadow[i * 6u + 0u] = 0xFFFFFFE0u;
            shadow[i * 6u + 1u] = 0u;
        }
    }

    /* Configure exception priorities (SVCall=0x80, PendSV=0x80) and enable faults */
    RKOS_REG32_WRITE(RKOS_SCB_SHPR1, 0x00000000u);
    RKOS_REG32_WRITE(RKOS_SCB_SHPR2, 0x80000000u);
    RKOS_REG32_WRITE(RKOS_SCB_SHPR3, 0x00800000u);
    *RKOS_VMA_PTR(uint32_t *, 0x2001F060u) = 0x80u;
    *RKOS_VMA_PTR(uint32_t *, 0x2001F064u) = 4u;
    RKOS_REG32_WRITE(RKOS_SCB_SHCSR, RKOS_SHCSR_ALL_FAULTS_ENA);

    rkos_arch_set_primask(0u);
    rkos_arch_set_basepri(0x80u);

    /* Invoke the 4 reconstructed `.init_array` static constructors directly */
    (void)rkos_init_qspi_objects();
    (void)rkos_init_crashlog_plugin();
    (void)rkos_init_aux_heap();
    (void)rkos_init_platform_drivers();

    rtkit_main();
}
