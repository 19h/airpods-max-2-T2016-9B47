/*
 * dpd.c -- digital pre-distortion experiments.
 *
 * The DPD engine exposes a sample RAM and a "GTB" counter that advances with
 * the gain-table base.  The host can ask, through MAC_SUB_DPD, for the
 * actuation table to be loaded at a given GTB count, for a test pattern to be
 * pushed through the RAM, or for a capture of the RAM to be taken and either
 * handed back by pointer or dumped on the debug console.  All of that happens
 * from the TX setup interrupt, so a request only takes effect on the next
 * packet.
 */
#include "btp/btp.h"
#include "btp/dpd.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/rtkit.h"

struct dpd_state dpd BTP_STATE(dpd);

#define DPD_RESULT_WORDS_CAPTURE    DPD_CAPTURE_WORDS       /* 0x2F0 */
#define DPD_RESULT_WORDS_TEST       DPD_TEST_WORDS          /* 0x1E0 */
#define DPD_TEST_FILL_BYTE          0x55

/* Bulk copies to and from the sample RAM go through plain pointers. */
#define DPD_RAM_PTR                 BTP_PTR(DPD_RAM_BASE)
#define DPD_ACTUATION_RAM_PTR       BTP_PTR(DPD_ACTUATION_RAM_BASE)

/* Print `count` words, four per line, each with `format`. */
static void dpd_dump_words(const volatile uint32_t *words, unsigned count, const char *format)
{
    for (unsigned i = 0; i < count; ++i) {
        if ((i & 3u) == 0)
            dbg_printf("\n");
        dbg_printf(format, words[i]);
    }
}

static void dpd_set_mode(uint32_t mode)
{
    DPD_CTRL = (DPD_CTRL & ~RFIELD_MASK(DPD_CTRL_MODE)) | FIELD_PREP(DPD_CTRL_MODE, mode);
}

/* Copy the sample RAM into the capture buffer and report GTB / timing. */
static void dpd_capture_ram(void)
{
    uint32_t err = DPD_STATUS & 0xFFFFu;
    uint32_t gtb_before = DPD_GTB;
    uint32_t t_start = systick_now();
    uint32_t t_end, gtb_after;

    memcpy(dpd.capture, DPD_RAM_PTR, sizeof dpd.capture);
    t_end = systick_now();
    gtb_after = DPD_GTB;
    dbg_printf(">>>>>> (err=%d gtb=%d>%d)\n", FIELD_GET(DPD_STATUS_CPU_ACCESS_ERR, err),
               gtb_before, gtb_after);
    dbg_printf("TS=%d %d %d\n", t_start, t_end, t_start - t_end);
}

void dpd_set_tables(const void *table, uint32_t length)
{
    const uint8_t *bytes = table;

    switch (length) {
    case DPD_TABLE_FULL_BYTES:
        dpd.actuation_table = bytes;
        dpd.test_table = bytes + DPD_TABLE_ACTUATION_BYTES;
        break;
    case DPD_TABLE_TEST_BYTES:
        dpd.test_table = bytes;
        break;
    case DPD_TABLE_ACTUATION_BYTES:
        dpd.actuation_table = bytes;
        break;
    default:
        dbg_printf("## DPD bad len %d!\n", length);
        break;
    }
}

/* Actuation: load the table into the actuation window once the engine runs,
 * then optionally report the RAM contents at a later GTB count. */
static void dpd_run_actuation(void)
{
    while (DPD_GTB == 0)
        ;
    DPD_CTRL = (DPD_CTRL & ~(RFIELD_MASK(DPD_CTRL_MODE) | DPD_CTRL_EN)) | DPD_CTRL_EN;
    memcpy(DPD_ACTUATION_RAM_PTR, dpd.actuation_table, DPD_TABLE_ACTUATION_BYTES);

    if (dpd.actuation_gtb == 0)
        return;
    while (DPD_GTB < dpd.actuation_gtb)
        ;
    dbg_printf("Act GTB=%d:\n", DPD_GTB);
    if (dpd.capture_gtb != 0)
        return;

    dpd.actuation_gtb = 0;
    if (dpd.result_armed) {
        dpd.result_len = DPD_RESULT_WORDS_CAPTURE;
        dpd.result_ptr = (uint32_t)(uintptr_t)dpd.capture;
        memcpy(dpd.capture, DPD_RAM_PTR, sizeof dpd.capture);
    } else {
        dpd_dump_words(DPD_ACTUATION_RAM, DPD_ACTUATION_WORDS, " %d,");
        dbg_printf("\n");
    }
}

