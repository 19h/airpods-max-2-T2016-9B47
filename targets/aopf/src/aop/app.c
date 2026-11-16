/*
 * app.c — Always-On Processor application subsystems: microsecond timestamp,
 * SOCR TLV-gated 64-bit shift-XOR mixer, Audio TLV ring serializer, Property
 * hash table lookup/allocation, and AudioProductsBattery build string selector.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/aopf.h"

#define AOPF_BATT_ROM_DEFAULT_VMA    0x00070a10u /* "BNI:AudioProductsBattery-900.106~38" */
#define AOPF_SOCR_TLV_VMA            0x00073a10u

/* @provenance RVA 0x1dac8 aop_get_timestamp_us */
uint64_t aop_get_timestamp_us(void)
{
    rtk_timebase_dev_t *tb = AOPF_VMA_PTR(rtk_timebase_dev_t *, AOPF_TIMEBASE_DEV_VMA);
    return rtk_timer_get_us(tb);
}

/* @provenance RVA 0x243a0 j_aop_get_timestamp_us */
uint64_t j_aop_get_timestamp_us(void)
{
    return aop_get_timestamp_us();
}

/* @provenance RVA 0x19f10 sub_19F10 */
uint64_t aop_socr_lfsr_mix(uint64_t seed_val)
{
    const rtk_tlv_hdr_t *tlv = AOPF_VMA_PTR(const rtk_tlv_hdr_t *, AOPF_SOCR_TLV_VMA);
    uint32_t mode_word = 0u;
    uint32_t copy_len = tlv->length;
    if (copy_len > sizeof(mode_word)) {
        copy_len = (uint32_t)sizeof(mode_word);
    }
    rtk_memcpy_wrap(&mode_word, (const uint8_t *)tlv + 8u, copy_len);

    /* When mode_word is in [1 .. 0xfffffffe], return seed_val unchanged */
    if ((mode_word - 1u) < 0xfffffffeu) {
        return seed_val;
    }

    uint64_t mixed = seed_val;
    uint64_t cur = seed_val;
    while (cur != 0ull) {
        cur >>= 1;
        mixed ^= cur;
    }
    return mixed;
}

/* @provenance RVA 0x3ccf8 Battery_getBuildString */
const char *Battery_getBuildString(void)
{
    const char *rom_bni = AOPF_VMA_PTR(const char *, AOPF_BATT_ROM_DEFAULT_VMA);
    const char *ram_ovr = AOPF_VMA_PTR(const char *, AOPF_BATT_OVERRIDE_STR_VMA);
    const char *empty_str = AOPF_VMA_PTR(const char *, AOPF_DEFAULT_EMPTY_STR_VMA);

    if (aopf_strncmp(rom_bni, ram_ovr, 0x1du) == 0) {
        return empty_str;
    }
    return rom_bni;
}

/* @provenance RVA 0xffc6 sub_FFC6 */
void aud_log_buf_append_tlv(aop_aud_log_buf_t *log_buf, uint8_t tag,
                            const void *payload, uint32_t payload_len)
{
    uint32_t cur_len = log_buf->length;
    uint32_t after_tag = cur_len + 1u;
    uint32_t needed_end = after_tag + payload_len;

    if (needed_end >= log_buf->capacity) {
        return;
    }

    log_buf->length = after_tag;
    log_buf->data[cur_len] = tag;
    rtk_memcpy_wrap(log_buf->data + log_buf->length, payload, payload_len);
    log_buf->length += payload_len;
}

/* @provenance RVA 0xffb0 aud_log_append_u32 */
aop_aud_log_buf_t *aud_log_append_u32(aop_aud_log_buf_t *log_buf, uint32_t word_val)
{
    aud_log_buf_append_tlv(log_buf, 0u, &word_val, 4u);
    return log_buf;
}

/* @provenance RVA 0xfffe aud_log_append_str */
aop_aud_log_buf_t *aud_log_append_str(aop_aud_log_buf_t *log_buf, const char *str_val)
{
    uint32_t len_with_nul = (uint32_t)aopf_strlen(str_val) + 1u;
    aud_log_buf_append_tlv(log_buf, 2u, str_val, len_with_nul);
    return log_buf;
}

