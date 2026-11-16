#ifndef RKOF_RTKIT_H
#define RKOF_RTKIT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "rkof/bus.h"

/* Architectural vector entry symbols (retained_native veneers) */
void Reset_Handler(void);
void NMI_Handler(void);
void Fault_Handler(void);
void SVCall_Handler(void);
void Default_Handler(void);

/* Boot, Remap, Canary, & Early Init (src/rtkit/boot.c) */
void rtkit_entry(void);
void __stack_chk_init(void);
RKOF_NORETURN void __stack_chk_fail(void);
void rtkit_start(void);
uint32_t sub_175C8(void);
void sub_175D4(void *stack_base, uint32_t byte_len);

/* Exceptions, Fault Capture, Assertions, & Panic (src/rtkit/exception.c) */
RKOF_NORETURN void sub_1748C(uint8_t *exc_frame, uint32_t exc_num);
uintptr_t sub_17970(uintptr_t lr_addr);
RKOF_NORETURN void rtkit_panic(const char *fmt, ...);
void sub_179C8(const char *fmt, va_list ap);
RKOF_NORETURN void rtkit_halt_forever(uint32_t reason);
void sub_17BD4(uint32_t magic_fourcc);
uint32_t sub_17BF8(const uint32_t *be_word_ptr);
RKOF_NORETURN void rtkit_assert_fail(const char *file, uint32_t line, const char *func);
RKOF_NORETURN void rtkit_assert_fail_hex64(
    const char *file,
    uint32_t line,
    const char *func,
    uint64_t val_a,
    uint64_t val_b
);
void sub_1887C4(void);
RKOF_NORETURN void rtkit_unimplemented_method(const char *method_name);

/* Critical Sections, NVIC, IRQ Table, & Spinlocks (src/rtkit/irq.c) */
uint32_t rtkit_enter_critical(void);
void sub_16F20(uint32_t saved_basepri);
uint32_t sub_16F26(void);
void sub_16F38(void);
uint32_t sub_16F44(void);
RKOF_NORETURN void sub_17024(void);
uint32_t sub_17064(uint32_t bit_seed, uint32_t exc_num, uint32_t enable);
uint32_t sub_170C8(void *unused_ctx, uint32_t exc_num, uintptr_t handler_fn, uintptr_t handler_arg);
uint32_t sub_1710C(void *unused_ctx, uint32_t exc_num, uint32_t logical_prio);
uint32_t sub_175F0(uint32_t *lock_word);
void sub_1760C(uint32_t *lock_word);

/* Cooperative / Priority Thread Switch (src/rtkit/sched.c) */
uintptr_t rtkit_sched_switch(uintptr_t current_sp);
uint32_t sub_1A2994(uint8_t *thread_obj);

/* SysTick & High-Resolution Timer Helpers (src/rtkit/systick.c) */
struct rkof_systick_dev {
    uint8_t  pad_00[0x18];
    uint32_t freq_hz;               /* +0x18 */
    uint8_t  ext_clk_flag;          /* +0x1C */
    uint8_t  pad_1d[0x0B];
    uint32_t ticks_per_100ns;       /* +0x28 */
    uint32_t ticks_per_us;          /* +0x2C */
    uint32_t rem_ticks_lo;          /* +0x30 */
    uint32_t rem_ticks_hi;          /* +0x34 */
    uint8_t  oneshot_mode;          /* +0x38 */
};

uint32_t sub_18A494(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t reload_ticks);
uint32_t sub_18A4D0(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t period_ticks);
uint32_t sub_18A510(struct rkof_systick_dev *dev, uint64_t *remaining_out);
uint64_t sub_18A568(const struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t ticks);
uint64_t sub_18A59C(const struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t duration_us);
void sub_18A5E0(struct rkof_systick_dev *dev, uint32_t unused_r1, uint64_t ticks);
void sub_18A134(uint64_t delay_ticks);

/* Heap Wrappers, Shell Registration, & Spicy Logging (src/rtkit/services.c) */
typedef int (*rkof_shell_cmd_fn)(void *ctx, const char *const *argv);

void sub_16D20(void *heap, void *ptr);
uint32_t sub_16D26(const uint8_t *heap);
void sub_16D46(uint8_t *heap, uint32_t enable);
uint32_t rtkit_shell_register_cmd(
    void *shell_ctx,
    const char *name,
    const char *help,
    rkof_shell_cmd_fn handler
);
void spicy_log_emit(uint32_t log_id, ...);
void spicy_log_emit_ex(uint32_t level, uint32_t log_id, ...);
uint32_t rtkit_heap_lock_or_init(void **heap_out);
void rtkit_heap_unlock(uint32_t lock_token);
void *rt_malloc(size_t size);
void rt_free(void *ptr);

#endif /* RKOF_RTKIT_H */
