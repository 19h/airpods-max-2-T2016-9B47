/*
 * trace.h -- RTKit trace ring, error logging and the debug console.
 */
#ifndef BTP_TRACE_H
#define BTP_TRACE_H

#include "btp.h"

/* Record types in the trace ring. */
enum trace_type {
    TRACE_TYPE_INIT     = 1,    /* subtype: enum trace_init_stage */
    TRACE_TYPE_ISR      = 2,    /* subtype 5, arg0: enum trace_isr */
    TRACE_TYPE_ERROR    = 4,    /* subtype: error class, arg0/arg1: details */
};

enum trace_init_stage {
    TRACE_INIT_EARLY_HW = 0,
    TRACE_INIT_IRQ      = 1,
    TRACE_INIT_RX       = 2,
    TRACE_INIT_TX       = 3,
};

#define TRACE_ISR_SUBTYPE           5
enum trace_isr {
    TRACE_ISR_TX_SETUP  = 1,
    TRACE_ISR_TX_DONE   = 2,
    TRACE_ISR_RX_SETUP  = 3,
    TRACE_ISR_RX_DONE   = 4,
};

/* Error classes (trace_record.subtype for TRACE_TYPE_ERROR). */
#define TRACE_ERR_CLASS_COMMAND     7
#define TRACE_ERR_CLASS_INTERNAL    7   /* same class is used for the null-payload sentinel */

/* Error identifiers (trace_record.arg1). */
enum trace_error_id {
    TRACE_ERR_IPC_UNKNOWN_EVENT     = 0x01,
    TRACE_ERR_IPC_UNKNOWN_COMMAND   = 0x02,
    TRACE_ERR_IPC_SHORT_REQUEST     = 0x03,
    TRACE_ERR_IPC_UNKNOWN_SUBCMD    = 0x04,
    TRACE_ERR_TXPWR_TABLE_UPDATE    = 0x12,
    TRACE_ERR_LOOPBACK_BAD_LENGTH   = 0x1E,
    TRACE_ERR_LOOPBACK_STATS_ARGS   = 0x21,
    TRACE_ERR_LOOPBACK_INACTIVE     = 0x22,
    TRACE_ERR_NVV_UNKNOWN_MODULE    = 0x26,
    TRACE_ERR_TELEMETRY_ARGS        = 0x27,
    TRACE_ERR_VERSION_ARGS          = 0x28,
    TRACE_ERR_DIAG_ARGS             = 0x29,
};

/*
 * Trace-site tokens.  In the original build each trace point names a site
 * object in a discarded section; the token stored in the ring is that object's
 * offset from the end of the RAM image.  The recovered values are kept.
 */
enum trace_site {
    TRACE_SITE_MAC_ISR_ENTER        = 0x000,
    TRACE_SITE_MAC_ISR_EXIT         = 0x020,
    TRACE_SITE_IRQ_INIT_ENTER       = 0x03E,
    TRACE_SITE_IRQ_INIT_EXIT        = 0x055,
    TRACE_SITE_RX_SETUP_ENTER       = 0x06A,
    TRACE_SITE_RX_SETUP_EXIT        = 0x087,
    TRACE_SITE_RX_DONE              = 0x0A2,
    TRACE_SITE_RX_INIT_ENTER        = 0x0BE,
    TRACE_SITE_RX_INIT_EXIT         = 0x0CE,
    TRACE_SITE_TX_SETUP_ENTER       = 0x0DC,
    TRACE_SITE_TX_SETUP_EXIT        = 0x0F9,
    TRACE_SITE_TX_DONE              = 0x114,
    TRACE_SITE_TX_INIT_ENTER        = 0x12A,
    TRACE_SITE_TX_INIT_EXIT         = 0x13A,
    TRACE_SITE_ERROR                = 0x148,
    TRACE_SITE_EARLY_HW_INIT_ENTER  = 0x154,
    TRACE_SITE_EARLY_HW_INIT_EXIT   = 0x168,
};

/* What a trace point hands to trace_log(); `site` is supplied separately. */
struct trace_event {
    uint32_t type;
    uint32_t subtype;
    uint32_t start_ts;
    uint32_t end_ts;
    uint32_t arg0;
    uint32_t arg1;
};

void               trace_log(enum trace_site site, const struct trace_event *event);
void               trace_log_error(uint32_t error_class, uint32_t arg, uint32_t error_id);
struct trace_ring *trace_ring_base(void);
uint32_t           trace_ring_size(void);

/* Convenience for the "enter / exit" pairs every init routine and ISR emits. */
static inline void trace_begin(struct trace_event *ev, uint32_t type,
                               uint32_t subtype, uint32_t arg0, uint32_t now)
{
    ev->type = type;
    ev->subtype = subtype;
    ev->start_ts = now;
    ev->end_ts = 0;
    ev->arg0 = arg0;
    ev->arg1 = 0;
}

/* Debug console.  The sink is compiled out in this image but the calls, and
 * their arguments, are evaluated exactly as in the original. */
int dbg_printf(const char *format, ...);

/* Fixed-point "%d.%02d" formatting into a ring of eight static slots. */
#define FMT_SLOT_COUNT              8
#define FMT_SLOT_SIZE               16

char    *fmt_fixed_point(int slot, int fraction, int next_slot, int whole);
uint32_t fmt_take_slot(uint32_t *next_slot);
int   roundf_half_up(float value);

#endif /* BTP_TRACE_H */
