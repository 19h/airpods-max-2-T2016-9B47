#include "rkos/rkos.h"

/*
 * Reconstructed FTAB/BMID/remap helpers (`0x3FAC`, `0x4924`, `0x4B84`, `0x4DC0`,
 * `0x4DE8`) and RTKit crashlog subsystem (`0x405C`, `0x5E48`, `0x5E64`, `0x5F70`,
 * `0x5F98`, `0x6348`, `0x67C0`).
 */

/* @provenance RVA 0x3fac sub_3FAC */
uint32_t rkos_fourcc_store_global(uint32_t fourcc)
{
    uint8_t *dst = (uint8_t *)RKOS_VMA_RAW(0x2001DA6Cu, 5u);
    dst[0] = (uint8_t)(fourcc >> 24);
    dst[1] = (uint8_t)(fourcc >> 16);
    dst[2] = (uint8_t)(fourcc >> 8);
    dst[3] = (uint8_t)fourcc;
    dst[4] = 0u;
    return fourcc >> 24;
}

void rkos_fourcc_to_str(uint32_t fourcc, char out[5])
{
    out[0] = (char)(fourcc >> 24);
    out[1] = (char)(fourcc >> 16);
    out[2] = (char)(fourcc >> 8);
    out[3] = (char)fourcc;
    out[4] = '\0';
}

/* @provenance RVA 0x4dc0 sub_4DC0 */
bool rkos_bmid_validate(uint32_t bmid_va)
{
    if (bmid_va == 0u) {
        return false;
    }
    const rkos_bmid_hdr_t *hdr = RKOS_VMA_PTR(const rkos_bmid_hdr_t *, bmid_va);
    return hdr->magic == RKOS_BMID_MAGIC &&
           hdr->magic_hi == 0u &&
           hdr->version == RKOS_BMID_VERSION;
}

/* @provenance RVA 0x4de8 sub_4DE8 */
uint32_t rkos_bmid_write_range(uint32_t base_va, uint32_t bmid_va, uint32_t slot,
                               uint32_t start_off, uint32_t length)
{
    if (!rkos_bmid_validate(bmid_va)) {
        return 0x80000008u;
    }

    const rkos_bmid_hdr_t *hdr = RKOS_VMA_PTR(const rkos_bmid_hdr_t *, bmid_va);
    uint32_t desc_off = *RKOS_VMA_PTR(const uint32_t *, base_va + hdr->desc_offset + 4u);
    uint32_t max_slot = *RKOS_VMA_PTR(const uint32_t *, base_va + desc_off);
    if (max_slot < slot) {
        return 0x80000001u;
    }

    uint32_t entries_rel = *RKOS_VMA_PTR(const uint32_t *, base_va + desc_off + 4u);
    uint32_t entry_va = base_va + entries_rel + 0x30u * slot;
    *RKOS_VMA_PTR(uint32_t *, entry_va + 0x10u) = start_off;
    *RKOS_VMA_PTR(uint32_t *, entry_va + 0x14u) = start_off + length - 1u;
    return 0u;
}

/* @provenance RVA 0x4b84 sub_4B84 */
uint32_t rkos_remap_build_table(uint32_t text_reserve, uint32_t total_budget,
                                uint32_t cfg_va, uint32_t count)
{
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE18u) = cfg_va;
    *RKOS_VMA_PTR(uint32_t *, 0x2001DE1Cu) = count;

    *RKOS_VMA_PTR(uint32_t *, 0x20000FA0u) = 0u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FA4u) = 0x20000u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FA8u) = 0x20000000u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FB4u) = 0x10000u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FBCu) = 0x40000u;

    uint32_t eff_reserve = (text_reserve == 0u) ? 0x20000u : text_reserve;
    uint32_t span = total_budget - eff_reserve;
    uint32_t base_cursor = span + 0x1FFB0000u;
    uint32_t map_cursor  = base_cursor + 0x10000u;

    *RKOS_VMA_PTR(uint32_t *, 0x20000FACu) = span;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FB0u) = base_cursor;
    *RKOS_VMA_PTR(uint32_t *, 0x20000FB8u) = map_cursor;

    uint32_t rem_pool = 0x40000u;
    int seen_primary = 0;

    for (uint32_t i = 0u; i < count; i++) {
        rkos_remap_entry_t *ent = RKOS_VMA_PTR(rkos_remap_entry_t *, cfg_va + i * 12u);
        uint32_t sz = ent->size;
        if (sz > rem_pool) {
            return 0x8000000Eu;
        }
        if (sz == 0u) {
            sz = rem_pool;
            ent->size = sz;
        }
        ent->mapped_va = map_cursor;
        if (ent->kind == 1u) {
            if (seen_primary != 0) {
                return 0x80000001u;
            }
            seen_primary = 1;
        }
        rem_pool -= sz;
        map_cursor += sz;
    }
    return 0u;
}

