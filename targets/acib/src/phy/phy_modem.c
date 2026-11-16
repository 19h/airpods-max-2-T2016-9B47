/*
 * src/phy/phy_modem.c — W3 Bluetooth modem, ACI hardware interface (0x42440000), and radio calibration.
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

/* @provenance RVA 0x07801c sub_7801C */
int32_t phy_mdm_fn_07801c(int32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(arg_1, (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))))));
    if (arg_2 != 0u) {
        acib_mem_write32(arg_2, (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))))));
    }
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x07802a sub_7802A */
int32_t phy_mdm_fn_07802a(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    if (((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x18u))))))) != 0u) {
        loc_0 = (int32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Au))))));
        loc_1 = (int32_t)(((int32_t)4u));
        if (((uint32_t)(int32_t)(loc_0)) == 0u) {
            loc_1 = (int32_t)(((int32_t)3u));
        }
    } else {
        loc_1 = (int32_t)(((int32_t)2u));
    }
    acib_mem_write32(arg_1, (uint32_t)(loc_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x078042 sub_78042 */
int32_t phy_mdm_fn_078042(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    bool loc_0 = false;
    loc_0 = (((((((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x18u))))))) == 0u) || (((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Au))))))) == 0u)) && (acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u))))) != 1u))) != 0);
    acib_mem_write8(arg_1, (uint8_t)(loc_0));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0782b8 sub_782B8 */
int32_t phy_mdm_fn_0782b8(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))));
}

/* @provenance RVA 0x0782bc sub_782BC */
int32_t phy_mdm_fn_0782bc(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u))))));
}

/* @provenance RVA 0x0782c0 sub_782C0 */
int32_t phy_mdm_fn_0782c0(int32_t arg_0, int32_t arg_1, int16_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xAu)))), (uint16_t)(arg_2));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(arg_1));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_3));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint16_t)(((int16_t)0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0782ce sub_782CE */
int32_t phy_mdm_fn_0782ce(int32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))));
    if (((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xAu))))))) != 0u) {
        acib_mem_write16(arg_2, (uint16_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xAu))))))) - ((uint32_t)(int32_t)(loc_1))))));
        acib_mem_write32(arg_1, (uint32_t)(((int32_t)(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u))))))) + acib_mem_read32(((uint32_t)(arg_0)))))));
        return (int32_t)(((int32_t)0u));
    } else {
        sub_740FE(0x39800u, 0x8404u, ((uint32_t)(loc_1)));
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x078328 sub_78328 */
uint32_t phy_mdm_fn_078328(void)
{
    acib_mem_write32(0x10021BFDu, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021BF9u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021BF4u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10021BF0u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x10021BEFu, (uint8_t)(((int8_t)1u)));
    return (uint32_t)(0x10021BE8u);
}

/* @provenance RVA 0x078374 sub_78374 */
uint32_t phy_mdm_fn_078374(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    if (arg_0 != 0u) {
        arg_0 = (uint32_t)((uint32_t)arg_0 * ((uint32_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(acib_mem_read_s32(0x10020970u))), ((uint32_t)((0x000F420Du + 0x33u))))))));
        if (arg_0 <= 0x3Cu) {
            for (loc_2 = (uint32_t)(((int32_t)0x23u)); loc_2 <= arg_0; loc_2 = (uint32_t)((uint32_t)loc_2 + ((uint32_t)(((int32_t)5u))))) {
                ;
            }
        } else {
            loc_0 = (uint32_t)((arg_0 - 0x3Cu));
            loc_1 = (uint32_t)(acib_dwt_cyccnt_read());
            do {
                arg_0 = (uint32_t)((acib_dwt_cyccnt_read() - loc_1));
            } while (arg_0 < loc_0);
        }
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0783b8 sub_783B8 */
int32_t phy_mdm_fn_0783b8(void)
{
    return (int32_t)((sub_7835C(1u), 0u));
}

/* @provenance RVA 0x0783be sub_783BE */
int32_t phy_mdm_fn_0783be(void)
{
    return (int32_t)((sub_7835C(2u), 0u));
}

/* @provenance RVA 0x0783c4 sub_783C4 */
int32_t phy_mdm_fn_0783c4(void)
{
    return (int32_t)((sub_7835C(3u), 0u));
}

/* @provenance RVA 0x0783ca sub_783CA */
int32_t phy_mdm_fn_0783ca(void)
{
    return (int32_t)((sub_7835C(4u), 0u));
}

/* @provenance RVA 0x0783d0 sub_783D0 */
int32_t phy_mdm_fn_0783d0(void)
{
    return (int32_t)((sub_7835C(5u), 0u));
}

/* @provenance RVA 0x0783d8 sub_783D8 */
int32_t phy_mdm_fn_0783d8(void)
{
    return (int32_t)((sub_7835C(0x64u), 0u));
}

/* @provenance RVA 0x0783e2 sub_783E2 */
uint32_t phy_mdm_fn_0783e2(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    while (arg_1 != 0u) {
        loc_0 = (uint32_t)(arg_1);
        arg_1 = (uint32_t)(((uint32_t)acib_arm_umod32(arg_0, arg_1)));
        arg_0 = (uint32_t)(loc_0);
    }
    return (uint32_t)(((uint16_t)(arg_0)));
}

/* @provenance RVA 0x0783f8 sub_783F8 */
int32_t phy_mdm_fn_0783f8(int16_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x1FFu)));
    acib_mem_write16(0x10021BECu, (uint16_t)(loc_0));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x078408 sub_78408 */
int32_t phy_mdm_fn_078408(void)
{
    uint8_t loc_0 = 0;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_0 = (uint8_t)(((int8_t)0u));
    loc_1 = (int32_t)(((int32_t)8u));
    loc_2 = (uint32_t)(((uint16_t)(acib_mem_read_s16(0x10021BECu))));
    while (((uint32_t)(int32_t)(loc_1)) != 0u) {
        loc_1 = (int32_t)((uint32_t)loc_1 - 1u);
        loc_0 = (uint8_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(loc_0)), 1u)))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(((uint8_t)(loc_2)))), 7u)))))));
        loc_2 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(((uint8_t)(loc_2)))) ^ ((uint32_t)(((uint8_t)(acib_arm_lsr32(((uint32_t)(loc_2)), 4u))))))))) & 1u)))), 8u)))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(((uint16_t)(loc_2)))), 1u)))))));
    }
    acib_mem_write16(0x10021BECu, (uint16_t)(loc_2));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x07843c sub_7843C */
uint32_t phy_mdm_fn_07843c(uint32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    uint32_t loc_3 = 0;
    int32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    uint32_t loc_7 = 0;
    loc_0 = (int32_t)(arg_1);
    loc_1 = (int32_t)(arg_2);
    loc_3 = (uint32_t)(arg_0);
    if (arg_1 > 0x1FEu) {
        loc_0 = (int32_t)(((int32_t)0x1FFu));
    }
    if (((uint32_t)(int32_t)(arg_2)) != 0u) {
        loc_1 = (int32_t)(((int32_t)0xFFFFFFFFu));
    }
    for (loc_4 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_0)) != ((uint32_t)(int32_t)(loc_4)); loc_4 = (int32_t)((uint32_t)loc_4 + 1u)) {
        arg_0 = (uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(phy_mdm_fn_078408())) ^ ((uint32_t)(int32_t)(loc_1)))))));
        acib_mem_write8((loc_3 + ((uint32_t)(int32_t)(loc_4))), (uint8_t)(((uint8_t)(arg_0))));
    }
    if (arg_1 >= 0x200u) {
        loc_5 = (uint32_t)((arg_1 - 0x1FFu));
        for (loc_6 = (int32_t)(((int32_t)0x1FFu)); true; loc_6 = (int32_t)((uint32_t)loc_6 + ((uint32_t)(((int32_t)0x1FFu))))) {
            loc_7 = (uint32_t)((loc_3 + ((uint32_t)(int32_t)(loc_6))));
            if (loc_5 < 0x200u) {
                break;
            }
            (void)memcpy_wrapper(acib_addr_ptr(loc_7), acib_addr_ptr(loc_3), 0x1FFu);
            loc_5 = (uint32_t)((uint32_t)loc_5 - ((uint32_t)(((int32_t)0x1FFu))));
        }
        return (uint32_t)((memcpy_wrapper(acib_addr_ptr(loc_7), acib_addr_ptr(loc_3), loc_5), loc_7));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x0786ae sub_786AE */
int32_t phy_mdm_fn_0786ae(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint8_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint8_t)(((int8_t)0u));
    for (loc_1 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_1)) != ((uint32_t)(int32_t)(arg_1)); loc_1 = (int32_t)((uint32_t)loc_1 + 1u)) {
        for (loc_3 = (int32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + ((uint32_t)(int32_t)(loc_1)))))))); ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_3)), 0x18u)))) != 0u; loc_3 = (int32_t)((uint32_t)loc_3 & ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) - 1u)))))) {
            loc_0 = (uint8_t)((uint32_t)loc_0 + 1u);
        }
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0786ce sub_786CE */
int32_t phy_mdm_fn_0786ce(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(acib_memcmp(acib_addr_ptr(((uint32_t)(arg_0))), acib_addr_ptr(((uint32_t)(arg_1))), 6u));
}

/* @provenance RVA 0x0786d6 sub_786D6 */
int32_t phy_mdm_fn_0786d6(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint16_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)))))));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(acib_mem_read32(((uint32_t)(arg_1)))));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0786e8 sub_786E8 */
uint32_t phy_mdm_fn_0786e8(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)(acib_rbit32(arg_0));
}

/* @provenance RVA 0x078794 sub_78794 */
int32_t phy_mdm_fn_078794(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F5860u + acib_arm_lsr32(((uint32_t)(arg_0)), 4u))))) | ((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x000F5860u + (arg_0 & 0xFu))))) & 0xFu)))))))))));
}

/* @provenance RVA 0x0787b0 sub_787B0 */
int32_t phy_mdm_fn_0787b0(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(phy_mdm_fn_078794((uint32_t)(((uint8_t)(arg_0)))));
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(phy_mdm_fn_078794((uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 8u))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_1)), 8u)))))));
}

/* @provenance RVA 0x0787cc sub_787CC */
int32_t phy_mdm_fn_0787cc(int32_t arg_0, uint32_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)((((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), ((uint32_t)(((int32_t)(0x20u - ((uint32_t)(int32_t)(arg_2)))))))))) | acib_arm_lsr32(((uint32_t)(arg_1)), ((uint32_t)(arg_2)))));
}

/* @provenance RVA 0x0788ae sub_788AE */
int32_t phy_mdm_fn_0788ae(uint32_t arg_0, int32_t arg_1, uint32_t arg_2, int32_t arg_3, int32_t arg_4, uint32_t raw_arg_5) {
    uint16_t arg_5 = (uint16_t)raw_arg_5;
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    (void)arg_5;
    uint32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    while (((uint32_t)(int32_t)(arg_1)) != 0u) {
        loc_4 = (uint32_t)(acib_mem_read8(arg_0));
        if (((uint32_t)(int32_t)(arg_3)) != 0u) {
            loc_4 = (uint32_t)(phy_mdm_fn_078794((uint32_t)(loc_4)));
        }
        loc_5 = (uint32_t)(acib_rev32(arg_2));
        arg_0 = (uint32_t)((uint32_t)arg_0 + 1u);
        arg_1 = (int32_t)((uint32_t)arg_1 - 1u);
        loc_6 = (int32_t)(((loc_4 ^ ((uint32_t)(((uint16_t)((loc_5 >> 16) & 0xFFFFu))))) ^ ((uint32_t)(int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(((uint8_t)((loc_4 ^ ((uint32_t)(((uint8_t)((loc_5 >> 16) & 0xFFu))))))))), 4u))))));
        arg_2 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x20u * ((uint32_t)(((uint8_t)(loc_6)))))))) ^ ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_6)), 0xCu)))))))) ^ ((uint32_t)(int32_t)(loc_6)))));
    }
    if (((uint32_t)(int32_t)(arg_4)) != 0u) {
        arg_2 = (uint32_t)(((uint32_t)arg_2 & ~0xFFFFu) | (((uint32_t)(phy_mdm_fn_0787b0((uint32_t)(((uint16_t)(arg_2)))))) & 0xFFFFu));
    }
    return (int32_t)(((int32_t)(((uint32_t)(arg_5)) ^ ((uint32_t)(((uint16_t)(arg_2)))))));
}