/* @provenance RVA 0x1001c sub_1001C */
aop_aud_log_buf_t *aud_log_append_end(aop_aud_log_buf_t *log_buf)
{
    aud_log_buf_append_tlv(log_buf, 3u, NULL, 0u);
    return log_buf;
}

/* @provenance RVA 0x26f4c sub_26F4C */
aop_prop_node_t *PropertyTable_lookup(aop_prop_table_t *tbl, uint16_t prop_key)
{
    if (tbl->cached_node != NULL && tbl->cached_node->prop_key == prop_key) {
        return tbl->cached_node;
    }

    uint32_t bucket_idx = tbl->hash_fn((uint32_t)prop_key, tbl->num_buckets);
    aop_prop_node_t *cur = tbl->buckets[bucket_idx];

    while (cur != NULL) {
        if (cur->prop_key == prop_key) {
            tbl->cached_node = cur;
            return cur;
        }
        uint16_t next_idx = cur->next_pool_idx;
        aop_prop_arena_t *arena = tbl->chain_arena;
        if (arena == NULL || next_idx >= arena->node_count) {
            cur = NULL;
        } else {
            uintptr_t next_addr = arena->base_addr + (uintptr_t)next_idx * arena->node_stride;
            cur = (aop_prop_node_t *)next_addr;
        }
    }
    return NULL;
}

/* @provenance RVA 0x26f94 PropertyTable_findOrAlloc */
aop_prop_node_t *PropertyTable_findOrAlloc(aop_prop_table_t *tbl, uint16_t prop_key)
{
    uint32_t lock_tok = 0xe00002c5u;
    if (tbl->inline_lock.vt != NULL && tbl->inline_lock.vt->lock_acquire != NULL) {
        lock_tok = tbl->inline_lock.vt->lock_acquire(&tbl->inline_lock);
    }

    aop_prop_node_t *found = PropertyTable_lookup(tbl, prop_key);
    if (found == NULL && tbl->free_remain != 0u && tbl->alloc_pool != NULL) {
        uint32_t slot_idx = tbl->free_remain - 1u;
        tbl->free_remain = slot_idx;
        aop_prop_node_t *fresh = &tbl->alloc_pool[slot_idx];
        fresh->node_flags = 0u;
        fresh->prop_key = prop_key;

        uint32_t bucket_idx = tbl->hash_fn((uint32_t)prop_key, tbl->num_buckets);
        aop_prop_node_t *old_head = tbl->buckets[bucket_idx];
        aop_prop_arena_t *arena = tbl->chain_arena;
        uint16_t chain_idx = 0xffffu;
        if (old_head != NULL && arena != NULL && arena->node_stride != 0u) {
            uintptr_t byte_off = (uintptr_t)old_head - arena->base_addr;
            uint32_t elem_idx = (uint32_t)(byte_off / arena->node_stride);
            if (elem_idx < arena->node_count) {
                chain_idx = (uint16_t)(elem_idx & arena->index_mask);
            }
        }
        fresh->next_pool_idx = chain_idx;
        tbl->buckets[bucket_idx] = fresh;
        found = fresh;
    }

    if (lock_tok != 0xe00002c5u &&
        tbl->inline_lock.vt != NULL && tbl->inline_lock.vt->lock_release != NULL) {
        tbl->inline_lock.vt->lock_release(&tbl->inline_lock);
    }
    return found;
}

uint32_t Property_setInlineOrPtr(aop_prop_node_t *node, uint32_t new_val)
{
    if (node == NULL) {
        return RTK_ERR_INVALID_ARG;
    }
    if (node->kind == 0u) {
        node->val_or_ptr = (uintptr_t)new_val;
        return RTK_OK;
    }
    if (node->kind == 2u) {
        if (node->val_or_ptr == 0u) {
            return 0x8000000fu;
        }
        *(uint32_t *)node->val_or_ptr = new_val;
        return RTK_OK;
    }
    return RTK_ERR_UNSUPPORTED;
}
