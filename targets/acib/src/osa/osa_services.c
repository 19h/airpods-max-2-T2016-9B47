/*
 * src/osa/osa_services.c — OSA system services, IPC buffer management, and shared controller helpers.
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

/* @provenance RVA 0x072174 sub_72174 */
uint32_t osa_svc_fn_072174(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (uint32_t)(osa_core_fn_06d804((uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xAu))))), (int32_t)(arg_2), (uint32_t)(arg_1)));
}

/* @provenance RVA 0x0722e4 sub_722E4 */
int32_t osa_svc_fn_0722e4(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32(arg_0));
    if ((((uint32_t)(int32_t)(loc_0)) != 0u) && (arg_1 >= 2u)) {
        for (loc_1 = (uint32_t)(((int32_t)1u)); loc_1 < arg_1; loc_1 = (uint32_t)((uint32_t)loc_1 + 1u)) {
            loc_2 = (uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) - 2u))))));
            if ((loc_2 & 0x1000u) != 0u) {
                return (int32_t)(((int32_t)0u));
            }
            loc_3 = (int32_t)(acib_mem_read32((((uint32_t)(0x10020B78u)) + (4u * ((7u * (acib_arm_lsr32(((uint32_t)(loc_2)), 6u) & 0x3Fu)) + 2u)))));
            loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) - 4u))))))) + ((uint32_t)(int32_t)(loc_3)))))) + 4u)));
            if (((uint32_t)(int32_t)(loc_3)) == 0u) {
                break;
            }
        }
        if (((uint32_t)(int32_t)(loc_3)) == 0u) {
            return (int32_t)(((int32_t)0u));
        }
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x072338 sub_72338 */
int32_t osa_svc_fn_072338(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_1 = (uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 2u))))));
    if ((loc_1 & 0x1000u) != 0u) {
        return (int32_t)(((int32_t)0u));
    }
    loc_2 = (int32_t)(acib_mem_read32((((uint32_t)(0x10020B78u)) + (4u * ((7u * (acib_arm_lsr32(((uint32_t)(loc_1)), 6u) & 0x3Fu)) + 2u)))));
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 4u))))))) + ((uint32_t)(int32_t)(loc_2)))));
    if (((uint32_t)(int32_t)(loc_2)) != 0u) {
        return (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + 4u)));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x072a78 sub_72A78 */
uint32_t osa_svc_fn_072a78(int32_t arg_0, uint32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0)))));
    acib_mem_write8((0x100213CAu + ((uint32_t)(int32_t)(loc_0))), (uint8_t)(acib_mem_read8(arg_1)));
    loc_1 = (uint32_t)((0x100213CAu + ((uint32_t)(int32_t)(loc_0))));
    acib_mem_write32(((uint32_t)((loc_1 + 5u))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)((loc_1 + 1u))), (uint32_t)(arg_3));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x072b0c sub_72B0C */
int32_t osa_svc_fn_072b0c(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mem_read32(((uint32_t)((0x100213CAu + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0)))))) + 5u)))))))));
    acib_mem_write32(arg_1, (uint32_t)(loc_0));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x072b3c sub_72B3C */
uint32_t osa_svc_fn_072b3c(void)
{
    acib_mem_write16(0x100213FFu, (uint16_t)(((int16_t)0u)));
    return (uint32_t)((memset(acib_addr_ptr(0x100213EFu), 0, 0x10u), 0x100213EFu));
}

/* @provenance RVA 0x072cbc sub_72CBC */
int32_t osa_svc_fn_072cbc(void)
{
    return (int32_t)(((uint16_t)(((int32_t)(2u - ((uint32_t)(((uint8_t)(acib_mem_read_s16(0x100213FFu))))))))));
}

