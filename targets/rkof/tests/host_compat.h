#ifndef RKOF_HOST_COMPAT_H
#define RKOF_HOST_COMPAT_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include "rkof/rkof.h"

struct rkof_host_panic_trap {
    int      armed;
    int      triggered;
    jmp_buf  env;
    char     last_msg[256];
    uint32_t halt_reason;
};

extern struct rkof_host_panic_trap g_rkof_panic_trap;

void rkof_host_bus_reset(const uint8_t *raw_image, size_t raw_len);
void rkof_host_set_remap_auto_ack(int enable);
void rkof_host_set_pmgr_clk_auto_ack(int enable);
void rkof_host_set_mock_pwr_state(uint32_t state);
uint32_t rkof_host_get_mock_pwr_state(void);
uint32_t rkof_host_get_pwr_set_calls(void);

#endif /* RKOF_HOST_COMPAT_H */