/* @provenance RVA 0x07890c sub_7890C */
int32_t phy_mdm_fn_07890c(uint32_t arg_0, int32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)(phy_mdm_fn_0788ae((uint32_t)(arg_0), (int32_t)(arg_1), (uint32_t)(arg_2), (int32_t)(((int32_t)0u)), (int32_t)(((int32_t)0u)), (uint16_t)(((int32_t)0u))));
}

/* @provenance RVA 0x07895c sub_7895C */
uint32_t phy_mdm_fn_07895c(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (uint32_t)((arg_0 - loc_0));
    if (arg_0 < loc_0) {
        return (uint32_t)((loc_0 - arg_0));
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x078970 sub_78970 */
uint32_t phy_mdm_fn_078970(uint32_t arg_0, uint32_t arg_1, int32_t arg_2, uint32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    if (arg_1 != 0x7FFFu) {
        if (((uint32_t)(int32_t)(arg_2)) != 0xFFFF8001u) {
            arg_0 = (uint32_t)(((arg_1 * arg_0) + ((0x100u - arg_0) * ((uint32_t)(int32_t)(arg_2)))));
            arg_1 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 8u));
        }
        acib_mem_write16(arg_3, (uint16_t)(arg_1));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x078b24 sub_78B24 */
uint64_t phy_mdm_fn_078b24(void)
{
    uint32_t hi_first = acib_mmio_read32(0x52160024u);
    uint32_t lo_val = acib_mmio_read32(0x52160020u);
    uint32_t hi_second = acib_mmio_read32(0x52160024u);
    if (hi_first != hi_second) {
        lo_val = acib_mmio_read32(0x52160020u);
    }
    return ((uint64_t)hi_second << 32) | (uint64_t)lo_val;
}

/* @provenance RVA 0x078b84 sub_78B84 */
int32_t phy_mdm_fn_078b84(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1002141Cu))))), 0x1Eu)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s8(0x1002141Cu))) & 2u)))) != 0u) {
        sub_6DC88();
        while ((((int32_t)1u)) != 0) {
            ;
        }
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x078ba2 sub_78BA2 */
uint32_t phy_mdm_fn_078ba2(uint32_t arg_0, int32_t arg_1, uint32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write32((arg_0 + 4u * 2u), (uint32_t)(((int32_t)0u)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(arg_2));
    acib_mem_write32((arg_0 + 4u * 3u), (uint32_t)(((uint32_t)((arg_0 + (4u * 5u))))));
    acib_mem_write32((arg_0 + 4u * 4u), (uint32_t)((((uint32_t)(arg_0)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_3)) + 0x14u)))))));
    acib_mem_write32(arg_0, (uint32_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - ((uint32_t)(int32_t)(arg_3)))))) - 0x14u)))), arg_2))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x078be8 sub_78BE8 */
int32_t phy_mdm_fn_078be8(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - ((uint32_t)(int32_t)(arg_0)))));
}

/* @provenance RVA 0x078c08 sub_78C08 */
uint32_t phy_mdm_fn_078c08(int32_t arg_0, uint32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    if (arg_1 == 0u) {
        return (uint32_t)(((int32_t)0u));
    }
    loc_0 = (int32_t)(((int32_t)0x64u));
    if (((uint32_t)(int32_t)(arg_2)) == 0u) {
        loc_0 = (int32_t)(((int32_t)1u));
    }
    return (uint32_t)(((uint32_t)acib_arm_udiv32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) * ((uint32_t)(int32_t)(loc_0)))))), arg_1)));
}

/* @provenance RVA 0x078c54 sub_78C54 */
uint32_t phy_mdm_fn_078c54(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(arg_0, (uint32_t)(arg_1));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(((int32_t)0u)));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x078c5c sub_78C5C */
int32_t phy_mdm_fn_078c5c(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(((uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))))) + ((uint32_t)(arg_1)))));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x078ca8 sub_78CA8 */
int32_t phy_mdm_fn_078ca8(int8_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 7u)));
}

/* @provenance RVA 0x078cae sub_78CAE */
uint32_t phy_mdm_fn_078cae(uint32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 3u));
}

/* @provenance RVA 0x078cb2 sub_78CB2 */
int32_t phy_mdm_fn_078cb2(int8_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0xF8u)))) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 7u)))))));
}

/* @provenance RVA 0x078cbe sub_78CBE */
int32_t phy_mdm_fn_078cbe(int8_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 7u)))) | ((uint32_t)(((uint8_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_1)))))))))));
}

/* @provenance RVA 0x078cca sub_78CCA */
int32_t phy_mdm_fn_078cca(int8_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 7u)));
}

/* @provenance RVA 0x078cd0 sub_78CD0 */
int32_t phy_mdm_fn_078cd0(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((acib_arm_lsr32(((uint32_t)(arg_0)), 3u) & 0x3FFu));
}

/* @provenance RVA 0x078cd6 sub_78CD6 */
int32_t phy_mdm_fn_078cd6(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((acib_arm_lsr32(((uint32_t)(arg_0)), 3u) & 0xFFFu));
}

/* @provenance RVA 0x078cdc sub_78CDC */
uint32_t phy_mdm_fn_078cdc(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (uint32_t)(((((uint32_t)(int32_t)(arg_0)) & 0xFFFFFFF8u) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 7u))))));
}

/* @provenance RVA 0x078ce2 sub_78CE2 */
int32_t phy_mdm_fn_078ce2(int16_t arg_0, int16_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0xE007u)))) | ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 0x3FFu)))))))))));
}

/* @provenance RVA 0x078cee sub_78CEE */
int32_t phy_mdm_fn_078cee(int16_t arg_0, int16_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x8007u)))) | ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 0xFFFu)))))))))));
}

/* @provenance RVA 0x078d00 sub_78D00 */
int32_t phy_mdm_fn_078d00(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    switch (arg_1) {
    case 2: {
        return (int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(arg_0)))));
    }
    case 3: {
        return (int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 6u)));
    }
    case 4: {
        return (int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(arg_0)))));
    }
    default: {
        return (int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))));
    }
    }
    return (int32_t)0;
}

/* @provenance RVA 0x078dc4 sub_78DC4 */
int32_t phy_mdm_fn_078dc4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001674u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x1000166Cu)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001674u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001670u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x10001678u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078e04 sub_78E04 */
int32_t phy_mdm_fn_078e04(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001674u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x1000166Cu)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001674u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001670u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x10001678u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078e44 sub_78E44 */
int32_t phy_mdm_fn_078e44(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001688u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001680u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001688u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001684u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x1000168Cu, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078e84 sub_78E84 */
int32_t phy_mdm_fn_078e84(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001688u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001680u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001688u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001684u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x1000168Cu, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078ec4 sub_78EC4 */
int32_t phy_mdm_fn_078ec4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x1000169Cu));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001694u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x1000169Cu, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001698u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016A0u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078f04 sub_78F04 */
int32_t phy_mdm_fn_078f04(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x1000169Cu));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001694u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x1000169Cu, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001698u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016A0u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078f44 sub_78F44 */
int32_t phy_mdm_fn_078f44(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x100016B0u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x100016A8u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x100016B0u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016ACu))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016B4u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x078f84 sub_78F84 */
int32_t phy_mdm_fn_078f84(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x100016B0u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x100016A8u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x100016B0u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016ACu))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016B4u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x079074 sub_79074 */
int32_t phy_mdm_fn_079074(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (uint32_t)(acib_mem_read32(0x1000164Cu));
    loc_2 = (uint32_t)((acib_mem_read32(0x1000164Cu) + 1u));
    if ((acib_mem_read32(0x1000164Cu) + 1u) >= acib_mem_read32(0x10001644u)) {
        loc_2 = (uint32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x1000164Cu, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32((acib_mem_read32(0x10001648u) + 4u * loc_1), (uint32_t)(loc_3));
    acib_mem_write32(0x10001650u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x0790b4 sub_790B4 */
int32_t phy_mdm_fn_0790b4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001660u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001660u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001660u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001658u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001660u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000165Cu))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x10001664u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x0790f4 sub_790F4 */
int32_t phy_mdm_fn_0790f4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001674u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001674u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x1000166Cu)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001674u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001670u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x10001678u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x079134 sub_79134 */
int32_t phy_mdm_fn_079134(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10001688u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001688u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001680u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x10001688u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001684u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x1000168Cu, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x079174 sub_79174 */
int32_t phy_mdm_fn_079174(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x1000169Cu));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x1000169Cu))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x10001694u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x1000169Cu, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10001698u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016A0u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x0791b4 sub_791B4 */
int32_t phy_mdm_fn_0791b4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x100016B0u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016B0u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x100016A8u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x100016B0u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016ACu))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016B4u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x0791f4 sub_791F4 */
int32_t phy_mdm_fn_0791f4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x100016C4u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016C4u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016C4u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x100016BCu)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x100016C4u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016C0u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016C8u, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x079234 sub_79234 */
int32_t phy_mdm_fn_079234(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    loc_1 = (int32_t)(acib_mem_read_s32(0x100016D8u));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016D8u))) + 1u)));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016D8u))) + 1u)))) >= ((uint32_t)(acib_mem_read_s32(0x100016D0u)))) {
        loc_2 = (int32_t)(((int32_t)0u));
    }
    acib_mem_write32(0x100016D8u, (uint32_t)(loc_2));
    loc_3 = (int32_t)(((acib_arm_lsr32(((uint32_t)(loc_0)), 2u) & 0x3FFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x16u))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x100016D4u))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))), (uint32_t)(loc_3));
    acib_mem_write32(0x100016DCu, (uint32_t)(loc_0));
    return (int32_t)(loc_3);
}

/* @provenance RVA 0x079530 sub_79530 */
uint32_t phy_mdm_fn_079530(void)
{
    acib_mem_write32(0x10021C68u, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10021C68u)) + 1u)));
    return (uint32_t)(0x10021C68u);
}

/* @provenance RVA 0x079540 sub_79540 */
uint32_t phy_mdm_fn_079540(void)
{
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(0x10021C68u);
    if (((uint32_t)(int32_t)(acib_mem_read_s32(0x10021C68u))) == 0u) {
        return (uint32_t)(((uint32_t)((sub_740A8(0x41008u, 0x80ABu), 0u))));
    }
    acib_mem_write32(0x10021C68u, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10021C68u)) - 1u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x07970c sub_7970C */