/* @provenance RVA 0x072e04 sub_72E04 */
uint32_t osa_svc_fn_072e04(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mem_read32((arg_0 + 4u * 1u)));
    loc_1 = (int32_t)(acib_mem_read32((arg_0 + 4u * 2u)));
    loc_2 = (int32_t)(acib_mem_read32((arg_0 + 4u * 3u)));
    acib_mem_write32(arg_0, (uint32_t)(((uint32_t)(acib_mem_read32(arg_0)) ^ acib_mem_read32(arg_1))));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(((int32_t)(acib_mem_read32((arg_1 + 4u * 1u)) ^ ((uint32_t)(int32_t)(loc_0))))));
    acib_mem_write32((arg_0 + 4u * 2u), (uint32_t)(((int32_t)(acib_mem_read32((arg_1 + 4u * 2u)) ^ ((uint32_t)(int32_t)(loc_1))))));
    acib_mem_write32((arg_0 + 4u * 3u), (uint32_t)(((int32_t)(acib_mem_read32((arg_1 + 4u * 3u)) ^ ((uint32_t)(int32_t)(loc_2))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x072e28 sub_72E28 */
uint32_t osa_svc_fn_072e28(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    uint8_t loc_4 = 0;
    uint8_t loc_5 = 0;
    int32_t loc_6 = 0;
    int32_t loc_7 = 0;
    uint8_t loc_8 = 0;
    int32_t loc_9 = 0;
    int32_t loc_10 = 0;
    uint8_t loc_11 = 0;
    int32_t loc_12 = 0;
    int32_t loc_13 = 0;
    int32_t loc_14 = 0;
    int8_t loc_15 = 0;
    int32_t loc_16 = 0;
    int32_t loc_17 = 0;
    uint8_t loc_18 = 0;
    uint8_t loc_19 = 0;
    int32_t loc_20 = 0;
    uint8_t loc_21 = 0;
    int32_t loc_22 = 0;
    int32_t loc_23 = 0;
    int32_t loc_24 = 0;
    int32_t loc_25 = 0;
    int32_t loc_26 = 0;
    loc_0 = (int32_t)(acib_mem_read8((arg_0 + 0xBu)));
    loc_1 = (int32_t)(acib_mem_read8((arg_0 + 0xCu)));
    loc_2 = (int32_t)(acib_mem_read8((arg_0 + 6u)));
    loc_3 = (int32_t)(acib_mem_read8((arg_0 + 1u)));
    loc_21 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_2)))));
    loc_25 = (int32_t)(acib_mem_read8(arg_0));
    loc_24 = (int32_t)(acib_mem_read8((arg_0 + 0xFu)));
    loc_20 = (int32_t)(acib_mem_read8((arg_0 + 2u)));
    loc_26 = (int32_t)(acib_mem_read8((arg_0 + 3u)));
    loc_4 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_3)))));
    acib_mem_write8((arg_0 + 0xFu), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_1)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_0)))))))))) ^ ((uint32_t)(loc_21)))))) ^ ((uint32_t)(loc_4))))));
    loc_5 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_1)))));
    loc_6 = (int32_t)(acib_mem_read8((arg_0 + 0xDu)));
    loc_7 = (int32_t)(acib_mem_read8((arg_0 + 0xEu)));
    acib_mem_write8((arg_0 + 0xEu), (uint8_t)(((int32_t)(((uint32_t)(loc_4)) ^ ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_0)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_2)))))))))) ^ ((uint32_t)(loc_5))))))))));
    loc_23 = (int32_t)(loc_7);
    loc_8 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_0)))));
    loc_9 = (int32_t)(acib_mem_read8((arg_0 + 7u)));
    loc_7 = (int32_t)(((uint32_t)loc_7 & ~0xFFu) | (((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_2)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_3)))))))))) ^ ((uint32_t)(loc_8)))))) ^ ((uint32_t)(loc_5)))))) & 0xFFu));
    loc_10 = (int32_t)(acib_mem_read8((arg_0 + 8u)));
    acib_mem_write8((arg_0 + 0xDu), (uint8_t)(loc_7));
    acib_mem_write8((arg_0 + 0xCu), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_3)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_1)))))))))) ^ ((uint32_t)(loc_21)))))) ^ ((uint32_t)(loc_8))))));
    loc_1 = (int32_t)(((uint32_t)loc_1 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_20)))))) & 0xFFu));
    loc_11 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_6)))));
    acib_mem_write8((arg_0 + 0xBu), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_10)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_9)))))))))) ^ ((uint32_t)(int32_t)(loc_1)))))) ^ ((uint32_t)(loc_11))))));
    loc_3 = (int32_t)(((uint32_t)loc_3 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_10)))))) & 0xFFu));
    loc_12 = (int32_t)(acib_mem_read8((arg_0 + 0xAu)));
    acib_mem_write8((arg_0 + 0xAu), (uint8_t)(((int32_t)(((uint32_t)(loc_11)) ^ ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_20)))))) ^ ((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_9)))))))))) ^ ((uint32_t)(int32_t)(loc_3))))))))));
    loc_22 = (int32_t)(loc_12);
    loc_13 = (int32_t)(loc_6);
    loc_2 = (int32_t)(((uint32_t)loc_2 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_9)))))) & 0xFFu));
    loc_14 = (int32_t)(acib_mem_read8((arg_0 + 9u)));
    loc_15 = (int8_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_20)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_6)))))))));
    loc_16 = (int32_t)(acib_mem_read8((arg_0 + 4u)));
    acib_mem_write8((arg_0 + 9u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_15)) ^ ((uint32_t)(int32_t)(loc_2)))))) ^ ((uint32_t)(int32_t)(loc_3))))));
    acib_mem_write8((arg_0 + 8u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) ^ ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_10)))))) ^ ((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_13)))))))))) ^ ((uint32_t)(int32_t)(loc_1))))))))));
    loc_10 = (int32_t)(((uint32_t)loc_10 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_23)))))) & 0xFFu));
    loc_13 = (int32_t)(((uint32_t)loc_13 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_14)))))) & 0xFFu));
    acib_mem_write8((arg_0 + 7u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_26)))))) ^ ((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_16)))))))))) ^ ((uint32_t)(int32_t)(loc_10)))))) ^ ((uint32_t)(int32_t)(loc_13))))));
    loc_2 = (int32_t)(((uint32_t)loc_2 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_16)))))) & 0xFFu));
    loc_17 = (int32_t)(acib_mem_read8((arg_0 + 5u)));
    acib_mem_write8((arg_0 + 6u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_26)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_23)))))))))) ^ ((uint32_t)(int32_t)(loc_2)))))) ^ ((uint32_t)(int32_t)(loc_13))))));
    loc_13 = (int32_t)(((uint32_t)loc_13 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_26)))))) & 0xFFu));
    acib_mem_write8((arg_0 + 5u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_23)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_14)))))))))) ^ ((uint32_t)(int32_t)(loc_13)))))) ^ ((uint32_t)(int32_t)(loc_2))))));
    acib_mem_write8((arg_0 + 4u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_14)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_16)))))))))) ^ ((uint32_t)(int32_t)(loc_10)))))) ^ ((uint32_t)(int32_t)(loc_13))))));
    loc_14 = (int32_t)(((uint32_t)loc_14 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_17)))))) & 0xFFu));
    loc_3 = (int32_t)(((uint32_t)loc_3 & ~0xFFu) | (((uint32_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_22)))))) & 0xFFu));
    acib_mem_write8((arg_0 + 3u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_24)))))) ^ ((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_25)))))))))) ^ ((uint32_t)(int32_t)(loc_3)))))) ^ ((uint32_t)(int32_t)(loc_14))))));
    loc_18 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_25)))));
    acib_mem_write8((arg_0 + 2u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_24)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_22)))))))))) ^ ((uint32_t)(loc_18)))))) ^ ((uint32_t)(int32_t)(loc_14))))));
    loc_19 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_24)))));
    acib_mem_write8((arg_0 + 1u), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_22)))))) ^ ((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_17)))))))))) ^ ((uint32_t)(loc_19)))))) ^ ((uint32_t)(loc_18))))));
    acib_mem_write8(arg_0, (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F553Fu + ((uint32_t)(int32_t)(loc_25)))))) ^ ((uint32_t)(acib_mem_read8((0x000F563Fu + ((uint32_t)(int32_t)(loc_17)))))))))) ^ ((uint32_t)(int32_t)(loc_3)))))) ^ ((uint32_t)(loc_19))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x072ffc sub_72FFC */
uint32_t osa_svc_fn_072ffc(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint8_t loc_3 = 0;
    int32_t loc_4 = 0;
    uint8_t loc_5 = 0;
    int32_t loc_6 = 0;
    uint8_t loc_7 = 0;
    int32_t loc_8 = 0;
    uint8_t loc_9 = 0;
    int32_t loc_10 = 0;
    loc_0 = (int32_t)(acib_mem_read8((arg_0 + 1u)));
    loc_1 = (int32_t)(acib_mem_read8((arg_0 + 2u)));
    loc_2 = (int32_t)(acib_mem_read8((arg_0 + 4u)));
    acib_mem_write8(arg_0, (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8(arg_0)))))));
    acib_mem_write8((arg_0 + 4u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_2))))));
    acib_mem_write8((arg_0 + 8u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 8u))))))));
    acib_mem_write8((arg_0 + 0xCu), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 0xCu))))))));
    acib_mem_write8((arg_0 + 1u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 5u))))))));
    acib_mem_write8((arg_0 + 5u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 9u))))))));
    acib_mem_write8((arg_0 + 9u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 0xDu))))))));
    loc_3 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_0)))));
    loc_4 = (int32_t)(acib_mem_read8((arg_0 + 0xAu)));
    acib_mem_write8((arg_0 + 0xDu), (uint8_t)(loc_3));
    acib_mem_write8((arg_0 + 0xAu), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_1))))));
    loc_5 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_4)))));
    loc_6 = (int32_t)(acib_mem_read8((arg_0 + 0xEu)));
    acib_mem_write8((arg_0 + 2u), (uint8_t)(loc_5));
    acib_mem_write8((arg_0 + 0xEu), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 6u))))))));
    loc_7 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_6)))));
    loc_8 = (int32_t)(acib_mem_read8((arg_0 + 0xFu)));
    acib_mem_write8((arg_0 + 6u), (uint8_t)(loc_7));
    acib_mem_write8((arg_0 + 0xFu), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 0xBu))))))));
    acib_mem_write8((arg_0 + 0xBu), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(acib_mem_read8((arg_0 + 7u))))))));
    loc_9 = (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_8)))));
    loc_10 = (int32_t)(acib_mem_read8((arg_0 + 3u)));
    acib_mem_write8((arg_0 + 3u), (uint8_t)(loc_9));
    acib_mem_write8((arg_0 + 7u), (uint8_t)(acib_mem_read8((0x000F543Fu + ((uint32_t)(int32_t)(loc_10))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0737d4 sub_737D4 */
int32_t osa_svc_fn_0737d4(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(osa_svc_fn_0754e0((int32_t)(acib_mem_read32((0x10000304u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0)))))) + 3u)))))))));
}

