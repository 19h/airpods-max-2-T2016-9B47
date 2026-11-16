#ifndef ACIB_PHY_IPC_H
#define ACIB_PHY_IPC_H

#include "acib/acib.h"
#include "acib/shared.h"

struct acib_phy_ipc_state {
    uint32_t req_addr;           /* 0x1002268C -> 0x4D280000 */
    uint32_t resp_addr;          /* 0x10022690 -> 0x4D280400 (or 0x4D2801E6) */
    uint8_t  cfg_bytes[0x30];    /* 0x10022694..0x100226C3 */
    uint32_t coex_table_handle;  /* 0x100226C4 */
};

extern struct acib_phy_ipc_state g_phy_ipc_state;        /* 0x1002268C */
extern uint8_t                   g_phy_table_shadow[0x120]; /* 0x10021EA9 */
extern uint32_t                  g_phy_crash_log_ctx;    /* 0x100217CC */
extern uint8_t                   g_rf_tx_mode_ctx[0xB4]; /* 0x10027958 */
extern uint8_t                   g_phy_ack_ctx[0x10];    /* 0x1002A168 */

uint32_t phy_hw_trig_frs_start_arm(uint32_t ev_base);
uint32_t phy_hw_trig_frs_abort_arm(uint32_t ev_base);

void     phy_ipc_init(void);
void     phy_ipc_is_ready(uint32_t *out_magic);
void     phy_ipc_cmd_set_op2(void);
void     phy_ipc_cmd_set_op3_u16(uint32_t enable);
int      phy_ipc_noop(void);
int      phy_ipc_cmd_set_op6_mode(uint8_t *out_b0, uint8_t *out_b1, uint32_t *out_word);
bool     phy_ipc_wait_and_ack_cmd(int wait_flag, uint32_t timeout_us);
int      phy_ipc_exec_cmd(uint16_t opcode, int wait_flag, uint32_t timeout_us);
bool     phy_ipc_ring_cmd_doorbell(int wait_flag, uint32_t timeout_us);
void    *phy_ipc_clear_cmd_status_flag(const void *src, size_t n);
int      phy_ipc_query_op3_ready(uint32_t lock_arg);
int      phy_ipc_complete_async_cmd(uint32_t timeout_us, uint32_t slot);
uint32_t phy_ipc_irq_handler(void);
uint32_t phy_aci_if_enable_gates(void);
int      phy_ipc_get_cached_version(void);
int      phy_ipc_wait_warm_boot_ack(uint32_t timeout_us);
int      phy_ipc_cmd_op9(void);
void    *phy_ipc_cmd_op10(void *dst);
uint32_t phy_ipc_cmd_op11_u8(uint32_t slot);
uint32_t phy_ipc_cmd_op12_u8(uint32_t slot);
uint32_t phy_ipc_cmd_op13_pair(uint32_t mode_idx, uint8_t *out_count);
uint32_t phy_ipc_frs_prepare_scan(uint32_t mode, uint8_t param, uint8_t dwell,
                                  uint8_t band_flags, uint8_t ctrl_flag,
                                  uint32_t unused6, uint32_t unused7,
                                  const void *custom_chan_bitmap);
int      phy_ipc_frs_validate_param(uint32_t count, uint32_t mode, uint32_t chan_code);
bool     phy_ipc_frs_is_scan_complete(void);
int      phy_ipc_frs_run_scan(uint32_t mode, uint8_t param, uint8_t dwell,
                              uint8_t band_flags, uint16_t custom_Mult,
                              const void *custom_chan_bitmap, uint8_t *out_packed_189b,
                              uint16_t *out_elapsed_us);
uint32_t phy_ipc_coex_ring_doorbell(const uint32_t *coex_word);
int      phy_ipc_frs_copy_results(uint32_t start_ch, size_t count, void *dst_rssi, size_t unused_end);
int      phy_ipc_frs_read_window(int16_t ch_offset, uint32_t count, void *dst_rssi);
uint32_t phy_ipc_coex_noop(void);
uint8_t  phy_ipc_coex_get_active(void);
void     phy_ipc_on_cmd_ack_clear_flags(void);
int      phy_ipc_sync_shared_table_dekker(uint32_t cb_arg);
int      phy_seq_rf_ctrl_apply_tx_mode(uint32_t profile_idx, uint32_t tx_param);

#endif /* ACIB_PHY_IPC_H */
