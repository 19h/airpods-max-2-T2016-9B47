/*
 * mac.c -- MAC <-> PHY command interface.
 *
 * The MAC talks to the PHY through a mailbox in shared memory and a pair of
 * doorbell registers.  Every request arrives as IRQ0: the MAC's event bits are
 * read from IPC_STATUS, acknowledged, and dispatched.  Commands are answered
 * in the response area of the mailbox and signalled back with the response
 * doorbell; FRS and crash completions ring their own doorbell bits.
 */
#include "btp/btp.h"
#include "btp/mac.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/rtkit.h"
#include "btp/status.h"
#include "btp/frs.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/txpwr.h"
#include "btp/dpd.h"
#include "btp/loopback.h"
#include "btp/syscfg.h"

struct mac_state     mac BTP_STATE(mac);
struct mac_mailbox  *mac_mbox BTP_DATA(mac_mbox) = (struct mac_mailbox *)MAC_MAILBOX_BASE;

_Static_assert(offsetof(struct mac_mailbox, resp) == 0x400, "mailbox response offset");
_Static_assert(offsetof(struct mac_mailbox, frs_rssi) == 0x4C0, "mailbox FRS result offset");
_Static_assert(offsetof(struct mac_mailbox, frs_status) == 0x5C0, "mailbox FRS status offset");
_Static_assert(offsetof(struct mac_mailbox, crash) == 0x5C1, "mailbox crash offset");
_Static_assert(offsetof(struct mac_mailbox, frs_gain) == 0x5FE, "mailbox FRS gain offset");
_Static_assert(offsetof(struct mac_request, frs) == MAC_REQUEST_FRS_OFFSET, "FRS request offset");

/* Outcome of a command: the status byte and the payload length reported back. */
struct mac_result {
    uint8_t status;         /* 0 = ok, 1 = failed */
    uint8_t length;         /* bytes valid in resp.payload */
};

#define MAC_RESULT(st, len)   ((struct mac_result){ (uint8_t)(st), (uint8_t)(len) })
#define MAC_FAILED(rc)        MAC_RESULT((rc) != 0, 0)

/* Assert sites of the original mac.c. */
#define MAC_LINE_DEBUG_ASSERT        0x32C
#define MAC_LINE_TXPWR_TABLE_FAILED  0x388
#define MAC_LINE_TXPWR_BAND_FAILED   0x3AF
#define MAC_LINE_WARM_BOOT_STATUS    0x3F1
#define MAC_LINE_UNKNOWN_COMMAND     0x437
#define MAC_LINE_UNKNOWN_EVENT       0x51C

static void mac_ring_doorbell(uint32_t bits)
{
    IPC_DOORBELL = (IPC_DOORBELL & ~IPC_EVENT_MASK) | bits;
}

/* @provenance RVA 0x04C8 mac_ipc_send_warm_boot_ack */
void mac_send_warm_boot_ack(void)
{
    if (mac.warm_boot_ack_pending == 1) {
        mac.warm_boot_ack_pending = 0;
        mac_ring_doorbell(IPC_TO_MAC_WARM_BOOT_ACK);
    }
}

/* @provenance RVA 0x04EC mac_ipc_send_crash_notification */
void mac_send_crash_notification(const struct crash_descriptor *desc)
{
    const volatile struct crash_descriptor *source = desc;
    uint32_t first = source->error_code;
    /* 0x04EC..0x050A samples the first word before loading the mailbox cell,
     * then alternates word reads and unaligned word stores. Neither aggregate
     * assignment nor memmove preserves this behavior for overlapping inputs. */
    struct mac_mailbox *mailbox = *(struct mac_mailbox *volatile *)&mac_mbox;
    volatile struct crash_descriptor *destination =
        (volatile struct crash_descriptor *)mailbox->crash;

    destination->error_code = first;
    destination->trace_ring = source->trace_ring;
    destination->trace_size = source->trace_size;
    destination->detail0 = source->detail0;
    destination->detail1 = source->detail1;
    mac_ring_doorbell(IPC_TO_MAC_CRASH);
}