/* @provenance RVA 0x073be8 sub_73BE8 */
uint32_t osa_svc_fn_073be8(void)
{
    return (uint32_t)(0x10021550u);
}

/* @provenance RVA 0x073bf0 sub_73BF0 */
int32_t osa_svc_fn_073bf0(void)
{
    return (int32_t)(((int32_t)0x15u));
}

/* @provenance RVA 0x073bf4 sub_73BF4 */
int32_t osa_svc_fn_073bf4(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint8_t)(acib_mem_read32((0x10000304u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0)))))) + 1u)))))) & 0xFFu)));
}

/* @provenance RVA 0x073ff8 sub_73FF8 */
int32_t osa_svc_fn_073ff8(void)
{
    return (int32_t)((((uint32_t)(int32_t)(acib_mem_read_s32(0x10021600u))) == 1u));
}

/* @provenance RVA 0x07400c sub_7400C */
int32_t osa_svc_fn_07400c(int32_t arg_0, int8_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x13u)))), (uint8_t)(arg_2));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint8_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x074012 sub_74012 */
int32_t osa_svc_fn_074012(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xEu)))), (uint32_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x074018 sub_74018 */
int32_t osa_svc_fn_074018(int32_t arg_0, int32_t arg_1, int8_t arg_2, int8_t arg_3, uint32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(arg_4));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 6u)))), (uint32_t)(loc_0));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 2u)))), (uint32_t)(arg_0));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 0xCu)))), (uint8_t)(arg_3));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 0xBu)))), (uint8_t)(arg_2));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 0xAu)))), (uint8_t)(((int8_t)0u)));
    acib_mem_write16(((uint32_t)(loc_1)), (uint16_t)(((int16_t)0xFFFF890Fu)));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(arg_4))) + 0xDu)));
    acib_mem_write32(arg_4, (uint32_t)(loc_2));
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x074060 sub_74060 */
int32_t osa_svc_fn_074060(int32_t arg_0, int32_t arg_1, int8_t arg_2, uint32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    return (int32_t)(osa_svc_fn_074018((int32_t)(arg_0), (int32_t)(arg_1), (int8_t)(arg_1), (int8_t)(arg_2), (uint32_t)(arg_3)));
}

/* @provenance RVA 0x0744a0 sub_744A0 */
int32_t osa_svc_fn_0744a0(int8_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((uint8_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)))));
    if (loc_0 <= 8u) {
        return (int32_t)(acib_mem_read8((0x000F5794u + loc_0)));
    } else {
        return (int32_t)(((int32_t)0xFFu));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x0744fc sub_744FC */
int32_t osa_svc_fn_0744fc(int8_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((uint8_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)))));
    if (loc_0 <= 8u) {
        return (int32_t)(acib_mem_read32((0x000F5770u + 4u * loc_0)));
    } else {
        return (int32_t)(((int32_t)8u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x07485c sub_7485C */
uint32_t osa_svc_fn_07485c(uint32_t arg_0)
{
    (void)arg_0;
    int8_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int8_t)((arg_0 & 7u));
    loc_1 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 3u));
    acib_mem_write8((0x10021634u + loc_1), (uint8_t)(((uint32_t)(acib_mem_read8((0x10021634u + loc_1))) | ((uint32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(loc_0)))))))));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x074920 sub_74920 */
int32_t osa_svc_fn_074920(int32_t arg_0)
{
    (void)arg_0;
    sub_6DC88();
    acib_mem_write32(0x10021620u, (uint32_t)((((uint32_t)(int32_t)(arg_0)) | 0x80000000u)));
    return (int32_t)((sub_6DD04(), 0u));
}

/* @provenance RVA 0x0750ac sub_750AC */
int32_t osa_svc_fn_0750ac(void)
{
    return (int32_t)((((uint32_t)(int32_t)(phy_mdm_fn_07895c((uint32_t)(acib_mem_read_s32(0x1002165Cu))))) > (0x3E8u * ((uint32_t)(((uint16_t)(acib_mem_read_s32(0x1002166Cu))))))));
}

/* @provenance RVA 0x075194 sub_75194 */
uint32_t osa_svc_fn_075194(uint32_t arg_0)
{
    (void)arg_0;
    int16_t loc_0 = 0;
    bool loc_1 = false;
    int16_t loc_2 = 0;
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x1002162Fu))) != 0u) {
        loc_0 = (int16_t)(((int16_t)0x3Cu));
        loc_1 = (((arg_0 == 0u)) != 0);
        if (arg_0 == 0u) {
            loc_0 = (int16_t)(((int16_t)0xF0u));
        }
        acib_mem_write16(0x100209B2u, (uint16_t)(loc_0));
        loc_2 = (int16_t)(((int16_t)0x222Eu));
        if (arg_0 == 0u) {
            loc_2 = (int16_t)(((int16_t)0xFFFF88B8u));
        }
        acib_mem_write16(0x100209B0u, (uint16_t)(loc_2));
        arg_0 = (uint32_t)((((uint32_t)(int32_t)(acib_mem_read_s32(0x10021620u))) & 0xFDFFFFFFu));
        if ((loc_1) != 0) {
            arg_0 = (uint32_t)((uint32_t)arg_0 + ((uint32_t)(((int32_t)0x2000000u))));
        }
        acib_mem_write32(0x10021620u, (uint32_t)(arg_0));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0751d0 sub_751D0 */
int8_t osa_svc_fn_0751d0(int8_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(0x1002162Fu, (uint8_t)(arg_0));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x075210 sub_75210 */
int32_t osa_svc_fn_075210(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    loc_2 = (uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0))))))));
    loc_3 = (int32_t)(((uint8_t)(acib_mem_read8((loc_2 + 0x10u)))));
    loc_4 = (int32_t)(acib_mem_read16((((uint32_t)(loc_2)) + (2u * 6u))));
    if (((uint32_t)(acib_mem_read8((loc_2 + 0x10u)))) != 0u) {
        loc_4 = (int32_t)((uint32_t)loc_4 - ((uint32_t)(((int32_t)0xDu))));
    }
    loc_5 = (uint32_t)(acib_mem_read32((((uint32_t)(loc_2)) + (4u * 1u))));
    loc_6 = (int32_t)(((int32_t)(acib_mem_read32(loc_5) & 0x3FFFFFu)));
    if (((int32_t)(((uint32_t)(int32_t)(loc_4)) - ((uint32_t)(int32_t)(loc_6)))) < arg_2) {
        sub_74270(0x2024000u, ((uint32_t)(arg_0)), ((uint32_t)(loc_3)));
        return (int32_t)(((int32_t)1u));
    } else {
        acib_mem_write32(arg_1, (uint32_t)((((uint32_t)(loc_5)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_6)) + 8u)))))));
        return (int32_t)(((int32_t)0u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x0754a0 sub_754A0 */
int32_t osa_svc_fn_0754a0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)((((int32_t)(((uint32_t)(int32_t)(arg_1)) + ((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(((uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0)))))) + 4u)))))))) + 8u)))) + ((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(acib_mem_read32(((uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0)))))) + 4u))))))))) & 0x3FFFFFu))))))))))))) <= ((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0)))))) + 0xCu)))))))))) + ((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(((uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0)))))) + 4u)))))))) + 8u))))))));
}

