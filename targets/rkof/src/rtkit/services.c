#include <stdarg.h>
#include "rkof/memmap.h"
#include "rkof/rtkit.h"

extern void rtkit_heap_free(void *heap, void *ptr, uint32_t mode);
extern void *rtkit_heap_alloc(void *heap, size_t size);
extern void sub_167E8(uint8_t *heap, uint32_t enable);
extern void *sub_186BE4(void *old_ptr, size_t new_size);
extern uint32_t sub_1A2A3C(void *lock_obj);
extern void sub_1A295C(void *lock_obj, uint32_t token);
extern void *sub_1AA14(void *heap_desc);
extern void sub_186A8C(uintptr_t region_base, size_t region_size);
extern void sub_C7A8C(uint32_t err_code, uint32_t subcode);

/* @provenance RVA 0x16d20 sub_16D20 */
void sub_16D20(void *heap, void *ptr)
{
    rtkit_heap_free(heap, ptr, 1u);
}

/* @provenance RVA 0x16d26 sub_16D26 */
uint32_t sub_16D26(const uint8_t *heap)
{
    const uint8_t *link_slot = heap + 0x48u;
    const uint8_t *last_node = NULL;
    const uint8_t *cur_node = NULL;

    for (;;) {
        last_node = cur_node;
        uintptr_t next_addr = *(const uintptr_t *)(const void *)link_slot;
        if (next_addr == 0u) {
            break;
        }
        cur_node = RKOF_PTR(const uint8_t *, next_addr);
        link_slot = cur_node + 0x0Cu;
    }

    if (last_node == NULL) {
        return 0u;
    }

    uint32_t raw_sz = *(const uint32_t *)(const void *)(last_node + 4u);
    return (raw_sz & ~3u) - 0x10u;
}

/* @provenance RVA 0x16d46 sub_16D46 */
void sub_16D46(uint8_t *heap, uint32_t enable)
{
    uint32_t cur = (uint32_t)(heap[0x4C] & 1u);
    if (enable == cur) {
        return;
    }
    heap[0x4C] = (uint8_t)enable;
    if (enable != 0u) {
        sub_167E8(heap, enable);
    }
}

/* @provenance RVA 0x1ca44 rtkit_shell_register_cmd */
uint32_t rtkit_shell_register_cmd(
    void *shell_ctx,
    const char *name,
    const char *help,
    rkof_shell_cmd_fn handler)
{
    if (shell_ctx == NULL) {
        return 0x80000008u;
    }

    uint8_t *ctx_bytes = (uint8_t *)shell_ctx;
    if ((ctx_bytes[0x234] & 1u) == 0u) {
        return 0x80000008u;
    }

    void *lock_ptr = ctx_bytes + 0x230u;
    uint32_t lock_token = sub_1A2A3C(lock_ptr);

    uint32_t cap = *(const uint32_t *)(const void *)(ctx_bytes + 0x224u);
    uint32_t count = *(const uint32_t *)(const void *)(ctx_bytes + 0x220u);
    uintptr_t table_addr = *(const uintptr_t *)(const void *)(ctx_bytes + 4u);
    uintptr_t *entries = (table_addr != 0u) ? RKOF_PTR(uintptr_t *, table_addr) : NULL;
    uint32_t status = 0x80000008u;

    if (count >= cap - 1u) {
        size_t grow_bytes = (size_t)(0x3Cu + cap * 12u);
        void *new_buf = sub_186BE4((void *)table_addr, grow_bytes);
        if (new_buf == NULL) {
            sub_1A295C(lock_ptr, lock_token);
            return status;
        }
        cap = *(const uint32_t *)(const void *)(ctx_bytes + 0x224u);
        count = *(const uint32_t *)(const void *)(ctx_bytes + 0x220u);
        *(uintptr_t *)(void *)(ctx_bytes + 4u) = (uintptr_t)new_buf;
        *(uint32_t *)(void *)(ctx_bytes + 0x224u) = cap + 5u;
        entries = RKOF_PTR(uintptr_t *, (uintptr_t)new_buf);
    }

    uintptr_t *slot = &entries[count * 3u];
    slot[0] = (uintptr_t)name;
    slot[1] = (uintptr_t)help;
    slot[2] = (uintptr_t)handler;
    *(uint32_t *)(void *)(ctx_bytes + 0x220u) = count + 1u;
    status = 0u;

    sub_1A295C(lock_ptr, lock_token);
    return status;
}