/* ------------------------------------------------------------------------- */
/* Event bits (no command)                                                   */
/* ------------------------------------------------------------------------- */

static void mac_handle_frs_request(void)
{
    struct mac_mailbox *mbox = mac_mbox;
    const struct mac_request *req = &mbox->req;
    uint8_t channels[MAC_FRS_GAIN_CHANNELS];
    int count = MAC_FRS_GAIN_CHANNELS;
    uint32_t rc;

    if (req->frs.band >= BTP_BAND_COUNT) {
        mbox->frs_status = 2;
        if (req->frs.notify & 1)
            mac_ring_doorbell(IPC_TO_MAC_FRS_DONE);
        return;
    }

    frs_unpack_channel_bitmap(req->frs.channel_bitmap, sizeof req->frs.channel_bitmap,
                              channels, &count);
    frs_configure(req->frs.band, req->frs.half_span, req->frs.passes & 7, true);
    frs_enter(req->frs.gain_index, mbox->frs_rssi, mbox->frs_gain);

    /* An empty bitmap falls back to the band's default channel list. */
    rc = frs_run_scan(count != 0 ? channels : NULL, count);
    if (rc == 0)
        frs_process_results(mbox->frs_rssi, mbox->frs_gain);
    mbox->frs_status = rc != 0;

    if (req->frs.notify & 1)
        mac_ring_doorbell(IPC_TO_MAC_FRS_DONE);
    frs_exit();
}

static void mac_handle_coex_request(void)
{
    if (mac_mbox->req.frs.coex_enable == 1)
        coex_enable_rx_gain_backoff();
    else
        coex_disable_rx_gain_backoff();
}

/* ------------------------------------------------------------------------- */
/* MAC_CMD_SUBCOMMAND                                                        */
/* ------------------------------------------------------------------------- */

static struct mac_result mac_handle_override(const struct mac_request *req)
{
    const volatile struct mac_request *input = req;
    const volatile uint8_t *p = input->u.sub.p.u8;
    uint8_t id = p[0];
    uint8_t enable = 0;
    uint16_t value = 0;
    btp_status_t rc;

    /* Preserve each selector's actual input reads. Some consume only a byte,
     * some only the halfword, and invalid selectors consume neither. */
    switch (id) {
    case MAC_OVR_DF1_MANTISSA:
    case MAC_OVR_RSSI_CAL:
    case MAC_OVR_MXDC_I:
    case MAC_OVR_MXDC_Q:
    case MAC_OVR_IQ_SCALE:
    case MAC_OVR_TXPWR_OFFSET:
    case MAC_OVR_PA_GAIN:
    case MAC_OVR_RX_FILTER_BW:
        value = input->u.sub.p.ovr.value;
        enable = p[1];
        break;
    case MAC_OVR_HF_COEFF_BYPASS:
    case MAC_OVR_RESERVED_21:
        enable = p[1];
        break;
    case MAC_OVR_RESERVED_6:
    case MAC_OVR_COEX_POWER_FLOOR:
        value = input->u.sub.p.ovr.value;
        break;
    case MAC_OVR_MAXGAIN_REF_LNA:
    case MAC_OVR_MAXGAIN_REF_MIX:
        value = p[2];
        break;
    default:
        if (id >= MAC_OVR_TXPWR_BAND_FIRST && id <= MAC_OVR_TXPWR_BAND_LAST) {
            value = input->u.sub.p.ovr.value;
            enable = p[1];
        } else if (id >= MAC_OVR_IQMM_FIRST && id <= MAC_OVR_IQMM_LAST) {
            enable = p[1];
            value = p[2];
        }
        break;
    }