/* @provenance RVA 0x0754e0 sub_754E0 */
uint32_t osa_svc_fn_0754e0(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x10021568u + ((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(arg_0))))))));
    acib_mem_write16((((uint32_t)(loc_0)) + (2u * 7u)), (uint16_t)(((uint32_t)(acib_mem_read16((((uint32_t)(loc_0)) + (2u * 7u)))) + 1u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x075b4c sub_75B4C */
uint32_t osa_svc_fn_075b4c(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    osa_svc_fn_07400c((int32_t)(((int32_t)(0x10021438u))), (int8_t)(arg_0), (int8_t)(arg_1));
    osa_svc_fn_074012((int32_t)(((int32_t)(0x10021438u))), (int32_t)(acib_mem_read_s32((0x10021420u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 1u))))))));
    return (uint32_t)(0x10021438u);
}

/* @provenance RVA 0x075d14 sub_75D14 */
uint32_t osa_svc_fn_075d14(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x100218A0u + 4u * ((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0))))))));
    acib_mem_write32((loc_0 + 4u * 6u), (uint32_t)(arg_1));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x075d28 sub_75D28 */
uint32_t osa_svc_fn_075d28(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x100218A0u + 4u * ((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0))))))));
    acib_mem_write32((loc_0 + 4u * 7u), (uint32_t)(arg_1));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x075d80 sub_75D80 */
uint32_t osa_svc_fn_075d80(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x100218A0u + 4u * ((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0))))))));
    acib_mem_write32((loc_0 + 4u * 6u), (uint32_t)(((int32_t)8u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x075da8 sub_75DA8 */
uint32_t osa_svc_fn_075da8(int32_t arg_0, int32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    int32_t loc_4 = 0;
    if (((uint32_t)(int32_t)(arg_0)) == 0xFFu) {
        loc_3 = (uint32_t)(acib_mem_read32(0x100218A0u));
        for (loc_4 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_4)) != 0x2Du; loc_4 = (int32_t)((uint32_t)loc_4 + ((uint32_t)(((int32_t)9u))))) {
            osa_svc_fn_072174((int32_t)(((int32_t)((0x100218A0u + 4u * ((uint32_t)(int32_t)(loc_4)))))), (uint32_t)(arg_2), (int32_t)(arg_1));
            loc_3 = (uint32_t)(osa_svc_fn_072174((int32_t)(((int32_t)((0x100218A0u + 4u * (((uint32_t)(int32_t)(loc_4)) + 3u))))), (uint32_t)(arg_2), (int32_t)(arg_1)));
        }
    } else {
        loc_2 = (uint32_t)((0x100218A0u + 4u * ((uint32_t)(int32_t)(((int32_t)(9u * ((uint32_t)(int32_t)(arg_0))))))));
        osa_svc_fn_072174((int32_t)(((int32_t)(loc_2))), (uint32_t)(arg_2), (int32_t)(arg_1));
        return (uint32_t)(osa_svc_fn_072174((int32_t)(((int32_t)((loc_2 + (4u * 3u))))), (uint32_t)(arg_2), (int32_t)(arg_1)));
    }
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x075e20 sub_75E20 */
int32_t osa_svc_fn_075e20(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u))))));
    loc_1 = (uint32_t)((acib_arm_lsr32(((uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 3u)))))), 2u) + 0xAu));
    if (((uint32_t)(int32_t)(loc_0)) == 0u) {
        loc_1 = (uint32_t)(((uint32_t)loc_1 & ~0xFFFFu) | (((uint32_t)(((int16_t)0xBu))) & 0xFFFFu));
    }
    return (int32_t)(((uint16_t)((loc_1 + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_0))))))))));
}

/* @provenance RVA 0x075fec sub_75FEC */
int32_t osa_svc_fn_075fec(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    if (arg_1 > 0x7Fu) {
        acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 5u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(arg_1)))) | ((uint32_t)(((uint16_t)((2u * (arg_1 & 0xFFC0u)))))))))) | 0x80u))));
        loc_0 = (int32_t)(((int32_t)7u));
    } else {
        acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 5u)))), (uint8_t)(arg_1));
        loc_0 = (int32_t)(((int32_t)6u));
    }
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(int32_t)(loc_0)))));
}

/* @provenance RVA 0x07600e sub_7600E */
int32_t osa_svc_fn_07600e(void)
{
    return (int32_t)(((int32_t)0x80u));
}

/* @provenance RVA 0x076348 sub_76348 */
int32_t osa_svc_fn_076348(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    int32_t loc_4 = 0;
    int32_t loc_5 = 0;
    uint32_t loc_6 = 0;
    int32_t loc_7 = 0;
    loc_0 = (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u))))));
    loc_2 = (uint32_t)((arg_0 + (4u * 2u)));
    if (loc_0 >= 0x21u) {
        loc_0 = (uint32_t)(((int32_t)0x20u));
        acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)))), (uint32_t)(((int32_t)0x20u)));
    }
    acib_mem_write32(arg_0, (uint32_t)((acib_arm_lsl32(loc_0, 8u) + 0xB9u)));
    loc_3 = (uint32_t)(acib_mmio_read32(0x52160020u));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(loc_3));
    (void)memcpy_wrapper(acib_addr_ptr((arg_0 + (4u * 2u))), acib_addr_ptr(acib_mem_read32(((uint32_t)(arg_1)))), loc_0);
    loc_4 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u))))));
    loc_5 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) & 3u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) & 3u)))) != 0u) {
        loc_6 = (uint32_t)((((uint32_t)(loc_2)) + ((uint32_t)(int32_t)(loc_4))));
        loc_7 = (int32_t)(((int32_t)(4u - ((uint32_t)(int32_t)(loc_5)))));
        (void)memset(acib_addr_ptr(loc_6), ((int32_t)0x20u), ((uint32_t)(((int32_t)(4u - ((uint32_t)(int32_t)(loc_5)))))));
        loc_4 = (int32_t)(((int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u))))) + ((uint32_t)(int32_t)(loc_7)))));
        acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)))), (uint32_t)(loc_4));
    }
    return (int32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 8u)))));
}