int32_t phy_mdm_fn_07970c(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    if (arg_0 > 0x17u) {
        return (int32_t)(((int32_t)0u));
    }
    loc_1 = (uint32_t)(((uint32_t)(0x520D8000u)));
    if (arg_0 > 0xFu) {
        arg_0 = (uint32_t)(((uint32_t)arg_0 & ~0xFFu) | (((uint32_t)((arg_0 - 0x10u))) & 0xFFu));
        loc_1 = (uint32_t)(((uint32_t)(0x520D8002u)));
    }
    loc_2 = (uint32_t)(acib_mem_read16(loc_1));
    return (int32_t)((acib_arm_lsr32(((uint32_t)(loc_2)), arg_0) & 1u));
}

/* @provenance RVA 0x079734 sub_79734 */
int32_t phy_mdm_fn_079734(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    bool loc_0 = false;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    int16_t loc_3 = 0;
    int16_t loc_4 = 0;
    if (arg_1 <= 0x17u) {
        loc_0 = (((arg_1 > 0xFu)) != 0);
        if (arg_1 > 0xFu) {
            arg_1 = (uint32_t)(((uint32_t)arg_1 & ~0xFFu) | (((uint32_t)((arg_1 - 0x10u))) & 0xFFu));
        }
        loc_1 = (int32_t)(((int32_t)acib_arm_lsl32(1u, arg_1)));
        loc_2 = (uint32_t)(0x520D8004u);
        if ((loc_0) != 0) {
            loc_2 = (uint32_t)(((uint32_t)((((uint32_t)(0x520D8004u)) + 2u))));
        }
        loc_3 = (int16_t)(acib_mem_read16(((uint32_t)(loc_2))));
        loc_4 = (int16_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) | ((uint32_t)(int32_t)(loc_1)))));
        if (((uint32_t)(int32_t)(arg_0)) == 0u) {
            loc_4 = (int16_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(((uint16_t)(loc_1)))))))))));
        }
        acib_mem_write16(((uint32_t)(loc_2)), (uint16_t)(loc_4));
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0797f4 sub_797F4 */
uint32_t phy_mdm_fn_0797f4(void)
{
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(acib_mmio_read32(0x520D4064u));
    acib_mmio_write32(0x520D4064u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) | 0x3FFu))));
    return (uint32_t)(0x520D4000u);
}

/* @provenance RVA 0x079818 sub_79818 */
int32_t phy_mdm_fn_079818(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    bool loc_0 = false;
    int8_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    int32_t loc_5 = 0;
    loc_0 = (((arg_0 > 5u)) != 0);
    if (arg_0 > 5u) {
        arg_0 = (uint32_t)(((uint32_t)arg_0 & ~0xFFu) | (((uint32_t)((arg_0 - 6u))) & 0xFFu));
    }
    loc_1 = (int8_t)((4u * arg_0));
    loc_2 = (int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), ((uint32_t)(loc_1)))));
    loc_3 = (int32_t)(((int32_t)acib_arm_lsl32(0xFu, ((uint32_t)(loc_1)))));
    loc_4 = (int32_t)(((int32_t)0x6Cu));
    if ((loc_0) != 0) {
        loc_4 = (int32_t)(((int32_t)0x80u));
    }
    loc_5 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(acib_mmio_read32(((uint32_t)((((uint32_t)(0x520D4000u)) + ((uint32_t)(int32_t)(loc_4)))))) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_3)))))))))) | ((uint32_t)(int32_t)(loc_2)))));
    acib_mmio_write32(((uint32_t)((((uint32_t)(0x520D4000u)) + ((uint32_t)(int32_t)(loc_4))))), (uint32_t)(loc_5));
    return (int32_t)(loc_5);
}

/* @provenance RVA 0x0798c4 sub_798C4 */
int32_t phy_mdm_fn_0798c4(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x520D401Cu));
    loc_1 = (uint32_t)((loc_0 | ((uint32_t)(int32_t)(arg_0))));
    if (((uint32_t)(int32_t)(arg_1)) == 0u) {
        loc_1 = (uint32_t)((loc_0 & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(arg_0))))))));
    }
    acib_mmio_write32(0x520D401Cu, (uint32_t)(loc_1));
    return (int32_t)(((((uint32_t)(int32_t)(arg_0)) & (~loc_0)) == 0u));
}

/* @provenance RVA 0x079928 sub_79928 */
int32_t phy_mdm_fn_079928(void)
{
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_0 = (int32_t)(((int32_t)0u));
    for (loc_1 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_1)) != 0x18u; loc_1 = (int32_t)((uint32_t)loc_1 + ((uint32_t)(((int32_t)0xCu))))) {
        loc_2 = (uint32_t)((((uint32_t)(0x10021C6Cu)) + ((uint32_t)(int32_t)(loc_1))));
        acib_mem_write16(((uint32_t)((((uint32_t)(0x10021C6Cu)) + ((uint32_t)(int32_t)(loc_1))))), (uint16_t)(((int16_t)0xFFFFFF00u)));
        acib_mem_write16((((uint32_t)(loc_2)) + (2u * 4u)), (uint16_t)(((int16_t)0u)));
        acib_mem_write32((((uint32_t)(loc_2)) + (4u * 1u)), (uint32_t)(((int32_t)0xFFFFFFFFu)));
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x079af0 sub_79AF0 */
int32_t phy_mdm_fn_079af0(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    loc_0 = (uint32_t)((0x10021C6Cu + 1u));
    for (loc_1 = (int32_t)(((int32_t)0u)); (((uint32_t)(int32_t)(loc_1)) != 2u) && ((((uint32_t)(acib_mem_read8(loc_0))) != ((uint32_t)(int32_t)(arg_0))) || (((uint32_t)(acib_mem_read8((loc_0 - 1u)))) == 0u)); loc_1 = (int32_t)((uint32_t)loc_1 + 1u)) {
        loc_0 = (uint32_t)((uint32_t)loc_0 + (1u * 0xCu));
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x079d84 sub_79D84 */
int32_t phy_mdm_fn_079d84(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x520E4148u));
    acib_mmio_write32(0x520E4148u, (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFFFFFFCu) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 3u)))))));
    loc_1 = (int32_t)(acib_mem_read_s32(0x10021CC0u));
    loc_2 = (int32_t)(acib_mmio_read32(0x520E4148u));
    acib_mem_write32(((uint32_t)(acib_mem_read_s32(0x10021CC0u))), (uint32_t)(loc_2));
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x079da8 sub_79DA8 */
int32_t phy_mdm_fn_079da8(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(((int32_t)acib_mmio_read32(0x520E4104u)));
    acib_mmio_write32(0x520E4104u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 0x4000000u))));
    loc_1 = (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10021CC4u))));
    loc_2 = (int32_t)(((int32_t)acib_mmio_read32(0x520E4104u)));
    acib_mem_write32(((uint32_t)(acib_mem_read_s32(0x10021CBCu))), (uint32_t)(loc_2));
    acib_mem_write32(loc_1, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x079dd0 sub_79DD0 */
int32_t phy_mdm_fn_079dd0(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x520E4100u));
    acib_mmio_write32(0x520E4100u, (uint32_t)((loc_0 | 0x8000000u)));
    loc_1 = (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10021CB8u))));
    loc_2 = (uint32_t)(acib_mmio_read32(0x520E4100u));
    acib_mem_write32(((uint32_t)(acib_mem_read_s32(0x10021CB4u))), (uint32_t)(loc_2));
    acib_mem_write32(loc_1, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07a500 sub_7A500 */
int32_t phy_mdm_fn_07a500(void)
{
    sub_740A8(0x46800u, 0x820Fu);
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x07a618 sub_7A618 */
uint32_t phy_mdm_fn_07a618(void)
{
    acib_mmio_write32(0x520E800Cu, (uint32_t)(((int32_t)0x10000u)));
    acib_mmio_write32(0x520E800Cu, (uint32_t)(((int32_t)0x10000u)));
    return (uint32_t)(0x520E8000u);
}

/* @provenance RVA 0x07a804 sub_7A804 */
int32_t phy_mdm_fn_07a804(void)
{
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x42440418u));
    acib_mmio_write32(0x42440418u, (uint32_t)(((loc_0 & 0xEFF0FCF0u) | 0x1004030Fu)));
    loc_1 = (uint32_t)(acib_mmio_read32(0x42440410u));
    acib_mmio_write32(0x42440410u, (uint32_t)(((loc_1 & 0xEFF0FCF0u) | 0x1004030Fu)));
    loc_2 = (int32_t)(acib_mmio_read32((0x520E4168u + 4u * 0x245u)));
    acib_mmio_write32((0x520E4168u + 4u * 0x245u), (uint32_t)(((((uint32_t)(int32_t)(loc_2)) & 0xFFFFFF00u) | 0x28u)));
    loc_3 = (int32_t)(acib_mmio_read32((0x520E4168u + 4u * 0x246u)));
    acib_mmio_write32((0x520E4168u + 4u * 0x246u), (uint32_t)(((((uint32_t)(int32_t)(loc_3)) & 0xFFFFFF00u) | 0x28u)));
    return (int32_t)(phy_mdm_fn_07b2e8((int32_t)(((int32_t)0u))));
}

/* @provenance RVA 0x07a978 sub_7A978 */
int32_t phy_mdm_fn_07a978(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    acib_mem_write32(0x100016E4u, (uint32_t)(((int32_t)0x3501B4F4u)));
    acib_mem_write32((arg_1 + 4u * 0x108u), (uint32_t)(((acib_mem_read32((arg_1 + 4u * 0x108u)) & 0xFFF0FFFFu) | 0x40000u)));
    loc_0 = (uint32_t)(acib_mmio_read32(0xE000ED10u));
    acib_mmio_write32(0xE000ED10u, (uint32_t)((loc_0 & 0xFFFFFFFBu)));
    acib_mem_write32((arg_1 + 4u * 0x102u), (uint32_t)(((acib_mem_read32((arg_1 + 4u * 0x102u)) & 0xFFF0FFFFu) | 0x40000u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint32_t)(((int32_t)2u)));
    loc_1 = (int32_t)(((int32_t)(acib_mem_read32((arg_1 + 4u * 0x100u)) | 0xF0000u)));
    acib_mem_write32((arg_1 + 4u * 0x100u), (uint32_t)(loc_1));
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x07aa74 sub_7AA74 */
uint32_t phy_mdm_fn_07aa74(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x42440420u));
    acib_mmio_write32(0x42440420u, (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xEFFFFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x1Cu)))))));
    loc_1 = (int32_t)(acib_mmio_read32(0x42440408u));
    acib_mmio_write32(0x42440408u, (uint32_t)(((((uint32_t)(int32_t)(loc_1)) & 0xEFFFFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x1Cu)))))));
    loc_2 = (int32_t)(acib_mmio_read32(0x42440400u));
    loc_3 = (uint32_t)(((((uint32_t)(int32_t)(loc_2)) & 0xEFFFFFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_0)), 0x1Cu))))));
    acib_mmio_write32(0x42440400u, (uint32_t)(loc_3));
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x07afc8 sub_7AFC8 */
int32_t phy_mdm_fn_07afc8(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    loc_1 = (uint32_t)(acib_mmio_read32(0x52160020u));
    sub_74270(0x2046812u, ((uint32_t)(arg_0)), loc_1);
    acib_mem_write8(0x10021DC2u, (uint8_t)((((uint32_t)(int32_t)(arg_0)) == 0u)));
    return (int32_t)((((uint32_t)(int32_t)(arg_0)) == 0u));
}

