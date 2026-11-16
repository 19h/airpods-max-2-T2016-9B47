#ifndef RKOS_HOST_BUS_H
#define RKOS_HOST_BUS_H

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rkos/rkos.h"

typedef int (*rkos_host_irq_fn_t)(uint32_t ctx_va);

void rkos_host_bus_reset(void);
void rkos_host_set_ipsr(uint32_t ipsr);
void rkos_host_set_psp(uint32_t psp);
void rkos_host_set_msp(uint32_t msp);
uint32_t rkos_host_register_cb(rkos_host_irq_fn_t fn);

void rkos_host_arm_trap(jmp_buf *jb);
void rkos_host_disarm_trap(void);
const char *rkos_host_last_panic_msg(void);
uint32_t rkos_host_last_halt_code(void);
uint32_t rkos_host_wfi_count(void);
uint32_t rkos_host_svc_count(void);
const char *rkos_host_uart_tx_log(void);

/* Prototype declarations for symbols defined in `host_bus.c` and called from `src/` */
void rkos_host_on_panic(const char *msg);
void rkos_host_on_halt(uint32_t code);
uint32_t rkos_host_invoke_irq_cb(uint32_t fn_va, uint32_t ctx_va);
uint32_t rkos_host_invoke_fn4_cb(uint32_t fn_va, uint32_t arg0, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3);
uint32_t rkos_host_invoke_mux_write(uint32_t fn_va, uint32_t dev_va, uint32_t mode,
                                    const uint8_t *buf, uint32_t *io_len);

#endif /* RKOS_HOST_BUS_H */
