#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_compat.h"

struct rkof_host_cpu_state g_rkof_host_cpu;
struct rkof_host_panic_trap g_rkof_panic_trap;

static uint8_t g_rom_buf[0x200000];
static uint8_t g_sram_buf[0x100000];      /* 0x20000000..0x200FFFFF */
static uint8_t g_remap_buf[0x1000];       /* 0x4000C000..0x4000CFFF */
static uint8_t g_uart_buf[0x1000];        /* 0x42019000..0x42019FFF */
static uint8_t g_pmgr_buf[0x40000];       /* 0x42440000..0x4247FFFF */
static uint8_t g_w3_radio_buf[0x1000];    /* 0x4D000000..0x4D000FFF */
static uint8_t g_w3_top_buf[0x1000];      /* 0x4D140000..0x4D140FFF */
static uint8_t g_w3_modem_buf[0x2000];    /* 0x4D154000..0x4D155FFF */
static uint8_t g_phy_sram_buf[0x20000];   /* 0x4D280000..0x4D29FFFF */
static uint8_t g_scs_buf[0x2000];         /* 0xE000E000..0xE000FFFF */
static uint8_t g_misc_mmio_buf[0x10000];  /* Fallback for 0x40000000..0x5FFFFFFF */
static uint32_t g_host_scratch_off = 0u;
static uint32_t g_scratch_rtsi;
static uint8_t g_zero_thread_sentinel[64];

static int g_remap_auto_ack = 1;
static int g_pmgr_clk_auto_ack = 1;
static uint32_t g_mock_pwr_state = 0u;
static uint32_t g_pwr_set_calls = 0u;
static uint64_t g_mock_timer_ticks = 0u;
static uint8_t g_mock_heap_arena[65536];
static size_t g_mock_heap_off = 0u;
static int g_reset_handler_called = 0;

static int host_mock_pwr_get(uintptr_t obj, uint32_t domain_id, uint32_t *state_out)
{
    (void)obj;
    (void)domain_id;
    *state_out = g_mock_pwr_state;
    return 0;
}

static int host_mock_pwr_set(uintptr_t obj, uint32_t domain_id, uint32_t state)
{
    (void)obj;
    (void)domain_id;
    g_mock_pwr_state = state;
    g_pwr_set_calls++;
    return 0;
}

static uintptr_t g_host_pwr_vtable[4];

void rkof_host_bus_reset(const uint8_t *raw_image, size_t raw_len)
{
    memset(&g_rkof_host_cpu, 0, sizeof(g_rkof_host_cpu));
    memset(&g_rkof_panic_trap, 0, sizeof(g_rkof_panic_trap));
    memset(g_rom_buf, 0, sizeof(g_rom_buf));
    memset(g_sram_buf, 0, sizeof(g_sram_buf));
    memset(g_remap_buf, 0, sizeof(g_remap_buf));
    memset(g_uart_buf, 0, sizeof(g_uart_buf));
    memset(g_pmgr_buf, 0, sizeof(g_pmgr_buf));
    memset(g_w3_radio_buf, 0, sizeof(g_w3_radio_buf));
    memset(g_w3_top_buf, 0, sizeof(g_w3_top_buf));
    memset(g_w3_modem_buf, 0, sizeof(g_w3_modem_buf));
    memset(g_phy_sram_buf, 0, sizeof(g_phy_sram_buf));
    memset(g_scs_buf, 0, sizeof(g_scs_buf));
    memset(g_zero_thread_sentinel, 0, sizeof(g_zero_thread_sentinel));
    g_scratch_rtsi = 0u;
    g_remap_auto_ack = 1;
    g_pmgr_clk_auto_ack = 1;
    g_mock_pwr_state = 2u;
    g_pwr_set_calls = 0u;
    g_mock_timer_ticks = 100u;
    g_mock_heap_off = 0u;
    g_reset_handler_called = 0;

    if (raw_image != NULL) {
        size_t rom_copy = (raw_len < RKOF_ROM_RODATA_END) ? raw_len : RKOF_ROM_RODATA_END;
        memcpy(g_rom_buf, raw_image, rom_copy);
        if (raw_len >= RKOF_RAM_DATA_FILE_OFF + RKOF_RAM_DATA_SIZE) {
            memcpy(g_sram_buf, raw_image + RKOF_RAM_DATA_FILE_OFF, RKOF_RAM_DATA_SIZE);
        }
    }

    g_host_pwr_vtable[0] = 0u;
    g_host_pwr_vtable[0x0Cu / sizeof(uintptr_t)] = (uintptr_t)host_mock_pwr_set;
    g_host_pwr_vtable[0x14u / sizeof(uintptr_t)] = (uintptr_t)host_mock_pwr_get;
    *(uintptr_t *)(void *)(g_sram_buf + (RKOF_DATA_PWR_VTABLE_PTR_ADDR - 0x20000000u)) =
        (uintptr_t)g_host_pwr_vtable;
}

