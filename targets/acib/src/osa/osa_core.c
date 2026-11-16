/*
 * src/osa/osa_core.c — OSA real-time operating system queues, mailboxes, timers, pools, and task state.
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

/* @provenance RVA 0x068300 sub_68300 */
int32_t osa_core_fn_068300(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int8_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_1 = (int8_t)(acib_mem_read_s8(arg_1));
    loc_0 = (uint32_t)((arg_1 + 1u));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint8_t)(loc_1));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x15u)));
    for (loc_3 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_3)) != 3u; loc_3 = (int32_t)((uint32_t)loc_3 + 1u)) {
        acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) + ((uint32_t)(int32_t)(loc_3)))))), (uint8_t)(acib_mem_read8((loc_0 + ((uint32_t)(int32_t)(loc_3))))));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x06838c sub_6838C */
int32_t osa_core_fn_06838c(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Cu)))), (uint8_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x068a6c sub_68A6C */
uint32_t osa_core_fn_068a6c(void)
{
    acib_mem_write8(0x1000EB5Eu, (uint8_t)(((int8_t)0u)));
    return (uint32_t)(0x1000E8A8u);
}

/* @provenance RVA 0x068adc sub_68ADC */
uint32_t osa_core_fn_068adc(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    (void)memset(acib_addr_ptr(arg_0), 0, 4u);
    loc_1 = (uint32_t)((memset(acib_addr_ptr((arg_0 + 4u)), 0, 4u), (arg_0 + 4u)));
    acib_mem_write8((arg_0 + 0x1Au), (uint8_t)(((int8_t)0u)));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x069110 sub_69110 */
int32_t osa_core_fn_069110(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s16(0x1000EB58u))));
}

/* @provenance RVA 0x069140 sub_69140 */
int32_t osa_core_fn_069140(void)
{
    if (((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1000EB60u))))) <= 2u) {
        return (int32_t)(acib_mem_read32((0x000F2998u + 4u * ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1000EB60u))))))));
    } else {
        return (int32_t)(((int32_t)0x110u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x06a7d0 sub_6A7D0 */
int8_t osa_core_fn_06a7d0(int8_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(0x1000EC2Cu, (uint8_t)(arg_0));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x06ab74 sub_6AB74 */
int32_t osa_core_fn_06ab74(void)
{
    return (int32_t)(acib_mem_read_s32(0x1000EC24u));
}

/* @provenance RVA 0x06aba4 sub_6ABA4 */
uint32_t osa_core_fn_06aba4(void)
{
    acib_mem_write8(0x1000ECC8u, (uint8_t)(((int8_t)4u)));
    acib_mem_write16(0x1000ECC6u, (uint16_t)(((int16_t)0x302u)));
    return (uint32_t)(0x1000ECC4u);
}

/* @provenance RVA 0x06b888 sub_6B888 */
uint32_t osa_core_fn_06b888(int16_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    acib_mem_write16(0x1000EDFCu, (uint16_t)(arg_0));
    acib_mem_write32(0x1000EDECu, (uint32_t)(arg_1));
    acib_mem_write16(0x1000EDFEu, (uint16_t)(((int16_t)0u)));
    loc_0 = (uint32_t)((memset(acib_addr_ptr(0x1000EDF0u), 0, 0xCu), 0x1000EDF0u));
    acib_mem_write8(0x1000EDF0u, (uint8_t)(((int8_t)0u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x06b8b4 sub_6B8B4 */
uint32_t osa_core_fn_06b8b4(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)((memcpy_wrapper(acib_addr_ptr(0x1000EDF0u), acib_addr_ptr(arg_0), 0xCu), 0x1000EDF0u));
}

/* @provenance RVA 0x06b968 sub_6B968 */
uint32_t osa_core_fn_06b968(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)))), (uint16_t)(((int16_t)0x100u)));
    loc_0 = (int32_t)(acib_mem_read_s32(0x1000EDECu));
    acib_mem_write16(((uint32_t)(arg_0)), (uint16_t)(acib_mem_read_s16(0x1000EDFCu)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(loc_0));
    return (uint32_t)((memset(acib_addr_ptr(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))), 0, 7u), ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))));
}

/* @provenance RVA 0x06bb80 sub_6BB80 */
uint32_t osa_core_fn_06bb80(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    int8_t loc_4 = 0;
    uint32_t loc_5 = 0;
    loc_0 = (int32_t)(((int32_t)0u));
    loc_1 = (uint32_t)(((uint32_t)((0x1000EE00u + 4u))));
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        loc_1 = (uint32_t)(((uint32_t)(0x1000EE58u)));
        loc_0 = (int32_t)(((int32_t)0x54u));
    }
    loc_2 = (int32_t)(acib_mem_read8((0x1000EE00u + ((uint32_t)(int32_t)(loc_0)))));
    if ((acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 4u))))) == 0x100004u) || ((void)((loc_3 = (uint32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1000EEA8u)))))))), ((((uint32_t)(int32_t)(arg_1)) == ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1000EEA8u))))))))) {
        acib_mem_write8(0x1000EEA8u, (uint8_t)(arg_1));
        loc_3 = (uint32_t)((loc_1 + ((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(loc_2))))))));
        acib_mem_write8((loc_3 + 0xCu), (uint8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 8u)))))));
        acib_mem_write8((loc_3 + 0xDu), (uint8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 9u)))))));
        acib_mem_write8((loc_3 + 0xEu), (uint8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 0xAu)))))));
        loc_4 = (int8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 0xBu))))));
        acib_mem_write32((((uint32_t)(loc_3)) + (4u * 2u)), (uint32_t)(((int32_t)0u)));
        acib_mem_write16(((uint32_t)(loc_3)), (uint16_t)(arg_1));
        acib_mem_write8((loc_3 + 0xFu), (uint8_t)(loc_4));
        acib_mem_write8((0x1000EE00u + ((uint32_t)(int32_t)(loc_0))), (uint8_t)(((uint32_t)acib_arm_umod32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) + 1u)))), 5u))));
        loc_5 = (uint32_t)(acib_mmio_read32(0x52160020u));
        acib_mem_write32((((uint32_t)(loc_3)) + (4u * 1u)), (uint32_t)(loc_5));
    }
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x06bbf0 sub_6BBF0 */
int32_t osa_core_fn_06bbf0(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    if (((uint32_t)(int32_t)(arg_2)) != 0u) {
        loc_0 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x1000EE54u))));
        loc_1 = (uint32_t)(((uint8_t)(acib_mem_read_s8(0x1000EE54u))));
        do {
            loc_2 = (uint32_t)((0x1000EE00u + (0x10u * loc_1)));
            if (acib_mem_read32((((uint32_t)(loc_2)) + (4u * 0x18u))) == 0u) {
                loc_3 = (uint32_t)(((uint32_t)((loc_2 + 0x58u))));
                if (((uint32_t)(acib_mem_read16(loc_3))) == ((uint32_t)(int32_t)(arg_0))) {
                    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
                        loc_4 = (uint32_t)(0xEEEEEEEEu);
                    } else {
                        loc_4 = (uint32_t)(acib_mmio_read32(0x52160020u));
                    }
                    acib_mem_write32((((uint32_t)(loc_3)) + (4u * 2u)), (uint32_t)(loc_4));
                    arg_2 = (int32_t)((uint32_t)arg_2 - 1u);
                }
            }
            if (((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_2)), 0x18u)))) == 0u) {
                break;
            }
            loc_1 = (uint32_t)(((uint32_t)acib_arm_umod32((((uint32_t)(((uint8_t)(loc_1)))) + 1u), 5u)));
        } while (loc_1 != ((uint32_t)(int32_t)(loc_0)));
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x06bc58 sub_6BC58 */
int32_t osa_core_fn_06bc58(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(osa_core_fn_06bbf0((int32_t)(arg_0), (int32_t)(((int32_t)1u)), (int32_t)(((int32_t)5u))));
}

