#include "acib/acib.h"
#include <stdint.h>

/*
 * Explicit link-boundary symbols for unported (pending_c) callees referenced
 * by higher-level reconstructed entry points when linking acib.elf.
 *
 * IMPORTANT: None of these symbols are marked reconstructed_c in
 * verification/ledger.json, and none carry @provenance tags.
 */

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

ACIB_WEAK void sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg)
{ (void)fmt; (void)args_ptr; (void)arg2; (void)arg3; (void)sp_arg; }
ACIB_WEAK void sub_EAC84(uint32_t code, uint32_t sub, const char *fmt, uint32_t *args_ptr, uint32_t *sp_arg)
{ (void)code; (void)sub; (void)fmt; (void)args_ptr; (void)sp_arg; }
ACIB_WEAK void sub_6CFF4(uint32_t halt_reason) { (void)halt_reason; for (;;) {} }
ACIB_WEAK uint32_t sub_EA86E(uint32_t addr) { return (addr >= 0x108u && addr < 0xEFEA8u) ? 1u : 0u; }
ACIB_WEAK void sub_E5E78(void *exc_frame, uint32_t ipsr) { (void)exc_frame; (void)ipsr; for (;;) {} }

ACIB_WEAK uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count) { (void)ev_base; (void)count; return 0u; }
ACIB_WEAK uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val)
{ (void)slot_ptr; (void)reg_addr; (void)val; return 2u; }
ACIB_WEAK uint32_t sub_7E9F8(uint32_t end_ptr) { return end_ptr; }
ACIB_WEAK void sub_DF764(void) {}
ACIB_WEAK void sub_7E084(uint16_t ver_major, uint16_t ver_minor, uint16_t ver_patch, uint16_t ver_build)
{ (void)ver_major; (void)ver_minor; (void)ver_patch; (void)ver_build; }
ACIB_WEAK void sub_7EE58(void) {}
ACIB_WEAK int  sub_74240(uint32_t log_id) { (void)log_id; return 0; }
ACIB_WEAK void sub_74256(uint32_t log_id, uint32_t val0) { (void)log_id; (void)val0; }
ACIB_WEAK void sub_74270(uint32_t log_id, uint32_t val0, uint32_t val1) { (void)log_id; (void)val0; (void)val1; }
ACIB_WEAK void sub_74290(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2)
{ (void)log_id; (void)val0; (void)val1; (void)val2; }
ACIB_WEAK void sub_742B2(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2, uint32_t val3)
{ (void)log_id; (void)val0; (void)val1; (void)val2; (void)val3; }
ACIB_WEAK void sub_7DC92(uint32_t err_id) { (void)err_id; }
ACIB_WEAK void sub_6DF3C(uint32_t ext_irq) { (void)ext_irq; }
ACIB_WEAK void sub_6DC88(void) {}
ACIB_WEAK void sub_6DD04(void) {}
ACIB_WEAK void sub_6DD70(uint32_t lock_arg) { (void)lock_arg; }
ACIB_WEAK void sub_6DDB8(void) {}
ACIB_WEAK void sub_6DE10(void) {}
ACIB_WEAK int  sub_7CC08(uint32_t flags, uint32_t slot) { (void)flags; (void)slot; return 1; }
ACIB_WEAK int  sub_7CC8C(uint32_t flags, uint32_t slot) { (void)flags; (void)slot; return 1; }
ACIB_WEAK void sub_740A8(uint32_t log_id, uint32_t line) { (void)log_id; (void)line; }
ACIB_WEAK void sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0) { (void)log_id; (void)line; (void)val0; }
ACIB_WEAK void sub_35354(uint8_t *out_buf, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4)
{ (void)out_buf; (void)w0; (void)w1; (void)w2; (void)w3; (void)w4; }
ACIB_WEAK void sub_EBA60(uint32_t ctx_addr, uint32_t tag_addr, const uint8_t *buf)
{ (void)ctx_addr; (void)tag_addr; (void)buf; }
ACIB_WEAK uint32_t sub_7513C(const char *msg, uint32_t code) { (void)msg; (void)code; return 1u; }
ACIB_WEAK void sub_7835C(uint32_t delay_us) { (void)delay_us; }
ACIB_WEAK void sub_7D820(uint32_t arg) { (void)arg; }
ACIB_WEAK int  sub_CF6B4(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3,
                         uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{ (void)p0; (void)p1; (void)p2; (void)p3; (void)s0; (void)s1; (void)s2; (void)s3; (void)s4; return 0; }
ACIB_WEAK void sub_96458(uint32_t reg_id, uint32_t val) { (void)reg_id; (void)val; }
ACIB_WEAK void sub_964AC(uint32_t val) { (void)val; }
ACIB_WEAK void sub_96434(uint32_t reg_id, uint32_t val) { (void)reg_id; (void)val; }

ACIB_WEAK void sub_707F0(void) {}
ACIB_WEAK void sub_DECFC(void) {}
ACIB_WEAK void sub_2F78(void) {}
ACIB_WEAK int  sub_71540(uint32_t arg) { (void)arg; return 0; }
ACIB_WEAK void sub_E696A(const char *fmt, const char *arg) { (void)fmt; (void)arg; }
ACIB_WEAK uint32_t sub_71240(void) { return 0u; }
ACIB_WEAK const char *sub_EBA64(uint32_t id) { (void)id; return NULL; }
ACIB_WEAK void sub_7076C(void) {}
ACIB_WEAK uint32_t sub_77A44(void) { return 0u; }
ACIB_WEAK uint32_t sub_6C850(uint32_t arg) { return arg; }
ACIB_WEAK void sub_73F20(uint32_t arg) { (void)arg; }
ACIB_WEAK void sub_7B1C4(uint32_t arg) { (void)arg; }
ACIB_WEAK void sub_74F10(uint32_t arg) { (void)arg; }
ACIB_WEAK void sub_6AFB0(void) {}
ACIB_WEAK void sub_776E0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{ (void)p0; (void)p1; (void)p2; (void)p3; }
ACIB_WEAK void sub_69978(uint32_t arg) { (void)arg; }
ACIB_WEAK uint32_t sub_598E8(uint32_t arg) { return arg; }
ACIB_WEAK uint32_t sub_133BC(uint32_t arg) { return arg; }
ACIB_WEAK uint32_t sub_188B4(uint32_t arg) { return arg; }
ACIB_WEAK void sub_4B5C2(uint32_t arg) { (void)arg; }
ACIB_WEAK void sub_2774(void) {}
ACIB_WEAK uint32_t sub_2584(void) { return 0u; }
ACIB_WEAK void sub_4B2B0(uint32_t arg) { (void)arg; }
ACIB_WEAK uintptr_t sub_E713C(const char *name, uintptr_t entry, uint32_t id,
                              uint32_t flags, uint32_t stack_sz, uint32_t prio)
{ (void)name; (void)entry; (void)id; (void)flags; (void)stack_sz; (void)prio; return (uintptr_t)id; }
ACIB_WEAK void *sub_E7C20(uintptr_t handle) { return (void *)handle; }
ACIB_WEAK void sub_2F8C(void) {}
ACIB_WEAK void sub_59884(void) {}
ACIB_WEAK void sub_5688(void) {}

uint64_t acib_uldivmod_core(uint64_t numer, uint64_t denom, uint64_t *rem_out);
void __aeabi_uldivmod(void);

uint64_t acib_uldivmod_core(uint64_t numer, uint64_t denom, uint64_t *rem_out)
{
    if (denom == 0ULL) {
        *rem_out = 0ULL;
        return 0ULL;
    }
    uint64_t quot = 0ULL;
    uint64_t rem  = 0ULL;
    for (uint32_t i = 0u; i < 64u; i++) {
        rem = (rem << 1) | (numer >> 63);
        numer <<= 1;
        quot <<= 1;
        if (rem >= denom) {
            rem -= denom;
            quot |= 1ULL;
        }
    }
    *rem_out = rem;
    return quot;
}

ACIB_NAKED void __aeabi_uldivmod(void)
{
    __asm__ volatile(
        "push    {r4, lr}\n\t"
        "sub     sp, #16\n\t"
        "add     r4, sp, #8\n\t"
        "str     r4, [sp]\n\t"
        "bl      acib_uldivmod_core\n\t"
        "ldr     r2, [sp, #8]\n\t"
        "ldr     r3, [sp, #12]\n\t"
        "add     sp, #16\n\t"
        "pop     {r4, pc}\n\t"
    );
}
