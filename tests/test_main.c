/*
 * test_main.c -- behavioural regression tests for the reconstruction.
 *
 * These exercise the parts of the image whose behaviour is fully determined by
 * the listing: the C library and formatter, fixed-point helpers, the crash
 * checksum, warm-boot detection, radio-revision decode, timestamps, telemetry
 * and the trace ring.  Hardware sequencing is compile-checked but not emulated.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/clock.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/txpwr.h"
#include "btp/frs.h"
#include "btp/status.h"
#include "btp/trace.h"
#include "btp/mac.h"
#include "btp/nvv.h"

#include <stdio.h>
#include <string.h>

/* Darwin fortifies snprintf through a macro; reach the firmware implementation. */
#undef snprintf
#undef vsnprintf

void btp_host_image_reset(void);
unsigned long btp_host_wfi_count(void);

static int failures;
static int checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define CHECK_STR(got, want)                                                 \
    do {                                                                     \
        ++checks;                                                            \
        if (strcmp((got), (want)) != 0) {                                    \
            fprintf(stderr, "FAIL %s:%d: got \"%s\" want \"%s\"\n",         \
                    __FILE__, __LINE__, (got), (want));                      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_strings(void)
{
    char buf[16];
    char overlap[8] = "abcdef";

    CHECK(strlen("abcdef") == 6);
    CHECK(strnlen("abcdef", 3) == 3);
    CHECK(strncmp("abc", "abd", 3) < 0);
    CHECK(strncmp("abc", "abc", 3) == 0);
    CHECK_STR(strrchr("a/b/c", '/'), "/c");
    CHECK(strrchr("abc", '/') == NULL);

    memset(buf, 0, sizeof buf);
    CHECK(strlcpy(buf, "hello", sizeof buf) == 5);
    CHECK_STR(buf, "hello");
    CHECK(strlcat(buf, "!", sizeof buf) == 6);
    CHECK_STR(buf, "hello!");

    memmove(overlap + 1, overlap, 5);
    CHECK_STR(overlap, "aabcde");
    memcpy(overlap, overlap + 1, 5);
    CHECK_STR(overlap, "abcdee");
}

static void test_formatter(void)
{
    char buf[64];
    int n;

    n = snprintf(buf, sizeof buf, "%s:%d:%u:%x:%#x", "ab", -7, 42u, 0x2Au, 0x2Au);
    CHECK_STR(buf, "ab:-7:42:2a:0x2a");
    CHECK(n == (int)strlen(buf));

    n = snprintf(buf, 5, "%s", "abcdefg");
    CHECK_STR(buf, "abcd");
    CHECK(n == 7);

    snprintf(buf, sizeof buf, "%5d|%-5d|%05d|%+d", 42, 42, 42, 42);
    CHECK_STR(buf, "   42|42   |00042|+42");

    snprintf(buf, sizeof buf, "%d.%02d", 3, 7);
    CHECK_STR(buf, "3.07");

    snprintf(buf, sizeof buf, "%s", (const char *)0);
    CHECK_STR(buf, "<null>");

    snprintf(buf, sizeof buf, "%4s|%c|%X|%i", "Ch", 'x', 0xBEEFu, -1);
    CHECK_STR(buf, "  Ch|x|BEEF|-1");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
    snprintf(buf, sizeof buf, "%q");
    CHECK_STR(buf, "%q");
    snprintf(buf, sizeof buf, "%f", 1.5);
    CHECK_STR(buf, "@");
#pragma clang diagnostic pop
}

static void test_rounding_and_checksum(void)
{
    uint32_t words[3] = { 0x12345678u, 0x9ABCDEF0u, 0x0F0F0F0Fu };

    CHECK(roundf_half_up(1.4f) == 1);
    CHECK(roundf_half_up(1.5f) == 2);
    CHECK(roundf_half_up(-1.2f) == -1);
    CHECK(xor_checksum_words(words, 3) == (words[0] ^ words[1] ^ words[2]));
}

static void test_fixed_point(void)
{
    char *s;

    btp_host_image_reset();
    s = frs_format_rssi(-301, FRS_RSSI_SCALE);        /* -75.25 dBm */
    CHECK_STR(s, "-75.25");
    s = frs_format_rssi(-2, FRS_RSSI_SCALE);          /* -0.50 */
    CHECK_STR(s, "0.50");
}

static void test_warm_boot(void)
{
    btp_host_image_reset();

    CHECK(!warm_boot_detected());
    warm_boot_set_state(1);
    CHECK(warm_boot_detected());
    CHECK(warm_boot_magic == WARM_BOOT_MAGIC);
    CHECK(phy_status->warm_boot_flag == 1);

    phy_status->warm_boot_flag = 0;
    CHECK(!warm_boot_detected());
}

static void test_radio_revision(void)
{
    btp_host_image_reset();

    RADIO_ID = FIELD_PREP(RADIO_ID_MAJOR, 'A') | FIELD_PREP(RADIO_ID_MINOR, 0);
    CHECK(radio_revision_detect() == RADIO_REV_A0);
    RADIO_ID = FIELD_PREP(RADIO_ID_MAJOR, 'A') | FIELD_PREP(RADIO_ID_MINOR, 1);
    CHECK(radio_revision_detect() == RADIO_REV_A1);
    RADIO_ID = FIELD_PREP(RADIO_ID_MAJOR, 'B') | FIELD_PREP(RADIO_ID_MINOR, 0);
    CHECK(radio_revision_detect() == RADIO_REV_B0);
    RADIO_ID = FIELD_PREP(RADIO_ID_MAJOR, 'C');
    CHECK(radio_revision_detect() == RADIO_REV_UNKNOWN);
    CHECK(radio_revision_get() == RADIO_REV_UNKNOWN);
}

static void test_timestamps(void)
{
    btp_host_image_reset();

    SEQ_TIMESTAMP_LO = 0x11223344u;
    SEQ_TIMESTAMP_HI = 0xAABBu;
    CHECK(seq_timer_now() == 0xAABB11223344ull);

    SEQ_RX_TRIGGER_TS_LO = 0x55667788u;
    SEQ_RX_TRIGGER_TS_HI = 0xCCDDu;
    CHECK(seq_timer_rx_trigger() == 0xCCDD55667788ull);
}

static void test_telemetry(void)
{
    uint8_t out[64];
    uint32_t copied = 0;
    const struct telemetry *t = (const struct telemetry *)out;

    btp_host_image_reset();
    tx.packet_count = 3;
    rx.packet_count = 2;
    rx.cfo_sum = 100;
    rx.cfo_max = 60;
    rx.cfo_min = -60;

    CHECK(telemetry_read_and_reset(out, &copied, sizeof out) == BTP_OK);
    CHECK(copied == sizeof(struct telemetry));
    CHECK(t->stats.tx_count == 3);
    CHECK(t->stats.rx_count == 2);
    CHECK(t->stats.cfo_mean == 50);
    CHECK(tx.packet_count == 0 && rx.packet_count == 0);
    CHECK(rx.cfo_max == RX_CFO_MAX_INIT && rx.cfo_min == RX_CFO_MIN_INIT);

    CHECK(telemetry_read_and_reset(out, &copied, 8) == BTP_ERR_INVALID);
}

static void test_trace_ring(void)
{
    struct trace_event ev;

    btp_host_image_reset();
    trace_begin(&ev, TRACE_TYPE_INIT, TRACE_INIT_RX, 0, 1234);
    trace_log(TRACE_SITE_RX_INIT_ENTER, &ev);
    CHECK(trace_ring->next == 1);
    CHECK(trace_ring->records[0].type == TRACE_TYPE_INIT);
    CHECK(trace_ring->records[0].start_ts == 1234);
    CHECK(trace_ring->records[0].site == TRACE_SITE_RX_INIT_ENTER);

    trace_log_error(TRACE_ERR_CLASS_COMMAND, 0x55, TRACE_ERR_IPC_UNKNOWN_COMMAND);
    CHECK(trace_ring->records[1].type == TRACE_TYPE_ERROR);
    CHECK(trace_ring->records[1].arg0 == 0x55);
    CHECK(trace_ring->records[1].arg1 == TRACE_ERR_IPC_UNKNOWN_COMMAND);
    CHECK(trace_ring->records[1].site == TRACE_SITE_ERROR);

    diag_set_freeze(DIAG_BUFFER_TRACE, true);
    trace_log(TRACE_SITE_RX_INIT_EXIT, &ev);
    CHECK(trace_ring->next == 2);
    diag_set_freeze(DIAG_BUFFER_TRACE, false);

    for (int i = 0; i < TRACE_RING_ENTRIES; ++i)
        trace_log(TRACE_SITE_RX_INIT_EXIT, &ev);
    CHECK(trace_ring->next == 2);
    CHECK(trace_ring_size() == 0x1FC);
}

static void test_channel_limits(void)
{
    btp_host_image_reset();
    txpwr_reset_channel_limits();
    CHECK(txpwr_get_channel_limit_word(0, 0) == 0xFFFFFFFFu);
    CHECK(txpwr_set_channel_limit(TXPWR_LIMIT_LOW, 1, 2, 0x10, -4) == BTP_OK);
    CHECK(txpwr.limits[1][2].low_channel == 0x10);
    CHECK(txpwr.limits[1][2].low_power == -4);
    CHECK((txpwr_get_channel_limit_word(1, 2) & 0xFFFF) == (0x10u | (0xFCu << 8)));
    CHECK(txpwr_set_channel_limit(TXPWR_LIMIT_LOW, 8, 0, 0, 0) == BTP_ERR_INVALID);
    CHECK(txpwr_set_channel_limit(TXPWR_LIMIT_LOW, 0, 9, 0, 0) == BTP_ERR_INVALID);
}

static void test_version(void)
{
    uint8_t out[0x80];
    uint32_t copied = 0;
    const struct version_info *v = (const struct version_info *)out;
    static const char module[] = "DAUG";

    btp_host_image_reset();
    nvv.mod_info = module;
    CHECK(version_info_read(out, &copied, sizeof out) == BTP_OK);
    CHECK(copied == sizeof(struct version_info));
    CHECK(strncmp(v->version, VERSION_STRING, sizeof v->version) == 0);
    CHECK(v->protocol == 0x000D);
    CHECK(strncmp(v->module, "DAUG", 5) == 0);
    CHECK(strncmp(v->stepping, VERSION_STEPPING, 3) == 0);
    CHECK(version_info_read(out, &copied, 0x7E) == BTP_ERR_INVALID);
}

int main(void)
{
    test_strings();
    test_formatter();
    test_rounding_and_checksum();
    test_fixed_point();
    test_warm_boot();
    test_radio_revision();
    test_timestamps();
    test_telemetry();
    test_trace_ring();
    test_channel_limits();
    test_version();

    printf("%d checks, %d failure%s\n", checks, failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
