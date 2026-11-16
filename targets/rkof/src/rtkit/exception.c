#include <stdarg.h>
#include "rkof/cpu.h"
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

extern uint32_t sub_1A375C(uintptr_t addr);
extern void sub_187240(const char *fmt, va_list ap);
extern void sub_187222(const char *str);
extern void sub_188300(uint32_t code, uint32_t subcode, const char *fmt, va_list ap);
extern void sub_1887D8(void);
extern void sub_17C98(void);
extern void sub_17CEC(void);
extern uint32_t sub_19CC9C(uint32_t *out_val);
extern uint32_t sub_19CBB4(uint32_t *out_a, uint32_t *out_b);
extern RKOF_NORETURN void sub_1A234C(void);

/* @provenance RVA 0x1748c sub_1748C */
RKOF_NORETURN void sub_1748C(uint8_t *exc_frame, uint32_t exc_num)
{
    uint32_t *exc_state = RKOF_PTR(uint32_t *, STATE_EXC_STATE_BASE);
    exc_state[0x4Cu / 4u] += 1u;

    uint32_t mapped_code;
    uint32_t idx = exc_num - 1u;
    if (idx <= 0xDu && ((0x243Fu >> (idx & 0xFFFFu)) & 1u) != 0u) {
        const uint32_t *code_table = RKOF_PTR(const uint32_t *, RKOF_RODATA_EXC_CODE_MAP_ADDR);
        mapped_code = code_table[idx];
    } else {
        mapped_code = (exc_num > 0xFu) ? 5u : 9u;
    }

    uint32_t *fault_info = RKOF_PTR(uint32_t *, STATE_FAULT_INFO_BASE);
    fault_info[0x10u / 4u] = (uint32_t)(uintptr_t)exc_frame;

    uint32_t frame_size = 0xE8u;
    if (exc_frame != NULL && (exc_frame[0x18] & 0x10u) != 0u) {
        frame_size = 0x60u;
    }
    fault_info[0x14u / 4u] = frame_size;

    uint32_t *frame_words = (uint32_t *)(void *)exc_frame;
    frame_words[0x0Cu / 4u] = exc_num;
    fault_info[0x0Cu / 4u] = mapped_code;

    uint32_t basepri = cpu_read_basepri();
    uint32_t afsr = MMIO32_READ(SCB_AFSR_ADDR);
    uint32_t cfsr = MMIO32_READ(SCB_CFSR_ADDR);
    uint32_t control = cpu_read_control();
    uint32_t dfsr = MMIO32_READ(SCB_DFSR_ADDR);
    uint32_t faultmask = cpu_read_faultmask();
    uint32_t hfsr = MMIO32_READ(SCB_HFSR_ADDR);
    uint32_t mmfar = MMIO32_READ(SCB_MMFAR_ADDR);
    uint32_t primask = cpu_read_primask();

    frame_words[1] = control;
    frame_words[0] = 0u;

    uint32_t irq_mask_pack = (basepri << 8u) & 0xFFFFu;
    irq_mask_pack = (irq_mask_pack & ~2u) | ((faultmask & 1u) << 1u);
    irq_mask_pack += (primask & 1u);
    frame_words[2] = irq_mask_pack;

    uint32_t exc_lr = frame_words[0x18u / 4u];
    uint32_t xpsr_off = ((exc_lr & 0x10u) != 0u) ? 0x5Cu : 0x9Cu;
    uint32_t lr_chk_off = ((exc_lr & 0x10u) != 0u) ? 0x54u : 0x94u;
    uint32_t lr_chk_val = *(const uint32_t *)(const void *)(exc_frame + lr_chk_off);
    uint32_t sp_delta = ((lr_chk_val & 0x10u) != 0u) ? 0x48u : 0xD0u;

    uintptr_t orig_sp = (uintptr_t)(exc_frame + 0x18u) + sp_delta;
    uint32_t stacked_xpsr = *(const uint32_t *)(const void *)(exc_frame + xpsr_off);
    orig_sp += ((stacked_xpsr >> 9u) & 1u) << 2u;

    frame_words[4] = 0u;
    frame_words[5] = 0u;
    frame_words[7] = (uint32_t)orig_sp;

    uint32_t sw_idx = exc_num - 3u;
    if (sw_idx <= 9u) {
        switch (sw_idx) {
        case 0u: /* HardFault (3) */
            frame_words[0] = cfsr;
            frame_words[4] = hfsr;
            frame_words[5] = ((cfsr & 0xFF00u) == 0u) ? mmfar : afsr;
            break;
        case 1u: /* MemManage (4) */
            frame_words[4] = cfsr;
            frame_words[5] = mmfar;
            break;
        case 2u: /* BusFault (5) */
            frame_words[4] = cfsr;
            frame_words[5] = afsr;
            break;
        case 3u: /* UsageFault (6) */
            frame_words[4] = cfsr;
            break;
        case 9u: /* DebugMon (12) */
            frame_words[4] = dfsr;
            break;
        default:
            break;
        }
    }

    sub_1887D8();
    rtkit_halt_forever(3u);
}