/* @provenance RVA 0x06bc68 sub_6BC68 */
int32_t osa_core_fn_06bc68(void)
{
    return (int32_t)(((int32_t)0xACu));
}

/* @provenance RVA 0x06c774 sub_6C774 */
int32_t osa_core_fn_06c774(void)
{
    return (int32_t)((sub_740A8(0x2000u, 0x80BCu), 0u));
}

/* @provenance RVA 0x06c80c sub_6C80C */
int32_t osa_core_fn_06c80c(void)
{
    uint8_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint8_t)(acib_mmio_read32(0xE000ED04u));
    loc_1 = (int32_t)(loc_0);
    if (((uint32_t)(loc_0)) < 0x10u) {
        loc_3 = (int32_t)(((int32_t)0x13u));
        if (((uint32_t)(int32_t)(loc_1)) == 0xFu) {
            loc_3 = (int32_t)(((int32_t)0x12u));
        }
        if (((uint32_t)(int32_t)(loc_1)) == 0u) {
            return (int32_t)(((int32_t)0x1Fu));
        }
    } else {
        loc_2 = (int32_t)(((uint8_t)(acib_arm_lsr32(((uint32_t)(acib_mmio_read32(((uint32_t)((((uint32_t)(0xE000E400u)) + (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_0)) - 0x10u)))) & 0xFFFFFFFCu))))))), ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_0)) - 0x10u)))))))) & 0x18u))))))));
        loc_3 = (int32_t)(((int32_t)0x12u));
        if (((uint32_t)(int32_t)(loc_2)) == 0x20u) {
            return (int32_t)(((int32_t)0x11u));
        }
    }
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x06c950 sub_6C950 */
uint32_t osa_core_fn_06c950(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((((uint32_t)(0x10021638u)) + ((uint32_t)(int32_t)(arg_0))));
    acib_mem_write8((loc_0 + 0x240u), (uint8_t)(arg_1));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x06cf54 sub_6CF54 */
int32_t osa_core_fn_06cf54(void)
{
    return (int32_t)((((uint32_t)(int32_t)(acib_mem_read_s8(0x1002187Du))) == 0u));
}

/* @provenance RVA 0x06d234 sub_6D234 */
int32_t osa_core_fn_06d234(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((arg_0 >= ((uint32_t)(0x10000000u))) && (arg_0 < 0x1002D2C4u)));
}

/* @provenance RVA 0x06d258 sub_6D258 */
int32_t osa_core_fn_06d258(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((((uint32_t)(int32_t)(arg_0)) != 0u));
}

/* @provenance RVA 0x06d804 sub_6D804 */
uint32_t osa_core_fn_06d804(uint32_t arg_0, int32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int8_t loc_0 = 0;
    bool loc_1 = false;
    int8_t loc_2 = 0;
    loc_0 = (int8_t)(arg_2);
    if (arg_2 <= 0x1Au) {
        loc_0 = (int8_t)(((int8_t)0x1Au));
    }
    loc_1 = (((((uint32_t)(int32_t)(arg_1)) == 0u)) != 0);
    loc_2 = (int8_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 3u)));
    if (!(loc_1 != 0)) {
        loc_0 = (int8_t)(arg_2);
    }
    acib_mem_write8(arg_0, (uint8_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_0))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06d858 sub_6D858 */
int32_t osa_core_fn_06d858(uint32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)((((arg_1 < 0x1Au) && (arg_0 > 0x1Au)) || (((uint32_t)(int32_t)(arg_2)) == 0u)));
}

/* @provenance RVA 0x06d878 sub_6D878 */
int32_t osa_core_fn_06d878(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 0x1001ACE4u)));
    acib_mem_write32(((uint32_t)(loc_0)), (uint32_t)(((int32_t)0u)));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 4u)))), (uint8_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x06d8d4 sub_6D8D4 */
int32_t osa_core_fn_06d8d4(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(acib_mem_read32((((uint32_t)(0x1001ACE4u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_0))))))))));
}

/* @provenance RVA 0x06d8e4 sub_6D8E4 */
uint32_t osa_core_fn_06d8e4(void)
{
    acib_mem_write32(0x1001AD2Cu, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x1001AD30u, (uint8_t)(((int8_t)0x1Au)));
    acib_mem_write32(0x1001AD24u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x1001AD28u, (uint8_t)(((int8_t)0x1Au)));
    return (uint32_t)(0x1001AD24u);
}