/* @provenance RVA 0x076398 sub_76398 */
int32_t osa_svc_fn_076398(void)
{
    return (int32_t)(((int32_t)0x28u));
}

/* @provenance RVA 0x0765d0 sub_765D0 */
int32_t osa_svc_fn_0765d0(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u))))), (uint32_t)(arg_0));
    acib_mem_write8(arg_0, (uint8_t)(((int8_t)0xFFFFFFBFu)));
    acib_mem_write8((arg_0 + 7u), (uint8_t)(acib_mem_read8(((uint32_t)(arg_1)))));
    return (int32_t)(((int32_t)8u));
}

/* @provenance RVA 0x076704 sub_76704 */
int32_t osa_svc_fn_076704(int16_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 4u))))));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 5u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - ((uint32_t)(int32_t)(loc_0)))))) - 7u)))))) | ((uint32_t)(((uint16_t)(((int32_t)(2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - ((uint32_t)(int32_t)(loc_0)))))) - 7u)))) & 0xFFC0u)))))))))))))) | 0x80u))));
    loc_1 = (uint32_t)(acib_mmio_read32(0x52160020u));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 1u)))), (uint32_t)(loc_1));
    acib_mem_write8(acib_mem_read32(((uint32_t)(arg_2))), (uint8_t)(((int8_t)0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0767d4 sub_767D4 */
int32_t osa_svc_fn_0767d4(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mem_read8(arg_1));
    acib_mem_write8(((uint32_t)(arg_0)), (uint8_t)(((int8_t)0x28u)));
    loc_1 = (uint32_t)(acib_mem_read8(arg_1));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 1u)))), (uint8_t)(loc_1));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)))), (uint16_t)(acib_mem_read16(((uint32_t)((arg_1 + 1u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(acib_mem_read32((((uint32_t)(arg_1)) + (4u * 2u)))));
    if (loc_1 < 0x1Fu) {
        (void)memcpy_wrapper(acib_addr_ptr(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))), acib_addr_ptr(acib_mem_read32((((uint32_t)(arg_1)) + (4u * 1u)))), loc_1);
        return (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 8u)));
    } else {
        sub_740A8(0x2A800u, 0x81E3u);
        return (int32_t)(((int32_t)0u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x076864 sub_76864 */
int32_t osa_svc_fn_076864(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint16_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) + 6u)))));
}

/* @provenance RVA 0x07686c sub_7686C */
int32_t osa_svc_fn_07686c(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(acib_mem_read8(arg_0))) + 8u)));
}

/* @provenance RVA 0x076944 sub_76944 */
int32_t osa_svc_fn_076944(uint32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_2 = 0;
    if (((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A00u))) > ((uint32_t)(acib_mem_read_s32(0x10021A04u)))) {
        loc_2 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A00u))) - ((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A04u))))));
        if (arg_1 < ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A00u))) - ((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A04u)))))))) {
            loc_2 = (uint32_t)(arg_1);
        }
        (void)memcpy_wrapper(acib_addr_ptr(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A04u))) + ((uint32_t)(int32_t)(acib_mem_read_s32(0x100219FCu)))))))), acib_addr_ptr(arg_0), loc_2);
        acib_mem_write32(0x10021A04u, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10021A04u)) + loc_2)));
    }
    acib_mem_write32(arg_2, (uint32_t)(arg_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x076988 sub_76988 */
int32_t osa_svc_fn_076988(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_1 = (int32_t)(((int32_t)0u));
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10021A08u))) != 0u) {
        loc_2 = (int32_t)(acib_mem_read_s32(0x100219FCu));
        acib_mem_write8(0x10021A08u, (uint8_t)(((int8_t)0u)));
        acib_mem_write32(arg_1, (uint32_t)(acib_mem_read_s32(0x10021A04u)));
        acib_mem_write32(0x10021A04u, (uint32_t)(((int32_t)0u)));
        acib_mem_write32(arg_0, (uint32_t)(loc_2));
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x0769a8 sub_769A8 */
uint32_t osa_svc_fn_0769a8(void)
{
    acib_mem_write8(0x10021A08u, (uint8_t)(((int8_t)1u)));
    return (uint32_t)(0x100219FCu);
}

/* @provenance RVA 0x0769f8 sub_769F8 */
uint32_t osa_svc_fn_0769f8(void)
{
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(0x10021A0Cu);
    acib_mem_write32(0x10021A10u, (uint32_t)(((int32_t)(0x526A8004u))));
    acib_mem_write32(0x10021A14u, (uint32_t)(((int32_t)(0x526A8004u))));
    acib_mem_write8(0x10021A1Au, (uint8_t)(((int8_t)1u)));
    acib_mem_write16(0x10021A18u, (uint16_t)(((int16_t)0u)));
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x1002189Du))) != 0u) {
        acib_mem_write32(((uint32_t)(acib_mem_read_s32(0x10021A1Cu))), (uint32_t)(((int32_t)0x55555555u)));
    }
    acib_mem_write32(0x10021A0Cu, (uint32_t)(((int32_t)0u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x076a30 sub_76A30 */
int8_t osa_svc_fn_076a30(int8_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(0x10021A1Bu, (uint8_t)(arg_0));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x076a48 sub_76A48 */
int32_t osa_svc_fn_076a48(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s8(0x10021A1Bu))));
}

/* @provenance RVA 0x076b10 sub_76B10 */
int32_t osa_svc_fn_076b10(void)
{
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10021A1Bu))) == 0u) {
        return (int32_t)(((uint8_t)(acib_mem_read_s8(0x10021A1Au))));
    }
    acib_mem_write8(0x10021A1Au, (uint8_t)(((int8_t)1u)));
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x076b24 sub_76B24 */
int32_t osa_svc_fn_076b24(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s8(0x10021AEAu))));
}

/* @provenance RVA 0x076bd4 sub_76BD4 */
int32_t osa_svc_fn_076bd4(void)
{
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10021A1Bu))) != 0u) {
        return (int32_t)(osa_svc_fn_076bda());
    } else {
        return (int32_t)((((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A14u))) == ((uint32_t)(int32_t)(acib_mem_read_s32(0x10021A10u)))));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x076bda sub_76BDA */
