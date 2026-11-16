/*
 * src/libc/bitops_math.c — Freestanding bit manipulation, formatting, and integer utility routines.
 *
 * Clean-room C11 reconstruction for Apple b515d (AirPods3,6) Bluetooth / ACI
 * firmware (`acib`), verified by Unicorn Cortex-M4 differential execution
 * and 3-stub mutation testing.
 */
#include "acib/acib.h"
#include "acib/bus.h"
#include "acib/cpu.h"
#include "acib/libc.h"
#include "acib/rtkit.h"
#include "acib/phy_ipc.h"
#include "acib/aci.h"
#include "acib/reconstructed_p2.h"

/* External pending_c link-boundary prototypes */
void sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg);
void sub_6CFF4(uint32_t halt_reason);
uint32_t sub_EA86E(uint32_t addr);
uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count);
uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val);
uint32_t sub_7E9F8(uint32_t end_ptr);
void sub_DF764(void);
void sub_7EE58(void);
int  sub_74240(uint32_t log_id);
void sub_74256(uint32_t log_id, uint32_t val0);
void sub_74270(uint32_t log_id, uint32_t val0, uint32_t val1);
void sub_74290(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2);
void sub_742B2(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2, uint32_t val3);
void sub_7DC92(uint32_t err_id);
void sub_6DF3C(uint32_t ext_irq);
void sub_6DC88(void);
void sub_6DD04(void);
void sub_6DD70(uint32_t lock_arg);
void sub_6DDB8(void);
void sub_6DE10(void);
void sub_740A8(uint32_t log_id, uint32_t line);
void sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0);
uint32_t sub_7513C(const char *msg, uint32_t code);
void sub_7835C(uint32_t delay_us);
void sub_7D820(uint32_t arg);
void sub_96458(uint32_t reg_id, uint32_t val);
void sub_964AC(uint32_t val);
void sub_96434(uint32_t reg_id, uint32_t val);
void sub_707F0(void);
void sub_DECFC(void);
void sub_2F78(void);
int  sub_71540(uint32_t arg);
uint32_t sub_71240(void);
void sub_7076C(void);
uint32_t sub_77A44(void);
uint32_t sub_6C850(uint32_t arg);
void sub_73F20(uint32_t arg);
void sub_7B1C4(uint32_t arg);
void sub_74F10(uint32_t arg);
void sub_6AFB0(void);
void sub_776E0(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);
void sub_69978(uint32_t arg);
uint32_t sub_598E8(uint32_t arg);
uint32_t sub_133BC(uint32_t arg);
uint32_t sub_188B4(uint32_t arg);
void sub_4B5C2(uint32_t arg);
void sub_2774(void);
uint32_t sub_2584(void);
void sub_4B2B0(uint32_t arg);
void sub_2F8C(void);
void sub_59884(void);
void sub_5688(void);

/* @provenance RVA 0x0e40f8 sub_E40F8 */
int32_t libc_util_fn_0e40f8(void)
{
    return (int32_t)(((int32_t)0x96u));
}

/* @provenance RVA 0x0e40fc sub_E40FC */
int32_t libc_util_fn_0e40fc(int32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    int32_t loc_4 = 0;
    int16_t loc_5 = 0;
    int32_t loc_6 = 0;
    uint16_t loc_7 = 0;
    uint16_t loc_8 = 0;
    uint16_t loc_9 = 0;
    uint32_t loc_10 = 0;
    int32_t loc_11 = 0;
    uint32_t loc_12 = 0;
    loc_2 = (int32_t)(acib_mem_read8(arg_1));
    loc_3 = (uint32_t)(acib_mem_read8((arg_1 + 1u)));
    loc_4 = (int32_t)(acib_mem_read32((0x10002340u + 4u * ((uint32_t)(int32_t)(loc_2)))));
    loc_5 = (int16_t)(osa_svc_fn_0a0f54((uint32_t)(((int32_t)(8u * ((uint32_t)(acib_mem_read8((arg_1 + 2u))))))), (int32_t)(loc_2)));
    loc_6 = (int32_t)(acib_mem_read8((arg_1 + 4u)));
    loc_7 = (uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_5)) + ((uint32_t)(int32_t)(loc_4)))));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_6)) & 0x10u)))) != 0u) {
        if (((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_6)), 0x1Fu)))) != 0u) {
            loc_10 = (uint32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 0x110u)))));
            loc_9 = (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_7)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 0x110u)))))))) + 0x96u)));
            loc_8 = (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s32(0x100290B1u))))) + ((uint32_t)(loc_7)))))) + 0xE0u)));
        } else {
            loc_10 = (uint32_t)(((int32_t)0u));
            loc_8 = (uint16_t)(loc_7);
            loc_9 = (uint16_t)(loc_7);
        }
        if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_6)) & 2u)))) != 0u) {
            loc_11 = (int32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 0x160u)))));
            if (((((uint32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(acib_mem_read8((arg_1 + 3u)))))))))))))) + ((uint32_t)(int32_t)(loc_11))) + 0x96u) > loc_10) {
                loc_10 = (uint32_t)(((uint32_t)loc_10 & ~0xFFFFu) | (((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(acib_mem_read8((arg_1 + 3u)))))))))))) + ((uint32_t)(int32_t)(loc_11)))))) + 0x96u)))) & 0xFFFFu));
            }
            loc_9 = (uint16_t)(((((uint32_t)(loc_7)) + loc_10) + 0x96u));
            loc_8 = (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_7)) + ((uint32_t)(((uint8_t)(acib_mem_read_s32(0x100290B1u))))))))) + 0xE0u)));
        }
    } else {
        loc_8 = (uint16_t)(loc_7);
        loc_9 = (uint16_t)(loc_7);
    }
    loc_12 = (uint32_t)((((loc_3 - (acib_arm_lsr32(((uint32_t)(loc_3)), 1u) & 0x55u)) & 0x33u) + (acib_arm_lsr32(((uint32_t)((loc_3 - (acib_arm_lsr32(((uint32_t)(loc_3)), 1u) & 0x55u)))), 2u) & 0x33u)));
    acib_mem_write32(arg_2, (uint32_t)(((int32_t)(((uint32_t)(loc_9)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_8)) + 0x96u)))) * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(loc_12)))) + ((uint32_t)(((uint8_t)(acib_arm_lsr32(((uint32_t)(loc_12)), 4u))))))))) & 0xFu)))) - 1u))))))))))));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0e42cc sub_E42CC */
int32_t libc_util_fn_0e42cc(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s32(0x1002CF4Cu))));
}
