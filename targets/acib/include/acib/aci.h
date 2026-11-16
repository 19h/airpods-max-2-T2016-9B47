#ifndef ACIB_ACI_H
#define ACIB_ACI_H

#include "acib/acib.h"

/* --- Trace Manager Buffer Control Block (0x1002CF68) --- */
struct acib_tm_buf_cb {
    uint8_t *base;               /* +0x00 */
    uint16_t used;               /* +0x04 */
    uint16_t capacity;           /* +0x06 */
};

extern struct acib_tm_buf_cb g_tm_default_cb;           /* 0x1002CF68 */
extern uint8_t               g_bt_ctrl_ctx[0x6B0];      /* 0x10020B78..0x10021228 */
extern uint8_t               g_osa_task_table[0x400];   /* 0x10021228..0x10021628 */

/* Reconstructed C: src/aci/trace.c */
struct acib_tm_buf_cb *tm_buffer_util_cb_init(uint32_t pool_id);
uint16_t tm_buffer_util_get_free(const struct acib_tm_buf_cb *cb);
void     tm_buffer_util_decrement_remaining(struct acib_tm_buf_cb *cb, uint32_t consume_bytes);
uint16_t tm_buffer_util_get_used(const struct acib_tm_buf_cb *cb);
void     tm_buffer_util_reset(struct acib_tm_buf_cb *cb);
void     tm_buffer_util_bind(struct acib_tm_buf_cb *cb, uint8_t *storage, uint16_t capacity);
uint32_t tm_buffer_util_has_storage(const struct acib_tm_buf_cb *cb);
uint8_t *tm_buffer_util_write_ptr(const struct acib_tm_buf_cb *cb);
uint8_t *tm_buffer_util_base_ptr(const struct acib_tm_buf_cb *cb);
uint32_t tm_pkt_write_hdr4(uint8_t *dst, const uint8_t *hdr_in);
uint32_t tm_pkt_write_hdr8(uint8_t *dst, const uint8_t *ts_info, uint32_t type_bits);
uint32_t tm_pkt_write_ts_event(uint8_t *dst, const uint8_t *ev_desc);

/* Reconstructed C: src/aci/osa.c */
void     osa_buf_tag_owner(uint8_t *buf_body);
uint32_t osa_crash_copy_recent_events(uint8_t *dst, uint32_t max_bytes);
void     osa_task_clear_pending_flag(uint32_t task_id);
uint8_t  osa_mbx_get_relay_dest(uint32_t msg_id);
uint16_t osa_task_get_msg_count(uint32_t task_id);

/* Reconstructed C: src/aci/bt_ctrl.c */
uint32_t hci_test_mode_get_opcode_from_type(uint32_t ber_type);
void     bt_bdaddr_swap(const uint8_t src[6], uint8_t dst[6]);
void     bt_ts_ring_record(uint32_t tag10);
void     bt_ts_ring_record_alt(uint32_t tag10);
void     acib_set_bt_ctrl_mode(uint8_t mode);
uint8_t  acib_get_bt_ctrl_mode(void);
uint8_t  acib_get_conn_slot_state(uint32_t slot_idx);
int      acib_log_build_banner_and_start(void);
int      main_entry(void);
void     bt_system_thread_init(void);

#endif /* ACIB_ACI_H */