/* @provenance RVA 0x07b01c sub_7B01C */
int32_t phy_mdm_fn_07b01c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int8_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    uint32_t loc_6 = 0;
    int32_t loc_7 = 0;
    int32_t loc_8 = 0;
    uint32_t loc_9 = 0;
    int32_t loc_10 = 0;
    loc_0 = (int8_t)((arg_0 & 0x1Fu));
    loc_1 = (int32_t)(((uint8_t)(acib_arm_lsr32(((uint32_t)(arg_0)), 5u))));
    loc_2 = (int32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(loc_0)))));
    loc_3 = (int32_t)(acib_mmio_read32((((uint32_t)(0x52160000u)) + (4u * ((uint32_t)(int32_t)(loc_1))))));
    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
        loc_4 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) | ((uint32_t)(int32_t)(loc_2)))));
        loc_9 = (uint32_t)((0x529C0000u + (4u * ((uint32_t)(int32_t)(loc_1)))));
        loc_10 = (int32_t)(acib_mem_read32((((uint32_t)(loc_9)) + (4u * 5u))));
        loc_6 = (uint32_t)(((uint32_t)((loc_9 + (4u * 5u)))));
        loc_8 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | ((uint32_t)(int32_t)(loc_10)))));
    } else {
        loc_4 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_2)))))))));
        loc_5 = (uint32_t)((0x529C0000u + (4u * ((uint32_t)(int32_t)(loc_1)))));
        loc_7 = (int32_t)(acib_mem_read32((((uint32_t)(loc_5)) + (4u * 5u))));
        loc_6 = (uint32_t)(((uint32_t)((loc_5 + (4u * 5u)))));
        loc_8 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_7)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_2)))))))));
    }
    acib_mmio_write32((((uint32_t)(0x52160000u)) + (4u * ((uint32_t)(int32_t)(loc_1)))), (uint32_t)(loc_4));
    acib_mem_write32(loc_6, (uint32_t)(loc_8));
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x07b1b0 sub_7B1B0 */
int32_t phy_mdm_fn_07b1b0(void)
{
    return (int32_t)((((uint32_t)(int32_t)(acib_mem_read_s8(0x10021E32u))) == 0u));
}

/* @provenance RVA 0x07b260 sub_7B260 */
int32_t phy_mdm_fn_07b260(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x42440420u));
    loc_1 = (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFF0FFFFu) | 0x40000u));
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        loc_1 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 0xF0000u)));
    }
    acib_mmio_write32(0x42440420u, (uint32_t)(loc_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07b2c0 sub_7B2C0 */
int32_t phy_mdm_fn_07b2c0(void)
{
    int8_t loc_0 = 0;
    loc_0 = (int8_t)(acib_mmio_read32((0x520DC0C4u + 4u * 0u)));
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & 1u)));
}

/* @provenance RVA 0x07b2e8 sub_7B2E8 */
int32_t phy_mdm_fn_07b2e8(int32_t arg_0)
{
    (void)arg_0;
    uint8_t loc_1 = 0;
    int32_t loc_2 = 0;
    sub_6DC88();
    if (((uint32_t)(int32_t)(arg_0)) != 0u) {
        loc_1 = (uint8_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s8(0x10021E7Eu))) + 1u)));
    } else {
        loc_1 = (uint8_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s8(0x10021E7Eu))) - 1u)));
        if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10021E7Eu))) == 0u) {
            loc_1 = (uint8_t)(((int8_t)0u));
        }
    }
    acib_mem_write8(0x10021E7Eu, (uint8_t)(loc_1));
    loc_2 = (int32_t)(loc_1);
    if (((uint32_t)(loc_1)) != 0u) {
        loc_2 = (int32_t)(((int32_t)0x11u));
    }
    acib_mmio_write32(0x520DC14Cu, (uint32_t)(loc_2));
    return (int32_t)((sub_6DD04(), 0u));
}

/* @provenance RVA 0x07b328 sub_7B328 */
int32_t phy_mdm_fn_07b328(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10021E7Eu))));
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10021E7Eu))) != 0u) {
        return (int32_t)(((int32_t)1u));
    }
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x07b380 sub_7B380 */
int32_t phy_mdm_fn_07b380(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    acib_mmio_write32(0x520E8010u, (uint32_t)(arg_0));
    loc_0 = (int32_t)(acib_mmio_read32(0x520E8014u));
    acib_mmio_write32(0x520E8014u, (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFC00000u) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) & 0x3FFFFFu)))))));
    acib_mmio_write32(0x520E800Cu, (uint32_t)(((int32_t)0x10001u)));
    acib_mem_write32(0x10021E3Cu, (uint32_t)(arg_1));
    acib_mem_write32(0x10021E38u, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07b51c sub_7B51C */
int32_t phy_mdm_fn_07b51c(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x98u)))), (uint32_t)(((uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x98u)))))) | 1u)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07b9ea def_7B9B0 */
int32_t phy_mdm_fn_07b9ea(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    sub_74256(0x47802u, arg_1);
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x07ba30 sub_7BA30 */
int32_t phy_mdm_fn_07ba30(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mmio_write32(0x521AC014u, (uint32_t)(arg_1));
    acib_mmio_write32(0x521AC018u, (uint32_t)(arg_0));
    acib_mmio_write32((0x520E4168u + 4u * 0x32u), (uint32_t)(arg_1));
    acib_mmio_write32((0x520E4168u + 4u * 0x33u), (uint32_t)(arg_0));
    acib_mmio_write32((0x520E4168u + 4u * 0x34u), (uint32_t)(arg_1));
    acib_mmio_write32((0x520E4168u + 4u * 0x35u), (uint32_t)(arg_0));
    acib_mmio_write32((0x520E4168u + 4u * 0x36u), (uint32_t)(arg_1));
    acib_mmio_write32((0x520E4168u + 4u * 0x37u), (uint32_t)(arg_0));
    acib_mmio_write32((0x520E4168u + 4u * 0x41u), (uint32_t)(arg_1));
    acib_mmio_write32((0x520E4168u + 4u * 0x42u), (uint32_t)(arg_0));
    return (int32_t)(arg_1);
}

/* @provenance RVA 0x07bb30 sub_7BB30 */
int32_t phy_mdm_fn_07bb30(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint32_t)loc_0 & ~0xFFu) | (((uint32_t)(acib_mmio_read32((0x521AC0D8u + 4u * 0xC1Eu)))) & 0xFFu));
    return (int32_t)(((uint8_t)(loc_0)));
}

/* @provenance RVA 0x07bc10 sub_7BC10 */
int32_t phy_mdm_fn_07bc10(void)
{
    int32_t loc_0 = 0;
    int16_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x521AC044u));
    loc_2 = (int32_t)(false);
    if (((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_0)), 0x1Fu)))) != 0u) {
        loc_1 = (int16_t)(acib_mmio_read32(0x521AC044u));
        if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) & 0x100u)))) != 0u) {
            return (int32_t)(true);
        }
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x07c0cc sub_7C0CC */
int32_t phy_mdm_fn_07c0cc(int8_t arg_0)
{
    (void)arg_0;
    uint8_t loc_0 = 0;
    loc_0 = (uint8_t)(acib_mmio_read32((0x521AC0B4u + 4u * 0u)));
    return (int32_t)(((int32_t)(((uint32_t)(((uint8_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(arg_0)))))))) & ((uint32_t)(loc_0)))));
}

/* @provenance RVA 0x07c154 sub_7C154 */
int8_t phy_mdm_fn_07c154(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mmio_read32((0x521AC0D8u + 4u * 0x7E6u)));
    acib_mmio_write32((0x521AC0D8u + 4u * 0x7E6u), (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFFFFFFEu) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 1u)))))));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x07c18c sub_7C18C */
int32_t phy_mdm_fn_07c18c(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint32_t)loc_0 & ~0xFFu) | (((uint32_t)(((int32_t)acib_mmio_read32(0x521AC0D4u)))) & 0xFFu));
    return (int32_t)(((uint8_t)(loc_0)));
}

/* @provenance RVA 0x07c19c sub_7C19C */
int32_t phy_mdm_fn_07c19c(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint32_t)loc_0 & ~0xFFu) | (((uint32_t)(acib_mmio_read32((0x521AC0D8u + 4u * 0u)))) & 0xFFu));
    return (int32_t)(((uint8_t)(loc_0)));
}

/* @provenance RVA 0x07c1ea sub_7C1EA */
int32_t phy_mdm_fn_07c1ea(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xB8u)))), (uint32_t)(((uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xB8u)))))) | 0xFu)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07c3a0 sub_7C3A0 */
uint32_t phy_mdm_fn_07c3a0(int32_t arg_0, int32_t arg_1, int32_t arg_2, uint32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int8_t loc_2 = 0;
    int8_t loc_3 = 0;
    int8_t loc_4 = 0;
    loc_0 = (uint32_t)((((uint32_t)(0x10021EA9u)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x24u * ((uint32_t)(int32_t)(arg_0)))))) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(arg_1))))))))))));
    if (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x10009B24u + ((uint32_t)(int32_t)(arg_1)))))) & 0xFEu)))) == 2u) {
        return (uint32_t)((memset(acib_addr_ptr(arg_3), ((int32_t)(acib_mem_read_s8(loc_0))), 0xBu), arg_3));
    }
    loc_2 = (int8_t)(acib_mem_read_s8((loc_0 + 2u)));
    loc_1 = (uint32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s8((loc_0 + 1u)))))));
    if (((int32_t)(((int8_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) - ((uint32_t)(int32_t)(acib_mem_read_s8((loc_0 + 3u)))))))))) > ((int32_t)(acib_mem_read_s8(loc_0)))) {
        loc_2 = (int8_t)((uint32_t)loc_2 - ((uint32_t)(acib_mem_read_s8((loc_0 + 3u)))));
    }
    acib_mem_write8((arg_3 + 9u), (uint8_t)(loc_2));
    acib_mem_write8((arg_3 + 0xAu), (uint8_t)(acib_mem_read_s8((loc_0 + 2u))));
    loc_3 = (int8_t)(acib_mem_read_s8(loc_0));
    for (loc_4 = (int8_t)(((int8_t)8u)); ((int32_t)(loc_4)) >= 0; loc_4 = (int8_t)((uint32_t)loc_4 - 1u)) {
        acib_mem_write8((arg_3 + ((uint32_t)(((uint8_t)(loc_4))))), (uint8_t)(loc_3));
        if (((int32_t)(((uint32_t)(int32_t)(loc_3)) - ((uint32_t)(int32_t)(((int32_t)(loc_1)))))) >= arg_2) {
            loc_3 = (int8_t)((uint32_t)loc_3 - ((uint32_t)(((int8_t)(loc_1)))));
        }
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x07c5a4 sub_7C5A4 */
uint32_t phy_mdm_fn_07c5a4(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)));
    loc_2 = (int32_t)(((int32_t)0u));
    loc_3 = (uint32_t)(((uint32_t)(0x10009B2Du)));
    while (((uint32_t)(int32_t)(loc_2)) != 8u) {
        acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + ((uint32_t)(int32_t)(((int32_t)(3u * ((uint32_t)(int32_t)(loc_2)))))))))), (uint16_t)(((int16_t)0xFFu)));
        loc_4 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + ((uint32_t)(int32_t)(((int32_t)(3u * ((uint32_t)(int32_t)(loc_2)))))))));
        acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 2u)))), (uint8_t)(((int8_t)0u)));
        loc_3 = (uint32_t)(osa_svc_fn_0e0900((int32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + ((uint32_t)(int32_t)(loc_2)))))))), (int32_t)(0x10009B2Du), (uint32_t)(((int32_t)8u)), (uint32_t)(((int32_t)0u))));
        if (loc_3 <= 7u) {
            acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + ((uint32_t)(int32_t)(((int32_t)(3u * ((uint32_t)(int32_t)(loc_2)))))))))), (uint8_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + ((uint32_t)(int32_t)(loc_2)))))))));
            acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 1u)))), (uint8_t)(acib_mem_read_s8((0x10021FC9u + (((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 1u))))))))))) + (2u * loc_3))))));
            loc_3 = (uint32_t)(((uint8_t)(acib_mem_read_s8((0x10021FC9u + (((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 1u))))))))))) + 1u)))) + (2u * loc_3)))))));
            acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) + 2u)))), (uint8_t)(loc_3));
        }
        loc_2 = (int32_t)((uint32_t)loc_2 + 1u);
    }
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x07c6a8 sub_7C6A8 */
uint32_t phy_mdm_fn_07c6a8(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    int8_t loc_5 = 0;
    uint32_t loc_6 = 0;
    loc_0 = (uint32_t)((((uint32_t)(0x10022449u)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(acib_mem_read8((0x000F7AA8u + ((uint32_t)(acib_mem_read8(arg_0)))))))))))));
    for (loc_1 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_1)) != 0x48u; loc_1 = (int32_t)((uint32_t)loc_1 + ((uint32_t)(((int32_t)0x18u))))) {
        loc_2 = (int32_t)(acib_mem_read32(((uint32_t)((arg_0 + 6u)))));
        loc_3 = (uint32_t)((loc_0 + ((uint32_t)(int32_t)(((int32_t)(0x48u * ((uint32_t)(acib_mem_read8((arg_0 + 1u))))))))));
        acib_mem_write32(((uint32_t)((loc_3 + ((uint32_t)(int32_t)(loc_1))))), (uint32_t)(acib_mem_read32(((uint32_t)((arg_0 + 2u))))));
        loc_4 = (uint32_t)((loc_3 + ((uint32_t)(int32_t)(loc_1))));
        acib_mem_write32((((uint32_t)(loc_4)) + (4u * 1u)), (uint32_t)(loc_2));
    }
    loc_5 = (int8_t)(acib_mem_read_s8(0x10021EA8u));
    loc_6 = (uint32_t)(((int32_t)(((uint32_t)(acib_mem_read8((arg_0 + 1u)))) + 1u)));
    if (loc_6 > ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10021EA8u)))))) {
        loc_5 = (int8_t)(loc_6);
    }
    acib_mem_write8(0x10021EA8u, (uint8_t)(loc_5));
    return (uint32_t)(loc_6);
}