/* @provenance RVA 0x06dd5c sub_6DD5C */
int32_t osa_core_fn_06dd5c(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x1001AE24u))));
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x1001AE24u))) != 0u) {
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x06df20 sub_6DF20 */
uint32_t osa_core_fn_06df20(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(((int32_t)acib_arm_lsl32(1u, (arg_0 & 0x1Fu))));
    loc_1 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 5u));
    acib_mem_write32(((uint32_t)(((4u * loc_1) - 0x1FFF1E00u))), (uint32_t)(loc_0));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x06e0dc sub_6E0DC */
uint32_t osa_core_fn_06e0dc(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    if (arg_0 > 0x23u) {
        return (uint32_t)(osa_core_fn_06e0ee());
    }
    acib_mem_write32(((uint32_t)((0x1001AEACu + (4u * arg_0)))), (uint32_t)(arg_1));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e0ee sub_6E0EE */
int32_t osa_core_fn_06e0ee(void)
{
    return (int32_t)((sub_740A8(0x8809u, 0x8199u), 0u));
}

/* @provenance RVA 0x06e112 sub_6E112 */
int32_t osa_core_fn_06e112(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1A0u)))), (uint32_t)(arg_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x06e124 sub_6E124 */
uint32_t osa_core_fn_06e124(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    acib_mem_write16(arg_0, (uint16_t)(((uint32_t)(acib_mem_read16(arg_0)) | ((uint32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_1)))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e134 sub_6E134 */
uint32_t osa_core_fn_06e134(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    acib_mem_write8(arg_0, (uint8_t)(((uint32_t)(acib_mem_read8(arg_0)) | ((uint32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_1)))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e164 sub_6E164 */
uint32_t osa_core_fn_06e164(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(arg_0, (uint32_t)(((uint32_t)(acib_mem_read32(arg_0)) | ((uint32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_1)))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e184 sub_6E184 */
uint32_t osa_core_fn_06e184(uint32_t arg_0, int16_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(3u, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e198 sub_6E198 */
uint32_t osa_core_fn_06e198(uint32_t arg_0, int32_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(3u, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e1ac sub_6E1AC */
uint32_t osa_core_fn_06e1ac(uint32_t arg_0, int16_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(7u, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e1c0 sub_6E1C0 */
uint32_t osa_core_fn_06e1c0(uint32_t arg_0, int32_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(7u, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e1d4 sub_6E1D4 */
uint32_t osa_core_fn_06e1d4(uint32_t arg_0, int16_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0xFu, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e1e8 sub_6E1E8 */
uint32_t osa_core_fn_06e1e8(uint32_t arg_0, int32_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0xFu, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e1fc sub_6E1FC */
uint32_t osa_core_fn_06e1fc(uint32_t arg_0, int32_t raw_arg_1, int32_t raw_arg_2) {
    int16_t arg_1 = (int16_t)raw_arg_1;
    int8_t arg_2 = (int8_t)raw_arg_2;
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0x1Fu, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e210 sub_6E210 */
uint32_t osa_core_fn_06e210(uint32_t arg_0, int32_t raw_arg_1, int32_t raw_arg_2) {
    int16_t arg_1 = (int16_t)raw_arg_1;
    int8_t arg_2 = (int8_t)raw_arg_2;
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0xFFu, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e224 sub_6E224 */
uint32_t osa_core_fn_06e224(uint32_t arg_0, int16_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & 0xF800u))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e230 sub_6E230 */
uint32_t osa_core_fn_06e230(uint32_t arg_0, int32_t raw_arg_1, int32_t raw_arg_2) {
    int16_t arg_1 = (int16_t)raw_arg_1;
    int8_t arg_2 = (int8_t)raw_arg_2;
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write16(arg_0, (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(arg_0))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(0xFFFu, ((uint32_t)(arg_2))))))))))))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x06e260 sub_6E260 */
int32_t osa_core_fn_06e260(uint32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 7u)));
}

/* @provenance RVA 0x06e26c sub_6E26C */
int32_t osa_core_fn_06e26c(uint32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 0xFu)));
}

/* @provenance RVA 0x06e278 sub_6E278 */
int32_t osa_core_fn_06e278(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 0x3Fu)));
}

/* @provenance RVA 0x06e284 sub_6E284 */
int32_t osa_core_fn_06e284(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 0x7Fu)));
}

/* @provenance RVA 0x06e290 sub_6E290 */
int32_t osa_core_fn_06e290(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 0x3FFu)));
}

/* @provenance RVA 0x06e29c sub_6E29C */
int32_t osa_core_fn_06e29c(uint32_t arg_0, int32_t raw_arg_1) {
    int8_t arg_1 = (int8_t)raw_arg_1;
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(acib_mem_read16(arg_0))), ((uint32_t)(arg_1)))))) & 0x1FFFu)));
}

/* @provenance RVA 0x06e500 sub_6E500 */
int32_t osa_core_fn_06e500(int32_t arg_0, int32_t arg_1, int32_t arg_2, int16_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(arg_1));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xEu)))), (uint16_t)(arg_3));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))), (uint16_t)(arg_3));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_4)) & 0xFC03Fu)))) + (acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))) & 0xEFF03FC0u))));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x06e610 sub_6E610 */
int32_t osa_core_fn_06e610(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    if (((uint32_t)(int32_t)(arg_0)) == 3u) {
        loc_1 = (uint32_t)(0x520FC800u);
    } else {
        if (((uint32_t)(int32_t)(arg_0)) != 2u) {
            return (int32_t)(osa_core_fn_06e622());
        }
        loc_1 = (uint32_t)(0x520F8800u);
    }
    return (int32_t)(acib_mem_read32((loc_1 + 4u * 2u)));
}

