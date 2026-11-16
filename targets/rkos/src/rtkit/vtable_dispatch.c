#include "rkos/rkos.h"

/*
 * Indirect function-pointer / vtable call dispatcher for reconstructed C11.
 *
 * Binary-backed `ROM_RODATA` (0x13090..0x17590) and `RAM_DATA` (0x20000000..0x20000BA4)
 * carry raw firmware Thumb code addresses (`0x0000xxxx | 1`). When `table_va`
 * resides in `ROM_RODATA` or `RAM_DATA`, we resolve the raw RVA directly to its
 * reconstructed C11 function on both ARMv7E-M and 64-bit `RKOS_HOST`.
 * When `table_va` resides in SRAM or Unicorn scratch (`0x0200xxxx`), ARMv7E-M
 * invokes `fn_va` directly so runtime callbacks and differential mock vtables
 * execute identically to raw firmware.
 */

static inline bool rkos_is_raw_code_table(uint32_t table_va)
{
    if (table_va >= RKOS_ROM_RODATA_BASE && table_va < RKOS_ROM_RODATA_END) {
        return true;
    }
    if (table_va >= RKOS_RAM_DATA_BASE && table_va < RKOS_RAM_DATA_END) {
        return true;
    }
    return false;
}

uint32_t rkos_call_fn0(uint32_t table_va, uint32_t fn_va)
{
    if (rkos_is_raw_code_table(table_va)) {
        uint32_t raw_rva = fn_va & ~1u;
        switch (raw_rva) {
        case 0x3834u:
            return (uint32_t)rkos_nvic_get_noop();
        case 0x4998u:
            return rkos_status_unsupported();
        case 0x42e0u:
        case 0x42e2u:
        case 0x42e4u:
        case 0x4338u:
        case 0x44c8u:
        case 0x484cu:
        case 0x48b0u:
        case 0x11374u:
            return 0u;
        default:
            break;
        }
    }
#ifdef RKOS_HOST
    return rkos_host_invoke_irq_cb(fn_va, 0u);
#else
    uint32_t (*fn)(void) = (uint32_t (*)(void))(uintptr_t)(fn_va | 1u);
    return fn();
#endif
}

uint32_t rkos_call_fn1(uint32_t table_va, uint32_t fn_va, uint32_t arg0)
{
    if (rkos_is_raw_code_table(table_va)) {
        uint32_t raw_rva = fn_va & ~1u;
        switch (raw_rva) {
        case 0x4998u:
            return rkos_status_unsupported();
        case 0x49a0u:
            return rkos_dev_claim_open(arg0);
        case 0x49b4u:
            return rkos_dev_release_open(arg0);
        default:
            break;
        }
    }
#ifdef RKOS_HOST
    return rkos_host_invoke_irq_cb(fn_va, arg0);
#else
    uint32_t (*fn)(uint32_t) = (uint32_t (*)(uint32_t))(uintptr_t)(fn_va | 1u);
    return fn(arg0);
#endif
}

uint32_t rkos_call_fn2(uint32_t table_va, uint32_t fn_va, uint32_t arg0, uint32_t arg1)
{
    if (rkos_is_raw_code_table(table_va)) {
        uint32_t raw_rva = fn_va & ~1u;
        switch (raw_rva) {
        case 0x373cu:
            return (uint32_t)rkos_nvic_is_pending(arg0, arg1);
        case 0x3788u:
            return rkos_nvic_clear_pending(arg0, arg1);
        case 0x37d0u:
            return rkos_nvic_set_pending(arg0, arg1);
        case 0x3988u:
            return (uint32_t)rkos_nvic_is_active(arg0, arg1);
        case 0x4998u:
            return rkos_status_unsupported();
        case 0xef7cu:
            return rkos_ftab_stream_seek(arg0, arg1);
        case 0xef98u:
            return rkos_ftab_stream_get_size(arg0, RKOS_VMA_PTR(uint32_t *, arg1));
        case 0xef9eu:
            return rkos_ftab_stream_get_backend(arg0, RKOS_VMA_PTR(uint32_t *, arg1));
        default:
            break;
        }
    }
#ifdef RKOS_HOST
    (void)arg1;
    return rkos_host_invoke_irq_cb(fn_va, arg0);
#else
    uint32_t (*fn)(uint32_t, uint32_t) =
        (uint32_t (*)(uint32_t, uint32_t))(uintptr_t)(fn_va | 1u);
    return fn(arg0, arg1);
#endif
}