/* @provenance RVA 0x07c720 sub_7C720 */
uint32_t phy_mdm_fn_07c720(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    int8_t loc_2 = 0;
    (void)memcpy_wrapper(acib_addr_ptr((((uint32_t)(0x10022449u)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x48u * ((uint32_t)(acib_mem_read8((0x000F7AA8u + ((uint32_t)(acib_mem_read8(arg_0))))))))))) + ((uint32_t)(int32_t)(((int32_t)(0x18u * ((uint32_t)(acib_mem_read8((arg_0 + 1u)))))))))))) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(acib_mem_read8((arg_0 + 2u)))))))))))))), acib_addr_ptr((arg_0 + 4u)), ((uint32_t)(acib_mem_read8((arg_0 + 3u)))));
    loc_1 = (uint32_t)(0x10021EA8u);
    loc_2 = (int8_t)(acib_mem_read_s8(0x10021EA8u));
    if ((((uint32_t)(acib_mem_read8((arg_0 + 1u)))) + 1u) > ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10021EA8u)))))) {
        loc_2 = (int8_t)(((int32_t)(((uint32_t)(acib_mem_read8((arg_0 + 1u)))) + 1u)));
    }
    acib_mem_write8(0x10021EA8u, (uint8_t)(loc_2));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x07c76c sub_7C76C */
int32_t phy_mdm_fn_07c76c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mem_read32((0x10022449u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x12u * ((uint32_t)(acib_mem_read8((0x000F7AA8u + ((uint32_t)(acib_mem_read8(arg_0))))))))))) + 1u)))) + ((uint32_t)(int32_t)(((int32_t)(6u * ((uint32_t)(acib_mem_read8((arg_0 + 1u)))))))))))) + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(acib_mem_read8((arg_0 + 2u)))))))))))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 1u)))), (uint32_t)(acib_mem_read32((0x10022449u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x12u * ((uint32_t)(acib_mem_read8((0x000F7AA8u + ((uint32_t)(acib_mem_read8(arg_0))))))))))) + ((uint32_t)(int32_t)(((int32_t)(6u * ((uint32_t)(acib_mem_read8((arg_0 + 1u)))))))))))) + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(acib_mem_read8((arg_0 + 2u))))))))))))))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 5u)))), (uint32_t)(loc_0));
    acib_mem_write8(((uint32_t)(arg_1)), (uint8_t)(((int8_t)8u)));
    return (int32_t)(((int32_t)8u));
}

/* @provenance RVA 0x07c7ac sub_7C7AC */
uint32_t phy_mdm_fn_07c7ac(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int16_t loc_2 = 0;
    uint32_t loc_3 = 0;
    loc_0 = (int32_t)(acib_mem_read8(arg_0));
    for (loc_1 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_1)) != 3u; loc_1 = (int32_t)((uint32_t)loc_1 + 1u)) {
        loc_2 = (int16_t)(acib_mem_read16((((uint32_t)(arg_0)) + (2u * 1u))));
        loc_3 = (uint32_t)((((uint32_t)(0x100016EAu)) + ((uint32_t)(int32_t)(((int32_t)(6u * ((uint32_t)(acib_mem_read8((arg_0 + 1u))))))))));
        if (((uint32_t)(int32_t)(loc_0)) == ((uint32_t)(int32_t)(loc_1))) {
            acib_mem_write16(((uint32_t)((loc_3 + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(loc_0))))))))), (uint16_t)(loc_2));
        } else {
            acib_mem_write16(((uint32_t)((loc_3 + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(loc_1))))))))), (uint16_t)(((uint32_t)(acib_mem_read16(((uint32_t)((loc_3 + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(loc_1))))))))))) & ((uint32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_2)))))))));
        }
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x07cc08 sub_7CC08 */
int32_t phy_mdm_fn_07cc08(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    int32_t loc_5 = 0;
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        acib_mmio_write32(0x42440410u, (uint32_t)(((uint32_t)(acib_mmio_read32(0x42440410u)) | 0x200u)));
        loc_2 = (uint32_t)((acib_mmio_read32(0x42440410u) | 0x100u));
        acib_mmio_write32(0x42440410u, (uint32_t)(loc_2));
    }
    acib_mmio_write32(0x42440410u, (uint32_t)(((acib_mmio_read32(0x42440410u) & 0xFFFFFFF0u) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0xFu)))))));
    loc_3 = (uint32_t)((acib_mmio_read32(0x42440410u) & 0xEFFFFFFFu));
    acib_mmio_write32(0x42440410u, (uint32_t)(loc_3));
    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
        loc_4 = (uint32_t)(acib_dwt_cyccnt_read());
        while ((((int32_t)1u)) != 0) {
            loc_5 = (int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(((uint8_t)(acib_mmio_read32(0x42440410u))))), 4u)));
            if (((uint32_t)(int32_t)(loc_5)) == ((uint32_t)(int32_t)(arg_0))) {
                break;
            }
            if ((acib_dwt_cyccnt_read() - loc_4) > ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10020974u))) * ((uint32_t)(int32_t)(arg_1))))))) {
                return (int32_t)(((int32_t)0u));
            }
        }
    }
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x07cc8c sub_7CC8C */
int32_t phy_mdm_fn_07cc8c(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    int32_t loc_5 = 0;
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        acib_mmio_write32(0x42440418u, (uint32_t)(((uint32_t)(acib_mmio_read32(0x42440418u)) | 0x200u)));
        loc_2 = (uint32_t)((acib_mmio_read32(0x42440418u) | 0x100u));
        acib_mmio_write32(0x42440418u, (uint32_t)(loc_2));
    }
    acib_mmio_write32(0x42440418u, (uint32_t)(((acib_mmio_read32(0x42440418u) & 0xFFFFFFF0u) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0xFu)))))));
    loc_3 = (uint32_t)((acib_mmio_read32(0x42440418u) & 0xEFFFFFFFu));
    acib_mmio_write32(0x42440418u, (uint32_t)(loc_3));
    if (((uint32_t)(int32_t)(arg_1)) != 0u) {
        loc_4 = (uint32_t)(acib_dwt_cyccnt_read());
        while ((((int32_t)1u)) != 0) {
            loc_5 = (int32_t)(((int32_t)acib_arm_lsr32(((uint32_t)(((uint8_t)(acib_mmio_read32(0x42440418u))))), 4u)));
            if (((uint32_t)(int32_t)(loc_5)) == ((uint32_t)(int32_t)(arg_0))) {
                break;
            }
            if ((acib_dwt_cyccnt_read() - loc_4) > ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10020974u))) * ((uint32_t)(int32_t)(arg_1))))))) {
                return (int32_t)(((int32_t)0u));
            }
        }
    }
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x07d2c8 sub_7D2C8 */
int32_t phy_mdm_fn_07d2c8(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int8_t loc_1 = 0;
    loc_0 = (int32_t)((acib_arm_lsl32(arg_0, 0x11u) - 0x4F0000u));
    if (arg_0 < 0x28u) {
        loc_0 = (int32_t)(acib_arm_lsl32(arg_0, 0x11u));
    }
    loc_1 = (int8_t)(((int32_t)acib_mmio_read32(0x520E4150u)));
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) & 1u)))) + ((uint32_t)(int32_t)(loc_0)))));
}

/* @provenance RVA 0x07e084 sub_7E084 */
int32_t phy_mdm_fn_07e084(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write32(0x100209A4u, (uint32_t)(arg_3));
    acib_mem_write32(0x100209A0u, (uint32_t)(arg_2));
    acib_mem_write32(0x1002099Cu, (uint32_t)(arg_1));
    acib_mem_write32(0x10020998u, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07e17c sub_7E17C */
uint32_t phy_mdm_fn_07e17c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(arg_1));
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)0x4200102u)));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x07e7f8 sub_7E7F8 */
int32_t phy_mdm_fn_07e7f8(void)
{
    return (int32_t)(acib_mmio_read32(0x520E8018u));
}

/* @provenance RVA 0x07e8f0 sub_7E8F0 */
uint32_t phy_mdm_fn_07e8f0(void)
{
    int32_t loc_0 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x529C009Cu));
    acib_mmio_write32(0x529C009Cu, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 0x2000000u))));
    loc_2 = (int32_t)(acib_mmio_read32(0x52160008u));
    acib_mmio_write32(0x52160008u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | 0x2000000u))));
    return (uint32_t)(0x52160000u);
}

/* @provenance RVA 0x07e978 sub_7E978 */
int32_t phy_mdm_fn_07e978(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(arg_1);
}

/* @provenance RVA 0x07e98a sub_7E98A */
int32_t phy_mdm_fn_07e98a(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)((sub_74270(0x4B000u, ((uint32_t)(arg_1)), ((uint32_t)(arg_2))), 0u));
    return (int32_t)(phy_mdm_fn_07e978((int32_t)(loc_0), (int32_t)(((int32_t)0u))));
}