    switch (id) {
    case MAC_OVR_DF1_MANTISSA:
        rc = tx_set_df1_mantissa_override(enable, (int16_t)value);
        break;
    case MAC_OVR_RSSI_CAL:
        rc = rx_set_rssi_cal_override(enable, (int16_t)value);
        break;
    case MAC_OVR_HF_COEFF_BYPASS:
        rc = tx_set_hf_coeff_bypass(enable);
        break;
    case MAC_OVR_MXDC_I:
        rc = rx_set_mxdc_i_override(enable, value);
        break;
    case MAC_OVR_MXDC_Q:
        rc = rx_set_mxdc_q_override(enable, value);
        break;
    case MAC_OVR_RESERVED_6:
        rc = tx_set_reserved_override_6();
        break;
    case MAC_OVR_IQ_SCALE:
        rc = txpwr_set_iq_scale_override(enable, value);
        break;
    case MAC_OVR_TXPWR_OFFSET:
        rc = txpwr_set_global_offset(enable, value);
        break;
    case MAC_OVR_RESERVED_21:
        rc = tx_set_reserved_override_21();
        break;
    case MAC_OVR_PA_GAIN:
        rc = txpwr_set_pa_gain_override(enable, (uint8_t)value);
        break;
    case MAC_OVR_RX_FILTER_BW:
        rc = rx_set_filter_bw_override(enable, (uint8_t)value);
        break;
    case MAC_OVR_MAXGAIN_REF_LNA:
        rx.maxgain_ref_lna = (uint8_t)value;
        rx_update_maxgain_tables();
        return MAC_RESULT(0, 0);
    case MAC_OVR_MAXGAIN_REF_MIX:
        rx.maxgain_ref_mix = (uint8_t)value;
        rx_update_maxgain_tables();
        return MAC_RESULT(0, 0);
    case MAC_OVR_COEX_POWER_FLOOR:
        txpwr_set_coex_floor(value);
        return MAC_RESULT(0, 0);
    default:
        if (id >= MAC_OVR_TXPWR_BAND_FIRST && id <= MAC_OVR_TXPWR_BAND_LAST)
            rc = txpwr_set_band_override(id - MAC_OVR_TXPWR_BAND_FIRST, enable, value);
        else if (id >= MAC_OVR_IQMM_FIRST && id <= MAC_OVR_IQMM_LAST)
            rc = rx_set_iqmm_override(enable, id - MAC_OVR_IQMM_FIRST, (uint8_t)value);
        else
            return MAC_RESULT(1, 0);
        break;
    }
    return MAC_FAILED(rc);
}

static struct mac_result mac_handle_syscfg(const struct mac_request *req)
{
    const uint8_t *p = req->u.sub.p.u8;

    switch (req->u.sub.p.syscfg.op) {
    case 2:
        return MAC_FAILED(syscfg_apply());
    case 1:
        return MAC_FAILED(syscfg_write(req->u.sub.p.syscfg.length,
                                       req->u.sub.p.syscfg.offset, p + 4));
    default:
        return MAC_RESULT(0, 0);
    }
}

static struct mac_result mac_handle_frs_dump(const struct mac_request *req)
{
    unsigned band = req->u.sub.p.u8[0];

    if (band >= BTP_BAND_COUNT)
        return MAC_RESULT(0, 0);

    /* Full-span, three-pass scan of the band's default channel list. */
    frs_configure(band, FRS_HALF_SPAN_MAX, FRS_PASS_MAX, false);
    frs_enter(1, NULL, NULL);
    frs_run_scan(NULL, 0);
    frs_dump_readings(band);
    frs_exit();
    return MAC_RESULT(0, 0);
}

static struct mac_result mac_handle_channel_limit(const struct mac_request *req)
{
    const struct mac_request *r = req;
    unsigned op = r->u.sub.p.limit.op;
    btp_status_t rc = 0;

    if (op <= TXPWR_LIMIT_LOW)
        rc = txpwr_set_channel_limit(op, r->u.sub.p.limit.band, r->u.sub.p.limit.pkt_type,
                                     r->u.sub.p.limit.channel, r->u.sub.p.limit.power);
    mac_mbox->resp.payload.u32[0] =
        txpwr_get_channel_limit_word(r->u.sub.p.limit.band, r->u.sub.p.limit.pkt_type);
    return MAC_RESULT(rc != 0, 4);
}

static struct mac_result mac_handle_diag(const struct mac_request *req)
{
    unsigned index = req->u.sub.p.diag.index;
    uint32_t offset = req->u.sub.p.diag.offset;
    uint32_t copied = 0;
    btp_status_t rc;