int32_t osa_svc_fn_076bda(void)
{
    osa_svc_fn_0769f8();
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x076d10 sub_76D10 */
uint32_t osa_svc_fn_076d10(void)
{
    return (uint32_t)(0x10021A20u);
}

/* @provenance RVA 0x076e40 sub_76E40 */
uint32_t osa_svc_fn_076e40(void)
{
    acib_mem_write32(0x10021AF0u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021AF8u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021AF4u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x10021AECu, (uint8_t)(((int8_t)0u)));
    return (uint32_t)(0x10021AECu);
}

/* @provenance RVA 0x077014 sub_77014 */
int32_t osa_svc_fn_077014(void)
{
    int32_t loc_0 = 0;
    acib_mem_write32(0x10021AF4u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x10021AECu, (uint8_t)(((int8_t)0u)));
    acib_mem_write32(0x10021AF8u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021AF0u, (uint32_t)(((int32_t)1u)));
    loc_0 = (int32_t)(osa_svc_fn_076a48());
    if (((uint32_t)(int32_t)(loc_0)) == 0u) {
        return (int32_t)(osa_svc_fn_076a30((int8_t)(((int32_t)1u))));
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x07703c sub_7703C */
uint32_t osa_svc_fn_07703c(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_0 = (uint32_t)(((uint32_t)((((uint32_t)(0x10021AFCu)) + ((uint32_t)(int32_t)(((int32_t)(0x14u * ((uint32_t)(int32_t)(arg_0))))))))));
    loc_1 = (int32_t)(acib_mem_read32((arg_1 + 4u * 1u)));
    loc_2 = (int32_t)(acib_mem_read32((arg_1 + 4u * 2u)));
    loc_3 = (int32_t)(acib_mem_read32((arg_1 + 4u * 3u)));
    loc_4 = (int32_t)(acib_mem_read32((arg_1 + 4u * 4u)));
    acib_mem_write32(loc_0, (uint32_t)(acib_mem_read32(arg_1)));
    acib_mem_write32((loc_0 + 4u * 1u), (uint32_t)(loc_1));
    acib_mem_write32((loc_0 + 4u * 2u), (uint32_t)(loc_2));
    acib_mem_write32((loc_0 + 4u * 3u), (uint32_t)(loc_3));
    acib_mem_write32((loc_0 + 4u * 4u), (uint32_t)(loc_4));
    return (uint32_t)((loc_0 + (4u * 5u)));
}

/* @provenance RVA 0x0770ec sub_770EC */
int32_t osa_svc_fn_0770ec(void)
{
    osa_svc_fn_0771ec((int32_t)(((int32_t)1u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0771ec sub_771EC */
int32_t osa_svc_fn_0771ec(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(0x10021B76u, (uint8_t)(((uint32_t)(acib_mem_read_s8(0x10021B76u)) | 2u)));
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        return (int32_t)(osa_svc_fn_07720c((int32_t)(((uint16_t)(acib_mem_read_s16(0x10021B74u))))));
    }
    acib_mem_write16(0x10021B74u, (uint16_t)(((int16_t)0x7530u)));
    return (int32_t)(osa_svc_fn_07720c((int32_t)(((int32_t)0x7530u))));
}

/* @provenance RVA 0x07720c sub_7720C */
uint32_t osa_svc_fn_07720c(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    uint32_t loc_7 = 0;
    uint32_t loc_8 = 0;
    int8_t loc_9 = 0;
    int32_t loc_10 = 0;
    int32_t loc_11 = 0;
    osa_svc_fn_0772a8();
    loc_1 = (uint32_t)(0x520D0000u);
    loc_2 = (int32_t)(acib_mmio_read32(0x520D0004u));
    acib_mmio_write32(0x520D0004u, (uint32_t)((((uint32_t)(int32_t)(loc_2)) & 0xFFFFFFFEu)));
    loc_3 = (uint32_t)(((uint16_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(acib_mem_read_s32(0x1002096Cu))), ((uint32_t)(((int32_t)(0x64u * ((uint32_t)(int32_t)(arg_0)))))))))));
    if (loc_3 <= 1u) {
        loc_3 = (uint32_t)(((int32_t)1u));
    }
    loc_4 = (uint32_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(acib_mem_read_s32(0x1002096Cu))), loc_3)));
    acib_mmio_write32(0x520D000Cu, (uint32_t)(((loc_3 & 0x7FFu) | acib_arm_lsl32(acib_arm_lsr32(((uint32_t)(acib_mmio_read32(0x520D000Cu))), 0xBu), 0xBu))));
    loc_5 = (uint32_t)(((int32_t)(0x64u * ((uint32_t)(((uint16_t)(acib_mem_read_s16(0x10021B74u))))))));
    if (((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(acib_mem_read_s32(0x1002096Cu))), loc_3)) == loc_5) {
        loc_6 = (int32_t)(((int32_t)0u));
    } else {
        loc_7 = (uint32_t)((loc_4 - loc_5));
        loc_8 = (uint32_t)(((uint32_t)acib_arm_udiv32(loc_5, (loc_4 - loc_5))));
        if ((loc_4 + (loc_7 * ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(((uint16_t)(loc_8)))))))))) < (loc_4 - (((uint32_t)(((uint16_t)(loc_8)))) * loc_7))) {
            loc_8 = (uint32_t)(((uint32_t)loc_8 & ~0xFFFFu) | (((uint32_t)((loc_8 + 1u))) & 0xFFFFu));
        }
        loc_9 = (int8_t)(loc_8);
        if (((uint32_t)(((uint16_t)(loc_8)))) >= 0x80u) {
            loc_9 = (int8_t)(((int8_t)0xFFFFFF80u));
        }
        loc_6 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_9)) & 0x7Fu)));
    }
    acib_mmio_write32(0x520D0010u, (uint32_t)((((uint32_t)(int32_t)(loc_6)) + (acib_mmio_read32(0x520D0010u) & 0xFFFFFF80u))));
    loc_10 = (int32_t)(acib_mmio_read32(0x520DC000u));
    acib_mmio_write32(0x520DC000u, (uint32_t)(((((uint32_t)(int32_t)(loc_10)) & 0xFFFF0FFFu) | 0x4000u)));
    loc_11 = (int32_t)(((int32_t)(acib_mmio_read32(0x520D0004u) | 1u)));
    acib_mmio_write32(0x520D0004u, (uint32_t)(loc_11));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x0772a8 sub_772A8 */
uint32_t osa_svc_fn_0772a8(void)
{
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x520D401Cu));
    acib_mmio_write32(0x520D401Cu, (uint32_t)((loc_0 | 0x2000u)));
    phy_mdm_fn_0783d0();
    acib_mem_write8(0x10021B76u, (uint8_t)(((uint32_t)(acib_mem_read_s8(0x10021B76u)) | 1u)));
    return (uint32_t)(0x10021B74u);
}

