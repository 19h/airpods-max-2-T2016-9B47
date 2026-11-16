/*
 * src/rtkit/endpoints.c — RTKit endpoint management, crash/diagnostic veneers, and platform runtime helpers.
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

/* @provenance RVA 0x0eab34 sub_EAB34 */
int32_t rtk_ep_fn_0eab34(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_1 = (int32_t)(((int32_t)0u));
    loc_2 = (uint32_t)(((uint32_t)(0x1002CFC8u)));
    loc_3 = (int32_t)(((int32_t)0x8000000Cu));
    while (((uint32_t)(int32_t)(loc_1)) != 9u) {
        if ((arg_0 >= acib_mem_read32(loc_2)) && (arg_0 < (acib_mem_read32(loc_2) + acib_mem_read32((loc_2 + 4u * 1u))))) {
            acib_mem_write32(arg_1, (uint32_t)(loc_1));
            return (int32_t)(((int32_t)0u));
        }
        loc_2 = (uint32_t)((uint32_t)loc_2 + (4u * 4u));
        loc_1 = (int32_t)((uint32_t)loc_1 + 1u);
    }
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x0eac70 sub_EAC70 */
int32_t rtk_ep_fn_0eac70(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32(0x100025B4u));
    if ((((uint32_t)(int32_t)(acib_mem_read_s32(0x100025B4u))) == 0x1Fu) || (((uint32_t)(int32_t)(acib_mem_read_s32(0x100025B4u))) == 0u)) {
        return (int32_t)(((int32_t)0u));
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0eb048 sub_EB048 */
int32_t rtk_ep_fn_0eb048(uint32_t arg_0)
{
    (void)arg_0;
    if (((uint32_t)(int32_t)(acib_mem_read_s32(0x100025D4u))) == 2u) {
        if (arg_0 != 0u) {
            acib_mem_write32(arg_0, (uint32_t)(acib_mem_read_s32(0x100025CCu)));
        }
        return (int32_t)(acib_mem_read_s32(0x100025C8u));
    } else {
        if (((uint32_t)(int32_t)(acib_mem_read_s32(0x100025D4u))) == 1u) {
            if (arg_0 != 0u) {
                acib_mem_write32(arg_0, (uint32_t)(acib_mem_read_s32(0x100025BCu)));
            }
            return (int32_t)(acib_mem_read_s32(0x100025B8u));
        } else {
            return (int32_t)(((int32_t)0u));
        }
    }
    return (int32_t)0;
}

/* @provenance RVA 0x0ebdac sub_EBDAC */
int32_t rtk_ep_fn_0ebdac(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_1 = (int32_t)(((int32_t)0u));
    if (((arg_1 - 0x20u) <= 0xFFFFFFEDu) && (arg_1 <= acib_mem_read32((arg_0 + 4u * 6u)))) {
        if (arg_1 >= 0xEu) {
            arg_1 = (uint32_t)((uint32_t)arg_1 - ((uint32_t)(((int32_t)0x12u))));
        }
        loc_2 = (uint32_t)(acib_mem_read16(((uint32_t)((acib_mem_read32((arg_0 + 4u * 3u)) + (2u * arg_1))))));
        if (acib_arm_lsl32(loc_2, 0x1Fu) != 0u) {
            return (int32_t)((acib_mem_read32((arg_0 + 4u * 2u)) + (0x34u * acib_arm_lsr32(((uint32_t)(loc_2)), 1u))));
        } else {
            return (int32_t)(((int32_t)0u));
        }
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x0ed424 sub_ED424 */
int32_t rtk_ep_fn_0ed424(int16_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    bool loc_1 = false;
    if ((((uint32_t)(((uint8_t)(arg_0)))) - 1u) < 2u) {
        loc_1 = (((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x200u)))) != 0u)) != 0);
        loc_0 = (int32_t)(((int32_t)8u));
        if (!(loc_1 != 0)) {
            return (int32_t)(((int32_t)3u));
        }
    } else {
        if (((uint32_t)(((uint8_t)(arg_0)))) != 0u) {
            switch (((uint32_t)(((uint8_t)(arg_0))))) {
            case 0x3u: {
                loc_1 = (((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x200u)))) != 0u)) != 0);
                loc_0 = (int32_t)(((int32_t)5u));
                if (!(loc_1 != 0)) {
                    return (int32_t)(((int32_t)4u));
                }
                break;
            }
            case 0x10u: {
                return (int32_t)(((int32_t)0x20u));
            }
            case 0x20u: {
                return (int32_t)(((int32_t)0x21u));
            }
            default: {
                return (int32_t)(((int32_t)0u));
            }
            }
        } else {
            return (int32_t)(((int32_t)1u));
        }
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0ed8b6 sub_ED8B6 */
uint32_t rtk_ep_fn_0ed8b6(uint32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (uint32_t)(((int32_t)0u));
    acib_mem_write32((arg_0 + 4u * 9u), (uint32_t)(((int32_t)0u)));
    acib_mem_write32((arg_0 + 4u * 6u), (uint32_t)(arg_2));
    acib_mem_write32((arg_0 + 4u * 5u), (uint32_t)(arg_1));
    acib_mem_write32((arg_0 + 4u * 8u), (uint32_t)(((int32_t)0u)));
    acib_mem_write32((arg_0 + 4u * 4u), (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + 0x20u)))) * ((uint32_t)(int32_t)(arg_1))))));
    while (((uint32_t)(int32_t)(arg_1)) != loc_0) {
        if (loc_0 >= acib_mem_read32((arg_0 + 4u * 5u))) {
            loc_1 = (uint32_t)(0u);
        } else {
            loc_1 = (uint32_t)(((uint32_t)((acib_mem_read32((arg_0 + 4u * 7u)) + (((uint32_t)(int32_t)(((int32_t)(acib_mem_read32((arg_0 + 4u * 6u)) + 0x20u)))) * loc_0)))));
        }
        acib_mem_write32(loc_1, (uint32_t)(((int32_t)0xFFFFFFFFu)));
        loc_0 = (uint32_t)((uint32_t)loc_0 + 1u);
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0eeb0c sub_EEB0C */
uint32_t rtk_ep_fn_0eeb0c(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    acib_mem_write32(arg_1, (uint32_t)(((((uint32_t)(int32_t)(arg_2)) & 0xFFFFFFFEu) | (acib_mem_read32(arg_1) & 2u))));
    acib_mem_write32(((uint32_t)((((uint32_t)(arg_1)) + (((uint32_t)(int32_t)(arg_2)) & 0xFFFFFFFCu)))), (uint32_t)(((uint32_t)(acib_mem_read32(((uint32_t)((((uint32_t)(arg_1)) + (((uint32_t)(int32_t)(arg_2)) & 0xFFFFFFFCu)))))) & (~2u))));
    loc_0 = (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x64u))))));
    loc_1 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x60u))))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x60u)))), (uint32_t)(loc_1));
    if (loc_0 < loc_1) {
        acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x64u)))), (uint32_t)(loc_1));
    }
    return (uint32_t)(arg_1);
}