void rkof_host_set_remap_auto_ack(int enable)
{
    g_remap_auto_ack = enable;
}

void rkof_host_set_pmgr_clk_auto_ack(int enable)
{
    g_pmgr_clk_auto_ack = enable;
}

void rkof_host_set_mock_pwr_state(uint32_t state)
{
    g_mock_pwr_state = state;
}

uint32_t rkof_host_get_mock_pwr_state(void)
{
    return g_mock_pwr_state;
}

uint32_t rkof_host_get_pwr_set_calls(void)
{
    return g_pwr_set_calls;
}

void *rkof_host_translate_ptr(uintptr_t addr)
{
    if (addr >= 4u && addr < 64u) {
        return g_zero_thread_sentinel + addr;
    }
    if (addr < sizeof(g_rom_buf)) {
        return g_rom_buf + addr;
    }
    if (addr >= 0x20000000u && addr < 0x20100000u) {
        return g_sram_buf + (addr - 0x20000000u);
    }
    if (addr == RKOF_SCRATCH_RTSI_ADDR) {
        return &g_scratch_rtsi;
    }
    if (addr >= 0x4000C000u && addr < 0x4000D000u) {
        return g_remap_buf + (addr - 0x4000C000u);
    }
    if (addr >= 0x42019000u && addr < 0x4201A000u) {
        return g_uart_buf + (addr - 0x42019000u);
    }
    if (addr >= 0x42440000u && addr < 0x42480000u) {
        return g_pmgr_buf + (addr - 0x42440000u);
    }
    if (addr >= 0x4D000000u && addr < 0x4D001000u) {
        return g_w3_radio_buf + (addr - 0x4D000000u);
    }
    if (addr >= 0x4D140000u && addr < 0x4D141000u) {
        return g_w3_top_buf + (addr - 0x4D140000u);
    }
    if (addr >= 0x4D154000u && addr < 0x4D156000u) {
        return g_w3_modem_buf + (addr - 0x4D154000u);
    }
    if (addr >= 0x4D280000u && addr < 0x4D2A0000u) {
        return g_phy_sram_buf + (addr - 0x4D280000u);
    }
    if (addr >= 0xE000E000u && addr < 0xE0010000u) {
        return g_scs_buf + (addr - 0xE000E000u);
    }
    if (addr >= 0x40000000u && addr < 0x60000000u) {
        return g_misc_mmio_buf + (addr & 0xFFFCu);
    }
    return (void *)addr;
}

uint32_t rkof_host_scratch_frame(uint32_t byte_size);
uint32_t rkof_host_scratch_frame(uint32_t byte_size)
{
    uint32_t aligned = (byte_size + 7u) & ~7u;
    if (aligned > 0x8000u) {
        aligned = 0x8000u;
    }
    if (g_host_scratch_off + aligned > 0x10000u) {
        g_host_scratch_off = 0u;
    }
    uint32_t guest_addr = 0x200EC000u + g_host_scratch_off;
    g_host_scratch_off += aligned;
    return guest_addr;
}

uint32_t rkof_host_vcall(uint32_t fn_addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
uint32_t rkof_host_vcall(uint32_t fn_addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)fn_addr;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 1u;
}

uint32_t rkof_host_mmio_read32(uintptr_t addr)
{
    if (addr == RKOF_REMAP_ACK_ADDR && g_remap_auto_ack) {
        return 1u;
    }
    const uint32_t *ptr = (const uint32_t *)rkof_host_translate_ptr(addr);
    return *ptr;
}

void rkof_host_mmio_write32(uintptr_t addr, uint32_t val)
{
    if (g_pmgr_clk_auto_ack && addr >= 0x42440400u && addr <= 0x42440420u) {
        if ((val & 0x0Fu) == 0x0Fu) {
            val |= 0xF0u;
        } else if ((val & 0x0Fu) == 0u) {
            val &= ~0xF0u;
        }
    }
    uint32_t *ptr = (uint32_t *)rkof_host_translate_ptr(addr);
    *ptr = val;
}

uint16_t rkof_host_mmio_read16(uintptr_t addr)
{
    return *(const uint16_t *)rkof_host_translate_ptr(addr);
}

