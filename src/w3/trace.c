/*
 * trace.c -- RTKit trace ring, error events and the debug console.
 *
 * The ring holds the last 18 events; the MAC can freeze it (diag_set_freeze)
 * and read it back as diagnostic buffer 0.  Each event records a start/end
 * SysTick stamp, two arguments and a token identifying the trace point.
 */
#include "btp/btp.h"
#include "btp/trace.h"
#include "btp/clock.h"
#include "btp/rtkit.h"
#include "btp/status.h"

struct fmt_state {
    char     slots[FMT_SLOT_COUNT][FMT_SLOT_SIZE];
    uint32_t next_slot;
};

static struct fmt_state fmt BTP_STATE(format);

/* .data pointer cell: the ring lives in the low RAM shared with the host. */
struct trace_ring *volatile trace_ring BTP_DATA(trace_ring) = (struct trace_ring *)TRACE_RING_BASE;

/* Use the image's 32-bit address arithmetic, including wrapped NVV-provided
 * cursor values, rather than an out-of-bounds C array subscript. */
static volatile uint32_t *trace_record_word(volatile struct trace_ring *ring,
                                          uint32_t index, unsigned word)
{
    uint32_t offset = 4u + 28u * index + 4u * word;

#ifdef BTP_HOST
    return (volatile uint32_t *)((volatile uint8_t *)ring + offset);
#else
    return BTP_PTR((uint32_t)(uintptr_t)ring + offset);
#endif
}

/* @provenance RVA 0x0314 btp_trace_log_entry */
/* @provenance RVA 0x0406 _btp_trace_outlined_current_record_base */
/* @provenance RVA 0x0412 _btp_trace_outlined_indexed_record_base */
void trace_log(enum trace_site site, const struct trace_event *event)
{
    volatile struct trace_ring *ring;
    bool irqs = irq_save();
    uint32_t freeze_mask = *(volatile uint32_t *)&diag.freeze_mask;

    irq_restore(irqs);
    if (freeze_mask & BIT(DIAG_BUFFER_TRACE))
        return;

    if (event) {
        const volatile struct trace_event *payload = event;

        ring = trace_ring;
        uint32_t index = ring->next;
        /* The first four event words are sampled before the first store. */
        uint32_t type = payload->type;
        uint32_t subtype = payload->subtype;
        uint32_t start = payload->start_ts;
        uint32_t end = payload->end_ts;

        *trace_record_word(ring, index, 0) = type;
        *trace_record_word(ring, ring->next, 1) = subtype;
        *trace_record_word(ring, ring->next, 2) = start;
        *trace_record_word(ring, ring->next, 3) = end;
        index = ring->next;
        uint32_t arg = payload->arg0;

        *trace_record_word(ring, index, 4) = arg;
        index = ring->next;
        arg = payload->arg1;
        *trace_record_word(ring, index, 5) = arg;
    } else {
        (void)systick_now();
        ring = trace_ring;
        *trace_record_word(ring, ring->next, 0) = TRACE_TYPE_ERROR;
        *trace_record_word(ring, ring->next, 1) = TRACE_ERR_CLASS_INTERNAL;
        *trace_record_word(ring, ring->next, 2) = 0;
        *trace_record_word(ring, ring->next, 3) = 0;
        *trace_record_word(ring, ring->next, 4) = 0;
        *trace_record_word(ring, ring->next, 5) = 0;
        site = 0;
    }
    *trace_record_word(ring, ring->next, 6) = (uint32_t)site;
    ring->next = (ring->next + 1u) % TRACE_RING_ENTRIES;
}

/* @provenance RVA 0x03F4 btp_trace_get_buffer */
struct trace_ring *trace_ring_base(void)
{
    return trace_ring;
}

/* @provenance RVA 0x0400 btp_trace_get_buffer_size */
uint32_t trace_ring_size(void)
{
    return offsetof(struct trace_ring, reserved);
}

/* @provenance RVA 0x4A4C btp_trace_log_error */
void trace_log_error(uint32_t error_class, uint32_t arg, uint32_t error_id)
{
    struct trace_event ev;
    uint32_t now = systick_now();

    ev.type = TRACE_TYPE_ERROR;
    ev.subtype = error_class;
    ev.start_ts = now;
    ev.end_ts = now;
    ev.arg0 = arg;
    ev.arg1 = error_id;
    trace_log(TRACE_SITE_ERROR, &ev);
}

/* @provenance RVA 0x041C btp_printf_stub */
int dbg_printf(const char *format, ...)
{
    /* Console output is compiled out of this build. */
    return 0;
}

/* @provenance RVA 0x4AB6 btp_format_fixed_point_slot */
char *fmt_fixed_point(int slot, int fraction, int next_slot, int whole)
{
    char *buf = fmt.slots[slot];

    snprintf(buf, FMT_SLOT_SIZE - 1, "%d.%02d", whole, fraction);
    return buf;
}

/* Allocate the next formatting slot; the caller formats into it. */
uint32_t fmt_take_slot(uint32_t *next_slot)
{
    uint32_t slot = fmt.next_slot;

    fmt.next_slot = (fmt.next_slot + 1) & (FMT_SLOT_COUNT - 1);
    *next_slot = fmt.next_slot;
    return slot;
}

/* @provenance RVA 0x4B54 btp_roundf_positive */
#ifndef BTP_HOST
__attribute__((naked))
int roundf_half_up(float value)
{
    /* Keep VCVT's architectural NaN/overflow behavior and FPSCR effects. */
    __asm__ volatile(
        "vcvt.s32.f32 s2, s0\n"
        "vmov r0, s2\n"
        "vcvt.f32.s32 s2, s2\n"
        "vsub.f32 s0, s0, s2\n"
        "vmov.f32 s2, #0.5\n"
        "vcmp.f32 s0, s2\n"
        "vmrs apsr_nzcv, fpscr\n"
        "it ge\n"
        "addge r0, #1\n"
        "bx lr\n"
    );
}
#else
int roundf_half_up(float value)
{
    int whole = (int)value;

    if (value - (float)whole >= 0.5f)
        ++whole;
    return whole;
}

#endif
