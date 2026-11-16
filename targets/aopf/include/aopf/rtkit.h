/*
 * rtkit.h — RTKit boot, MPU/VM, IRQ/NVIC, exception/assert, fastlock,
 * drivers, and AKF mailbox prototypes for aopf.
 */

#ifndef AOPF_RTKIT_H
#define AOPF_RTKIT_H

#include <stdint.h>
#include <stddef.h>
#include "aopf/shared.h"

/* Boot, stack guard & VM/MPU non-overlap verification (src/rtkit/boot.c) */
void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Default_Handler(void);
void SVCall_PendSV_Handler(void);
void rtk_boot_corrupt_halt(void);
void rtk_stack_chk_guard_init(void);
void rtk_stack_chk_fail(void);
int rtk_mpu_perm_is_rw(uint32_t perm_code);
int rtk_mpu_perm_is_readable(uint32_t perm_code);
rtk_vm_entry_t *rtk_vm_entry_alloc(uint32_t base_addr, uint32_t size_bytes);
int rtk_mpu_verify_non_overlap(const rtk_mpu_shadow_entry_t *table, uint32_t count);
int Vm_initAndVerifyNonOverlap(void);
void rtk_idle_wfi_loop(void);
void rtk_start(void);

/* Panic, assert & unimplemented-method traps (src/rtkit/exception.c) */
void rtk_panic(const char *fmt, ...);
void pmgr_invalid_powerstate_panic(void *dev, int bad_state);
void rtk_assert_fail_hex(const char *file_name, uint32_t line_no, uint32_t status_hex);
void rtk_assert_fail(const char *file_name, int line_no, const char *expr);
void rtk_assert_laguna_ok(int line_no, uint64_t val_lhs, uint64_t val_rhs);
void rtk_assert_line_fail(const char *file_name, uint32_t line_no);
void laguna_assert_fail(const char *file_name, uint32_t line_no);
void laguna_assert(const char *file_name, uint32_t line_no, int cond);
uint32_t rtk_unimplemented_method(void);
uint32_t rtk_device_claim_mmio(rtk_dev_hdr_t *dev);
void rtk_fault_entry(uint32_t *exc_frame, uint32_t exc_return);

/* Critical sections, IRQ state & NVIC controller (src/rtkit/irq.c) */
uint32_t rtk_enter_critical(void);
void rtk_exit_critical(uint32_t saved_basepri);
int rtk_in_exception_handler(void);
int rtk_are_irqs_enabled(void);
void rtk_irq_enable_all(void);
int rtk_irq_save_if_thread(void);
void rtk_irq_restore_if_saved(int saved_flag);
void rtk_check_msp_guard(void);
void SysTick_IRQ_Handler(void);
void rtk_svc_pendsv_dispatch(uint32_t *exc_frame, uint32_t exc_return);

uint32_t nvic_get_cpsr(rtk_nvic_dev_t *dev);
int nvic_set_irq_enabled(rtk_nvic_dev_t *dev, uint32_t irq, int enable);
uint32_t nvic_register_irq(rtk_nvic_dev_t *dev, uint32_t irq, void (*fn)(void *), void *ctx);
uint32_t nvic_set_irq_priority(rtk_nvic_dev_t *dev, uint32_t irq, uint32_t prio);
void nvic_dispatch_active_irq(rtk_nvic_dev_t *dev);
int nvic_is_irq_pending(rtk_nvic_dev_t *dev, uint32_t irq);
uint32_t nvic_clear_irq_pending(rtk_nvic_dev_t *dev, uint32_t irq);
uint32_t nvic_trigger_irq(rtk_nvic_dev_t *dev, uint32_t irq);
uint32_t nvic_nop(rtk_nvic_dev_t *dev);
uint32_t nvic_ioctl(rtk_nvic_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
int nvic_is_irq_active(rtk_nvic_dev_t *dev, uint32_t irq);

/* Fastlock, spinlock, stack watermark & priority bitmap (src/rtkit/threading.c) */
uint32_t rtk_spinlock_acquire(volatile uint32_t *lock_word);
void rtk_spinlock_release(volatile uint32_t *lock_word);
uint32_t rtk_fastlock_trylock(volatile uint32_t *lock_word);
uint32_t rtk_fastlock_lock(volatile uint32_t *lock_word);
uint32_t rtk_fastlock_unlock(volatile uint32_t *lock_word);
void rtk_fastlock_unlock_if_zero(volatile uint32_t *lock_word, uint32_t prev_status);
size_t rtk_stack_unused_bytes(const uint32_t *stack_bottom, size_t stack_bytes);
uint32_t rtk_readyq_highest_prio(uint32_t ready_bitmap);

/* SysTick & Timebase / TimerBank drivers (src/drivers/systick.c, timers.c) */
uint64_t rtk_timer_get_us(rtk_timebase_dev_t *tb);
void rtk_delay_us(uint64_t delay_us);
void SysTick_setCallback(rtk_systick_dev_t *dev, void (*cb)(void *), void *ctx);
void SysTick_programReload(rtk_systick_dev_t *dev, uint64_t reload_ticks);
uint32_t SysTick_armTicks(rtk_systick_dev_t *dev, uint64_t ticks);
uint32_t SysTick_armUs(rtk_systick_dev_t *dev, uint64_t delay_us);
uint32_t SysTick_cancel(rtk_systick_dev_t *dev, uint64_t *rem_ticks_out);
uint64_t SysTick_usToTicks(rtk_systick_dev_t *dev, uint64_t delay_us);
uint64_t SysTick_ticksToUs(rtk_systick_dev_t *dev, uint64_t ticks);
void SysTick_isr(rtk_systick_dev_t *dev);
uint32_t SysTick_ioctl(rtk_systick_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);

uint64_t Timebase_readRaw48(rtk_timebase_dev_t *dev);
void Timebase_setEpochDelta(rtk_timebase_dev_t *dev, uint64_t raw_now, uint64_t target_ticks);
uint32_t Timebase_syncOffset(rtk_timebase_dev_t *dev, uint64_t target_ticks);
uint64_t Timebase_getTicks(rtk_timebase_dev_t *dev);
uint64_t Timebase_getTicksWrap(rtk_timebase_dev_t *dev);
uint64_t Timebase_usToTicks(rtk_timebase_dev_t *dev, uint64_t duration_us);
uint64_t Timebase_ticksToUsSlow(rtk_timebase_dev_t *dev, uint64_t ticks);
uint64_t Timebase_ticksToUs(rtk_timebase_dev_t *dev, uint64_t ticks);
uint32_t Timebase_ioctl_impl(rtk_timebase_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
uint32_t Timebase_ioctl(rtk_timebase_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);

uint32_t TimerBank_configChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, uint32_t inc_val, uint32_t cmp_val);
uint32_t TimerBank_enableChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, int periodic);
uint32_t TimerBank_startChannel(rtk_timer_bank_dev_t *dev, uint32_t chan, uint32_t cb_modulo,
                                void (*cb)(void *), void *ctx);