/* @shim RVA 0x405c rtkit_crashlog_init (pending_c: not shown equivalent to raw) */
void rtkit_crashlog_init(uint32_t shm_va)
{
    uint32_t cfg_va = *RKOS_VMA_PTR(const uint32_t *, 0x2001DE18u);
    uint32_t count  = *RKOS_VMA_PTR(const uint32_t *, 0x2001DE1Cu);
    uint32_t clog_base = 0u;
    uint32_t clog_size = 0u;
    uint32_t found = 0u;

    for (uint32_t i = 0u; i < count; i++) {
        const rkos_remap_entry_t *ent = RKOS_VMA_PTR(const rkos_remap_entry_t *, cfg_va + i * 12u);
        if (ent->kind == 2u) {
            clog_base = ent->mapped_va;
            clog_size = ent->size;
            found = 1u;
            break;
        }
    }

    if (found == 0u || clog_base == 0u || clog_size == 0u) {
        rtkit_panic("!clog");
    }

    uint32_t rel_bmid = *RKOS_VMA_PTR(const uint32_t *, shm_va);
    uint32_t bmid_va  = shm_va + rel_bmid;
    if (rkos_bmid_validate(bmid_va)) {
        const rkos_bmid_hdr_t *hdr = RKOS_VMA_PTR(const rkos_bmid_hdr_t *, bmid_va);
        uint32_t root_rel = *RKOS_VMA_PTR(const uint32_t *, shm_va + hdr->desc_offset + 8u);
        uint32_t root_va  = shm_va + root_rel;
        *RKOS_VMA_PTR(uint32_t *, root_va + 0x10u) = clog_base;
        *RKOS_VMA_PTR(uint32_t *, root_va + 0x14u) = clog_base + clog_size - 1u;

        if ((rkos_bmid_write_range(shm_va, bmid_va, 0u, clog_base + 0x2000u, 0x2000u) & 0x90000000u) != 0u) {
            rtkit_panic("!cl_p0");
        }
        if ((rkos_bmid_write_range(shm_va, bmid_va, 1u, clog_base + 0x4000u, 0x4000u) & 0x90000000u) != 0u) {
            rtkit_panic("!cl_p1");
        }
    }
}

int rkos_ftab_parse_header_buf(const void *buf, size_t buf_len, uint32_t *out_count)
{
    if (buf == NULL || buf_len < sizeof(rkos_ftab_hdr_t)) {
        return -1;
    }
    const rkos_ftab_hdr_t *hdr = (const rkos_ftab_hdr_t *)buf;
    if (rkos_timingsafe_bcmp(hdr->magic, "rkosftab", 8u) != 0) {
        return -1;
    }
    uint32_t cnt = hdr->entry_count;
    if (buf_len < sizeof(rkos_ftab_hdr_t) + (size_t)cnt * sizeof(rkos_ftab_entry_t)) {
        return -1;
    }
    if (out_count != NULL) {
        *out_count = cnt;
    }
    return 0;
}