    switch (req->u.sub.p.diag.op) {
    case 0:
        if (diag.buffers[index].freezable != 1)
            return MAC_RESULT(0, 0);
        diag_set_freeze(index, false);
        return MAC_RESULT(0, 0);

    case 1:
        if (offset == 0) {
            /* First chunk: rebuild the table, freeze the buffer and checksum it. */
            diag_buffers_init();
            diag.freeze_mask = 0;
            if (index < DIAG_BUFFER_COUNT) {
                struct diag_buffer *buf = &diag.buffers[index];

                if (buf->freezable == 1)
                    diag_set_freeze(index, true);
                buf->checksum = xor_checksum_words(BTP_PTR(buf->base), buf->length >> 2);
            }
        }
        rc = diag_buffer_read(mac_mbox->resp.payload.u8, &copied, index, offset,
                              req->u.sub.p.diag.length, MAC_RESPONSE_PAYLOAD_SIZE);
        return MAC_RESULT(rc != 0, copied);

    default:
        return MAC_RESULT(0, 0);
    }
}

static struct mac_result mac_handle_debug(const struct mac_request *req)
{
    switch (req->u.sub.p.u8[0]) {
    case MAC_DEBUG_ASSERT:
        btp_panic_at(MAC_LINE_DEBUG_ASSERT, BTP_PANIC_INVALID_REQUEST);
        break;
    case MAC_DEBUG_HANG:
        for (;;)
            ;
    default:
        break;
    }
    return MAC_RESULT(0, 0);
}

/* Readback-style subcommands share one shape: fill the payload, report size. */
static struct mac_result mac_readback(btp_status_t (*read)(void *, uint32_t *, uint32_t))
{
    uint32_t copied = 0;
    btp_status_t rc = read(mac_mbox->resp.payload.u8, &copied, MAC_RESPONSE_PAYLOAD_SIZE);

    return MAC_RESULT(rc != 0, copied);
}

static struct mac_result mac_dispatch_subcommand(const struct mac_request *req)
{
    const uint8_t *p = req->u.sub.p.u8;
    uint16_t id = req->u.sub.id;
    uint16_t len = req->u.sub.length;