/* @provenance RVA 0x07ea1c sub_7EA1C */
int32_t phy_mdm_fn_07ea1c(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    if (((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 0xAu)))) >= 9u) {
        loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)));
        if (loc_0 > 6u) {
            return (int32_t)(phy_mdm_fn_07ed18((uint32_t)(((int32_t)0x4B001u))));
        } else {
            arg_0 = (int32_t)(((int32_t)acib_arm_lsl32(1u, loc_0)));
            loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) ^ 0x7Fu)))) & acib_mmio_read32((0x521AC0D8u + 4u * 0xFCBu)))));
            loc_2 = (int32_t)(acib_mmio_read32((0x521AC0D8u + 4u * 0xFCBu)));
            acib_mmio_write32((0x521AC0D8u + 4u * 0xFCBu), (uint32_t)((((uint32_t)(int32_t)(loc_1)) | (((uint32_t)(int32_t)(loc_2)) & 0xFFFFFF80u))));
            if (((uint32_t)(int32_t)(arg_1)) != 0u) {
                arg_0 = (int32_t)((uint32_t)arg_0 | ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10023A6Cu))))));
                acib_mem_write8(0x10023A6Cu, (uint8_t)(arg_0));
            }
        }
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x07ead4 sub_7EAD4 */
int32_t phy_mdm_fn_07ead4(void)
{
    int8_t loc_0 = 0;
    loc_0 = (int8_t)(acib_mmio_read32((0x521AC0D8u + 4u * 0xFD5u)));
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & 0x3Fu)));
}

/* @provenance RVA 0x07ed0c sub_7ED0C */
int32_t phy_mdm_fn_07ed0c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 6u)))) + 4u))));
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x07ed18 sub_7ED18 */
int32_t phy_mdm_fn_07ed18(uint32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((sub_740A8(arg_0, 0x8245u), 0u));
}

/* @provenance RVA 0x07edba sub_7EDBA */
uint32_t phy_mdm_fn_07edba(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    int32_t loc_6 = 0;
    loc_2 = (uint32_t)(phy_ipc_cmd_op11_u8(((uint32_t)(arg_0))));
    loc_3 = (uint32_t)(phy_ipc_cmd_op12_u8(((uint32_t)(arg_0))));
    loc_4 = (uint32_t)(phy_ipc_cmd_op11_u8(((uint32_t)(arg_1))));
    loc_5 = (uint32_t)(phy_ipc_cmd_op12_u8(((uint32_t)(arg_1))));
    for (loc_6 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_6)) != 0x14u; loc_6 = (int32_t)((uint32_t)loc_6 + 1u)) {
        acib_mmio_write32((loc_4 + 4u * ((uint32_t)(int32_t)(loc_6))), (uint32_t)(acib_mmio_read32((loc_2 + 4u * ((uint32_t)(int32_t)(loc_6))))));
        acib_mmio_write32((loc_5 + 4u * ((uint32_t)(int32_t)(loc_6))), (uint32_t)(acib_mmio_read32((loc_3 + 4u * ((uint32_t)(int32_t)(loc_6))))));
    }
    return (uint32_t)(loc_5);
}

/* @provenance RVA 0x07ee04 sub_7EE04 */
int32_t phy_mdm_fn_07ee04(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int16_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    int32_t loc_5 = 0;
    loc_1 = (int16_t)(arg_1);
    loc_2 = (int32_t)(osa_core_fn_06f2d8((uint32_t)(arg_1)));
    if (((uint32_t)(int32_t)(loc_2)) == 0xFFu) {
        loc_5 = (int32_t)(((int32_t)0xFFu));
        loc_3 = (int32_t)(((int32_t)0u));
    } else {
        loc_3 = (int32_t)(loc_2);
        loc_4 = (uint32_t)(phy_ipc_cmd_op11_u8(((uint32_t)(arg_0))));
        acib_mmio_write32(loc_4, (uint32_t)((((acib_mmio_read32(loc_4) & 0xFFFF0000u) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_3)), 7u))))) | ((uint32_t)(((uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) - ((uint32_t)(int32_t)(acib_mem_read_s16((0x10000640u + 2u * ((uint32_t)(int32_t)(((int32_t)(3u * ((uint32_t)(int32_t)(loc_3)))))))))))))))))));
        loc_5 = (int32_t)(((int32_t)0u));
    }
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_5)), 8u)))))));
}

/* @provenance RVA 0x07f020 sub_7F020 */
int32_t phy_mdm_fn_07f020(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    if ((((uint32_t)(int32_t)(arg_1)) != 0u) && (((uint32_t)(int32_t)(arg_2)) != 0u)) {
        loc_0 = (uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))))))))))) - 0x50u)))));
        if (((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))) < 0x28u) {
            loc_0 = (uint32_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))))))))))));
        }
        loc_1 = (uint32_t)(acib_mem_read16(loc_0));
        arg_0 = (int32_t)((acib_arm_lsr32(((uint32_t)(loc_1)), 7u) & 7u));
        loc_2 = (int32_t)((loc_1 & 0x7Fu));
    } else {
        loc_2 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))))))) + 0xB1u)));
        if (((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))) < 0x28u) {
            loc_2 = (int32_t)(((int32_t)(2u * ((uint32_t)(((uint8_t)((((uint32_t)(arg_0)) >> 8) & 0xFFu)))))));
        }
    }
    return (int32_t)(((int32_t)(((uint32_t)(((uint8_t)(arg_0)))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_2)), 8u)))))));
}

/* @provenance RVA 0x07f1b4 sub_7F1B4 */
int32_t phy_mdm_fn_07f1b4(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    bool loc_3 = false;
    uint32_t loc_4 = 0;
    int32_t loc_5 = 0;
    int32_t loc_6 = 0;
    uint32_t loc_7 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x520E4004u));
    loc_1 = (uint32_t)((acib_arm_lsr32(((uint32_t)(loc_0)), 8u) & 0x7Fu));
    loc_2 = (int32_t)(((int32_t)(acib_arm_lsr32(((uint32_t)(acib_mmio_read32(0x520E4004u))), 0x15u) & 7u)));
    loc_3 = (((loc_1 >= 0x28u)) != 0);
    loc_4 = (uint32_t)((loc_1 - 0x28u));
    loc_5 = (int32_t)(((int32_t)(5u * ((uint32_t)(int32_t)(loc_2)))));
    if (loc_1 < 0x28u) {
        loc_4 = (uint32_t)(loc_1);
    }
    loc_6 = (int32_t)(((int32_t)0x680u));
    if (!(loc_3 != 0)) {
        loc_6 = (int32_t)(((int32_t)0x400u));
    }
    loc_7 = (uint32_t)(acib_mmio_read16(((uint32_t)((((uint32_t)(0x520E4000u)) + ((((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(loc_5)))))) + (2u * loc_4)) + ((uint32_t)(int32_t)(loc_6))))))));
    acib_mem_write32(arg_0, (uint32_t)(acib_arm_lsr32(((uint32_t)(loc_7)), 7u)));
    return (int32_t)((((uint32_t)(((uint16_t)(acib_mem_read_s16((0x10000640u + 2u * (3u * acib_arm_lsr32(((uint32_t)(loc_7)), 7u)))))))) + (loc_7 & 0x7Fu)));
}

/* @provenance RVA 0x07fd38 sub_7FD38 */
int32_t phy_mdm_fn_07fd38(void)
{
    return (int32_t)((sub_6DD04(), 0u));
}

/* @provenance RVA 0x0801d0 sub_801D0 */
int32_t phy_mdm_fn_0801d0(void)
{
    uint8_t loc_1 = 0;
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x1002030Cu))) == 0u) {
        return (int32_t)(((int32_t)0u));
    }
    loc_1 = (uint8_t)(((int8_t)1u));
    do {
        if (((uint32_t)(((uint8_t)(((uint32_t)(acib_mem_read_s32((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(loc_1)))))) + 5u)))))))) & 0xFFu)))) == 0u) {
            return (int32_t)(((int32_t)1u));
        }
        loc_1 = (uint8_t)((uint32_t)loc_1 + 1u);
    } while (((uint32_t)(loc_1)) <= ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x1002030Cu))))));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x08035c sub_8035C */
uint32_t phy_mdm_fn_08035c(void)
{
    acib_mem_write8(0x10009CF0u, (uint8_t)(((int8_t)0xFFFFFFFFu)));
    acib_mem_write32(0x10009CECu, (uint32_t)(((int32_t)2u)));
    acib_mem_write16(0x10009CF2u, (uint16_t)(((int16_t)0x505u)));
    return (uint32_t)(0x10009CECu);
}

/* @provenance RVA 0x080374 sub_80374 */
int32_t phy_mdm_fn_080374(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    if (((uint32_t)(int32_t)(arg_0)) != ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10009CF0u)))))) {
        loc_0 = (uint32_t)((0x100200E0u + 4u * ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
        if (((uint32_t)(acib_mem_read8((((uint32_t)(0x10009CECu)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32((loc_0 + 4u * 4u)))) + 5u)))))))) != ((uint32_t)(int32_t)(arg_0))) {
            arg_0 = (int32_t)(((uint8_t)(((int32_t)(3u - ((uint32_t)(int32_t)(acib_mem_read_s32(0x10009CECu))))))));
            acib_mem_write8((((uint32_t)(loc_0)) + 0x1Du), (uint8_t)(((int8_t)1u)));
            acib_mem_write32((loc_0 + 4u * 4u), (uint32_t)(arg_0));
        }
    }
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0803b2 sub_803B2 */
int32_t phy_mdm_fn_0803b2(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)((sub_740FE(0x4C803u, 0xE4u, ((uint32_t)(arg_2))), 0u));
}

/* @provenance RVA 0x080514 sub_80514 */
int32_t phy_mdm_fn_080514(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    loc_1 = (uint32_t)(((uint32_t)(((0x10u * arg_0) | 0x52200000u))));
    acib_mem_write32((loc_1 + 4u * 2u), (uint32_t)(acib_mem_read16(arg_1)));
    acib_mem_write32((loc_1 + 4u * 3u), (uint32_t)(acib_mem_read16((arg_1 + 2u * 1u))));
    acib_mmio_write32((0x52200000u + 4u * (4u * arg_0)), (uint32_t)(acib_mem_read16((arg_1 + 2u * 2u))));
    acib_mem_write32((loc_1 + 4u * 1u), (uint32_t)(acib_mem_read16((arg_1 + 2u * 3u))));
    loc_2 = (int32_t)(acib_mem_read_s32(0x10020090u));
    if (((uint32_t)(int32_t)(acib_mem_read_s32(0x10020090u))) != 0u) {
        if (arg_0 == 3u) {
            acib_mmio_write32(0x520E406Cu, (uint32_t)(acib_mem_read32((((uint32_t)(arg_1)) + (4u * 1u)))));
            loc_2 = (int32_t)(acib_mem_read32(((uint32_t)(arg_1))));
            acib_mmio_write32(0x520E4070u, (uint32_t)(acib_mem_read32(((uint32_t)(arg_1)))));
        } else {
            if (arg_0 > 2u) {
                return (int32_t)((sub_740FE(0x4D800u, 0x8227u, arg_0), 0u));
            } else {
                loc_3 = (int32_t)(((8u * arg_0) + 0x520E4000u));
                acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + 0x30u)))), (uint32_t)(acib_mem_read32((((uint32_t)(arg_1)) + (4u * 1u)))));
                loc_2 = (int32_t)(acib_mem_read32(((uint32_t)(arg_1))));
                acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + 0x34u)))), (uint32_t)(acib_mem_read32(((uint32_t)(arg_1)))));
            }
        }
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x080728 sub_80728 */
uint32_t phy_mdm_fn_080728(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (uint32_t)((((uint32_t)(0x52208000u)) + ((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0))))))));
    loc_1 = (int32_t)(acib_mem_read_s32(arg_1));
    loc_2 = (int32_t)(acib_mem_read_s32((arg_1 + 4u * 1u)));
    acib_mem_write32((((uint32_t)(loc_0)) + (4u * 0x40u)), (uint32_t)(loc_1));
    acib_mem_write32((((uint32_t)(loc_0)) + (4u * 0x41u)), (uint32_t)(loc_2));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x080800 sub_80800 */