/* @provenance RVA 0x06e622 sub_6E622 */
int32_t osa_core_fn_06e622(void)
{
    sub_74240(0xB802u);
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x06e6f0 sub_6E6F0 */
int32_t osa_core_fn_06e6f0(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    bool loc_0 = false;
    loc_0 = (((((uint32_t)(int32_t)(arg_0)) == 3u)) != 0);
    if (((uint32_t)(int32_t)(arg_0)) != 3u) {
        loc_0 = (((((uint32_t)(int32_t)(arg_0)) == 1u)) != 0);
    }
    if (!(loc_0 != 0)) {
        if (((uint32_t)(int32_t)(arg_0)) != 0u) {
            sub_740A8(0xC000u, 0x8210u);
        } else {
            if ((((uint32_t)(int32_t)(osa_svc_fn_0a0d88())) == 0u) || (((uint32_t)(int32_t)(osa_svc_fn_093af0())) == 0u)) {
                acib_mem_write8(arg_1, (uint8_t)(((uint32_t)(acib_mem_read8(arg_1)) & (~8u))));
            }
        }
    }
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x06f108 sub_6F108 */
uint32_t osa_core_fn_06f108(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) - 8u)));
    acib_mmio_write16(((uint32_t)((((uint32_t)(0x521B8090u)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 4u))))))), (uint16_t)(arg_1));
    acib_mmio_write16(((uint32_t)((((uint32_t)(0x521B8090u)) + ((uint32_t)(int32_t)(loc_0))))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) & 0x7FFu)))) | 0x8000u))));
    acib_mem_write32(((uint32_t)((((uint32_t)(0x100200A4u)) + ((uint32_t)(int32_t)(loc_0))))), (uint32_t)(arg_1));
    loc_1 = (uint32_t)((((uint32_t)(0x100200A4u)) + ((uint32_t)(int32_t)(loc_0))));
    acib_mem_write32((((uint32_t)(loc_1)) + (4u * 1u)), (uint32_t)(arg_2));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x06f134 sub_6F134 */
int32_t osa_core_fn_06f134(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (uint32_t)((((uint32_t)(0x100200A4u)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
    loc_1 = (int32_t)(acib_mem_read32((((uint32_t)(loc_0)) - (4u * 2u))));
    loc_2 = (int32_t)(acib_mem_read32((((uint32_t)(loc_0)) - (4u * 1u))));
    acib_mem_write32((arg_1 + 4u * 1u), (uint32_t)(loc_2));
    acib_mem_write32(arg_1, (uint32_t)(loc_1));
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x06f198 sub_6F198 */
int32_t osa_core_fn_06f198(int32_t arg_0)
{
    (void)arg_0;
    int16_t loc_0 = 0;
    uint16_t loc_1 = 0;
    int16_t loc_2 = 0;
    loc_0 = (int16_t)(acib_mmio_read16((0x521B8088u + 2u * 2u)));
    loc_1 = (uint16_t)(acib_mmio_read16((0x521B8088u + 2u * 0u)));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(((int32_t)(((uint32_t)(loc_1)) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & 0x3FFu)))), 0x10u))))))));
    loc_2 = (int16_t)(acib_mmio_read16(0x521B8084u));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint16_t)(loc_2));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x06f1e0 sub_6F1E0 */
int32_t osa_core_fn_06f1e0(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_1 = (uint32_t)((arg_0 - 0x200u));
    loc_2 = (int32_t)(((int32_t)1u));
    if (((((loc_1 >= 0x20u) && ((arg_0 > 0x13u) || (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(1u, arg_0)))) & 0xFF022u)))) == 0u))) && (((arg_0 - 0x8044u) > 0x17u) || (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(1u, (arg_0 - 0x44u))))) & 0x900001u)))) == 0u))) && (((arg_0 - 0x5Cu) >= 2u) && (((arg_0 - 0x8016u) >= 2u) && (arg_0 != 0x12Du)))) && (arg_0 != 0x9816u)) {
        return (int32_t)(((int32_t)0u));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x06f2d8 sub_6F2D8 */
int32_t osa_core_fn_06f2d8(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_0 = (uint32_t)(0x10000642u);
    for (loc_1 = (int32_t)(((int32_t)0u)); true; loc_1 = (int32_t)((uint32_t)loc_1 + 1u)) {
        if (((uint32_t)(int32_t)(loc_1)) != 3u) {
            if (((uint32_t)(int32_t)(loc_1)) == 8u) {
                break;
            }
            loc_2 = (uint32_t)(acib_mem_read16((((uint32_t)(loc_0)) - (2u * 1u))));
            if ((arg_0 >= loc_2) && ((loc_2 + ((uint32_t)(((uint8_t)(acib_mem_read_s8(loc_0)))))) > arg_0)) {
                return (int32_t)(((uint8_t)(loc_1)));
            }
        }
        loc_0 = (uint32_t)((uint32_t)loc_0 + (1u * 6u));
    }
    loc_1 = (int32_t)(((uint32_t)loc_1 & ~0xFFu) | (((uint32_t)(((int8_t)0xFFFFFFFFu))) & 0xFFu));
    return (int32_t)(((uint8_t)(loc_1)));
}

/* @provenance RVA 0x06f308 sub_6F308 */
int32_t osa_core_fn_06f308(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(osa_core_fn_06f2d8((uint32_t)(arg_0)));
}

/* @provenance RVA 0x06f30c sub_6F30C */
int32_t osa_core_fn_06f30c(void)
{
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x520D50E0u));
    acib_mmio_write32(0x520D50E0u, (uint32_t)((loc_0 | 0x3038u)));
    acib_mmio_write32(0x520D50D8u, (uint32_t)(((int32_t)1u)));
    return (int32_t)(phy_mdm_fn_0783c4());
}

/* @provenance RVA 0x06f514 sub_6F514 */
int32_t osa_core_fn_06f514(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    bool loc_1 = false;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x520D50F0u));
    loc_1 = (((((uint32_t)(int32_t)(arg_0)) == 1u)) != 0);
    loc_2 = (int32_t)(((int32_t)0x800u));
    if ((loc_1) != 0) {
        loc_2 = (int32_t)(((int32_t)0x400u));
    }
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | ((uint32_t)(int32_t)(loc_0)))));
    acib_mmio_write32(0x520D50F0u, (uint32_t)(loc_3));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x06f534 sub_6F534 */
int32_t osa_core_fn_06f534(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    bool loc_1 = false;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x520D50F0u));
    loc_1 = (((((uint32_t)(int32_t)(arg_0)) == 1u)) != 0);
    loc_2 = (int32_t)(((int32_t)0xFFFFF7FFu));
    if ((loc_1) != 0) {
        loc_2 = (int32_t)(((int32_t)0xFFFFFBFFu));
    }
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) & ((uint32_t)(int32_t)(loc_0)))));
    acib_mmio_write32(0x520D50F0u, (uint32_t)(loc_3));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x06f554 sub_6F554 */