    if (req->length < 4) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, req->length, TRACE_ERR_IPC_SHORT_REQUEST);
        return MAC_RESULT(1, 0);
    }

    switch (id) {
    case MAC_SUB_SYSCFG:
        return mac_handle_syscfg(req);

    case MAC_SUB_LOOPBACK_MODE:
        if (len != 8) {
            trace_log_error(TRACE_ERR_CLASS_COMMAND, len, TRACE_ERR_LOOPBACK_BAD_LENGTH);
            return MAC_RESULT(1, 0);
        }
        return MAC_FAILED(loopback_configure(req->u.sub.p.u32));

    case MAC_SUB_LOOPBACK_STATS:
        return mac_readback(loopback_read_stats);

    case MAC_SUB_TXPWR_MODE:
        if (len != 1)
            return MAC_RESULT(1, 0);
        return MAC_FAILED(tx_set_power_mode_override(p[0]));

    case MAC_SUB_TXPWR_TEST_MODE:
        if (len != 7)
            return MAC_RESULT(1, 0);
        return MAC_FAILED(txpwr_set_test_mode(req->u.sub.p.txpwr_test.mode,
                                              req->u.sub.p.txpwr_test.stage,
                                              req->u.sub.p.txpwr_test.post_stage,
                                              req->u.sub.p.txpwr_test.delay,
                                              req->u.sub.p.txpwr_test.seq_code,
                                              req->u.sub.p.txpwr_test.gain_code,
                                              req->u.sub.p.txpwr_test.override_value));

    case MAC_SUB_SLOPE_CAL_BYPASS:
        if (len != 1)
            return MAC_RESULT(1, 0);
        return MAC_FAILED(tx_set_slope_cal_bypass(p[0]));

    case MAC_SUB_OVERRIDE:
        if (len != 4)
            return MAC_RESULT(1, 0);
        return mac_handle_override(req);

    case MAC_SUB_FRS_DUMP:
        if (len != 1)
            return MAC_RESULT(1, 0);
        return mac_handle_frs_dump(req);

    case MAC_SUB_DPD: {
        int out_len = MAC_RESPONSE_PAYLOAD_SIZE + 4;

        if (len != 0x10)
            return MAC_RESULT(1, 0);
        dpd_handle_command(req->u.sub.p.u32, mac_mbox->resp.payload.u32, &out_len);
        return MAC_RESULT(0, out_len);
    }

    case MAC_SUB_TELEMETRY:
        return mac_readback(telemetry_read_and_reset);

    case MAC_SUB_VERSION:
        return mac_readback(version_info_read);

    case MAC_SUB_CHANNEL_LIMIT:
        if (len != 5)
            return MAC_RESULT(1, 0);
        return mac_handle_channel_limit(req);

    case MAC_SUB_CW_TONE:
        if (len != 4)
            return MAC_RESULT(1, 0);
        return MAC_FAILED(rf_test_cw_tone(req->u.sub.p.tone.enable, req->u.sub.p.tone.band,
                                          req->u.sub.p.tone.channel, req->u.sub.p.tone.mode));

    case MAC_SUB_DIAG_BUFFER:
        if (len != 5)
            return MAC_RESULT(1, 0);
        return mac_handle_diag(req);

    case MAC_SUB_DEBUG:
        return mac_handle_debug(req);

    case MAC_SUB_PRBS:
        if (len != 3)
            return MAC_RESULT(1, 0);
        return MAC_FAILED(rf_test_prbs(req->u.sub.p.prbs.mode, req->u.sub.p.prbs.band,
                                       req->u.sub.p.prbs.channel));

    case MAC_SUB_CORR_MAGIC:
        if (len != 1)
            return MAC_RESULT(1, 0);
        rf_test_set_correlator_magic(p[0]);
        return MAC_RESULT(0, 0);

    default:
        trace_log_error(TRACE_ERR_CLASS_COMMAND, id, TRACE_ERR_IPC_UNKNOWN_SUBCMD);
        return MAC_RESULT(1, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* Commands                                                                  */
/* ------------------------------------------------------------------------- */

static void mac_respond(uint16_t command, uint8_t status, uint8_t length)
{
    volatile struct mac_response *resp = &mac_mbox->resp;

    resp->flags |= MAC_RESPONSE_READY;
    resp->command = command;
    resp->status = status;
    resp->length = length;
}

/* Both TX power table uploads answer as MAC_CMD_TXPWR_TABLE with a one-byte
 * status and escalate a failure to a panic. */
static uint8_t mac_finish_txpwr_update(btp_status_t rc, int line)
{
    uint8_t failed = 0;

    if (rc != 0) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, rc, TRACE_ERR_TXPWR_TABLE_UPDATE);
        failed = 1;
    }
    mac_respond(MAC_CMD_TXPWR_TABLE, failed, 1);
    *(volatile uint8_t *)&mac_mbox->resp.payload.u8[0] = failed;
    if (rc != 0)
        btp_panic(rc, 0, 0, line, BTP_FILENAME, NULL);
    return failed;
}

