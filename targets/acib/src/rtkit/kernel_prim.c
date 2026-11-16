/*
 * src/rtkit/kernel_prim.c — RTKit kernel primitives, thread/context helpers, and low-level runtime support.
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

/* @provenance RVA 0x0e70bc sub_E70BC */
int32_t rtk_kern_fn_0e70bc(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    for (loc_0 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(arg_1)) != ((uint32_t)(int32_t)(loc_0)); loc_0 = (int32_t)((uint32_t)loc_0 + 1u)) {
        if (((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(int32_t)(loc_0))))))))) == 0u) {
            return (int32_t)(loc_0);
        }
    }
    return (int32_t)(arg_1);
}

/* @provenance RVA 0x0e7458 sub_E7458 */
uint32_t rtk_kern_fn_0e7458(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x1002D0C8u, (uint32_t)(((uint32_t)(acib_mem_read32(0x1002D0C8u)) | ((uint32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_1)))))))));
    loc_0 = (uint32_t)((0x1002D0D0u + 4u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_1))))))));
    loc_1 = (uint32_t)(((uint32_t)(acib_mem_read32((loc_0 + 4u * 1u)))));
    acib_mem_write32((loc_0 + 4u * 1u), (uint32_t)(arg_0));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(loc_1));
    acib_mem_write32(loc_1, (uint32_t)(arg_0));
    acib_mem_write32(0x1002D0CCu, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x1002D0CCu)) + 1u)));
    return (uint32_t)(0x1002D0CCu);
}

/* @provenance RVA 0x0e7774 sub_E7774 */
int32_t rtk_kern_fn_0e7774(int32_t arg_0)
{
    (void)arg_0;
    uint8_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint8_t loc_2 = 0;
    loc_0 = (uint8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x15u))))));
    loc_1 = (uint32_t)(((uint32_t)(0x10002558u)));
    while ((((int32_t)1u)) != 0) {
        loc_1 = (uint32_t)(acib_mem_read32(((uint32_t)(loc_1))));
        loc_2 = (uint8_t)(loc_0);
        if (loc_1 == 0u) {
            break;
        }
        if (((uint32_t)(int32_t)(arg_0)) == (acib_mem_read32(acib_mem_read32((((uint32_t)(loc_1)) + (4u * 3u)))) & 0xFFFFFFFCu)) {
            if (((uint32_t)(acib_mem_read8((loc_1 + 0x14u)))) > ((uint32_t)(loc_0))) {
                loc_2 = (uint8_t)(acib_mem_read8((loc_1 + 0x14u)));
            }
            loc_0 = (uint8_t)(loc_2);
        }
    }
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint8_t)(loc_0));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0e77a8 sub_E77A8 */
uint32_t rtk_kern_fn_0e77a8(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    if (((uint32_t)(int32_t)(arg_0)) != 0u) {
        acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_1));
    } else {
        acib_mem_write32((0x1002D0D0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) + 1u))))), (uint32_t)(arg_1));
    }
    acib_mem_write32(arg_1, (uint32_t)(arg_0));
    if (acib_mem_read32((0x1002D0D0u + 4u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))))) == 0u) {
        acib_mem_write32(0x1002D0C8u, (uint32_t)(((uint32_t)(acib_mem_read32(0x1002D0C8u)) & ((uint32_t)(((int32_t)(~((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_2)))))))))))));
    }
    acib_mem_write32(0x1002D0CCu, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x1002D0CCu)) - 1u)));
    return (uint32_t)(0x1002D0CCu);
}

/* @provenance RVA 0x0e7de4 sub_E7DE4 */
int32_t rtk_kern_fn_0e7de4(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32(0x1002D28Cu));
    acib_mem_write32(0x1002D28Cu, (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1002D28Cu))) + 4u)))))));
    acib_mem_write32(0x1002D294u, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x1002D294u)) - 1u)));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0e7f0c sub_E7F0C */
uint32_t rtk_kern_fn_0e7f0c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 8u))))));
    loc_1 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0xCu))))));
    if (((uint32_t)(int32_t)(loc_1)) != 0u) {
        acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) + 8u)))), (uint32_t)(loc_0));
        if (((uint32_t)(int32_t)(loc_0)) != 0u) {
            acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 0xCu)))), (uint32_t)(loc_1));
        }
    } else {
        loc_2 = (int32_t)(((int32_t)0u));
        if (((uint32_t)(int32_t)(loc_0)) != 0u) {
            acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 0xCu)))), (uint32_t)(((int32_t)0u)));
            loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 2u)));
        }
        acib_mem_write32(arg_0, (uint32_t)(loc_2));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0e84ea sub_E84EA */
uint32_t rtk_kern_fn_0e84ea(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mem_read32((arg_0 + 4u * 1u)));
    loc_1 = (uint32_t)(acib_mem_read32((arg_0 + 4u * 4u)));
    loc_2 = (uint32_t)(((uint32_t)acib_arm_umod32(acib_mem_read32((arg_0 + 4u * 2u)), loc_1)));
    loc_3 = (uint32_t)((loc_2 - ((uint32_t)acib_arm_umod32(loc_0, loc_1))));
    if (loc_2 < ((uint32_t)acib_arm_umod32(loc_0, loc_1))) {
        loc_3 = (uint32_t)((uint32_t)loc_3 + ((uint32_t)(loc_1)));
    }
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x0e89b0 IRQ21_Handler */
int32_t rtk_kern_fn_0e89b0(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x30u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x2Cu)))), (uint32_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0e9544 sub_E9544 */
int32_t rtk_kern_fn_0e9544(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    if (arg_1 >= acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x24u)))))) {
        return (int32_t)(((int32_t)0x80000001u));
    }
    loc_0 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))));
    loc_1 = (int32_t)(((int32_t)acib_arm_lsl32(1u, (4u * arg_1))));
    loc_2 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 0x10u))))));
    loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_1)))))))));
    if (((uint32_t)(int32_t)(arg_2)) != 0u) {
        loc_3 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | ((uint32_t)(int32_t)(loc_1)))));
    }
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 0x10u)))), (uint32_t)(loc_3));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0e9798 IRQ34_Handler */
int32_t rtk_kern_fn_0e9798(void)
{
    return (int32_t)(((int32_t)0x80000003u));
}

/* @provenance RVA 0x0e99e0 sub_E99E0 */
int32_t rtk_kern_fn_0e99e0(int32_t arg_0, int32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    loc_4 = (uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(int32_t)(arg_1)))))));
    loc_1 = (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(int32_t)(arg_1))))))));
    loc_2 = (int32_t)(acib_mem_read32((loc_4 = (uint32_t)((uint32_t)loc_4 + 4u))));
    loc_3 = (int32_t)(acib_mem_read32((loc_4 = (uint32_t)((uint32_t)loc_4 + 4u))));
    acib_mem_write32((arg_2 + 4u * 3u), (uint32_t)(acib_mem_read32((loc_4 + 4u * 1u))));
    acib_mem_write32((arg_2 + 4u * 2u), (uint32_t)(loc_3));
    acib_mem_write32((arg_2 + 4u * 1u), (uint32_t)(loc_2));
    acib_mem_write32(arg_2, (uint32_t)(loc_1));
    return (int32_t)(loc_1);
}