int32_t osa_core_fn_06f554(int32_t arg_0)
{
    (void)arg_0;
    bool loc_0 = false;
    int32_t loc_1 = 0;
    loc_0 = (((((uint32_t)(int32_t)(arg_0)) == 1u)) != 0);
    loc_1 = (int32_t)(((int32_t)0x100u));
    if ((loc_0) != 0) {
        loc_1 = (int32_t)(((int32_t)0x80u));
    }
    acib_mmio_write32(0x520D50D8u, (uint32_t)(loc_1));
    return (int32_t)(phy_mdm_fn_0783c4());
}

/* @provenance RVA 0x06f578 sub_6F578 */
uint32_t osa_core_fn_06f578(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(((uint16_t)(arg_0)));
    loc_1 = (uint32_t)(((uint16_t)((arg_0 >> 16) & 0xFFFFu)));
    acib_mmio_write32(0x520D5124u, (uint32_t)(loc_0));
    acib_mmio_write32(0x520D5128u, (uint32_t)(loc_1));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x06f7a0 sub_6F7A0 */
uint32_t osa_core_fn_06f7a0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - ((uint32_t)(int32_t)(arg_0)))))) & 0xFFFFFFFu)));
    if (acib_arm_lsr32(((uint32_t)(loc_0)), 0x1Bu) != 0u) {
        loc_0 = (uint32_t)((uint32_t)loc_0 - ((uint32_t)(((int32_t)0x10000000u))));
    }
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x06f7b4 sub_6F7B4 */
uint32_t osa_core_fn_06f7b4(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - ((uint32_t)(int32_t)(arg_0)))))) & 0x3FFFFFFu)));
    if (acib_arm_lsr32(((uint32_t)(loc_0)), 0x19u) != 0u) {
        loc_0 = (uint32_t)((uint32_t)loc_0 - ((uint32_t)(((int32_t)0x4000000u))));
    }
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x06f8b8 sub_6F8B8 */
int32_t osa_core_fn_06f8b8(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u))))))) + ((uint32_t)(int32_t)(arg_2)))));
    loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(((uint32_t)(arg_0))) + ((uint32_t)(int32_t)(arg_1)))))) + ((uint32_t)(int32_t)(((int32_t)acib_arm_sdiv32(loc_0, ((int32_t)0x4E2u))))))))) + ((uint32_t)(int32_t)(acib_arm_asr32(((int32_t)acib_arm_smod32(loc_0, ((int32_t)0x4E2u))), 0x1Fu))))))) & 0x3FFFFFFu)));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_3)) + 4u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_smod32(loc_0, ((int32_t)0x4E2u))))) + ((uint32_t)(int32_t)((((((int32_t)acib_arm_smod32(loc_0, ((int32_t)0x4E2u))) < 0)) ? (int32_t)(((int32_t)0x4E2u)) : (int32_t)(((int32_t)0u)))))))));
    acib_mem_write32(((uint32_t)(arg_3)), (uint32_t)(loc_1));
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x06f8fc sub_6F8FC */
int32_t osa_core_fn_06f8fc(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(((uint32_t)(arg_1))) - acib_mem_read32(((uint32_t)(arg_0))))))) & 0x3FFFFFFu)));
    loc_1 = (int32_t)(loc_0);
    if (acib_arm_lsr32(((uint32_t)(loc_0)), 0x19u) != 0u) {
        loc_1 = (int32_t)((loc_0 - 0x4000000u));
    }
    loc_2 = (int32_t)((loc_1 > 0));
    if ((acib_arm_lsr32(((uint32_t)(loc_0)), 0x19u) == 0u) && (loc_1 <= 0)) {
        return (int32_t)((((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u))))))) > ((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))))))));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x06f9d8 sub_6F9D8 */
int32_t osa_core_fn_06f9d8(int32_t arg_0, int32_t arg_1, uint32_t raw_arg_2) {
    uint16_t arg_2 = (uint16_t)raw_arg_2;
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - acib_mem_read32(((uint32_t)(arg_0))))))) & 0x3FFFFFFu)));
    if (acib_arm_lsr32(((uint32_t)(loc_0)), 0x19u) != 0u) {
        loc_0 = (uint32_t)((uint32_t)loc_0 + ((uint32_t)(((int32_t)0x7C000000u))));
    }
    return (int32_t)((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(arg_2)) + acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))))))) - ((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u))))))))))) + (0x4E2u * loc_0)));
}

/* @provenance RVA 0x06fb04 sub_6FB04 */
int32_t osa_core_fn_06fb04(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
    loc_1 = (int32_t)(acib_mem_read_s32((loc_0 + 4u * 2u)));
    loc_2 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_1))))))));
    loc_3 = (int32_t)(acib_mem_read_s16((((uint32_t)(loc_0)) + (2u * 6u))));
    acib_mem_write32((loc_2 + 4u * 2u), (uint32_t)(loc_1));
    acib_mem_write16((((uint32_t)(loc_2)) + (2u * 6u)), (uint16_t)(loc_3));
    osa_core_fn_06f8b8((int32_t)(((int32_t)(0x10020200u))), (int32_t)(loc_1), (int32_t)(loc_3), (int32_t)(((int32_t)(loc_2))));
    acib_mem_write8((((uint32_t)(loc_2)) + 0x1Du), (uint8_t)(((int8_t)1u)));
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x06fb34 sub_6FB34 */
int32_t osa_core_fn_06fb34(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (int32_t)(((int16_t)(uint16_t)(((uint32_t)(acib_mem_read_s32((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 3u)))))))) & 0xFFFFu)));
    loc_1 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_1))))))));
    loc_2 = (int32_t)(((uint16_t)(loc_0)));
    if (((uint32_t)(((uint16_t)(loc_0)))) != ((uint32_t)(acib_mem_read16((((uint32_t)(loc_1)) + (2u * 6u)))))) {
        loc_3 = (int32_t)(acib_mem_read_s32((loc_1 + 4u * 2u)));
        acib_mem_write16((((uint32_t)(loc_1)) + (2u * 6u)), (uint16_t)(loc_0));
        osa_core_fn_06f8b8((int32_t)(((int32_t)(0x10020200u))), (int32_t)(loc_3), (int32_t)(loc_0), (int32_t)(((int32_t)(loc_1))));
        acib_mem_write8((((uint32_t)(loc_1)) + 0x1Du), (uint8_t)(((int8_t)1u)));
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x06fb6c sub_6FB6C */
int32_t osa_core_fn_06fb6c(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_0 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_2))))))));
    loc_1 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_1))))))));
    loc_2 = (int32_t)(acib_mem_read_s32((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 2u)))))));
    loc_3 = (int32_t)(acib_mem_read_s16((((uint32_t)(loc_1)) + (2u * 6u))));
    loc_4 = (int32_t)(((uint16_t)(loc_3)));
    if ((((uint32_t)(int32_t)(loc_2)) != ((uint32_t)(int32_t)(acib_mem_read_s32((loc_0 + 4u * 2u))))) || (((uint32_t)(((uint16_t)(loc_3)))) != ((uint32_t)(acib_mem_read16((((uint32_t)(loc_0)) + (2u * 6u))))))) {
        acib_mem_write16((((uint32_t)(loc_0)) + (2u * 6u)), (uint16_t)(loc_3));
        acib_mem_write32((loc_0 + 4u * 2u), (uint32_t)(loc_2));
        osa_core_fn_06f8b8((int32_t)(((int32_t)(0x10020200u))), (int32_t)(loc_2), (int32_t)(loc_3), (int32_t)(((int32_t)(loc_0))));
        acib_mem_write8((((uint32_t)(loc_0)) + 0x1Du), (uint8_t)(((int8_t)1u)));
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)(loc_4);
}