/* Test: push a known pattern through the RAM and read it back. */
static void dpd_run_test(void)
{
    uint32_t err;

    dpd.test_enable = 0;
    dbg_printf("GTB DPD test %d (%d)\n", dpd.test_gtb, DPD_GTB);
    while (DPD_GTB < dpd.test_gtb)
        ;
    err = FIELD_GET(DPD_STATUS_CPU_ACCESS_ERR, DPD_STATUS);
    if (dpd.test_table != NULL)
        memcpy(DPD_RAM_PTR, dpd.test_table, DPD_TABLE_TEST_BYTES);
    else
        memset(DPD_RAM_PTR, DPD_TEST_FILL_BYTE, DPD_TABLE_TEST_BYTES);
    memcpy(dpd.capture, DPD_RAM_PTR, DPD_TABLE_TEST_BYTES);
    dbg_printf(">>>>>> (err=%d gtb=%d)\n", err, DPD_GTB);

    if (dpd.result_armed) {
        dpd.result_len = DPD_RESULT_WORDS_TEST;
        dpd.result_ptr = (uint32_t)(uintptr_t)dpd.capture;
    } else {
        dpd_dump_words(dpd.capture, DPD_RESULT_WORDS_TEST, " %08x,");
        dbg_printf("\n<<<<<< (err=%d)\n", FIELD_GET(DPD_STATUS_CPU_ACCESS_ERR, DPD_STATUS));
    }
}

/* Capture: switch to the requested mode at the GTB count and snapshot the RAM. */
static void dpd_run_capture(void)
{
    dbg_printf("GTB DPD %d (%d)\n", dpd.capture_gtb, DPD_GTB);
    while (DPD_GTB < dpd.capture_gtb)
        ;
    dpd_set_mode(dpd.mode);
    dpd_capture_ram();

    if (dpd.result_armed) {
        dpd.result_len = DPD_RESULT_WORDS_CAPTURE;
        dpd.result_ptr = (uint32_t)(uintptr_t)dpd.capture;
    }
    dpd_dump_words(dpd.capture, DPD_RESULT_WORDS_CAPTURE, " %d,");
    dbg_printf("\n<<<<<< (err=%d)\n", FIELD_GET(DPD_STATUS_CPU_ACCESS_ERR, DPD_STATUS));
    dpd.capture_gtb = 0;
}

/* Called from tx_setup_isr() once the packet has been programmed. */
void dpd_on_tx_setup(void)
{
    if (dpd.actuation_enable && dpd.actuation_table != NULL)
        dpd_run_actuation();
    if (dpd.test_enable)
        dpd_run_test();
    if (dpd.capture_gtb)
        dpd_run_capture();
}

/* @provenance RVA 0x3598 dpd_capture_done_isr */
void dpd_capture_done_isr(void)
{
    nvic_disable_irq(IRQ_DPD_CAPTURE);
    DPD_IRQ = DPD_IRQ_CAPTURE_DONE;
    if (dpd.capture_gtb == 0)
        dpd_set_mode(dpd.mode);
    dpd_capture_ram();
    dpd_dump_words(dpd.capture, DPD_RESULT_WORDS_CAPTURE, " %u,");
    dbg_printf("<<<<<< (dpd_cpu_access_err=%d)\n", FIELD_GET(DPD_STATUS_CPU_ACCESS_ERR, DPD_STATUS));
}

/* @provenance RVA 0x3684 dpd_handle_command */
uint32_t dpd_handle_command(const uint32_t *cmd, uint32_t *out, int *out_len)
{
    const volatile uint32_t *input = cmd;
    volatile uint32_t *output = out;
    volatile int *length = out_len;
    volatile struct dpd_state *state = &dpd;

    state->result_armed = 1;
    uint32_t opcode = input[0];

    switch (opcode) {
    case DPD_CMD_SET_MODE: {
        uint32_t target_mode = input[2];
        uint32_t trigger = input[3];

        state->mode = target_mode;
        state->capture_gtb = trigger;
        uint32_t control = DPD_CTRL;
        uint32_t initial_mode = input[1];

        DPD_CTRL = (control & ~RFIELD_MASK(DPD_CTRL_MODE))
                 | FIELD_PREP(DPD_CTRL_MODE, initial_mode) | DPD_CTRL_EN;
        trigger = state->capture_gtb;
        target_mode = state->mode;
        initial_mode = input[1];
        dbg_printf("DPD mode %d>%d set up @%d\n", initial_mode, target_mode, trigger);
        break;
    }
    case DPD_CMD_ACTUATE: {
        uint32_t enable = input[1];
        uint32_t trigger = input[2];

        state->actuation_enable = enable;
        state->actuation_gtb = trigger;
        dbg_printf("DPD actuation %d (GTB=%d)\n", enable, trigger);
        break;
    }
    case DPD_CMD_TEST: {
        uint32_t enable = input[1];
        uint32_t trigger = input[2];

        state->test_enable = enable;
        state->test_gtb = trigger;
        dbg_printf("DPD test %d, GTB=%d\n", enable, trigger);
        break;
    }
    case DPD_CMD_GET_RESULT: {
        if (out == NULL || out_len == NULL)
            return (uint32_t)-1;
        uint32_t capacity = (uint32_t)*length;

        if (capacity < 8)
            return (uint32_t)-2;
        uint32_t count = state->result_len;
        uint32_t pointer = state->result_ptr;

        dbg_printf("DPD get: %x %x %d %d\n", pointer, out, count, capacity);
        output[0] = state->result_ptr;
        output[1] = state->result_len;
        *length = 8;
        return 0;
    }
    default:
        return (uint32_t)-1;
    }

    state->result_ptr = 0;
    *length = 0;
    return 0;
}