uint32_t rkos_call_fn3(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    if (rkos_is_raw_code_table(table_va)) {
        uint32_t raw_rva = fn_va & ~1u;
        switch (raw_rva) {
        case 0x13d0u:
            (void)rkos_memset(RKOS_VMA_RAW(arg0, (size_t)arg2), (int)arg1, (size_t)arg2);
            return arg0;
        case 0x359cu:
            return (uint32_t)rkos_nvic_set_enable(arg0, arg1, (int)arg2);
        case 0x35bcu:
            return (uint32_t)rkos_nvic_set_enable_state((int)arg0, arg1, (int)arg2);
        case 0x3664u:
            return rkos_nvic_set_priority(arg0, arg1, arg2);
        case 0x119acu:
            rkos_ccdigest_final_64be(arg0, RKOS_VMA_RAW(arg1, 128u),
                                     (uint8_t *)RKOS_VMA_RAW(arg2, 32u));
            return 0u;
        case 0x11dc8u:
            rkos_sha256_compress(RKOS_VMA_PTR(uint32_t *, arg0), (size_t)arg1,
                                 RKOS_VMA_RAW(arg2, (size_t)arg1 * 64u));
            return 0u;
        case 0x11f38u:
            rkos_sha512_compress(RKOS_VMA_PTR(uint64_t *, arg0), (size_t)arg1,
                                 RKOS_VMA_RAW(arg2, (size_t)arg1 * 128u));
            return 0u;
        case 0x1220cu:
            rkos_ccdigest_final_128be(arg0, RKOS_VMA_RAW(arg1, 256u),
                                      (uint8_t *)RKOS_VMA_RAW(arg2, 64u));
            return 0u;
        default:
            break;
        }
    }
#ifdef RKOS_HOST
    if (table_va == 0x200008A0u) {
        (void)rkos_memset((void *)(uintptr_t)arg0, (int)arg1, (size_t)arg2);
        return arg0;
    }
    return rkos_host_invoke_irq_cb(fn_va, arg0);
#else
    uint32_t (*fn)(uint32_t, uint32_t, uint32_t) =
        (uint32_t (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)(fn_va | 1u);
    return fn(arg0, arg1, arg2);
#endif
}

uint32_t rkos_call_fn4(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (rkos_is_raw_code_table(table_va)) {
        uint32_t raw_rva = fn_va & ~1u;
        switch (raw_rva) {
        case 0x3620u:
            return rkos_nvic_register_handler(arg0, arg1, arg2, arg3);
        case 0x4998u:
            return rkos_status_unsupported();
        case 0xefa4u:
            return rkos_pair_fill_same(arg0, arg1,
                                       RKOS_VMA_PTR(uint32_t *, arg2),
                                       RKOS_VMA_PTR(uint32_t *, arg3));
        default:
            break;
        }
    }
#ifdef RKOS_HOST
    return rkos_host_invoke_fn4_cb(fn_va, arg0, arg1, arg2, arg3);
#else
    uint32_t (*fn)(uint32_t, uint32_t, uint32_t, uint32_t) =
        (uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)(fn_va | 1u);
    return fn(arg0, arg1, arg2, arg3);
#endif
}

uint32_t rkos_call_fn5(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2,
                       uint32_t arg3, uint32_t arg4)
{
    (void)table_va;
#ifdef RKOS_HOST
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    return rkos_host_invoke_irq_cb(fn_va, arg0);
#else
    uint32_t (*fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) =
        (uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))(uintptr_t)(fn_va | 1u);
    return fn(arg0, arg1, arg2, arg3, arg4);
#endif
}