/* Returns the failure flag reported in the exit trace record. */
static uint32_t mac_handle_command(void)
{
    struct mac_mailbox *mbox = mac_mbox;
    const struct mac_request *req = &mbox->req;
    uint16_t command = req->command;
    uint8_t failed;

    switch (command) {
    case MAC_CMD_GET_VERSION:
        failed = 0;
        mac_respond(command, 0, 8);
        volatile uint16_t *version = mbox->resp.payload.u16;

        /* ROM 0x5F4: 8E251/8E258 load 0xB7 (183); 9A348 loads 0xB9
         * (185), then stores a halfword at mailbox offset 0x408. This is
         * the sole changed instruction outside the RTKit gate at 0x6256. */
        version[0] = BTP_FIRMWARE_VERSION;
        version[1] = 0;
        version[2] = 0;
        version[3] = 0;
        break;

    case MAC_CMD_SUBCOMMAND: {
        struct mac_result result = mac_dispatch_subcommand(req);

        failed = result.status;
        mac_respond(command, result.status, result.length);
        break;
    }

    case MAC_CMD_TXPWR_TABLE:
        failed = mac_finish_txpwr_update(
            txpwr_update_table((const uint8_t *)mbox + MAC_REQUEST_TXPWR_TABLE_OFFSET),
            MAC_LINE_TXPWR_TABLE_FAILED);
        break;

    case MAC_CMD_PING: {
        /* Reports whether the MAC's event register has drained. */
        uint32_t pending = IPC_STATUS & IPC_EVENT_MASK;

        failed = 1;
        mac_respond(command, 0, 1);
        *(volatile uint8_t *)&mbox->resp.payload.u8[0] = pending == 0;
        break;
    }

    case MAC_CMD_WARM_BOOT_ACK: {
        uint32_t pending = IPC_STATUS & IPC_EVENT_MASK;

        failed = 1;
        if (pending == 0) {
            mac.warm_boot_ack_pending = 1;
            failed = 0;
        }
        mac_respond(command, failed, 0);
        if (pending != 0)
            btp_panic_at(MAC_LINE_WARM_BOOT_STATUS, BTP_PANIC_WARM_BOOT_STATUS);
        REG_SET_BITS(SYS_CLK_CTRL2, SYS_CLK_CTRL2_RESET_REQ);
        REG_SET_BITS(SYS_CLK_CTRL2, SYS_CLK_CTRL2_RESET_ACK);
        break;
    }

    case MAC_CMD_TXPWR_BAND_TABLE:
        failed = mac_finish_txpwr_update(
            txpwr_update_band_table((const uint32_t *)req->u.band_table.table,
                                    req->u.band_table.band),
            MAC_LINE_TXPWR_BAND_FAILED);
        break;

    default:
        mac_respond(command, 1, 0);
        trace_log_error(TRACE_ERR_CLASS_COMMAND, command, TRACE_ERR_IPC_UNKNOWN_COMMAND);
        btp_panic_at(MAC_LINE_UNKNOWN_COMMAND, BTP_PANIC_INVALID_REQUEST);
        failed = 1;
        break;
    }

    mac_ring_doorbell(IPC_TO_MAC_RESPONSE);
    return (uint32_t)failed << 16;
}

/* @provenance RVA 0x0524 mac_ipc_irq_handler */
void mac_isr(void)
{
    struct trace_event ev;
    uint32_t timestamp = systick_now();
    uint32_t status = IPC_STATUS;
    uint32_t events = status & IPC_EVENT_MASK;
    uint32_t result;

    trace_begin(&ev, TRACE_TYPE_ISR, TRACE_ISR_SUBTYPE, status << 16, timestamp);
    ev.arg1 = (status & IPC_FROM_MAC_COMMAND) ? mac_mbox->req.command : 0;
    trace_log(TRACE_SITE_MAC_ISR_ENTER, &ev);

    if (events == 0) {
        /* Spurious interrupt: nothing to acknowledge. */
        result = 1u << 16;
    } else {
        IPC_ACK = (IPC_ACK & ~IPC_EVENT_MASK) + events;

        if (status & IPC_FROM_MAC_COMMAND) {
            result = mac_handle_command();
        } else if (status & IPC_FROM_MAC_FRS_REQUEST) {
            mac_handle_frs_request();
            result = 0;
        } else if (status & IPC_FROM_MAC_RESERVED1) {
            result = 0;
        } else if (status & IPC_FROM_MAC_COEX_REQUEST) {
            mac_handle_coex_request();
            result = 0;
        } else {
            trace_log_error(TRACE_ERR_CLASS_COMMAND, events, TRACE_ERR_IPC_UNKNOWN_EVENT);
            btp_panic_at(MAC_LINE_UNKNOWN_EVENT, BTP_PANIC_INVALID_REQUEST);
            result = 1u << 16;
        }
    }

    ev.start_ts = 0;
    ev.arg1 |= result;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_MAC_ISR_EXIT, &ev);
}