/* @provenance RVA 0x0efa0c sub_EFA0C */
int32_t rtk_ep_fn_0efa0c(void)
{
    return (int32_t)((sub_740A8(0x4C004u, 0x80A1u), 0u));
}

/* @provenance RVA 0x0efa48 sub_EFA48 */
int32_t rtk_ep_fn_0efa48(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_mdm_fn_0851ca((int32_t)(arg_0)));
}

/* @provenance RVA 0x0efa54 sub_EFA54 */
int32_t rtk_ep_fn_0efa54(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_mdm_fn_0851ca((int32_t)(arg_0)));
}

/* @provenance RVA 0x0efa60 sub_EFA60 */
int32_t rtk_ep_fn_0efa60(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_mdm_fn_0851ca((int32_t)(arg_0)));
}

/* @provenance RVA 0x0efa6c sub_EFA6C */
int32_t rtk_ep_fn_0efa6c(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_mdm_fn_0851ca((int32_t)(arg_0)));
}

/* @provenance RVA 0x0efa78 sub_EFA78 */
int32_t rtk_ep_fn_0efa78(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_mdm_fn_0851ca((int32_t)(arg_0)));
}

/* @provenance RVA 0x0efa84 sub_EFA84 */
int32_t rtk_ep_fn_0efa84(void)
{
    return (int32_t)((sub_740FE(0x8C002u, 0x8367u, 0u), 0u));
}

/* @provenance RVA 0x0efa9c sub_EFA9C */
int32_t rtk_ep_fn_0efa9c(void)
{
    return (int32_t)((sub_740FE(0x8C002u, 0x8367u, 1u), 0u));
}

/* @provenance RVA 0x0efb3c sub_EFB3C */
int32_t rtk_ep_fn_0efb3c(void)
{
    return (int32_t)((sub_740A8(0x96002u, 0u), 0u));
}