int rkos_ftab_lookup_tag_buf(const void *buf, size_t buf_len, const char tag[4],
                             uint32_t *out_off, uint32_t *out_len)
{
    uint32_t count = 0u;
    if (rkos_ftab_parse_header_buf(buf, buf_len, &count) != 0) {
        return -1;
    }
    const rkos_ftab_entry_t *entries =
        (const rkos_ftab_entry_t *)((const uint8_t *)buf + sizeof(rkos_ftab_hdr_t));
    for (uint32_t i = 0u; i < count; i++) {
        if (rkos_memcmp(entries[i].tag, tag, 4u) == 0) {
            if (out_off != NULL) {
                *out_off = entries[i].data_offset;
            }
            if (out_len != NULL) {
                *out_len = entries[i].data_length;
            }
            return 0;
        }
    }
    return -1;
}

/* @shim RVA 0x4800 sub_4800 (pending_c: not shown equivalent to raw) */
uint32_t rkos_vec16_push(uint32_t vec_va, const uint32_t item[4])
{
    uint32_t *vec = RKOS_VMA_PTR(uint32_t *, vec_va);
    uint32_t cap = vec[0];
    uint32_t len = vec[1];
    uint32_t data_va = vec[2];

    if (cap == len) {
        uint32_t new_cap = (cap == 0u) ? 8u : (2u * cap);
        data_va = rkos_realloc(data_va, 16u * new_cap);
        if (data_va == 0u) {
            return 0x80000005u;
        }
        len = vec[1];
        vec[0] = new_cap;
        vec[2] = data_va;
    }

    uint32_t *slot = RKOS_VMA_PTR(uint32_t *, data_va + 16u * len);
    vec[1] = len + 1u;
    slot[0] = item[0];
    slot[1] = item[1];
    slot[2] = item[2];
    slot[3] = item[3];
    return 0u;
}

/* @provenance RVA 0x4998 sub_4998 */
uint32_t rkos_status_unsupported(void)
{
    return 0x80000003u;
}

/* @shim RVA 0x49a0 sub_49A0 (pending_c: not shown equivalent to raw) */
uint32_t rkos_dev_claim_open(uint32_t obj_va)
{
    uint8_t *open_flag = RKOS_VMA_PTR(uint8_t *, obj_va + 4u);
    if (*open_flag != 0u) {
        return 0x80000009u;
    }
    *open_flag = 1u;
    return 0u;
}

/* @shim RVA 0x49b4 sub_49B4 (pending_c: not shown equivalent to raw) */
uint32_t rkos_dev_release_open(uint32_t obj_va)
{
    uint8_t *open_flag = RKOS_VMA_PTR(uint8_t *, obj_va + 4u);
    if (*open_flag != 1u) {
        return 0x80000006u;
    }
    *open_flag = 0u;
    return 0u;
}

/* @provenance RVA 0xef7c sub_EF7C */
uint32_t rkos_ftab_stream_seek(uint32_t stream_va, uint32_t offset)
{
    uint32_t size = *RKOS_VMA_PTR(const uint32_t *, stream_va + 4u);
    if (offset > size) {
        return 0x80000001u;
    }
    if (size == 0u) {
        return 0x8000000Bu;
    }
    *RKOS_VMA_PTR(uint32_t *, stream_va + 8u) = offset;
    return 0u;
}

/* @provenance RVA 0xef98 sub_EF98 */
uint32_t rkos_ftab_stream_get_size(uint32_t stream_va, uint32_t *out_size)
{
    uint32_t size = *RKOS_VMA_PTR(const uint32_t *, stream_va + 4u);
    *out_size = size;
    return size;
}

/* @provenance RVA 0xef9e sub_EF9E */
uint32_t rkos_ftab_stream_get_backend(uint32_t stream_va, uint32_t *out_backend)
{
    uint32_t backend = *RKOS_VMA_PTR(const uint32_t *, stream_va + 0x10u);
    *out_backend = backend;
    return backend;
}

/* @provenance RVA 0xefa4 sub_EFA4 */
uint32_t rkos_pair_fill_same(uint32_t val, uint32_t unused_arg,
                             uint32_t *out_first, uint32_t *out_second)
{
    (void)unused_arg;
    *out_first = val;
    *out_second = val;
    return val;
}