uint32_t TimerBank_stopChannel(rtk_timer_bank_dev_t *dev, uint32_t chan);
uint32_t TimerBank_unimpl28(void);
uint32_t TimerBank_unimpl32(void);
void TimerBank_clearAllIrqs(rtk_timer_bank_dev_t *dev);
void TimerBank_isr(rtk_timer_bank_dev_t *dev);
uint32_t TimerBank_ioctl(rtk_timer_bank_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);

/* Fuse, PMGR, WDT & AKF Mailbox drivers (src/drivers/platform.c, src/rtkit/ipc.c) */
uint32_t Fuse_readWords(rtk_fuse_dev_t *dev, uint32_t addr_cmd, uint32_t *out_words, uint32_t *io_count);
uint32_t Fuse_ioctl(rtk_fuse_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
uint32_t Fuse_readOTP(uint32_t *out_word);

uint32_t PMGR_lookupRegOffset(rtk_pmgr_dev_t *dev, uint32_t dev_id, uint32_t idx, uint32_t out_pair[2]);
uint32_t PMGR_ioctl(rtk_pmgr_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);
uint32_t PMGR_getPowerState(rtk_pmgr_dev_t *dev, uint32_t dev_id, uint32_t *out_state);

uint32_t WDT_setRunning(rtk_wdt_dev_t *dev, int running);
uint32_t WDT_kick(rtk_wdt_dev_t *dev);
uint32_t WDT_updateBarkPrescaler(rtk_wdt_dev_t *dev, uint32_t prescaler_idx);
uint32_t WDT_setBarkTimeoutUs(rtk_wdt_dev_t *dev, uint64_t bark_us);
uint64_t WDT_getBarkTimeoutUs(rtk_wdt_dev_t *dev);
uint32_t WDT_setBiteEnable(rtk_wdt_dev_t *dev, int enable);
uint32_t WDT_setBiteTimeoutUs(rtk_wdt_dev_t *dev, uint64_t bite_us);
uint64_t WDT_getBiteTimeoutUs(rtk_wdt_dev_t *dev);
uint32_t WDT_setCallback(rtk_wdt_dev_t *dev, void (*cb)(void *), void *ctx);
uint32_t WDT_ioctl(rtk_wdt_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);

uint32_t Mbox_unimpl04(void);
uint32_t Mbox_setIrqHandler(rtk_mbox_dev_t *dev, void (*cb)(void *), void *ctx);
void Mbox_read128(uintptr_t mmio_base, uint32_t fifo_off, rtk_mbox_msg128_t *out_msg);
void Mbox_armRxEmptyIrq(rtk_mbox_dev_t *dev);
uint32_t Mbox_popOne(rtk_mbox_dev_t *dev, rtk_mbox_msg128_t *out_msg);
uint32_t Mbox_tryReadOne(rtk_mbox_dev_t *dev, rtk_mbox_msg128_t *out_msg);
void Mbox_checkError(rtk_mbox_dev_t *dev, const char *op_name);
uint32_t Mbox_ioctl(rtk_mbox_dev_t *dev, uint32_t cmd, void *arg0, void *arg1);

#endif /* AOPF_RTKIT_H */