int32_t phy_mdm_fn_080800(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5220E000u));
    loc_1 = (int32_t)(acib_mmio_read32(((uint32_t)(0x5220E010u))));
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & ((uint32_t)(int32_t)(((int32_t)(~((uint32_t)(int32_t)(loc_1)))))))))))));
}

/* @provenance RVA 0x080be0 sub_80BE0 */
int32_t phy_mdm_fn_080be0(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint16_t)(acib_mmio_read16((0x5220C018u + 2u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_0)))))))))));
}

/* @provenance RVA 0x080c18 sub_80C18 */
int32_t phy_mdm_fn_080c18(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(0x10023B21u);
    if (((uint32_t)(int32_t)(arg_0)) == 0u) {
        loc_0 = (uint32_t)(0x10023B20u);
    }
    acib_mem_write8(loc_0, (uint8_t)(arg_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x081e0c sub_81E0C */
int32_t phy_mdm_fn_081e0c(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, int32_t arg_4, int32_t arg_5)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    (void)arg_5;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(arg_3));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x14u)))), (uint32_t)(arg_5));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x12u)))), (uint16_t)(((int16_t)0x420u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xEu)))), (uint32_t)(arg_4));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))), (uint16_t)(((int16_t)0x400u)));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(((int32_t)0x8000302u)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x081e30 sub_81E30 */
int64_t phy_mdm_fn_081e30(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int64_t)(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0x12u))))))))));
}

/* @provenance RVA 0x081ea4 sub_81EA4 */
uint32_t phy_mdm_fn_081ea4(void)
{
    acib_mem_write8(0x10023B42u, (uint8_t)(((int8_t)0u)));
    acib_mem_write8(0x10023B3Cu, (uint8_t)(((int8_t)0xFFFFFFFFu)));
    acib_mem_write32(0x10023B3Eu, (uint32_t)(((int32_t)0u)));
    return (uint32_t)(0x10023B3Cu);
}

/* @provenance RVA 0x082b64 sub_82B64 */
uint32_t phy_mdm_fn_082b64(int8_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_1 = (uint32_t)((((uint32_t)(0x10005B58u)) + ((uint32_t)(int32_t)(((int32_t)(0x13Cu * ((uint32_t)(((uint8_t)((((uint32_t)(acib_mem_read_s32(0x10005671u))) >> 24) & 0xFFu))))))))));
    if (((uint32_t)(int32_t)(acib_mem_read_s8((loc_1 + 0x83u)))) == 3u) {
        loc_2 = (uint32_t)((0x1000A03Cu + 4u * ((uint32_t)(int32_t)(((int32_t)(0xEu * ((uint32_t)(((uint8_t)(acib_mem_read_s8((loc_1 + 0xA8u))))))))))));
        sub_6DD70(0u);
        acib_mem_write8((((uint32_t)(loc_2)) + 0x4D4u), (uint8_t)(arg_0));
        return (uint32_t)(((uint32_t)((sub_6DDB8(), 0u))));
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x082d60 sub_82D60 */
uint32_t phy_mdm_fn_082d60(void)
{
    acib_mem_write32(0x10005660u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x1000566Cu, (uint8_t)(((int8_t)0u)));
    acib_mem_write32(0x10005650u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x100056B9u, (uint8_t)(((int8_t)0u)));
    acib_mem_write8(0x10005669u, (uint8_t)(((int8_t)0u)));
    acib_mem_write8(0x10005664u, (uint8_t)(((int8_t)0u)));
    acib_mem_write16(0x10005682u, (uint16_t)(((int16_t)0u)));
    acib_mem_write16(0x10005666u, (uint16_t)(((int16_t)0u)));
    return (uint32_t)(0x10005618u);
}

/* @provenance RVA 0x082fe8 sub_82FE8 */
int32_t phy_mdm_fn_082fe8(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    int16_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mem_read_s32(0x10005671u));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(acib_mem_read_s32(0x10005660u)));
    loc_1 = (int16_t)(acib_mem_read_s16(0x1000566Fu));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 6u)))), (uint32_t)(loc_0));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint16_t)(loc_1));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x083160 sub_83160 */
int32_t phy_mdm_fn_083160(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10005664u))));
    if (((uint32_t)(int32_t)(acib_mem_read_s8(0x10005664u))) != 0u) {
        loc_0 = (int32_t)(((int32_t)1u));
    }
    return (int32_t)(((int32_t)(((uint32_t)((((uint32_t)(int32_t)(acib_mem_read_s32(0x10005660u))) == 1u))) | ((uint32_t)(int32_t)(loc_0)))));
}

/* @provenance RVA 0x083180 sub_83180 */
int32_t phy_mdm_fn_083180(void)
{
    return (int32_t)(((uint8_t)(acib_mem_read_s32(0x10005671u))));
}

/* @provenance RVA 0x083370 sub_83370 */
int32_t phy_mdm_fn_083370(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    int16_t loc_2 = 0;
    loc_0 = (uint32_t)(((uint8_t)((((uint32_t)(acib_mem_read_s16(0x1000566Fu))) >> 8) & 0xFFu)));
    loc_1 = (uint32_t)(((uint16_t)(acib_mmio_read16((0x5220C040u + 2u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(acib_mem_read8((((uint32_t)(0x10005B58u)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x13Cu * ((uint32_t)(((uint8_t)((((uint32_t)(acib_mem_read_s32(0x10005671u))) >> 24) & 0xFFu)))))))) + 0x87u)))))))))))))))));
    acib_mem_write16(arg_1, (uint16_t)(loc_1));
    loc_2 = (int16_t)((loc_0 - loc_1));
    if (loc_0 < loc_1) {
        loc_2 = (int16_t)(((int16_t)1u));
    }
    acib_mem_write16(arg_0, (uint16_t)(loc_2));
    return (int32_t)((loc_0 < loc_1));
}

/* @provenance RVA 0x083500 sub_83500 */
int32_t phy_mdm_fn_083500(int32_t arg_0, int16_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(((int32_t)(3u * ((uint32_t)(int32_t)(arg_2)))));
    loc_1 = (int32_t)((((uint32_t)(int32_t)(arg_0)) == 0u));
    acib_mmio_write16((((uint32_t)(0x52206802u)) + (2u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(loc_0)))))))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16((0x000F64ECu + 2u * ((uint32_t)(int32_t)(loc_1)))))) & 0xF18Fu)))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 9u))))))));
    loc_2 = (int32_t)(acib_mem_read16((0x000F6568u + 2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) | ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(loc_1)))))))))))))) + 2u)))))));
    acib_mmio_write32(((uint32_t)((((uint32_t)(0x52206802u)) + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_0)))))) + 2u))))))), (uint32_t)(loc_2));
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x0836b0 sub_836B0 */
uint32_t phy_mdm_fn_0836b0(void)
{
    acib_mem_write32(0x10023B50u, (uint32_t)(((int32_t)0u)));
    return (uint32_t)(0x10023B50u);
}

/* @provenance RVA 0x08377c sub_8377C */
int32_t phy_mdm_fn_08377c(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)((sub_74270(0x1053808u, ((uint32_t)(arg_1)), ((uint32_t)(arg_2))), 0u));
}

/* @provenance RVA 0x083930 sub_83930 */
uint32_t phy_mdm_fn_083930(void)
{
    acib_mem_write16(0x10023BA5u, (uint16_t)(((uint32_t)(acib_mem_read_s16(0x10023BA5u)) + 1u)));
    return (uint32_t)(0x10023B50u);
}

/* @provenance RVA 0x0839dc sub_839DC */
uint32_t phy_mdm_fn_0839dc(void)
{
    acib_mem_write8(0x10023B7Bu, (uint8_t)(((int8_t)0xFFFFFFFBu)));
    acib_mem_write32(0x10023B7Cu, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10023B64u, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x10023B6Bu, (uint8_t)(((int8_t)0u)));
    return (uint32_t)((memset(acib_addr_ptr(0x10023B85u), 0, 0x26u), 0x10023B85u));
}

/* @provenance RVA 0x083a2c sub_83A2C */
int64_t phy_mdm_fn_083a2c(int64_t arg_0)
{
    (void)arg_0;
    int8_t loc_0 = 0;
    int16_t loc_1 = 0;
    int32_t loc_2 = 0;
    int8_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_0 = (int8_t)(acib_mem_read_s8(0x10023B73u));
    loc_1 = (int16_t)(((uint8_t)((((uint32_t)(acib_mem_read_s16(0x10023B75u))) >> 8) & 0xFFu)));
    loc_2 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10023B72u))));
    acib_mem_write8(((uint32_t)(((int64_t)(((uint64_t)(arg_0)) + ((uint64_t)(((int32_t)0xAu))))))), (uint8_t)(((uint8_t)((((uint32_t)(acib_mem_read_s32(0x10023B8Du))) >> 16) & 0xFFu))));
    loc_3 = (int8_t)(acib_mem_read_s32(0x10023BA7u));
    acib_mem_write8(((uint32_t)(((int64_t)(((uint64_t)(arg_0)) + ((uint64_t)(((int32_t)5u))))))), (uint8_t)(loc_0));
    acib_mem_write16(((uint32_t)(((int64_t)(((uint64_t)(arg_0)) + ((uint64_t)(((int32_t)6u))))))), (uint16_t)(loc_1));
    acib_mem_write8(((uint32_t)(((int64_t)(((uint64_t)(arg_0)) + ((uint64_t)(((int32_t)0xBu))))))), (uint8_t)(loc_3));
    loc_4 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10023B6Fu))));
    acib_mem_write16(((uint32_t)(((int64_t)(((uint64_t)(arg_0)) + ((uint64_t)(((int32_t)8u))))))), (uint16_t)(acib_mem_read_s16(0x10023B6Du)));
    arg_0 = (int64_t)(((uint64_t)arg_0 & 0xFFFFFFFFULL) | (((uint64_t)((uint32_t)((((uint32_t)(int32_t)(loc_2)) != ((uint32_t)(int32_t)(loc_4)))))) << 32));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(((uint32_t)((((uint64_t)(arg_0)) >> 32) & 0xFFFFFFFFULL))));
    return (int64_t)(arg_0);
}

/* @provenance RVA 0x0842a4 sub_842A4 */
uint32_t phy_mdm_fn_0842a4(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x10023BC0u + 4u * ((uint32_t)(int32_t)(arg_0))));
    acib_mem_write32((loc_0 + 4u * 0x8Eu), (uint32_t)(((uint32_t)(acib_mem_read_s32((loc_0 + 4u * 0x8Eu))) + 1u)));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x08436c sub_8436C */
int32_t phy_mdm_fn_08436c(void)
{
    return (int32_t)((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BE8u))) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BF4u))) | ((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BF0u))))))) | ((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BD8u))))))) | ((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BDCu))))))) | ((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BE0u))))))) | ((uint32_t)(int32_t)(acib_mem_read_s32(0x10023BE4u))))))))))) != 0u));
}

