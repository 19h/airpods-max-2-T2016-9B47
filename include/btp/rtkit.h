/*
 * rtkit.h -- the RTKit runtime layer: boot, interrupt control, scheduler
 * hooks, exception handling and panic.
 */
#ifndef BTP_RTKIT_H
#define BTP_RTKIT_H

#include "btp.h"

/* ---- build metadata ------------------------------------------------------- */

/* FourCC tags of the metadata records appended to .data. */
#define RTK_META_TAG(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

struct rtk_metadata_u32 {
    uint32_t tag;
    uint32_t length;        /* 4 */
    uint32_t value;
};

struct __attribute__((packed)) rtk_metadata_u8 {
    uint32_t tag;
    uint32_t length;        /* 1 */
    uint8_t  value;
};

struct __attribute__((packed)) rtk_metadata_slid {
    uint32_t tag;
    uint32_t length;
    uint8_t value[8];
};

struct __attribute__((packed)) rtk_image_metadata {
    struct rtk_metadata_u32 soc, soc_rev, fpga, stack_guard;
    struct rtk_metadata_u32 ram_size, ram_start, ram_end, text_size;
    struct rtk_metadata_u8 data_reload;
    struct rtk_metadata_slid slid;
};

/* Records read back at boot; the others are informational. */
extern struct rtk_image_metadata rtk_metadata;
#define rtk_meta_stack_guard (rtk_metadata.stack_guard)
#define rtk_meta_data_reload (rtk_metadata.data_reload)
extern uint32_t __stack_chk_guard;

/* ---- interrupt priorities --------------------------------------------------- */

#define RTK_BASEPRI_LEVEL_DEFAULT   0x80u
#define RTK_IRQ_PRIORITY_DEFAULT    4u
#define RTK_ERR_INVALID_PRIORITY    0x80000001u

struct rtk_irq_state {
    uint32_t basepri_level;         /* value programmed into BASEPRI */
    uint32_t priority_level;        /* argument of the last successful config */
};
extern struct rtk_irq_state rtk_irq;

/* ---- exception bookkeeping ---------------------------------------------------- */

/* RTKit's architecture-independent exception vocabulary. */
enum rtk_exception_class {
    RTK_EXC_RESET           = 0,
    RTK_EXC_UNDEF           = 1,
    RTK_EXC_SVC             = 2,
    RTK_EXC_PREFETCH_ABORT  = 3,
    RTK_EXC_DATA_ABORT      = 4,
    RTK_EXC_IRQ             = 5,
    RTK_EXC_FIQ             = 6,
    RTK_EXC_NMI             = 7,
    RTK_EXC_HARDFAULT       = 8,
    RTK_EXC_UNKNOWN         = 9,
    RTK_EXC_CLASS_COUNT     = 9,    /* classes with a name */
};

/* error_code reported for an exception: class + RTK_EXC_ERROR_BASE. */
#define RTK_EXC_ERROR_BASE          0x1E
#define RTK_EXC_ERROR_UNCLASSIFIED  0x27

/*
 * Frame built by the exception entry veneer (CONSIDERATIONS.md section 3.4).
 * The hardware frame follows `context`; `frame_size` below is 0x60 for a
 * basic frame and 0xE8 when the FPU registers were stacked.
 */
struct rtk_exception_frame {
    uint32_t cfsr;              /* +0x00, HardFault only */
    uint32_t control;           /* +0x04 */
    uint32_t masks;             /* +0x08, BASEPRI<<8 | FAULTMASK<<1 | PRIMASK */
    uint32_t exception_number;  /* +0x0C */
    uint32_t status;            /* +0x10, HFSR / CFSR / DFSR by fault type */
    uint32_t fault_address;     /* +0x14, MMFAR / BFAR */
    uint32_t exc_return;        /* +0x18 */
    uint32_t caller_sp;         /* +0x1C, SP at the time of the exception */
    uint32_t context[];         /* R4-R11, S16-S31 if present, then the HW frame */
};

#define RTK_FRAME_SIZE_BASIC        0x60u
#define RTK_FRAME_SIZE_EXTENDED     0xE8u

struct rtk_state {
    uint32_t reserved0[2];
    uint8_t  resched_pending;   /* set by the scheduler, consumed on PendSV */
    uint8_t  reserved1[3];
    uint32_t fault_count;
    uint32_t reserved2;
    uint32_t exception_class;   /* enum rtk_exception_class */
    uint32_t exception_frame;   /* struct rtk_exception_frame * */
    uint32_t exception_frame_size;
};
extern struct rtk_state rtk;

struct rtk_mpu_region {
    uint32_t rbar;
    uint32_t rasr;
    uint32_t reserved[4];
};

struct rtk_mpu_state {
    uint32_t region_count;
    uint32_t min_region_size;
    uint32_t region_mask;
    uint32_t reserved_0c;
    struct rtk_mpu_region regions[16];
};
extern struct rtk_mpu_state rtk_mpu;

/* ---- boot (rtkit/boot.c) ------------------------------------------------------ */

void Reset_Handler(void);
void rtk_init_stacks(void);
void rtk_boot(void);
void rtk_cold_boot(void);
void rtk_warm_boot(void);
void rtk_stack_guard_init(void);
void __stack_chk_fail(void);
int  rtk_platform_late_init(void);

/* ---- interrupts and idle (rtkit/irq.c) -------------------------------------- */

uint32_t rtk_set_irq_priority_level(uint32_t level);
uint32_t rtk_configure_irq_priority(uint32_t level);
uint32_t nvic_enable_irq(unsigned irq);
uint32_t nvic_disable_irq(unsigned irq);
uint32_t nvic_clear_pending_irq(unsigned irq);
bool     irq_save(void);
bool     irq_restore(bool enabled);
void     irq_enable(void);
uint32_t rtk_cpu_idle(void);
uint32_t rtk_platform_idle(uint32_t unused);
void     rtk_cpu_halt(void);
void     rtk_platform_halt(void);
void     rtk_abort(void);
void     rtk_assert_failed(void);

/* ---- scheduler hooks (rtkit/threading.c) ------------------------------------------ */

void    *rtk_clear_context_targets(void *save_target_out, void *restore_target_out);
uint8_t *rtk_pendsv_switch(uint8_t *sp, int unused, uint8_t **switch_to,
                           uint32_t *save_to);
uint32_t rtk_stack_check(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);

/* ---- exceptions and panic (rtkit/exception.c) ---------------------------- */

void     Fault_Handler(uint32_t exc_return);
void     SVC_Handler(uint32_t exc_return);
void     rtk_exception_entry(uint32_t exc_return);
void     rtk_svcall_entry(uint32_t exc_return);
uint32_t rtk_exception_frame_size(const struct rtk_exception_frame *frame);
void     rtk_handle_exception(struct rtk_exception_frame *frame, uint32_t exception_number);
int      btp_exception_panic(struct rtk_exception_frame *frame, uint32_t exception_class);
void     btp_report_exception(uint32_t error_code, uint32_t detail0, uint32_t detail1,
                              const char *name);
int      btp_panic(uint32_t error_code, uint32_t detail0, uint32_t detail1,
                   int line, const char *file, const char *exception_name);
int      btp_halt_if_fatal(int code);
void     btp_halt(void);

/* Assert with the line number of the original source file. */
#define btp_panic_at(line, code) \
    btp_panic((code), 0, 0, (line), BTP_FILENAME, NULL)

/* Previous source spellings; descriptive names above are canonical. */
#define rtk_sched_take_switch_request rtk_clear_context_targets

#endif /* BTP_RTKIT_H */
