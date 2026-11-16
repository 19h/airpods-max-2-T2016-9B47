#ifndef ACIB_RTKIT_H
#define ACIB_RTKIT_H

#include "acib/acib.h"

#define ACIB_STACK_WATERMARK_LO  0x534B5452u /* "RTKS" */
#define ACIB_STACK_WATERMARK_HI  0x4B434154u /* "TACK" */
#define ACIB_WARM_BOOT_MAGIC     0xCAFE4B0Bu

typedef uint32_t (*acib_irq_handler_fn)(uintptr_t ctx);

struct ACIB_PACKED acib_rtkit_uuid_header {
    char     magic[4];               /* +0x00: "uuid" */
    uint32_t version;                /* +0x04: 5 */
    uint32_t runtime_str_rva;        /* +0x08: 0x000f8860 */
    uint32_t product_str_rva;        /* +0x0C: 0x000f5e40 */
    uint8_t  image_uuid[16];         /* +0x10: e8ded9ec-b944-30a3-9c80-1069bd1d094b */
    uint32_t reserved0;              /* +0x20: 0 */
    uint32_t reserved1;              /* +0x24: 0 */
    uint32_t metadata_rva;           /* +0x28: 0x000f9640 */
    uint32_t metadata_len;           /* +0x2C: 0x00000116 */
    uint32_t rom_end_rva;            /* +0x30: 0x000f984c */
    uint32_t reserved2[3];           /* +0x34..+0x3F: 0 */
    uint32_t runtime_str_rva_dup;    /* +0x40: 0x000f8860 */
    uint32_t product_str_rva_dup;    /* +0x44: 0x000f5e40 */
};

struct acib_fault_summary {
    uint32_t r0_or_code;         /* +0x00 */
    uint32_t zero_04;            /* +0x04 */
    uint32_t fault_sub;          /* +0x08 */
    uint32_t zero_0c;            /* +0x0C */
    uint32_t fault_sub_dup;      /* +0x10 */
    uint32_t zero_14;            /* +0x14 */
    uint32_t pc;                 /* +0x18 */
    uint32_t zero_1c;            /* +0x1C */
    uint32_t lr;                 /* +0x20 */
    uint32_t zero_24;            /* +0x24 */
    uint32_t sp;                 /* +0x28 */
    uint32_t zero_2c;            /* +0x2C */
    uint32_t xpsr;               /* +0x30 */
    uint32_t zero_34;            /* +0x34 */
};

struct acib_clock_ctx {
    uint32_t fallback_cpu_hz;    /* 0x10020968 */
    uint32_t ref_clk_hz;         /* 0x1002096C (40 MHz = 0x02625A00) */
    uint32_t calibrated_cpu_hz;  /* 0x10020970 */
    uint32_t cycles_per_us;      /* 0x10020974 */
    uint32_t inv_clk_ratio;      /* 0x10020978 */
    uint32_t pad_14[4];          /* 0x1002097C..0x1002098B */
    uint32_t deadline_hdr[2];    /* 0x1002098C..0x10020993 */
    uint32_t sleep_state;        /* 0x10020994 */
    uint32_t pad_2c[4];          /* 0x10020998..0x100209A7 */
    uint8_t  low_freq_mode;      /* 0x100209A8 */
    uint8_t  pad_41[3];          /* 0x100209A9..0x100209AB */
};

struct acib_board_props {
    uint32_t btm1;               /* 0x10022984 */
    uint32_t btm2;               /* 0x10022988 */
    uint32_t fpga;               /* 0x1002298C */
    uint8_t  dev_flag;           /* 0x10022990 */
    uint8_t  pad[3];             /* 0x10022991..0x10022993 */
};

/* Pinned BSS globals defined in src/rtkit/image.c */
extern acib_irq_handler_fn   g_rtk_irq_handlers[130];   /* 0x1000EEB4 */
extern uintptr_t             g_rtk_irq_contexts[130];   /* 0x1000F0BC */
extern uint32_t              g_psp_init_stack[384];     /* 0x1001AF80..0x1001B580 */
extern uint32_t              g_msp_stack[512];          /* 0x1001B580..0x1001BD80 */
extern uint32_t              g_boot_mode_ctx[10];       /* 0x1002006C..0x10020094 */
extern uint32_t              g_bt_sys_threads_state[4]; /* 0x10020310..0x10020320 */
extern struct acib_clock_ctx g_acib_clock_ctx;          /* 0x10020968..0x100209AC */
extern uint32_t              g_acib_panic_reason;       /* 0x10020B60 */
extern void                 *g_rtk_thread_ptr_table[16];/* 0x10021C04..0x10021C44 */
extern struct acib_board_props g_acib_board_props;      /* 0x10022984..0x10022994 */
extern uintptr_t             g_rtk_nmi_saved_sp;        /* 0x1002CFB4 */
extern uint32_t              g_rtk_ready_mask;          /* 0x1002D0C8 */
extern uint32_t              g_rtk_ready_pad;           /* 0x1002D0CC */
extern uintptr_t             g_rtk_ready_queues[92];    /* 0x1002D0D0..0x1002D240 */
extern uint8_t               g_rtk_sched_need_resched;  /* 0x1002D240 */
extern uint8_t               g_rtk_sched_pad[3];        /* 0x1002D241..0x1002D243 */
extern uint8_t               g_rtk_sched_preempted;     /* 0x1002D244 */
extern uint32_t              g_rtk_irq_mask_basepri;    /* 0x1002D2BC */
extern uint32_t              g_rtk_irq_prio_group;      /* 0x1002D2C0 */

