#ifndef ACIB_HOST_BUS_H
#define ACIB_HOST_BUS_H

#include "acib/acib.h"
#include "acib/bus.h"
#include "acib/cpu.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#define ACIB_HOST_MAX_MMIO_LOG 256u

struct acib_mmio_write_rec {
    uint32_t addr;
    uint32_t val;
    uint8_t  width;
};

struct acib_host_panic_state {
    bool        armed;
    bool        triggered;
    jmp_buf     env;
    const char *fmt;
    uint32_t    arg0;
    uint32_t    arg1;
    uint32_t    arg2;
    uint32_t    halt_reason;
};

struct acib_host_spy_state {
    uint32_t    phy_ver[4];
    uint32_t    phy_ver_calls;
    uint32_t    last_log_id;
    uint32_t    log_calls;
    uint32_t    crash_dump_words[5];
    uint32_t    crash_dump_calls;
    uint32_t    rf_reg_writes;
    uint32_t    last_rf_reg_id;
    uint32_t    last_rf_reg_val;
    uint32_t    spawned_threads;
    const char *last_thread_name;
    /* Simulated PHY behavior on doorbell write to 0x4D154C68 */
    bool        auto_ack_sync_cmd;
    bool        mutate_legacy_probe_on_cmd;
    uint8_t     frs_auto_complete_status;
    bool        frs_timeout_mode;
    /* Simulated timer advance per read */
    uint32_t    timer_step_per_read;
    uint32_t    cyccnt_step_per_read;
};

extern struct acib_host_panic_state g_host_panic;
extern struct acib_host_spy_state   g_host_spy;
extern struct acib_mmio_write_rec   g_host_mmio_writes[ACIB_HOST_MAX_MMIO_LOG];
extern uint32_t                     g_host_mmio_write_count;

void     acib_host_reset(void);
void     acib_host_set_mmio32(uint32_t addr, uint32_t raw_val);
uint32_t acib_host_get_mmio32(uint32_t addr);
uint8_t *acib_host_mbox_ptr(uint32_t offset);

#endif /* ACIB_HOST_BUS_H */