/* @provenance RVA 0xc7ab4 spicy_log_emit */
void spicy_log_emit(uint32_t log_id, ...)
{
    va_list ap;
    va_start(ap, log_id);
    uintptr_t logger_addr = *RKOF_PTR(const uintptr_t *, STATE_SPICY_LOG_BASE);
    if (logger_addr == 0u) {
        va_end(ap);
        sub_C7A8C(0x005E4305u, 0u);
        rtkit_panic("Fatal error 0x%x", 0x005E4305u);
    }
    const uintptr_t *vtable = *RKOF_PTR(const uintptr_t *const *, logger_addr);
    typedef void (*spicy_emit_fn)(void *, uint32_t, va_list);
    spicy_emit_fn fn = (spicy_emit_fn)vtable[0x24u / sizeof(uintptr_t)];
    fn((void *)logger_addr, log_id, ap);
    va_end(ap);
}

/* @provenance RVA 0xc7af4 spicy_log_emit_ex */
void spicy_log_emit_ex(uint32_t level, uint32_t log_id, ...)
{
    va_list ap;
    va_start(ap, log_id);
    uintptr_t logger_addr = *RKOF_PTR(const uintptr_t *, STATE_SPICY_LOG_BASE);
    if (logger_addr == 0u) {
        va_end(ap);
        sub_C7A8C(0x005E4306u, 0u);
        rtkit_panic("Fatal error 0x%x", 0x005E4306u);
    }
    const uintptr_t *vtable = *RKOF_PTR(const uintptr_t *const *, logger_addr);
    typedef void (*spicy_emit_ex_fn)(void *, uint32_t, uint32_t, va_list);
    spicy_emit_ex_fn fn = (spicy_emit_ex_fn)vtable[0x28u / sizeof(uintptr_t)];
    fn((void *)logger_addr, level, log_id, ap);
    va_end(ap);
}

/* @provenance RVA 0x186ac0 rtkit_heap_lock_or_init */
uint32_t rtkit_heap_lock_or_init(void **heap_out)
{
    void *lock_obj = RKOF_PTR(void *, STATE_HEAP_LOCK_BASE);
    uint32_t token = sub_1A2A3C(lock_obj);

    uintptr_t *heap_slot = RKOF_PTR(uintptr_t *, STATE_CORE_PTRS_BASE + 8u);
    if (*heap_slot == 0u) {
        uintptr_t regions[2] = { 0x2000F138u, 0x20025240u };
        size_t sizes[2] = { 0x2000F138u - 0x2000F138u, 0x0001B400u };
        void *created = sub_1AA14(RKOF_PTR(void *, 0x200CD6E4u));
        *heap_slot = (uintptr_t)created;
        for (uint32_t idx = 0u; idx < 2u; idx++) {
            sub_186A8C(regions[idx], sizes[idx]);
        }
    }

    if (heap_out != NULL) {
        *heap_out = (void *)(*heap_slot);
    }
    return token;
}

/* @provenance RVA 0x186b3c rtkit_heap_unlock */
void rtkit_heap_unlock(uint32_t lock_token)
{
    void *lock_obj = RKOF_PTR(void *, STATE_HEAP_LOCK_BASE);
    sub_1A295C(lock_obj, lock_token);
}

/* @provenance RVA 0x186bbc rt_malloc */
void *rt_malloc(size_t size)
{
    uint32_t token = rtkit_heap_lock_or_init(NULL);
    uintptr_t heap_addr = *RKOF_PTR(const uintptr_t *, STATE_CORE_PTRS_BASE + 8u);
    void *block = rtkit_heap_alloc((void *)heap_addr, size);
    rtkit_heap_unlock(token);
    return block;
}

/* @provenance RVA 0x186e14 rt_free */
void rt_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    uint8_t *raw = (uint8_t *)ptr;
    uint32_t guard_tag = *(const uint32_t *)(const void *)(raw - 4u);
    if (guard_tag == 0xAB5C155Au) {
        uintptr_t real_addr = *(const uintptr_t *)(const void *)(raw - 8u);
        ptr = RKOF_PTR(void *, real_addr);
    }

    uint32_t token = rtkit_heap_lock_or_init(NULL);
    uintptr_t heap_addr = *RKOF_PTR(const uintptr_t *, STATE_CORE_PTRS_BASE + 8u);
    sub_16D20((void *)heap_addr, ptr);
    rtkit_heap_unlock(token);
}