/* @shim RVA 0x5f70 sub_5F70 (pending_c: not shown equivalent to raw) */
bool rkos_crashlog_is_active(void)
{
    const uint8_t *cl = (const uint8_t *)RKOS_VMA_RAW(RKOS_CRASHLOG_STATE_VA, 0x64u);
    uint32_t depth = *(const uint32_t *)(const void *)(cl + 0x54u);
    uint8_t reason = cl[0x34u];
    return (depth > 1u) || ((uint32_t)(reason - 1u) < 0x3Fu);
}

/* @shim RVA 0x5f98 watchdog_expire_panic (pending_c: not shown equivalent to raw) */
void watchdog_expire_panic(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    uint32_t saved_sp = *RKOS_VMA_PTR(const uint32_t *, 0x20000D40u);
    crashlog_dump(0x21u, saved_sp);
    rtkit_halt(6u);
}

/* @shim RVA 0x5e64 sub_5E64 (pending_c: not shown equivalent to raw) */
uint32_t rkos_crashlog_register_OnList(uint32_t list_va, uint32_t sig, uint32_t ver,
                                       uint32_t cb_va, uint32_t ctx_va)
{
    if (cb_va == 0u) {
        return 0x80000001u;
    }
    uint32_t node_va = rtkit_malloc(0x18u);
    if (node_va == 0u) {
        return 0x80000005u;
    }

    uint32_t *node = RKOS_VMA_PTR(uint32_t *, node_va);
    node[0] = sig;
    node[1] = ver;
    node[2] = cb_va;
    node[3] = ctx_va;
    node[4] = 0u;

    uint32_t lock_state = mutex_lock(RKOS_CRASHLOG_STATE_VA + 0x50u);
    uint32_t *tail_slot = RKOS_VMA_PTR(uint32_t *, list_va + 4u);
    uint32_t prev_tail = *tail_slot;
    if (prev_tail == 0u) {
        prev_tail = list_va;
    }
    *tail_slot = node_va + 0x10u;
    *RKOS_VMA_PTR(uint32_t *, prev_tail) = node_va;
    node[5] = prev_tail;
    rkos_spinlock_release(RKOS_CRASHLOG_STATE_VA + 0x50u, lock_state);
    return 0u;
}

/* @shim RVA 0x5e48 sub_5E48 (pending_c: not shown equivalent to raw) */
uint32_t rkos_crashlog_register_section(uint32_t sig, uint32_t ver,
                                        uint32_t cb_va, uint32_t ctx_va)
{
    return rkos_crashlog_register_OnList(RKOS_CRASHLOG_STATE_VA + 8u, sig, ver, cb_va, ctx_va);
}

/* @shim RVA 0x6348 crashlog_print_banner (pending_c: not shown equivalent to raw) */
void crashlog_print_banner(const char *client_name)
{
    const char *rtk_ver = (const char *)RKOS_VMA_RAW(0x00016090u, 64u);
    (void)rkos_printf("RTKit: %s - Client: %s\n", rtk_ver,
                      (client_name != NULL) ? client_name : "AirPods3,6");
}

/* @shim RVA 0x67c0 crashlog_dump (pending_c: not shown equivalent to raw) */
void crashlog_dump(uint32_t reason, uint32_t fault_pc)
{
    uint8_t *cl = (uint8_t *)RKOS_VMA_RAW(RKOS_CRASHLOG_STATE_VA, 0x64u);
    uint32_t *depth = (uint32_t *)(void *)(cl + 0x54u);
    (*depth)++;

    uint32_t buf_va = *(const uint32_t *)(const void *)(cl + 0x18u);
    if (buf_va != 0u) {
        rkos_clhe_hdr_t *hdr = RKOS_VMA_PTR(rkos_clhe_hdr_t *, buf_va);
        hdr->signature     = RKOS_CLHE_MAGIC;
        hdr->version       = RKOS_CLHE_VER_3_3;
        hdr->total_length  = sizeof(rkos_clhe_hdr_t);
        hdr->flags_reason  = (reason & 0xFFu) | 0x100u;
        hdr->fault_pc      = fault_pc;
        hdr->reserved14    = 0u;
        hdr->nested_reason = 0u;
        hdr->platform_tag  = *(const uint32_t *)(const void *)cl;
    }
}
