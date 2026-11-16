/*
 * exception.c — RTKit & Laguna assertion, panic, fault reporting, and common
 * device MMIO claim helpers reconstructed from aopf.i64.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/rtkit.h"

#if !defined(__arm__) && !defined(__thumb__)
void aopf_host_record_panic(const char *msg);
#endif

/*
 * Link-boundary panic sink modelling rtk_panic (RVA 0x20330; tracked as
 * pending_c in ledger.json until crashlog serializer sub_5B078 is reconstructed).
 */
void rtk_panic(const char *fmt, ...)
{
    char panic_buf[160];
    (void)rtk_irq_save_if_thread();
    va_list ap;
    va_start(ap, fmt);
    (void)rtk_vsnprintf(panic_buf, sizeof(panic_buf),
                        (fmt != NULL) ? fmt : "panic", ap);
    va_end(ap);
    (void)rtk_printf("%s\n", panic_buf);

#if defined(__arm__) || defined(__thumb__)
    for (;;) {
        aopf_wfi();
    }
#else
    aopf_host_record_panic(panic_buf);
#endif
}

/* @shim RVA 0x2031c pmgr_invalid_powerstate_panic */
void pmgr_invalid_powerstate_panic(void *dev, int bad_state)
{
    (void)dev;
    rtk_panic("PMGR: Invalid AON powerstate %d", bad_state);
}

/* @shim RVA 0x1daa8 rtk_assert_fail_hex */
void rtk_assert_fail_hex(const char *file_name, uint32_t line_no, uint32_t status_hex)
{
    const void *caller_lr = __builtin_return_address(0);
    rtk_panic("[%s:%d]%s<%p> %#lx",
              (file_name != NULL) ? file_name : "?",
              (int)line_no,
              " assert: ",
              caller_lr,
              (unsigned long)status_hex);
}

/* @shim RVA 0x4f3f8 rtk_assert_fail */
void rtk_assert_fail(const char *file_name, int line_no, const char *expr)
{
    rtk_panic("ASSERTION FAILED: %s:%d\n\t%s\n",
              (file_name != NULL) ? file_name : "?",
              line_no,
              (expr != NULL) ? expr : "?");
}

/* @shim RVA 0x4f40c rtk_assert_laguna_ok */
void rtk_assert_laguna_ok(int line_no, uint64_t val_lhs, uint64_t val_rhs)
{
    rtk_panic("ASSERTION FAILED: %s:%d: \n\t%s, %llx, %llx\n",
              "laguna_service_t2016.c",
              line_no,
              "status == RTK_ST_OK",
              (unsigned long long)val_lhs,
              (unsigned long long)val_rhs);
}

/* @shim RVA 0x60380 rtk_assert_line_fail */
void rtk_assert_line_fail(const char *file_name, uint32_t line_no)
{
    rtk_assert_fail_hex(file_name, line_no, 0u);
}

/* @shim RVA 0x35464 laguna_assert_fail */
void laguna_assert_fail(const char *file_name, uint32_t line_no)
{
    rtk_panic("%s : %lu",
              (file_name != NULL) ? file_name : "?",
              (unsigned long)line_no);
}

/* @shim RVA 0x35460 laguna_assert */
void laguna_assert(const char *file_name, uint32_t line_no, int cond)
{
    if (cond == 0) {
        laguna_assert_fail(file_name, line_no);
    }
}

/* @shim RVA 0x514f8 rtk_unimplemented_method */
uint32_t rtk_unimplemented_method(void)
{
    rtk_panic("!Mthd:%p", __builtin_return_address(0));
    return RTK_ERR_UNSUPPORTED;
}

/* @provenance RVA 0x51508 rtk_device_claim_mmio */
uint32_t rtk_device_claim_mmio(rtk_dev_hdr_t *dev)
{
    uint8_t flags = dev->claimed_flags;
    if ((flags & 1u) == 0u) {
        uintptr_t lo = dev->mmio_base;
        uint32_t hi = dev->mmio_hi;
        if ((lo | (uintptr_t)hi) != 0u) {
            if (lo == 0u) {
                return RTK_ERR_NO_MMIO;
            }
            dev->mmio_base = lo;
        }
    }
    dev->claimed_flags = (uint8_t)(flags | 1u);
    return RTK_OK;
}

void rtk_fault_entry(uint32_t *exc_frame, uint32_t exc_return)
{
    uint32_t cfsr = bus_read32(SCB_CFSR_ADDR);
    uint32_t hfsr = bus_read32(SCB_HFSR_ADDR);
    uint32_t mmfar = bus_read32(SCB_MMFAR_ADDR);
    uint32_t bfar = bus_read32(SCB_BFAR_ADDR);
    uint32_t pc_val = (exc_frame != NULL) ? exc_frame[6] : 0u;
    rtk_panic("ARM fault pc=0x%x lr=0x%x cfsr=0x%x hfsr=0x%x mmfar=0x%x bfar=0x%x",
              pc_val, exc_return, cfsr, hfsr, mmfar, bfar);
}