void rkof_host_mmio_write16(uintptr_t addr, uint16_t val)
{
    *(uint16_t *)rkof_host_translate_ptr(addr) = val;
}

uint8_t rkof_host_mmio_read8(uintptr_t addr)
{
    return *(const uint8_t *)rkof_host_translate_ptr(addr);
}

void rkof_host_mmio_write8(uintptr_t addr, uint8_t val)
{
    *(uint8_t *)rkof_host_translate_ptr(addr) = val;
}

/* Host mocks for architectural veneers & external pending_c functions */
void Reset_Handler(void)
{
    g_reset_handler_called = 1;
}

void NMI_Handler(void) {}
void Fault_Handler(void) {}
void SVCall_Handler(void) {}
void Default_Handler(void) {}

void rtkit_app_init(void);
void rtkit_app_init(void) {}

void *rtkit_heap_alloc(void *heap, size_t size);
void *rtkit_heap_alloc(void *heap, size_t size)
{
    (void)heap;
    size_t aligned = (size + 7u) & ~7u;
    if (g_mock_heap_off + aligned > sizeof(g_mock_heap_arena)) {
        return NULL;
    }
    void *res = g_mock_heap_arena + g_mock_heap_off;
    g_mock_heap_off += aligned;
    return res;
}

void rtkit_heap_free(void *heap, void *ptr, uint32_t mode);
void rtkit_heap_free(void *heap, void *ptr, uint32_t mode)
{
    (void)heap;
    (void)ptr;
    (void)mode;
}

void sub_141B0(void *log_pack);
void sub_141B0(void *log_pack) { *(uint16_t *)log_pack = 0u; }

void sub_141B8(void *log_pack, uint32_t tag_id);
void sub_141B8(void *log_pack, uint32_t tag_id)
{
    uint32_t *w = (uint32_t *)log_pack;
    w[0] = 0x04000100u;
    w[1] = tag_id;
}

int sub_15830(void);
int sub_15830(void) { return 0; }

void sub_167E8(uint8_t *heap, uint32_t enable);
void sub_167E8(uint8_t *heap, uint32_t enable) { (void)heap; (void)enable; }

void sub_17C98(void);
void sub_17C98(void) {}

void sub_17CEC(void);
void sub_17CEC(void) {}

void sub_1878C(void *log_pack, uint32_t media_id, uint32_t off, uint32_t exp_crc, uint32_t got_crc);
void sub_1878C(void *log_pack, uint32_t media_id, uint32_t off, uint32_t exp_crc, uint32_t got_crc)
{
    (void)log_pack;
    (void)media_id;
    (void)off;
    (void)exp_crc;
    (void)got_crc;
}

void sub_189C4(void *log_pack, const char *arg0);
void sub_189C4(void *log_pack, const char *arg0) { (void)log_pack; (void)arg0; }

void sub_18EC0(void *log_pack, uint32_t tag_id, uint32_t len);
void sub_18EC0(void *log_pack, uint32_t tag_id, uint32_t len)
{
    (void)log_pack;
    (void)tag_id;
    (void)len;
}

void sub_1A5A8(uint32_t coproc_id);
void sub_1A5A8(uint32_t coproc_id) { (void)coproc_id; }

void sub_1A5F4(uintptr_t log_token, const void *log_pack);
void sub_1A5F4(uintptr_t log_token, const void *log_pack) { (void)log_token; (void)log_pack; }

void sub_1A646(uintptr_t log_token, const void *log_pack);
void sub_1A646(uintptr_t log_token, const void *log_pack) { (void)log_token; (void)log_pack; }

void *sub_1AA14(void *heap_desc);
void *sub_1AA14(void *heap_desc) { return heap_desc; }

int sub_1BA50(void);
int sub_1BA50(void) { return 0; }

int sub_1BA72(void);
int sub_1BA72(void) { return 0; }

int sub_22F14(uint32_t ep, uintptr_t cb_fn, uint32_t arg);
int sub_22F14(uint32_t ep, uintptr_t cb_fn, uint32_t arg)
{
    (void)ep;
    (void)cb_fn;
    (void)arg;
    return 0;
}

void sub_23B78(uint32_t dst_id, uint32_t op, uint32_t subop, uint32_t len, const void *payload);
void sub_23B78(uint32_t dst_id, uint32_t op, uint32_t subop, uint32_t len, const void *payload)
{
    (void)dst_id;
    (void)op;
    (void)subop;
    (void)len;
    (void)payload;
}

void sub_C7A8C(uint32_t err_code, uint32_t subcode);
void sub_C7A8C(uint32_t err_code, uint32_t subcode) { (void)err_code; (void)subcode; }