/* @provenance RVA 0x084860 sub_84860 */
uint32_t phy_mdm_fn_084860(void)
{
    return (uint32_t)(0x10023F19u);
}

/* @provenance RVA 0x0849c8 sub_849C8 */
int8_t phy_mdm_fn_0849c8(int8_t arg_0)
{
    (void)arg_0;
    acib_mem_write8(0x10023EBCu, (uint8_t)(arg_0));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x084b20 sub_84B20 */
int32_t phy_mdm_fn_084b20(void)
{
    return (int32_t)(((uint16_t)(acib_mem_read_s16(0x10023F03u))));
}

/* @provenance RVA 0x084b80 sub_84B80 */
int32_t phy_mdm_fn_084b80(uint32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_0 = 0;
    acib_mem_write16(arg_0, (uint16_t)(acib_mem_read_s16(0x10023F03u)));
    acib_mem_write16(arg_1, (uint16_t)(acib_mem_read_s16(0x10023F07u)));
    loc_0 = (int32_t)(((uint16_t)(acib_mem_read_s16(0x10023F09u))));
    acib_mem_write16(arg_2, (uint16_t)(acib_mem_read_s16(0x10023F09u)));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x084bb6 sub_84BB6 */
int32_t phy_mdm_fn_084bb6(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint16_t)(((int16_t)0u)));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(((int32_t)0x8C00u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))), (uint32_t)(((int32_t)0u)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x084bd4 sub_84BD4 */
uint32_t phy_mdm_fn_084bd4(void)
{
    acib_mem_write16(0x10001AFDu, (uint16_t)(((int16_t)0xFFFFFFFFu)));
    acib_mem_write8(0x10001AFCu, (uint8_t)(((int8_t)0xFFFFFFFFu)));
    return (uint32_t)(0x10001AFCu);
}

/* @provenance RVA 0x084be0 sub_84BE0 */
int32_t phy_mdm_fn_084be0(void)
{
    return (int32_t)((((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001AFCu))))) != 0xFFu));
}

/* @provenance RVA 0x084c5c sub_84C5C */
uint32_t phy_mdm_fn_084c5c(void)
{
    uint32_t loc_0 = 0;
    acib_mem_write8(0x10001B56u, (uint8_t)(((int8_t)8u)));
    acib_mem_write8(0x10001B2Du, (uint8_t)(((int8_t)0u)));
    loc_0 = (uint32_t)(acib_mmio_read32(0x52160020u));
    acib_mem_write32(0x10001B58u, (uint32_t)(loc_0));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x084c84 sub_84C84 */
int32_t phy_mdm_fn_084c84(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10001AFCu))));
    if (((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001AFCu))))) != 0xFFu) {
        loc_1 = (int32_t)(phy_ipc_cmd_op9());
        if (((uint32_t)(int32_t)(loc_1)) != 0u) {
            (void)phy_ipc_cmd_op10(acib_addr_ptr(0x10001B2Du));
            acib_mem_write8(0x10001B56u, (uint8_t)(arg_0));
            return (int32_t)(((int32_t)(0x10001B56u)));
        }
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x084cb0 sub_84CB0 */
int32_t phy_mdm_fn_084cb0(int32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_1 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)));
    loc_2 = (int32_t)(false);
    if ((loc_1 <= 6u) && (((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001AFCu))))) != 0xFFu)) {
        loc_3 = (int32_t)(acib_mem_read_s8((0x10005768u + ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - 1u)))))));
        loc_4 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10009A41u))));
        if (loc_3 >= 0) {
            loc_4 = (int32_t)(((uint8_t)(loc_3)));
        }
        return (int32_t)((((uint32_t)(int32_t)(loc_4)) == ((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001AFCu)))))));
    }
    return (int32_t)(loc_2);
}

/* @provenance RVA 0x084cec sub_84CEC */
uint32_t phy_mdm_fn_084cec(int8_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_1 = 0;
    loc_1 = (uint32_t)(((uint32_t)(phy_mdm_fn_084cb0((int32_t)(arg_1)))));
    if (loc_1 != 0u) {
        acib_mem_write8(0x10001B03u, (uint8_t)(arg_0));
        acib_mem_write32(0x10001AFFu, (uint32_t)(acib_mem_read_s32(0x10001B58u)));
        return (uint32_t)(0x10001AFCu);
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x084d10 sub_84D10 */
uint32_t phy_mdm_fn_084d10(void)
{
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((uint32_t)(phy_mdm_fn_084cb0((int32_t)(((uint8_t)(acib_mem_read_s8(0x10001B56u))))))));
    if (loc_0 != 0u) {
        loc_0 = (uint32_t)(((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001B2Du))))));
        if (((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(((uint8_t)(acib_mem_read_s8(0x10001B2Du))))), 0x1Fu)))) != 0u) {
            acib_mem_write8(0x10001B08u, (uint8_t)(acib_mem_read_s8(0x10001B2Du)));
            acib_mem_write32(0x10001B04u, (uint32_t)(acib_mem_read_s32(0x10001B58u)));
            acib_mem_write32(0x10001B09u, (uint32_t)(acib_mem_read_s32(0x10001B32u)));
            (void)memcpy_wrapper(acib_addr_ptr(0x10001B0Du), acib_addr_ptr(0x10001B36u), 0x10u);
            return (uint32_t)((memcpy_wrapper(acib_addr_ptr(0x10001B1Du), acib_addr_ptr(0x10001B46u), 0x10u), 0x10001B1Du));
        }
    }
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x084d88 sub_84D88 */
uint32_t phy_mdm_fn_084d88(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(arg_0);
    if (((uint32_t)(int32_t)(arg_0)) != 2u) {
        loc_0 = (int32_t)(((int32_t)0u));
    }
    loc_1 = (uint32_t)(((uint32_t)((((uint32_t)(0x5220B000u)) + ((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(arg_1))))))))));
    osa_core_fn_06e198((uint32_t)((loc_1 + (4u * 0x5Au))), (int32_t)(loc_0), (int8_t)(((int32_t)0x1Cu)));
    return (uint32_t)(osa_core_fn_06e198((uint32_t)((loc_1 + (4u * 0x5Cu))), (int32_t)(loc_0), (int8_t)(((int32_t)0x1Cu))));
}

/* @provenance RVA 0x084dbc sub_84DBC */
uint32_t phy_mdm_fn_084dbc(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(arg_0);
    if (((uint32_t)(int32_t)(arg_0)) != 2u) {
        loc_0 = (int32_t)(((int32_t)0u));
    }
    return (uint32_t)(osa_core_fn_06e198((uint32_t)((((uint32_t)(0x5220B000u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(arg_1)))))) + 0x5Bu))))))), (int32_t)(loc_0), (int8_t)(((int32_t)0x1Cu))));
}

/* @provenance RVA 0x084ddc sub_84DDC */
uint32_t phy_mdm_fn_084ddc(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    phy_mdm_fn_084d88((int32_t)(arg_0), (int32_t)(arg_1));
    return (uint32_t)(phy_mdm_fn_084dbc((int32_t)(arg_0), (int32_t)(arg_1)));
}

/* @provenance RVA 0x084dfe sub_84DFE */
int32_t phy_mdm_fn_084dfe(void)
{
    return (int32_t)((sub_740A8(0x55800u, 0x80E7u), 0u));
}

/* @provenance RVA 0x084e98 sub_84E98 */
uint32_t phy_mdm_fn_084e98(int32_t arg_0, int16_t arg_1, int8_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_3 = 0;
    loc_3 = (uint32_t)((memset(acib_addr_ptr(((uint32_t)(arg_0))), 0, 8u), ((uint32_t)(arg_0))));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 5u)))), (uint8_t)(arg_2));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u)))), (uint16_t)(arg_1));
    return (uint32_t)(loc_3);
}

/* @provenance RVA 0x085008 sub_85008 */
uint32_t phy_mdm_fn_085008(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - ((uint32_t)(int32_t)(arg_1)))))) & 0x7FFFFFFFu)));
    if (loc_0 > 0x40000000u) {
        return (uint32_t)((0x80000000u - loc_0));
    }
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x08501c sub_8501C */
int32_t phy_mdm_fn_08501c(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) - ((uint32_t)(int32_t)(arg_0)))))) & 0x7FFFFFFFu) > 0x40000000u));
}

/* @provenance RVA 0x085030 sub_85030 */
int32_t phy_mdm_fn_085030(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) - ((uint32_t)(int32_t)(arg_1)))))) & 0x7FFFFFFFu) <= 0x40000000u));
}

/* @provenance RVA 0x085044 sub_85044 */
int32_t phy_mdm_fn_085044(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    if (arg_1 != 0u) {
        return (int32_t)(((uint32_t)acib_arm_udiv32(arg_0, arg_1)));
    }
    sub_740A8(0x56001u, 0x83FDu);
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0851ca sub_851CA */
int32_t phy_mdm_fn_0851ca(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)((sub_740FE(0x56000u, 0x83FDu, ((uint32_t)(arg_0))), 0u));
}

/* @provenance RVA 0x0851d8 sub_851D8 */
uint32_t phy_mdm_fn_0851d8(void)
{
    acib_mem_write32(0x10023F2Cu, (uint32_t)(((int32_t)0u)));
    acib_mem_write8(0x10023F30u, (uint8_t)(((int8_t)0u)));
    acib_mem_write32(0x10023F20u, (uint32_t)(((int32_t)(0x5Eu * ((uint32_t)(int32_t)(acib_mem_read_s32(0x10020978u)))))));
    return (uint32_t)(0x10023F20u);
}

/* @provenance RVA 0x085990 sub_85990 */
int32_t phy_mdm_fn_085990(void)
{
    return (int32_t)((sub_740A8(0x5700Eu, 0x8406u), 0u));
}

/* @provenance RVA 0x085b2e sub_85B2E */
int32_t phy_mdm_fn_085b2e(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)((sub_74270(0x1057802u, ((uint32_t)(arg_1)), ((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x1Eu)))))))), 0u));
}

/* @provenance RVA 0x085c5c sub_85C5C */
int32_t phy_mdm_fn_085c5c(void)
{
    return (int32_t)((sub_740A8(0x57808u, 0x8406u), 0u));
}

/* @provenance RVA 0x085dd0 sub_85DD0 */
uint32_t phy_mdm_fn_085dd0(uint32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write16(arg_1, (uint16_t)(((int16_t)0x7D0u)));
    acib_mem_write32(arg_0, (uint32_t)(0x100248D6u));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x085ea0 sub_85EA0 */
int32_t phy_mdm_fn_085ea0(uint32_t arg_0, uint32_t arg_1, int32_t arg_2, uint32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    int32_t loc_2 = 0;
    if (((uint32_t)(acib_mem_read8(arg_0))) != 0u) {
        sub_74240(0x1058801u);
        sub_6DD70(0u);
        (void)memcpy_wrapper(acib_addr_ptr(0x10023F45u), acib_addr_ptr(arg_0), 0x15u);
        sub_6DDB8();
        loc_2 = (int32_t)(((uint8_t)(acib_mem_read_s8(0x10023F45u))));
    } else {
        sub_74240(0x1058802u);
        loc_2 = (int32_t)(((int32_t)0u));
        acib_mem_write8(0x10023F45u, (uint8_t)(((int8_t)0u)));
    }
    acib_mem_write8(arg_1, (uint8_t)(loc_2));
    return (int32_t)(loc_2);
}