/* Retained native veneers */
void Reset_Handler(void);
void Fault_Handler(void);
void NMI_Handler(void);
void SVCall_Handler(void);
uint32_t rtk_sleep_restore_context(void);

/* Reconstructed C: src/rtkit/boot.c */
uint32_t acib_mpu_default_attr(void);
uint32_t acib_mpu_enable(void);
uint32_t acib_mpu_disable(void);
uint32_t acib_mpu_config_region(uint32_t region, uint32_t base, uint32_t size_order,
                                uint32_t srd, uint32_t tex_scb, uint32_t xn, uint32_t ap);
void     rtk_early_hw_init(void);
uint32_t acib_dwt_cyccnt_read(void);
uint32_t acib_get_calibrated_cpu_hz(void);
uint32_t acib_measure_cpu_hz(uint32_t timer_base_addr);
void     acib_clear_sleep_deadline(void);
void    *acib_clock_timing_init(void);
void    *acib_load_rtkit_board_props(void);
uint32_t acib_board_set_flags(void);
uint32_t acib_board_has_flag_bit0(void);
uint32_t acib_board_get_rev_nibble(void);
uint32_t acib_soc_is_fpga_emulation(void);
void     rtk_init_stacks_watermark(void);
uint32_t rtk_get_wake_count(void);
uint32_t rtk_get_sleep_count(void);
void     rtk_stack_watermark_fill(uint32_t *stack_words, uint32_t byte_len);
uint32_t rtk_stack_watermark_check(const uint32_t *stack_words, uint32_t byte_len);

/* Reconstructed C: src/rtkit/exception.c */
void     __stack_chk_fail(void);
void     rtk_abort_panic(const char *fmt, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void     acib_panic_null(uint32_t unused_r0, uint32_t unused_r1, uint32_t arg2, uint32_t arg3);
void     acib_panic_code_line(uint32_t encoded, uint32_t arg1);
void     acib_panic_code_group(uint32_t encoded, uint32_t arg1, uint32_t unused_r2, uint32_t arg3);
void     acib_ctx_store_fault_arg(uint8_t *ctx, uint32_t val);
void     rtk_fault_extract_frame(const uint8_t *raw_ctx, struct acib_fault_summary *out);
uint32_t rtk_unwind_adjust_lr(uint32_t lr_val);

/* Reconstructed C: src/rtkit/irq.c */
uint32_t rtk_irq_mask_save(void);
void     rtk_irq_mask_restore(uint32_t prev_basepri);
uint32_t rtk_primask_disable_save(void);
void     rtk_primask_restore(uint32_t was_enabled);
void     rtk_irq_enable_all(void);
uint32_t rtk_irq_are_enabled(void);
uint32_t rtk_irq_set_prio_group(uint32_t prio_bits);
uint32_t rtk_cpu_in_thread_mode(void);
uint32_t outlined_sub_EF0DC(uint32_t val);
uint32_t rtk_spinlock_try_enter(uint32_t *lock_word);
int      SysTick_Handler(void *exc_frame, uint32_t ipsr);
void    *rtk_sched_switch(void *saved_sp);
uint32_t rtk_nvic_method_4(void *self);
uint32_t rtk_nvic_method_5(void *self, uint32_t vec, int enable);
uint32_t rtk_nvic_method_7(void *self, uint32_t vec, acib_irq_handler_fn fn, uintptr_t ctx);
uint32_t rtk_nvic_method_8(void *self, uint32_t vec, uint32_t prio);
uint32_t rtk_nvic_dispatch_irq(void);
bool     IRQ16_Handler(void *self, uint32_t vec);
uint32_t rtk_nvic_method_15(void *self, uint32_t vec);
uint32_t rtk_nvic_method_6(void *self, uint32_t vec);
uint32_t rtk_nvic_method_9(void *self);

#endif /* ACIB_RTKIT_H */