/* @provenance RVA 0x077320 sub_77320 */
int32_t osa_svc_fn_077320(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))), (uint16_t)(((int16_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(((int32_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x10u)))), (uint32_t)(((int32_t)0x7000001u)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x077758 sub_77758 */
int32_t osa_svc_fn_077758(void)
{
    return (int32_t)((((uint32_t)(int32_t)(acib_mem_read_s8(0x10021B8Cu))) == 1u));
}

/* @provenance RVA 0x0777bc sub_777BC */
uint32_t osa_svc_fn_0777bc(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    acib_mem_write32(arg_0, (uint32_t)(acib_mem_read32(arg_1)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(acib_mem_read32((arg_1 + 4u * 1u))));
    acib_mem_write32((arg_0 + 4u * 2u), (uint32_t)(acib_mem_read32((arg_1 + 4u * 2u))));
    loc_0 = (int32_t)(acib_mem_read32((arg_1 + 4u * 3u)));
    acib_mem_write32((arg_0 + 4u * 4u), (uint32_t)(((int32_t)0u)));
    acib_mem_write32((arg_0 + 4u * 3u), (uint32_t)(loc_0));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0777d4 sub_777D4 */
int32_t osa_svc_fn_0777d4(void)
{
    return (int32_t)(osa_svc_fn_0777e0((uint32_t)(0x10000708u)));
}

/* @provenance RVA 0x0777e0 sub_777E0 */
int32_t osa_svc_fn_0777e0(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    int32_t loc_7 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32((arg_0 + 4u * 3u)));
    loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(arg_0))) + 0x542023ABu)));
    loc_2 = (int32_t)(acib_mem_read_s32((arg_0 + 4u * 4u)));
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32((arg_0 + 4u * 2u)))) + ((uint32_t)(int32_t)(loc_0)))));
    loc_4 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32((arg_0 + 4u * 1u)))) ^ ((uint32_t)(int32_t)(((int32_t)(0x20u * ((uint32_t)(int32_t)(acib_mem_read_s32((arg_0 + 4u * 1u)))))))))));
    acib_mem_write32((arg_0 + 4u * 2u), (uint32_t)(loc_0));
    acib_mem_write32(arg_0, (uint32_t)(loc_1));
    loc_5 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + ((uint32_t)(int32_t)(loc_2)))));
    loc_6 = (int32_t)(((loc_4 ^ acib_arm_lsr32(((uint32_t)(loc_4)), 7u)) ^ acib_arm_lsl32((loc_4 ^ acib_arm_lsr32(((uint32_t)(loc_4)), 7u)), 0x16u)));
    loc_7 = (int32_t)(acib_arm_lsr32(((uint32_t)(loc_5)), 0x1Fu));
    loc_5 = (uint32_t)((uint32_t)loc_5 & ((uint32_t)((~0x80000000u))));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(loc_6));
    acib_mem_write32((arg_0 + 4u * 4u), (uint32_t)(loc_7));
    acib_mem_write32((arg_0 + 4u * 3u), (uint32_t)(loc_5));
    return (int32_t)(((loc_5 + ((uint32_t)(int32_t)(loc_1))) + ((uint32_t)(int32_t)(loc_6))));
}

/* @provenance RVA 0x077858 sub_77858 */
int32_t osa_svc_fn_077858(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(osa_svc_fn_0777d4());
}

/* @provenance RVA 0x077860 sub_77860 */
int32_t osa_svc_fn_077860(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_1 = (int32_t)(arg_0);
    for (loc_2 = (uint32_t)(((int32_t)0u)); loc_2 < arg_1; loc_2 = (uint32_t)((uint32_t)loc_2 + ((uint32_t)(((int32_t)4u))))) {
        arg_0 = (int32_t)(osa_svc_fn_0777d4());
        acib_mem_write32(((uint32_t)((((uint32_t)(int32_t)(loc_1)) + loc_2))), (uint32_t)(arg_0));
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0778f8 sub_778F8 */
uint32_t osa_svc_fn_0778f8(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (uint32_t)((0x10020310u + 4u * ((uint32_t)(int32_t)(((int32_t)(6u * ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1002046Cu)))))))))));
    acib_mem_write32((loc_1 + 4u * 0x33u), (uint32_t)(loc_0));
    acib_mem_write32((loc_1 + 4u * 0x34u), (uint32_t)(arg_0));
    acib_mem_write32((loc_1 + 4u * 0x35u), (uint32_t)(arg_1));
    acib_mem_write32((loc_1 + 4u * 0x37u), (uint32_t)(arg_3));
    acib_mem_write32((loc_1 + 4u * 0x36u), (uint32_t)(arg_2));
    loc_2 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x1002046Cu))));
    acib_mem_write32((loc_1 + 4u * 0x38u), (uint32_t)(arg_4));
    loc_3 = (uint32_t)(((uint32_t)acib_arm_umod32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) + 1u)))), 6u)));
    acib_mem_write8(0x1002046Cu, (uint8_t)(loc_3));
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x077948 sub_77948 */
uint32_t osa_svc_fn_077948(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)((acib_arm_lsr32(((uint32_t)(0xCE18u)), arg_0) & ((uint32_t)((arg_0 < 0x10u)))));
}

/* @provenance RVA 0x07795c sub_7795C */
uint32_t osa_svc_fn_07795c(int16_t arg_0)
{
    (void)arg_0;
    return (uint32_t)((acib_arm_lsr32(((uint32_t)(0xCD1u)), ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 0x24u))))) & ((uint32_t)((((uint32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 0x24u)))))) < 0xCu)))));
}

/* @provenance RVA 0x077978 sub_77978 */
int32_t osa_svc_fn_077978(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((uint8_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F5840u + acib_arm_lsr32(((uint32_t)((((uint32_t)(int32_t)(arg_0)) ^ arg_1))), 4u))))) + ((uint32_t)(acib_mem_read8((0x000F5840u + ((((uint32_t)(int32_t)(arg_0)) ^ arg_1) & 0xFu))))))))));
}

/* @provenance RVA 0x077a60 sub_77A60 */
int32_t osa_svc_fn_077a60(void)
{
    sub_6DC88();
    acib_mem_write32(0x1000071Cu, (uint32_t)(((int32_t)0xAu)));
    return (int32_t)((sub_6DD04(), 0u));
}