/* @provenance RVA 0x06fda8 sub_6FDA8 */
uint32_t osa_core_fn_06fda8(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mem_read_s16(arg_1));
    if (loc_0 < ((int32_t)0x4E2u)) {
        if (loc_0 <= ((int32_t)0xFFFFFB1Eu)) {
            acib_mem_write32(arg_0, (uint32_t)(((uint32_t)(acib_mem_read32(arg_0)) - ((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(((int32_t)(0u - ((uint32_t)(int32_t)(loc_0)))))), 0x4E2u)))));
            acib_mem_write16(arg_1, (uint16_t)((((uint32_t)(int32_t)(loc_0)) + (0x4E2u * ((uint32_t)acib_arm_udiv32(((uint32_t)(((uint16_t)(((int32_t)(0u - ((uint32_t)(int32_t)(((int16_t)(loc_0)))))))))), 0x4E2u))))));
        }
    } else {
        acib_mem_write32(arg_0, (uint32_t)(((uint32_t)(acib_mem_read32(arg_0)) + ((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(loc_0)), 0x4E2u)))));
        acib_mem_write16(arg_1, (uint16_t)(((uint32_t)acib_arm_umod32(((uint32_t)(((uint16_t)(loc_0)))), 0x4E2u))));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0700bc sub_700BC */
uint32_t osa_core_fn_0700bc(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
    acib_mem_write8((((uint32_t)(loc_0)) + 0x1Cu), (uint8_t)(((int8_t)0u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x070294 sub_70294 */
int32_t osa_core_fn_070294(int32_t arg_0, int32_t arg_1, int16_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    int32_t loc_7 = 0;
    uint32_t loc_8 = 0;
    int32_t loc_9 = 0;
    loc_4 = (int32_t)(osa_svc_fn_0ce910());
    if (((uint32_t)(int32_t)(loc_4)) == 0u) {
        sub_6DD70(0u);
    }
    loc_5 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
    loc_6 = (int32_t)(((int16_t)(((int32_t)(((uint32_t)(acib_mem_read16((((uint32_t)(loc_5)) + (2u * 6u))))) + ((uint32_t)(int32_t)(arg_2)))))));
    loc_7 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32((loc_5 + 4u * 2u)))) + ((uint32_t)(int32_t)(arg_1)))));
    if (loc_6 < ((int32_t)0x4E2u)) {
        if (loc_6 <= ((int32_t)0xFFFFFB1Eu)) {
            loc_8 = (uint32_t)(((int32_t)(0u - ((uint32_t)(int32_t)(loc_6)))));
            loc_6 = (int32_t)(((uint32_t)loc_6 & ~0xFFFFu) | (((uint32_t)((((uint32_t)(int32_t)(loc_6)) + (0x4E2u * ((uint32_t)acib_arm_udiv32(((uint32_t)(((uint16_t)(((int32_t)(0u - ((uint32_t)(int32_t)(((int16_t)(loc_6)))))))))), 0x4E2u)))))) & 0xFFFFu));
            loc_7 = (int32_t)((uint32_t)loc_7 - ((uint32_t)(((uint32_t)acib_arm_udiv32(loc_8, 0x4E2u)))));
        }
    } else {
        loc_6 = (int32_t)(((uint32_t)loc_6 & ~0xFFFFu) | (((uint32_t)(((uint32_t)acib_arm_umod32(((uint32_t)(((uint16_t)(loc_6)))), 0x4E2u)))) & 0xFFFFu));
        loc_7 = (int32_t)((uint32_t)loc_7 + ((uint32_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(((int16_t)(((int32_t)(((uint32_t)(acib_mem_read16((((uint32_t)(loc_5)) + (2u * 6u))))) + ((uint32_t)(int32_t)(arg_2)))))))), 0x4E2u)))));
    }
    acib_mem_write16((((uint32_t)(loc_5)) + (2u * 6u)), (uint16_t)(loc_6));
    acib_mem_write32((loc_5 + 4u * 2u), (uint32_t)(loc_7));
    loc_9 = (int32_t)(osa_core_fn_06f8b8((int32_t)(((int32_t)(0x10020200u))), (int32_t)(loc_7), (int32_t)(((int16_t)(loc_6))), (int32_t)(((int32_t)(loc_5)))));
    if (((uint32_t)(int32_t)(arg_3)) != 0u) {
        loc_9 = (int32_t)(((int32_t)1u));
        acib_mem_write8((((uint32_t)(loc_5)) + 0x1Du), (uint8_t)(((int8_t)1u)));
    }
    if (((uint32_t)(int32_t)(loc_4)) == 0u) {
        return (int32_t)((sub_6DDB8(), 0u));
    }
    return (int32_t)(loc_9);
}