void sub_F1C94(void);
void sub_F1C94(void) {}

void sub_F1D1C(void);
void sub_F1D1C(void) {}

void sub_186A8C(uintptr_t region_base, size_t region_size);
void sub_186A8C(uintptr_t region_base, size_t region_size)
{
    (void)region_base;
    (void)region_size;
}

void *sub_186BE4(void *old_ptr, size_t new_size);
void *sub_186BE4(void *old_ptr, size_t new_size)
{
    void *next = rtkit_heap_alloc(NULL, new_size);
    if (next != NULL && old_ptr != NULL) {
        memcpy(next, old_ptr, new_size / 2u);
    }
    return next;
}

void sub_187222(const char *str);
void sub_187222(const char *str) { (void)str; }

void sub_187240(const char *fmt, va_list ap);
void sub_187240(const char *fmt, va_list ap)
{
    if (fmt != NULL) {
        va_list copy;
        va_copy(copy, ap);
        vsnprintf(g_rkof_panic_trap.last_msg, sizeof(g_rkof_panic_trap.last_msg), fmt, copy);
        va_end(copy);
    }
}

struct host_fmt_sink {
    uintptr_t putc_fn;
    size_t    cap;
    char     *dst;
    size_t    written;
    uint32_t  flags;
};

int sub_18726C(void *sink_ctx, const char *fmt, va_list ap);
int sub_18726C(void *sink_ctx, const char *fmt, va_list ap)
{
    struct host_fmt_sink *sink = (struct host_fmt_sink *)sink_ctx;
    int total = vsnprintf(sink->dst, sink->cap, fmt, ap);
    if (total > 0) {
        sink->written = (size_t)total;
    }
    return total;
}

uint32_t sub_187A64(const char *str, char **endptr, int base);
uint32_t sub_187A64(const char *str, char **endptr, int base)
{
    return (uint32_t)strtoul(str, endptr, base);
}

int sub_1880F8(uint32_t tag_fourcc, uint32_t flags, uintptr_t cb_fn, uint32_t ctx);
int sub_1880F8(uint32_t tag_fourcc, uint32_t flags, uintptr_t cb_fn, uint32_t ctx)
{
    (void)tag_fourcc;
    (void)flags;
    (void)cb_fn;
    (void)ctx;
    return 0;
}

void sub_188300(uint32_t code, uint32_t subcode, const char *fmt, va_list ap);
void sub_188300(uint32_t code, uint32_t subcode, const char *fmt, va_list ap)
{
    (void)code;
    (void)subcode;
    (void)fmt;
    (void)ap;
}

void sub_1887D8(void);
void sub_1887D8(void) {}

int sub_18E930(uint32_t *word_out, uint16_t aligned_off);
int sub_18E930(uint32_t *word_out, uint16_t aligned_off)
{
    *word_out = 0xFFFFFFFFu - (uint32_t)aligned_off;
    return 0;
}

uint32_t sub_19CBB4(uint32_t *out_a, uint32_t *out_b);
uint32_t sub_19CBB4(uint32_t *out_a, uint32_t *out_b)
{
    *out_a = 0x200A0000u;
    *out_b = 0x1000u;
    return 0u;
}

uint32_t sub_19CC9C(uint32_t *out_val);
uint32_t sub_19CC9C(uint32_t *out_val)
{
    *out_val = 0xCAFE1234u;
    return 0u;
}

RKOF_NORETURN void sub_1A234C(void);
RKOF_NORETURN void sub_1A234C(void)
{
    if (g_rkof_panic_trap.armed) {
        g_rkof_panic_trap.triggered = 1;
        longjmp(g_rkof_panic_trap.env, 1);
    }
    abort();
}

void sub_1A295C(void *lock_obj, uint32_t token);
void sub_1A295C(void *lock_obj, uint32_t token) { (void)lock_obj; (void)token; }

uint32_t sub_1A2A3C(void *lock_obj);
uint32_t sub_1A2A3C(void *lock_obj) { (void)lock_obj; return 7u; }

uint64_t sub_1A3204(void *timer_dev);
uint64_t sub_1A3204(void *timer_dev)
{
    (void)timer_dev;
    g_mock_timer_ticks += 250u;
    return g_mock_timer_ticks;
}

uint32_t sub_1A375C(uintptr_t addr);
uint32_t sub_1A375C(uintptr_t addr)
{
    return (addr >= 0x100u && addr < RKOF_ROM_RODATA_END) ? 1u : 0u;
}

void sub_13328(uint32_t coproc_slot);
void sub_13328(uint32_t coproc_slot) { (void)coproc_slot; }
