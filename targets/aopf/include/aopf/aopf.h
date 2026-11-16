/*
 * aopf.h — Top-level Always-On Processor application, audio TLV ring log,
 * property hash table, SOCR LFSR mixer, and battery version prototypes.
 */

#ifndef AOPF_TOP_H
#define AOPF_TOP_H

#include <stdint.h>
#include <stddef.h>
#include "aopf/memmap.h"
#include "aopf/regs.h"
#include "aopf/cpu.h"
#include "aopf/bus.h"
#include "aopf/shared.h"
#include "aopf/libc.h"
#include "aopf/rtkit.h"

/* AOP timestamp, LFSR mixer & battery helpers (src/aop/app.c) */
uint64_t aop_get_timestamp_us(void);
uint64_t j_aop_get_timestamp_us(void);
uint64_t aop_socr_lfsr_mix(uint64_t seed_val);
const char *Battery_getBuildString(void);

/* AOP Audio binary TLV log serializer (src/aop/app.c) */
void aud_log_buf_append_tlv(aop_aud_log_buf_t *log_buf, uint8_t tag,
                            const void *payload, uint32_t payload_len);
aop_aud_log_buf_t *aud_log_append_u32(aop_aud_log_buf_t *log_buf, uint32_t word_val);
aop_aud_log_buf_t *aud_log_append_str(aop_aud_log_buf_t *log_buf, const char *str_val);
aop_aud_log_buf_t *aud_log_append_end(aop_aud_log_buf_t *log_buf);

/* AOP Property key-value hash table (src/aop/app.c) */
aop_prop_node_t *PropertyTable_lookup(aop_prop_table_t *tbl, uint16_t prop_key);
aop_prop_node_t *PropertyTable_findOrAlloc(aop_prop_table_t *tbl, uint16_t prop_key);
uint32_t Property_setInlineOrPtr(aop_prop_node_t *node, uint32_t new_val);

#endif /* AOPF_TOP_H */