/* @provenance RVA 0x07035c sub_7035C */
int32_t osa_core_fn_07035c(int32_t arg_0, int16_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint16_t loc_0 = 0;
    uint16_t loc_1 = 0;
    loc_0 = (uint16_t)(((int32_t)(((uint32_t)(((uint16_t)(((uint32_t)(acib_mem_read_s32((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 3u)))))))) & 0xFFFFu)))) + ((uint32_t)(int32_t)(arg_1)))));
    loc_1 = (uint16_t)(loc_0);
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(loc_0)) & 0x8000u)))) != 0u) {
        loc_1 = (uint16_t)(((int32_t)(((uint32_t)(loc_0)) + 0x4E2u)));
    }
    if (((int32_t)(((int16_t)(loc_0)))) > ((int32_t)0x4E1u)) {
        return (int32_t)(((uint16_t)(((int32_t)(((uint32_t)(loc_0)) - 0x4E2u)))));
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x0704c4 sub_704C4 */
uint32_t osa_core_fn_0704c4(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    int32_t loc_5 = 0;
    int32_t loc_6 = 0;
    int32_t loc_7 = 0;
    int32_t loc_8 = 0;
    int32_t loc_9 = 0;
    int32_t loc_10 = 0;
    int32_t loc_11 = 0;
    int32_t loc_12 = 0;
    int32_t loc_13 = 0;
    int32_t loc_14 = 0;
    int32_t loc_15 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32((0x100200E0u + 4u * 0u)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(acib_mem_read_s32(0x100200E4u)));
    acib_mem_write32(arg_0, (uint32_t)(loc_0));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10020100u));
    acib_mem_write32((arg_0 + 4u * 3u), (uint32_t)(acib_mem_read_s32(0x10020104u)));
    acib_mem_write32((arg_0 + 4u * 2u), (uint32_t)(loc_1));
    loc_2 = (int32_t)(acib_mem_read_s32(0x10020120u));
    acib_mem_write32((arg_0 + 4u * 5u), (uint32_t)(acib_mem_read_s32(0x10020124u)));
    acib_mem_write32((arg_0 + 4u * 4u), (uint32_t)(loc_2));
    loc_3 = (int32_t)(acib_mem_read_s32(0x10020140u));
    acib_mem_write32((arg_0 + 4u * 7u), (uint32_t)(acib_mem_read_s32(0x10020144u)));
    acib_mem_write32((arg_0 + 4u * 6u), (uint32_t)(loc_3));
    loc_4 = (int32_t)(acib_mem_read_s32(0x10020160u));
    acib_mem_write32((arg_0 + 4u * 9u), (uint32_t)(acib_mem_read_s32(0x10020164u)));
    acib_mem_write32((arg_0 + 4u * 8u), (uint32_t)(loc_4));
    loc_5 = (int32_t)(acib_mem_read_s32(0x10020180u));
    acib_mem_write32((arg_0 + 4u * 0xBu), (uint32_t)(acib_mem_read_s32(0x10020184u)));
    acib_mem_write32((arg_0 + 4u * 0xAu), (uint32_t)(loc_5));
    loc_6 = (int32_t)(acib_mem_read_s32(0x100201A0u));
    acib_mem_write32((arg_0 + 4u * 0xDu), (uint32_t)(acib_mem_read_s32(0x100201A4u)));
    acib_mem_write32((arg_0 + 4u * 0xCu), (uint32_t)(loc_6));
    loc_7 = (int32_t)(acib_mem_read_s32(0x100201C0u));
    acib_mem_write32((arg_0 + 4u * 0xFu), (uint32_t)(acib_mem_read_s32(0x100201C4u)));
    acib_mem_write32((arg_0 + 4u * 0xEu), (uint32_t)(loc_7));
    loc_8 = (int32_t)(acib_mem_read_s32(0x100201E0u));
    acib_mem_write32((arg_0 + 4u * 0x11u), (uint32_t)(acib_mem_read_s32(0x100201E4u)));
    acib_mem_write32((arg_0 + 4u * 0x10u), (uint32_t)(loc_8));
    loc_9 = (int32_t)(acib_mem_read_s32(0x10020200u));
    acib_mem_write32((arg_0 + 4u * 0x13u), (uint32_t)(acib_mem_read_s32(0x10020204u)));
    acib_mem_write32((arg_0 + 4u * 0x12u), (uint32_t)(loc_9));
    loc_10 = (int32_t)(acib_mem_read_s32(0x10020220u));
    acib_mem_write32((arg_0 + 4u * 0x15u), (uint32_t)(acib_mem_read_s32(0x10020224u)));
    acib_mem_write32((arg_0 + 4u * 0x14u), (uint32_t)(loc_10));
    loc_11 = (int32_t)(acib_mem_read_s32(0x10020240u));
    acib_mem_write32((arg_0 + 4u * 0x17u), (uint32_t)(acib_mem_read_s32(0x10020244u)));
    acib_mem_write32((arg_0 + 4u * 0x16u), (uint32_t)(loc_11));
    loc_12 = (int32_t)(acib_mem_read_s32(0x10020260u));
    acib_mem_write32((arg_0 + 4u * 0x19u), (uint32_t)(acib_mem_read_s32(0x10020264u)));
    acib_mem_write32((arg_0 + 4u * 0x18u), (uint32_t)(loc_12));
    loc_13 = (int32_t)(acib_mem_read_s32(0x10020280u));
    acib_mem_write32((arg_0 + 4u * 0x1Bu), (uint32_t)(acib_mem_read_s32(0x10020284u)));
    acib_mem_write32((arg_0 + 4u * 0x1Au), (uint32_t)(loc_13));
    loc_14 = (int32_t)(acib_mem_read_s32(0x100202A0u));
    acib_mem_write32((arg_0 + 4u * 0x1Du), (uint32_t)(acib_mem_read_s32(0x100202A4u)));
    acib_mem_write32((arg_0 + 4u * 0x1Cu), (uint32_t)(loc_14));
    loc_15 = (int32_t)(acib_mem_read_s32(0x100202C0u));
    acib_mem_write32((arg_0 + 4u * 0x1Fu), (uint32_t)(acib_mem_read_s32(0x100202C4u)));
    acib_mem_write32((arg_0 + 4u * 0x1Eu), (uint32_t)(loc_15));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x070580 sub_70580 */