/* @provenance RVA 0x077c4a sub_77C4A */
uint32_t osa_svc_fn_077c4a(int32_t arg_0, uint32_t arg_1, uint32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int8_t loc_1 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_5 = 0;
    uint32_t loc_6 = 0;
    int8_t loc_7 = 0;
    int32_t loc_8 = 0;
    int8_t loc_9 = 0;
    uint32_t loc_10 = 0;
    uint32_t loc_11 = 0;
    int32_t loc_12 = 0;
    int32_t loc_13 = 0;
    int32_t loc_14 = 0;
    loc_1 = (int8_t)((arg_2 & 7u));
    loc_3 = (uint32_t)(arg_2);
    loc_5 = (uint32_t)((8u - (arg_2 & 7u)));
    if (loc_5 >= arg_1) {
        loc_5 = (uint32_t)(arg_1);
    }
    if (loc_5 > 7u) {
        loc_5 = (uint32_t)(((uint32_t)loc_5 & ~0xFFu) | (((uint32_t)(((int8_t)0u))) & 0xFFu));
    } else {
        loc_6 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_2)), 3u));
        loc_3 = (uint32_t)((arg_2 + loc_5));
        loc_7 = (int8_t)(acib_mem_read8(((uint32_t)((((uint32_t)(int32_t)(arg_0)) + acib_arm_lsr32(((uint32_t)(arg_2)), 3u))))));
        loc_8 = (int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0xFFFFFFFFu, loc_5)))))))), ((uint32_t)(loc_1)))));
        loc_9 = (int8_t)(((int32_t)(((uint32_t)(int32_t)(loc_7)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(((uint8_t)(loc_8)))))))))));
        if (((uint32_t)(int32_t)(arg_3)) == 1u) {
            loc_9 = (int8_t)(((int32_t)(((uint32_t)(int32_t)(loc_7)) | ((uint32_t)(int32_t)(loc_8)))));
        }
        acib_mem_write8(((uint32_t)((((uint32_t)(int32_t)(arg_0)) + loc_6))), (uint8_t)(loc_9));
    }
    loc_10 = (uint32_t)(((uint8_t)(loc_5)));
    if (arg_1 > ((uint32_t)(((uint8_t)(loc_5))))) {
        loc_11 = (uint32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(((uint8_t)((arg_1 - loc_5))))), 3u)));
        if (loc_11 != 0u) {
            loc_12 = (int32_t)(((int32_t)0u));
            if (((uint32_t)(int32_t)(arg_3)) == 1u) {
                loc_12 = (int32_t)(((int32_t)0xFFu));
            }
            (void)memset(acib_addr_ptr(((uint32_t)((((uint32_t)(int32_t)(arg_0)) + acib_arm_lsr32(((uint32_t)(loc_3)), 3u))))), loc_12, loc_11);
            loc_5 = (uint32_t)(((uint32_t)loc_5 & ~0xFFu) | (((uint32_t)((loc_5 + (8u * loc_11)))) & 0xFFu));
            loc_3 = (uint32_t)((uint32_t)loc_3 + ((uint32_t)((8u * loc_11))));
            loc_10 = (uint32_t)(((uint8_t)(loc_5)));
        }
    }
    if (arg_1 > loc_10) {
        loc_13 = (int32_t)(((int32_t)acib_arm_lsl32(0xFFFFFFFFu, (arg_1 - loc_5))));
        loc_14 = (int32_t)(acib_mem_read8(((uint32_t)((((uint32_t)(int32_t)(arg_0)) + acib_arm_lsr32(((uint32_t)(loc_3)), 3u))))));
        if (((uint32_t)(int32_t)(arg_3)) == 1u) {
            loc_10 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_14)) | ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_13)))))))));
        } else {
            loc_10 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_13)) & ((uint32_t)(int32_t)(loc_14)))));
        }
        acib_mem_write8(((uint32_t)((((uint32_t)(int32_t)(arg_0)) + acib_arm_lsr32(((uint32_t)(loc_3)), 3u)))), (uint8_t)(loc_10));
    }
    return (uint32_t)(loc_10);
}

/* @provenance RVA 0x077ec4 sub_77EC4 */
int32_t osa_svc_fn_077ec4(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
        loc_0 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 5u));
        loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)));
        loc_2 = (int32_t)(acib_mem_read32(((uint32_t)((((uint32_t)(int32_t)(loc_1)) + (4u * acib_arm_lsr32(((uint32_t)(arg_0)), 5u)))))));
        loc_3 = (int32_t)(((int32_t)acib_arm_lsl32(1u, (arg_0 & 0x1Fu))));
        if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) & ((uint32_t)(int32_t)(loc_3)))))) == 0u) {
            acib_mem_write32(((uint32_t)((((uint32_t)(int32_t)(loc_1)) + (4u * loc_0)))), (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) | ((uint32_t)(int32_t)(loc_2))))));
            return (int32_t)(((int32_t)1u));
        }
    } else {
        osa_svc_fn_077fac((uint32_t)(((int32_t)0x38006u)));
    }
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x077fa4 sub_77FA4 */
int32_t osa_svc_fn_077fa4(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(osa_svc_fn_077ec4((uint32_t)(arg_0), (int32_t)(arg_1)));
}

/* @provenance RVA 0x077fa8 sub_77FA8 */
int32_t osa_svc_fn_077fa8(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(osa_svc_fn_077ec4((uint32_t)(arg_0), (int32_t)(arg_1)));
}

/* @provenance RVA 0x077fac sub_77FAC */
int32_t osa_svc_fn_077fac(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((sub_740A8(arg_0, 0x83FCu), 0u));
}

/* @provenance RVA 0x077fc0 sub_77FC0 */
int32_t osa_svc_fn_077fc0(int32_t arg_0, int32_t arg_1, int16_t arg_2, int32_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x18u)))), (uint16_t)(arg_2));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(arg_1));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Au)))), (uint16_t)(((int16_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(((int32_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Cu)))), (uint32_t)(((int32_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))), (uint32_t)(((int32_t)0u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(arg_4));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint32_t)((((uint32_t)(int32_t)(arg_3)) != 0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x077fe0 sub_77FE0 */
int32_t osa_svc_fn_077fe0(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Au))))))) + ((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x18u))))))))));
}

/* @provenance RVA 0x093af0 sub_93AF0 */
int32_t osa_svc_fn_093af0(void)
{
    return (int32_t)(((acib_mem_read32((0x100260D0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10026340u))))))))) + 0x64u)))))) == 0u) && (acib_mem_read32((0x100260D0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x1Cu * ((uint32_t)(int32_t)(((int32_t)(1u - ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10026340u))))))))))))) + 0x64u)))))) == 0u)));
}

/* @provenance RVA 0x0a0d88 sub_A0D88 */
int32_t osa_svc_fn_0a0d88(void)
{
    return (int32_t)((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1002853Cu))))) ^ 0x15u)))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s8((0x100284F4u + 0u)))))) ^ 0x15u)))))))) == 0u));
}

/* @provenance RVA 0x0a0f54 sub_A0F54 */
uint32_t osa_svc_fn_0a0f54(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    switch (arg_1) {
    case 4: {
        arg_0 = (uint32_t)(((uint32_t)arg_0 & ~0xFFFFu) | (((uint32_t)((2u * arg_0))) & 0xFFFFu));
        break;
    }
    case 3: {
        arg_0 = (uint32_t)(((uint32_t)arg_0 & ~0xFFFFu) | (((uint32_t)((8u * arg_0))) & 0xFFFFu));
        break;
    }
    case 2: {
        arg_0 = (uint32_t)acib_arm_lsr32((uint32_t)arg_0, 1u);
        break;
    }
    default: {
        break;
    }
    }
    return (uint32_t)(((uint16_t)(arg_0)));
}

/* @provenance RVA 0x0ce910 sub_CE910 */
int32_t osa_svc_fn_0ce910(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s8(0x1000AB8Cu))));
}

/* @provenance RVA 0x0e0900 sub_E0900 */
int32_t osa_svc_fn_0e0900(int32_t arg_0, int32_t arg_1, uint32_t arg_2, uint32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int8_t loc_0 = 0;
    loc_0 = (int8_t)(arg_2);
    if (arg_3 > arg_2) {
        loc_0 = (int8_t)(arg_3);
    }
    while (arg_3 < arg_2) {
        if (((uint32_t)(acib_mem_read8(((uint32_t)((((uint32_t)(int32_t)(arg_1)) + arg_3)))))) == ((uint32_t)(int32_t)(arg_0))) {
            return (int32_t)(((uint8_t)(arg_3)));
        }
        arg_3 = (uint32_t)((uint32_t)arg_3 + 1u);
    }
    arg_3 = (uint32_t)(((uint32_t)arg_3 & ~0xFFu) | (((uint32_t)(loc_0)) & 0xFFu));
    return (int32_t)(((uint8_t)(arg_3)));
}
