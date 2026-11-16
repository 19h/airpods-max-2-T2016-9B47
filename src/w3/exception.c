/*
 * exception.c -- W3 crash records, MAC notification and application halt.
 * The firmware retains the path WxBtPhy/firmware/w3/exception.c. RTKit fault
 * capture calls this application bridge; machine entry stays in rtkit/.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/mac.h"

static const char *const rtk_exception_names[RTK_EXC_CLASS_COUNT] = {
    [RTK_EXC_RESET]          = "RESET",
    [RTK_EXC_UNDEF]          = "UNDEF",
    [RTK_EXC_SVC]            = "SVC",
    [RTK_EXC_PREFETCH_ABORT] = "PREFETCH_ABORT",
    [RTK_EXC_DATA_ABORT]     = "DATA_ABORT",
    [RTK_EXC_IRQ]            = "IRQ",
    [RTK_EXC_FIQ]            = "FIQ",
    [RTK_EXC_NMI]            = "NMI",
    [RTK_EXC_HARDFAULT]      = "HARDFAULT",
};

/* ---- panic ------------------------------------------------------------------- */

/* @provenance RVA 0x0220 btp_fault_exception_bridge */
int btp_exception_panic(struct rtk_exception_frame *frame, uint32_t exception_class)
{
    char name[32];
    uint32_t error_code;

    if (exception_class > RTK_EXC_HARDFAULT) {
        error_code = RTK_EXC_ERROR_UNCLASSIFIED;
    } else {
        snprintf(name, sizeof name, "%s", rtk_exception_names[exception_class]);
        error_code = exception_class + RTK_EXC_ERROR_BASE;
    }
    btp_report_exception(error_code, (uint32_t)(uintptr_t)frame,
                         rtk_exception_frame_size(frame), name);
    return 0;
}

/* @provenance RVA 0x4968 btp_assert_or_exception_panic */
int btp_panic(uint32_t error_code, uint32_t detail0, uint32_t detail1,
              int line, const char *file, const char *exception_name)
{
    struct crash_descriptor desc;
    struct crash_record *record;

    clk_enter_panic_mode();
    irq_save();

    snprintf(crash_record->message, sizeof crash_record->message,
             "assert at line %d in %s", line, file);

    desc.error_code = error_code;
    desc.trace_ring = (uint32_t)(uintptr_t)trace_ring_base();
    desc.trace_size = trace_ring_size();

    record = crash_record;
    record->error_code = error_code;
    memcpy(record->image_identity, BTP_PTR(ROM_IMAGE_HEADER), sizeof record->image_identity);

    if (detail0 != 0) {
        record->detail0 = detail0;
        record->detail1 = detail1;
        snprintf(record->message, sizeof record->message, "Exception: %s", exception_name);
    } else {
        detail1 = 0;
        record->detail0 = 0;
        record->detail1 = 0;
        snprintf(record->message, sizeof record->message, "Assert at line %d in %s", line, file);
    }
    desc.detail0 = detail0;
    desc.detail1 = detail1;

    mac_send_crash_notification(&desc);
    return btp_halt_if_fatal(BTP_HALT_FATAL);
}

/* @provenance RVA 0x4A0C btp_report_hardware_exception */
void btp_report_exception(uint32_t error_code, uint32_t detail0, uint32_t detail1,
                          const char *name)
{
    btp_panic(error_code, detail0, detail1, 0x6C, BTP_FILENAME, name);
}

/* @provenance RVA 0x0210 btp_system_halt_on_code5 */
int btp_halt_if_fatal(int code)
{
    if (code == BTP_HALT_FATAL)
        btp_halt();
    return code;
}

/* @provenance RVA 0x0216 btp_system_halt */
void btp_halt(void)
{
    rtk_platform_halt();
}