int32_t osa_core_fn_070580(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    if (((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)))) > 3u) {
        return (int32_t)(false);
    }
    loc_1 = (int32_t)(false);
    if (((uint32_t)(acib_mem_read8((((uint32_t)(0x100202E0u)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))))))) != 0u) {
        loc_2 = (uint32_t)(acib_mmio_read32(0x52160020u));
        return (int32_t)(((loc_2 - acib_mem_read32((((uint32_t)(0x100202E0u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_0)))))) + 1u)))))))) > 0x2DC6C0u));
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x0705ec sub_705EC */
int32_t osa_core_fn_0705ec(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    if (((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)))) <= 3u) {
        loc_0 = (uint32_t)((0x100202E0u + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
        acib_mem_write8(loc_0, (uint8_t)(((int8_t)0u)));
        acib_mem_write32((((uint32_t)(loc_0)) + (4u * 1u)), (uint32_t)(((int32_t)0u)));
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07060c sub_7060C */
uint32_t osa_core_fn_07060c(uint32_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(arg_0, (uint8_t)(acib_mem_read_s8(0x100200F4u)));
    acib_mem_write8((arg_0 + 1u), (uint8_t)(acib_mem_read_s8(0x10020114u)));
    acib_mem_write8((arg_0 + 2u), (uint8_t)(acib_mem_read_s8(0x10020134u)));
    acib_mem_write8((arg_0 + 3u), (uint8_t)(acib_mem_read_s8(0x10020154u)));
    acib_mem_write8((arg_0 + 4u), (uint8_t)(acib_mem_read_s8(0x10020174u)));
    acib_mem_write8((arg_0 + 5u), (uint8_t)(acib_mem_read_s8(0x10020194u)));
    acib_mem_write8((arg_0 + 6u), (uint8_t)(acib_mem_read_s8(0x100201B4u)));
    acib_mem_write8((arg_0 + 7u), (uint8_t)(acib_mem_read_s8(0x100201D4u)));
    acib_mem_write8((arg_0 + 8u), (uint8_t)(acib_mem_read_s8(0x100201F4u)));
    acib_mem_write8((arg_0 + 9u), (uint8_t)(acib_mem_read_s8(0x10020214u)));
    acib_mem_write8((arg_0 + 0xAu), (uint8_t)(acib_mem_read_s8(0x10020234u)));
    acib_mem_write8((arg_0 + 0xBu), (uint8_t)(acib_mem_read_s8(0x10020254u)));
    acib_mem_write8((arg_0 + 0xCu), (uint8_t)(acib_mem_read_s8(0x10020274u)));
    acib_mem_write8((arg_0 + 0xDu), (uint8_t)(acib_mem_read_s8(0x10020294u)));
    acib_mem_write8((arg_0 + 0xEu), (uint8_t)(acib_mem_read_s8(0x100202B4u)));
    acib_mem_write8((arg_0 + 0xFu), (uint8_t)(acib_mem_read_s8(0x100202D4u)));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x070cac sub_70CAC */
uint32_t osa_core_fn_070cac(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)((memcpy_wrapper(acib_addr_ptr(0x10020908u), acib_addr_ptr(arg_0), 0x1Cu), 0x10020908u));
}

/* @provenance RVA 0x070cc0 sub_70CC0 */
uint32_t osa_core_fn_070cc0(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)((memcpy_wrapper(acib_addr_ptr(0x10020924u), acib_addr_ptr(arg_0), 0x24u), 0x10020924u));
}

/* @provenance RVA 0x0712f0 sub_712F0 */
int32_t osa_core_fn_0712f0(void)
{
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int16_t loc_2 = 0;
    int32_t loc_3 = 0;
    int16_t loc_4 = 0;
    int32_t loc_5 = 0;
    (void)memset(acib_addr_ptr(0x100209ACu), 0, 0x14u);
    acib_mem_write16(0x100209B9u, (uint16_t)(((int16_t)0u)));
    loc_0 = (int32_t)(acib_mem_read_s32(0x100209BCu));
    loc_1 = (uint32_t)(acib_mmio_read32(0x52160020u));
    acib_mem_write32(0x100209BCu, (uint32_t)(((int32_t)1u)));
    loc_2 = (int16_t)(((int16_t)0xF0u));
    if (((uint32_t)(int32_t)(loc_0)) == 2u) {
        loc_2 = (int16_t)(((int16_t)4u));
    }
    acib_mem_write16(0x100209B2u, (uint16_t)(loc_2));
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) - 2u)));
    loc_4 = (int16_t)(loc_3);
    if (((uint32_t)(int32_t)(loc_3)) != 0u) {
        loc_4 = (int16_t)(((int16_t)0xFFFF88B8u));
    }
    acib_mem_write16(0x100209B0u, (uint16_t)(loc_4));
    if (((uint32_t)(int32_t)(loc_3)) != 0u) {
        loc_3 = (int32_t)(((int32_t)0x88B8u));
    }
    loc_5 = (int32_t)((((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_3)))))) + loc_1));
    acib_mem_write32(0x100209ACu, (uint32_t)(loc_5));
    return (int32_t)(loc_5);
}

/* @provenance RVA 0x071588 sub_71588 */
int32_t osa_core_fn_071588(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(0x100209B4u, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x071ca4 sub_71CA4 */
int32_t osa_core_fn_071ca4(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(acib_mem_read16((((uint32_t)(0x10020B78u)) + (2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0xEu * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u))))))) & 0x3Fu)))))))) + 0xBu))))))))))))) - 4u)));
}

/* @provenance RVA 0x071cc4 sub_71CC4 */
int32_t osa_core_fn_071cc4(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 2u))))))) & 0x3Fu)));
}

/* @provenance RVA 0x071cd0 sub_71CD0 */
int32_t osa_core_fn_071cd0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
        acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(((int32_t)(((uint32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - acib_mem_read32((((uint32_t)(0x10020B78u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(7u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 2u))))))) & 0x3Fu)))))))) + 2u))))))))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u))))))) & 0xE03Fu)))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 2u))))))) & 0x3Fu)))), 6u)))))))), 0x10u))))))));
    } else {
        acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)))), (uint16_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)))))) | 0x1000u)));
    }
    return (int32_t)(arg_0);
}