/* @provenance RVA 0x17970 sub_17970 */
uintptr_t sub_17970(uintptr_t lr_addr)
{
    uintptr_t even_pc = lr_addr & ~(uintptr_t)1u;
    uintptr_t cand = even_pc - 2u;
    if (sub_1A375C(cand) == 0u) {
        return even_pc - 3u;
    }

    uint16_t insn16 = *RKOF_PTR(const uint16_t *, cand);
    if ((insn16 & ~0x0078u) == 0x4780u) {
        return cand;
    }

    cand = even_pc - 4u;
    if (sub_1A375C(cand) == 0u) {
        return even_pc - 3u;
    }
    return cand;
}

/* @provenance RVA 0x179c8 sub_179C8 */
void sub_179C8(const char *fmt, va_list ap)
{
    uintptr_t *panic_hook_slot = RKOF_PTR(uintptr_t *, STATE_PANIC_HOOK_BASE + 0x14u);
    uintptr_t hook_obj = *panic_hook_slot;
    if (hook_obj != 0u) {
        const uintptr_t *vtable = *RKOF_PTR(const uintptr_t *const *, hook_obj);
        typedef void (*panic_hook_fn)(void *, uint32_t, const char *, va_list);
        panic_hook_fn fn = (panic_hook_fn)vtable[1];
        fn((void *)hook_obj, 0x20u, fmt, ap);
    }

    sub_187240(fmt, ap);
    sub_187222(RKOF_PTR(const char *, 0x001F322Du));
    sub_188300(0x20u, 0u, fmt, ap);
}

/* @provenance RVA 0x179a4 rtkit_panic */
RKOF_NORETURN void rtkit_panic(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    (void)sub_16F26();
    sub_179C8(fmt, ap);
    va_end(ap);
    rtkit_halt_forever(4u);
}

/* @provenance RVA 0x17a10 rtkit_halt_forever */
RKOF_NORETURN void rtkit_halt_forever(uint32_t reason)
{
    sub_17C98();
    if (reason <= 6u && (((1u << reason) & 0x58u) != 0u)) {
        const uint32_t *plat_mode = RKOF_PTR(const uint32_t *, RKOF_DATA_PLATFORM_MODE_ADDR);
        if (plat_mode[0x24u / 4u] == 1u) {
            uint32_t *exc_state = RKOF_PTR(uint32_t *, STATE_EXC_STATE_BASE);
            uintptr_t pbl_ptr = exc_state[0x78u / 4u];
            if (pbl_ptr != 0u) {
                uint32_t *pbl = RKOF_PTR(uint32_t *, pbl_ptr);
                uint32_t info_word = 0u;
                if (sub_19CC9C(&info_word) == 0u) {
                    pbl[4] = info_word;
                    pbl[3] = 0x526C6250u; /* "PblR" */
                }
                uint32_t log_base = 0u;
                uint32_t log_size = 0u;
                uint32_t rc = sub_19CBB4(&log_base, &log_size);
                if (rc == 0u || rc == 0x80000007u) {
                    pbl[1] = log_base;
                    pbl[2] = log_size;
                    pbl[0] = 0x526C6250u + 0x400u; /* "PblV" */
                }
            }
            sub_17CEC();
        }
    }
    sub_1A234C();
}

/* @provenance RVA 0x17bd4 sub_17BD4 */
void sub_17BD4(uint32_t magic_fourcc)
{
    uint8_t *slot = RKOF_PTR(uint8_t *, STATE_CRASH_MAGIC_BASE);
    slot[0x48] = 0u;
    slot[0x47] = (uint8_t)magic_fourcc;
    slot[0x46] = (uint8_t)(magic_fourcc >> 8u);
    slot[0x45] = (uint8_t)(magic_fourcc >> 16u);
    slot[0x44] = (uint8_t)(magic_fourcc >> 24u);
}

/* @provenance RVA 0x17bf8 sub_17BF8 */
uint32_t sub_17BF8(const uint32_t *be_word_ptr)
{
    return cpu_bswap32(*be_word_ptr);
}

/* @provenance RVA 0x187128 rtkit_assert_fail */
RKOF_NORETURN void rtkit_assert_fail(const char *file, uint32_t line, const char *func)
{
    rtkit_panic("ASSERTION FAILED: %s:%d\n\t%s\n", file, line, func);
}

/* @provenance RVA 0x18713c rtkit_assert_fail_hex64 */
RKOF_NORETURN void rtkit_assert_fail_hex64(
    const char *file,
    uint32_t line,
    const char *func,
    uint64_t val_a,
    uint64_t val_b)
{
    rtkit_panic(
        "ASSERTION FAILED: %s:%d: \n\t%s, %llx, %llx\n",
        file,
        line,
        func,
        (unsigned long long)val_a,
        (unsigned long long)val_b
    );
}

/* @provenance RVA 0x1887c4 sub_1887C4 */
void sub_1887C4(void)
{
    uint8_t *cd_state = RKOF_PTR(uint8_t *, RKOF_DATA_COREDUMP_STATE_ADDR);
    uint32_t active = *(const uint32_t *)(const void *)(cd_state + 0x54u);
    if (active != 0u) {
        cd_state[0x5D] = 1u;
    }
}

/* @provenance RVA 0x18a0dc rtkit_unimplemented_method */
RKOF_NORETURN void rtkit_unimplemented_method(const char *method_name)
{
    rtkit_panic("!Mthd:%p", (const void *)method_name);
}
