/*
 * src/phy/phy_rf_ctrl.c — W3 RF transceiver register accessors (0x52610000 / 0x52614000 / 0x5261C000) and control.
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

/* @provenance RVA 0x09500c sub_9500C */
int32_t phy_rf_fn_09500c(void)
{
    sub_740A8(0x87812u, 0x83F4u);
    return (int32_t)(((int32_t)0xAu));
}

/* @provenance RVA 0x09518a sub_9518A */
int32_t phy_rf_fn_09518a(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    return (int32_t)((sub_740FE(((uint32_t)(arg_0)), 0x83D2u, ((uint32_t)(arg_2))), 0u));
}

/* @provenance RVA 0x0951a4 sub_951A4 */
int32_t phy_rf_fn_0951a4(int32_t arg_0, int8_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 5u)))), (uint8_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) | 0x80u))));
    return (int32_t)(((int32_t)0xAu));
}

/* @provenance RVA 0x095238 sub_95238 */
uint32_t phy_rf_fn_095238(void)
{
    acib_mmio_write32(0x52620034u, (uint32_t)(((int32_t)1u)));
    return (uint32_t)(0x52620000u);
}

/* @provenance RVA 0x095248 sub_95248 */
uint32_t phy_rf_fn_095248(void)
{
    acib_mmio_write32(0x52624034u, (uint32_t)(((int32_t)1u)));
    return (uint32_t)(0x52624000u);
}

/* @provenance RVA 0x095278 sub_95278 */
uint32_t phy_rf_fn_095278(void)
{
    acib_mmio_write32(0x52628034u, (uint32_t)(((int32_t)1u)));
    return (uint32_t)(0x52628000u);
}

/* @provenance RVA 0x095288 sub_95288 */
uint32_t phy_rf_fn_095288(void)
{
    acib_mmio_write32(0x5262C034u, (uint32_t)(((int32_t)1u)));
    return (uint32_t)(0x5262C000u);
}

/* @provenance RVA 0x095298 sub_95298 */
int32_t phy_rf_fn_095298(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    if (((uint32_t)(int32_t)(arg_0)) != 0u) {
        loc_0 = (int32_t)(acib_mmio_read32(0x52618048u));
        acib_mmio_write32(0x52618048u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 0x7FFFu))));
    }
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0952b0 sub_952B0 */
int32_t phy_rf_fn_0952b0(void)
{
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261800Cu));
    loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 1u)));
    acib_mmio_write32(0x5261800Cu, (uint32_t)(loc_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0952dc sub_952DC */
int32_t phy_rf_fn_0952dc(void)
{
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261800Cu));
    loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 2u)));
    acib_mmio_write32(0x5261800Cu, (uint32_t)(loc_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0954f8 sub_954F8 */
int32_t phy_rf_fn_0954f8(int32_t arg_0)
{
    (void)arg_0;
    int16_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int16_t)(acib_mmio_read32(0x52618014u));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & 0x3FFu))));
    loc_1 = (uint32_t)(acib_mmio_read32(0x52618014u));
    acib_mem_write32(((uint32_t)(arg_0)), (uint32_t)(acib_arm_lsr32(((uint32_t)(loc_1)), 0xAu)));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x095626 sub_95626 */
int32_t phy_rf_fn_095626(void)
{
    sub_740A8(0x8900Bu, 0x8361u);
    return (int32_t)(((int32_t)1u));
}

/* @provenance RVA 0x095650 sub_95650 */
uint32_t phy_rf_fn_095650(void)
{
    acib_mmio_write32(0x52618038u, (uint32_t)(((int32_t)0x7Fu)));
    return (uint32_t)(0x52618000u);
}

/* @provenance RVA 0x095660 sub_95660 */
int32_t phy_rf_fn_095660(uint32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    if (arg_0 < 0xFu) {
        acib_mmio_write32((((uint32_t)(0x52634000u)) + (4u * (arg_0 + 0xF0u))), (uint32_t)(arg_1));
        for (loc_1 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_1)) != 0x10u; loc_1 = (int32_t)((uint32_t)loc_1 + 1u)) {
            acib_mmio_write32((((uint32_t)(0x52634000u)) + (4u * ((0x10u * arg_0) + ((uint32_t)(int32_t)(loc_1))))), (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_1)))))))))))));
        }
        loc_2 = (int32_t)(((int32_t)(acib_mmio_read32(0x52634434u) | 1u)));
        acib_mmio_write32(0x52634434u, (uint32_t)(loc_2));
        return (int32_t)(((int32_t)0u));
    } else {
        sub_740FE(0x89800u, 0x8356u, arg_0);
        return (int32_t)(((int32_t)0x11u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x0956c4 sub_956C4 */
uint32_t phy_rf_fn_0956c4(void)
{
    acib_mmio_write32(0x52634428u, (uint32_t)(((int32_t)3u)));
    return (uint32_t)(0x52634000u);
}

/* @provenance RVA 0x0956d8 sub_956D8 */
int32_t phy_rf_fn_0956d8(void)
{
    return (int32_t)(acib_mmio_read32(0x5263441Cu));
}

/* @provenance RVA 0x09580c sub_9580C */
uint32_t phy_rf_fn_09580c(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    acib_mem_write32(arg_0, (uint32_t)(((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_4)), 0x19u)))))))) | acib_arm_lsl32((((uint32_t)(int32_t)(arg_4)) & 0xFFFFFF00u), 0x15u)) + 1u)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(arg_3));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x09582c sub_9582C */
uint32_t phy_rf_fn_09582c(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write32(arg_0, (uint32_t)(((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_3)), 0x19u)))))))) | acib_arm_lsl32((((uint32_t)(int32_t)(arg_3)) & 0xFFFFFF00u), 0x15u)) + 1u)));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x095844 sub_95844 */
uint32_t phy_rf_fn_095844(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write32(arg_0, (uint32_t)((((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_3)), 0x19u)))))))) | acib_arm_lsl32((((uint32_t)(int32_t)(arg_3)) & 0xFFFFFF00u), 0x13u)) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_3)) & 0xF0000u)))), 0xCu))))) + 1u)));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x095868 sub_95868 */
uint32_t phy_rf_fn_095868(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_3)), 0x19u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_3)) & 0x3F0000u)))), 0xAu)))))))) + 1u))));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(((uint8_t)((((uint32_t)(arg_3)) >> 8) & 0xFFu))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x095918 sub_95918 */
uint32_t phy_rf_fn_095918(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, uint32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    acib_mem_write32(arg_0, (uint32_t)(((((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | acib_arm_lsl32(arg_4, 0x19u)) | acib_arm_lsl32((arg_4 & 0x70000u), 0xDu)) | acib_arm_lsl32((arg_4 & 0xFFFFFF00u), 0x16u)) | 0x10000001u)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_3)))))) & 0x3FEu)))) | ((uint32_t)(((uint8_t)((arg_4 >> 24) & 0xFFu))))))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x095954 sub_95954 */
uint32_t phy_rf_fn_095954(uint32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_4)), 0x19u)))))))) + 1u))));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(arg_3));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x09596c sub_9596C */
uint32_t phy_rf_fn_09596c(uint32_t arg_0, int32_t arg_1, int32_t arg_2, uint32_t arg_3, uint32_t arg_4, uint32_t arg_5)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    (void)arg_5;
    acib_mem_write32(arg_0, (uint32_t)((((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_2)))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))))) | acib_arm_lsl32(arg_3, 0x19u)) | (4u * (arg_3 & 0x3F000000u))) + 1u)));
    acib_mem_write32((arg_0 + 4u * 1u), (uint32_t)(((((((((acib_arm_lsl32(arg_4, 0x19u) | (2u * (arg_5 & 7u))) | ((uint32_t)(((uint8_t)((arg_4 >> 8) & 0xFFu))))) | (acib_arm_lsr32(((uint32_t)(arg_4)), 0xCu) & 0xFF0u)) | (acib_arm_lsr32(((uint32_t)(arg_5)), 0xBu) & 0x3FE0u)) + (acib_arm_lsr32(((uint32_t)(arg_4)), 0xAu) & 0x3FC000u)) | (acib_arm_lsr32(((uint32_t)(arg_3)), 1u) & 0x1F8000u)) | acib_arm_lsl32((arg_5 & 0x700u), 0xDu)) | acib_arm_lsl32((arg_3 & 0xFFFFFF00u), 0x10u))));
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x095c14 sub_95C14 */
int32_t phy_rf_fn_095c14(void)
{
    acib_mmio_write32(0x5261C05Cu, (uint32_t)(((int32_t)0x1FFFFu)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x095c2c sub_95C2C */
int32_t phy_rf_fn_095c2c(void)
{
    acib_mmio_write32(0x5261C060u, (uint32_t)(((int32_t)0x1FFFFu)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x095c44 sub_95C44 */
int32_t phy_rf_fn_095c44(uint32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(0x100278F4u, (uint32_t)(arg_0));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x095d6c sub_95D6C */
uint32_t phy_rf_fn_095d6c(void)
{
    uint32_t loc_1 = 0;
    loc_1 = (uint32_t)(acib_mmio_read32(0x5261C000u));
    acib_mmio_write32(0x5261C000u, (uint32_t)((loc_1 | 1u)));
    return (uint32_t)(0x5261C000u);
}

/* @provenance RVA 0x095d80 sub_95D80 */
uint32_t phy_rf_fn_095d80(void)
{
    uint32_t loc_1 = 0;
    loc_1 = (uint32_t)(acib_mmio_read32(0x5261C000u));
    acib_mmio_write32(0x5261C000u, (uint32_t)((loc_1 | 1u)));
    return (uint32_t)(0x5261C000u);
}

/* @provenance RVA 0x095d94 sub_95D94 */
uint32_t phy_rf_fn_095d94(void)
{
    uint32_t loc_1 = 0;
    loc_1 = (uint32_t)(acib_mmio_read32(0x5261C000u));
    acib_mmio_write32(0x5261C000u, (uint32_t)((loc_1 & 0xFFFFFFFEu)));
    return (uint32_t)(0x5261C000u);
}

/* @provenance RVA 0x095da8 sub_95DA8 */
uint32_t phy_rf_fn_095da8(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261C00Cu));
    loc_1 = (uint32_t)((((acib_rev16_u32(((uint32_t)(acib_mem_read16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 3u))))))), 0x10u))))) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 2u))))))) & 0xFu)))), 0x18u))))) + (((uint32_t)(int32_t)(loc_0)) & 0xF0000000u)));
    acib_mmio_write32(0x5261C00Cu, (uint32_t)(loc_1));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x095dd4 sub_95DD4 */
int32_t phy_rf_fn_095dd4(int32_t arg_0)
{
    (void)arg_0;
    acib_mmio_write32(0x5261C010u, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x095de4 sub_95DE4 */
int32_t phy_rf_fn_095de4(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3, int32_t arg_4)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    (void)arg_4;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_1)), 0x10u)))))));
    acib_mmio_write32(0x5261C01Cu, (uint32_t)(loc_0));
    acib_mmio_write32(0x5261C020u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(arg_3)), 0x10u))))))));
    acib_mmio_write32(0x5261C024u, (uint32_t)(arg_4));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x095e04 sub_95E04 */
int8_t phy_rf_fn_095e04(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261C014u));
    acib_mmio_write32(0x5261C014u, (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFFFFF80u) | ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x7Fu)))))));
    return (int8_t)(arg_0);
}

/* @provenance RVA 0x095e18 sub_95E18 */
uint32_t phy_rf_fn_095e18(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261C014u));
    loc_1 = (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFF80FFFFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x7Fu)))), 0x10u))))));
    acib_mmio_write32(0x5261C014u, (uint32_t)(loc_1));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x095e34 sub_95E34 */
uint32_t phy_rf_fn_095e34(int8_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261C014u));
    loc_1 = (uint32_t)(((((uint32_t)(int32_t)(loc_0)) & 0xFFFF80FFu) | ((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x7Fu)))), 8u))))));
    acib_mmio_write32(0x5261C014u, (uint32_t)(loc_1));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x095e50 sub_95E50 */
uint32_t phy_rf_fn_095e50(int8_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (uint32_t)(acib_mmio_read32(0x5261C000u));
    loc_1 = (uint32_t)(((loc_0 & 0xFFFFFFFDu) | ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 1u))))))))));
    acib_mmio_write32(0x5261C000u, (uint32_t)(loc_1));
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x095eb8 sub_95EB8 */
uint32_t phy_rf_fn_095eb8(void)
{
    acib_mmio_write32(0x5261C004u, (uint32_t)(((int32_t)1u)));
    return (uint32_t)(0x5261C000u);
}

/* @provenance RVA 0x095ec8 sub_95EC8 */
int32_t phy_rf_fn_095ec8(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261C004u));
    return (int32_t)((((uint32_t)(int32_t)(loc_0)) != 0u));
}

/* @provenance RVA 0x095f40 sub_95F40 */
uint32_t phy_rf_fn_095f40(void)
{
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    loc_0 = (int32_t)(((int32_t)0u));
    loc_1 = (uint32_t)(0x100278FCu);
    while (((uint32_t)(int32_t)(loc_0)) != 2u) {
        loc_2 = (uint32_t)((0x5261C000u + (4u * ((uint32_t)(int32_t)(loc_0)))));
        acib_mem_write32(loc_1, (uint32_t)(((int32_t)0u)));
        acib_mem_write32((loc_1 - (4u * 1u)), (uint32_t)(((int32_t)0u)));
        loc_1 = (uint32_t)((uint32_t)loc_1 + (4u * 2u));
        loc_0 = (int32_t)((uint32_t)loc_0 + 1u);
        acib_mem_write32((((uint32_t)(loc_2)) + (4u * 0x13u)), (uint32_t)(((int32_t)0u)));
        acib_mem_write32((((uint32_t)(loc_2)) + (4u * 0x11u)), (uint32_t)(((uint32_t)(acib_mem_read32((((uint32_t)(loc_2)) + (4u * 0x11u)))) | 1u)));
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x095f78 sub_95F78 */
int32_t phy_rf_fn_095f78(int32_t arg_0, uint32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x100278F8u + (4u * ((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_0)))))))));
    acib_mem_write32(loc_0, (uint32_t)(arg_1));
    acib_mem_write32((loc_0 + 4u * 1u), (uint32_t)(arg_2));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x095f8c sub_95F8C */
uint32_t phy_rf_fn_095f8c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    if (arg_0 > 1u) {
        return (uint32_t)(((uint32_t)(phy_rf_fn_095fa6())));
    }
    loc_0 = (uint32_t)((0x5261C000u + (4u * arg_0)));
    acib_mem_write32((((uint32_t)(loc_0)) + (4u * 0x13u)), (uint32_t)(((uint32_t)(acib_mem_read32((((uint32_t)(loc_0)) + (4u * 0x13u)))) | ((uint32_t)(arg_1)))));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x095fa6 sub_95FA6 */
int32_t phy_rf_fn_095fa6(void)
{
    return (int32_t)((sub_740A8(0x8C001u, 0x8365u), 0u));
}

/* @provenance RVA 0x095fbc sub_95FBC */
int32_t phy_rf_fn_095fbc(int32_t arg_0)
{
    uint32_t fifo_base = acib_mem_read32(0x000F2A5Cu);
    uint32_t idx = (uint32_t)arg_0;
    if ((acib_mmio_read32(fifo_base + 0x34u + 4u * idx) & 0x3Fu) != 0u) {
        do {
            (void)acib_mmio_read32(fifo_base + 0x2Cu + 4u * idx);
        } while ((acib_mmio_read32(fifo_base + 0x34u + 4u * idx) & 0x3Fu) != 0u);
    }
    sub_6DF3C((idx == 0u) ? 0x60u : 0x61u);
    return 0;
}

/* @provenance RVA 0x096124 sub_96124 */
uint32_t phy_rf_fn_096124(void)
{
    acib_mmio_write32(0x52640024u, (uint32_t)(((int32_t)3u)));
    return (uint32_t)(0x52640000u);
}

/* @provenance RVA 0x096134 sub_96134 */
int32_t phy_rf_fn_096134(void)
{
    return (int32_t)(acib_mem_read_s32(0x10027908u));
}

/* @provenance RVA 0x096140 sub_96140 */
int32_t phy_rf_fn_096140(int32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(0x10027908u, (uint32_t)(((uint32_t)(acib_mem_read_s32(0x10027908u)) | ((uint32_t)(arg_0)))));
    acib_mmio_write32(0x5263006Cu, (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x09616c sub_9616C */
uint32_t phy_rf_fn_09616c(void)
{
    acib_mmio_write32(0x52630070u, (uint32_t)(((int32_t)0xFFu)));
    return (uint32_t)(0x52630000u);
}

/* @provenance RVA 0x0961bc sub_961BC */
uint32_t phy_rf_fn_0961bc(uint32_t arg_0)
{
    (void)arg_0;
    uint32_t loc_0 = 0;
    for (; arg_0 != 0u; acib_mmio_write32((0x5261C074u + 4u * loc_0), (uint32_t)(((int32_t)0u)))) {
        loc_0 = (uint32_t)(acib_clz32(acib_rbit32(arg_0)));
        arg_0 = (uint32_t)((uint32_t)arg_0 & ((uint32_t)((arg_0 - 1u))));
    }
    return (uint32_t)(arg_0);
}

/* @provenance RVA 0x096274 sub_96274 */
uint32_t phy_rf_fn_096274(void)
{
    acib_mmio_write32(0x52644038u, (uint32_t)(((int32_t)0xFu)));
    return (uint32_t)(0x52644000u);
}

/* @provenance RVA 0x096294 sub_96294 */
int32_t phy_rf_fn_096294(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    sub_740FE(0x8E801u, 0x8395u, ((uint32_t)(arg_2)));
    return (int32_t)(((int32_t)0xEu));
}

/* @provenance RVA 0x096330 sub_96330 */
uint32_t phy_rf_fn_096330(void)
{
    acib_mmio_write32(0x52648020u, (uint32_t)(((int32_t)7u)));
    return (uint32_t)(0x52648000u);
}

/* @provenance RVA 0x096444 sub_96444 */
uint32_t phy_rf_fn_096444(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    loc_0 = (uint32_t)((0x52610040u + (4u * ((uint32_t)(int32_t)(arg_0)))));
    acib_mem_write32((((uint32_t)(loc_0)) + (4u * 0xCu)), (uint32_t)(arg_1));
    return (uint32_t)(loc_0);
}

/* @provenance RVA 0x096484 sub_96484 */
int32_t phy_rf_fn_096484(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint16_t)(acib_mmio_read32((((uint32_t)(0x52610040u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu))))))))));
}

/* @provenance RVA 0x0964c0 sub_964C0 */
int32_t phy_rf_fn_0964c0(uint32_t arg_0)
{
    (void)arg_0;
    bool loc_0 = false;
    int8_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    loc_0 = (((arg_0 >= 0x20u)) != 0);
    loc_1 = (int8_t)((arg_0 - 0x20u));
    if (arg_0 < 0x20u) {
        loc_1 = (int8_t)(arg_0);
    }
    loc_2 = (int32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(loc_1)))));
    loc_3 = (int32_t)(((int32_t)0x418u));
    loc_4 = (int32_t)(((int32_t)0u));
    if (!(loc_0 != 0)) {
        loc_3 = (int32_t)(((int32_t)0x414u));
    }
    acib_mmio_write32(((uint32_t)((((uint32_t)(0x52614000u)) + ((uint32_t)(int32_t)(loc_3))))), (uint32_t)(loc_2));
    return (int32_t)(loc_4);
}

/* @provenance RVA 0x0964ec sub_964EC */
uint32_t phy_rf_fn_0964ec(void)
{
    acib_mmio_write32(0x5261445Cu, (uint32_t)(((int32_t)7u)));
    return (uint32_t)(0x52614000u);
}

/* @provenance RVA 0x096510 sub_96510 */
uint32_t phy_rf_fn_096510(void)
{
    acib_mmio_write32(0x52614458u, (uint32_t)(((int32_t)7u)));
    return (uint32_t)(0x52614000u);
}

/* @provenance RVA 0x09655e sub_9655E */
int32_t phy_rf_fn_09655e(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    sub_740FE(0x8F800u, 0x835Bu, ((uint32_t)(arg_3)));
    return (int32_t)(((int32_t)0x11u));
}

/* @provenance RVA 0x09659a sub_9659A */
int32_t phy_rf_fn_09659a(int32_t arg_0, int32_t arg_1, int32_t arg_2, int32_t arg_3)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    (void)arg_3;
    sub_740FE(0x8F801u, 0x835Bu, ((uint32_t)(arg_3)));
    return (int32_t)(((int32_t)0x11u));
}

/* @provenance RVA 0x0965f0 sub_965F0 */
uint32_t phy_rf_fn_0965f0(void)
{
    acib_mmio_write32(0x5264C26Cu, (uint32_t)(((int32_t)7u)));
    return (uint32_t)(0x5264C000u);
}

/* @provenance RVA 0x096604 sub_96604 */
uint32_t phy_rf_fn_096604(void)
{
    acib_mmio_write32(0x5265026Cu, (uint32_t)(((int32_t)7u)));
    return (uint32_t)(0x52650000u);
}

/* @provenance RVA 0x096618 sub_96618 */
int32_t phy_rf_fn_096618(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(((int32_t)0u));
    switch (arg_0) {
    case 0: {
        loc_1 = (int32_t)(((int32_t)acib_mmio_read32(0x52634440u)));
        break;
    }
    case 1: {
        loc_1 = (int32_t)(((int32_t)acib_mmio_read32(0x52648038u)));
        break;
    }
    case 2: {
        return (int32_t)(loc_1);
    }
    case 3: {
        loc_1 = (int32_t)(((int32_t)acib_mmio_read32(0x5264402Cu)));
        break;
    }
    case 4: {
        loc_1 = (int32_t)(((int32_t)acib_mmio_read32(0x5263C03Cu)));
        break;
    }
    default: {
        sub_740A8(0x90000u, 0x8364u);
        loc_1 = (int32_t)(((int32_t)0u));
        break;
    }
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x09667c sub_9667C */
uint32_t phy_rf_fn_09667c(void)
{
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_4 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x52634434u));
    acib_mmio_write32(0x52634434u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 1u))));
    loc_1 = (int32_t)(acib_mmio_read32(0x5264802Cu));
    acib_mmio_write32(0x5264802Cu, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) | 1u))));
    loc_2 = (int32_t)(acib_mmio_read32(0x52644020u));
    acib_mmio_write32(0x52644020u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | 1u))));
    loc_4 = (int32_t)(acib_mmio_read32(0x5263C030u));
    acib_mmio_write32(0x5263C030u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) | 1u))));
    return (uint32_t)(0x5263C000u);
}

/* @provenance RVA 0x0966b4 sub_966B4 */
uint32_t phy_rf_fn_0966b4(void)
{
    int32_t loc_0 = 0;
    int32_t loc_1 = 0;
    int32_t loc_2 = 0;
    int32_t loc_4 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x52634438u));
    acib_mmio_write32(0x52634438u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) | 1u))));
    loc_1 = (int32_t)(acib_mmio_read32(0x52648030u));
    acib_mmio_write32(0x52648030u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_1)) | 1u))));
    loc_2 = (int32_t)(acib_mmio_read32(0x52644024u));
    acib_mmio_write32(0x52644024u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | 1u))));
    loc_4 = (int32_t)(acib_mmio_read32(0x5263C034u));
    acib_mmio_write32(0x5263C034u, (uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_4)) | 1u))));
    return (uint32_t)(0x5263C000u);
}

/* @provenance RVA 0x0966ec sub_966EC */
int32_t phy_rf_fn_0966ec(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_1 = 0;
    int8_t loc_2 = 0;
    loc_1 = (int32_t)(((int32_t)0u));
    switch (arg_0) {
    case 0: {
        loc_1 = (int32_t)(((uint32_t)loc_1 & ~0xFFu) | (((uint32_t)(((int32_t)acib_mmio_read32(0x5263443Cu)))) & 0xFFu));
        loc_1 = (int32_t)(((uint8_t)(loc_1)));
        break;
    }
    case 1: {
        loc_2 = (int8_t)(((int32_t)acib_mmio_read32(0x52648034u)));
        loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) & 0xFu)));
        break;
    }
    case 2: {
        return (int32_t)(loc_1);
    }
    case 3: {
        loc_1 = (int32_t)(((uint32_t)loc_1 & ~0xFFu) | (((uint32_t)(((int32_t)acib_mmio_read32(0x52644028u)))) & 0xFFu));
        loc_1 = (int32_t)(((uint8_t)(loc_1)));
        break;
    }
    case 4: {
        loc_1 = (int32_t)(((uint32_t)loc_1 & ~0xFFu) | (((uint32_t)(((int32_t)acib_mmio_read32(0x5263C038u)))) & 0xFFu));
        loc_1 = (int32_t)(((uint8_t)(loc_1)));
        break;
    }
    default: {
        sub_740A8(0x90004u, 0x8364u);
        loc_1 = (int32_t)(((int32_t)0u));
        break;
    }
    }
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x096954 sub_96954 */
int32_t phy_rf_fn_096954(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(arg_0)))))) + 0x520D4000u)));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) + 0x9Cu)))), (uint32_t)((((uint32_t)(int32_t)(arg_1)) | 0x80000000u)));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x09696c sub_9696C */
int32_t phy_rf_fn_09696c(int32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    acib_mmio_write32((((uint32_t)(0x520D4000u)) + (4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0x27u)))))), (uint32_t)(((int32_t)0u)));
    loc_0 = (int32_t)(((int32_t)acib_arm_lsl32(1u, ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xAu)))))));
    acib_mmio_write32(0x520D4068u, (uint32_t)(loc_0));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x096998 sub_96998 */
int32_t phy_rf_fn_096998(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(acib_arm_lsr32(((uint32_t)(acib_mmio_read32((0x520D409Cu + 4u * ((uint32_t)(int32_t)(arg_0)))))), 0x1Fu));
}

/* @provenance RVA 0x0969ac sub_969AC */
int32_t phy_rf_fn_0969ac(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((int32_t)(acib_mmio_read32((0x520D409Cu + 4u * ((uint32_t)(int32_t)(arg_0)))) & 0x7FFFFFFFu)));
}

/* @provenance RVA 0x0969c4 sub_969C4 */
int32_t phy_rf_fn_0969c4(int32_t arg_0)
{
    (void)arg_0;
    bool loc_0 = false;
    int64_t loc_1 = 0;
    loc_0 = (((((uint32_t)(int32_t)(arg_0)) == 0u)) != 0);
    loc_1 = (int64_t)(((int64_t)0x100000000ULL));
    if ((loc_0) != 0) {
        loc_1 = (int64_t)(((uint64_t)loc_1 & 0xFFFFFFFFULL) | (((uint64_t)((uint32_t)(((int32_t)3u)))) << 32));
    }
    acib_mmio_write32(0x52618018u, (uint32_t)(((uint32_t)((((uint64_t)(loc_1)) >> 32) & 0xFFFFFFFFULL))));
    return (int32_t)(loc_1);
}

/* @provenance RVA 0x0969e0 sub_969E0 */
int32_t phy_rf_fn_0969e0(void)
{
    int32_t loc_0 = 0;
    uint32_t loc_1 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x52618018u));
    loc_1 = (uint32_t)((((uint32_t)(int32_t)(loc_0)) & 0xFFFFFFFEu));
    acib_mmio_write32(0x52618018u, (uint32_t)(loc_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0969f8 sub_969F8 */
int32_t phy_rf_fn_0969f8(void)
{
    int32_t loc_0 = 0;
    int32_t loc_2 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x52618018u));
    if (((uint32_t)(int32_t)(((int32_t)acib_arm_lsl32(((uint32_t)(int32_t)(loc_0)), 0x1Fu)))) != 0u) {
        return (int32_t)(phy_rf_fn_096a14());
    }
    loc_2 = (int32_t)(((int32_t)(acib_mmio_read32(0x5261801Cu) | 1u)));
    acib_mmio_write32(0x5261801Cu, (uint32_t)(loc_2));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x096a14 sub_96A14 */
int32_t phy_rf_fn_096a14(void)
{
    sub_740A8(0x91000u, 0x8363u);
    return (int32_t)(((int32_t)8u));
}

/* @provenance RVA 0x096a28 sub_96A28 */
int32_t phy_rf_fn_096a28(void)
{
    return (int32_t)(acib_mmio_read32(0x52618028u));
}

/* @provenance RVA 0x096a48 sub_96A48 */
int32_t phy_rf_fn_096a48(void)
{
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(acib_mmio_read32(0x5261802Cu));
    return (int32_t)((((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(loc_0)) & 0x10000u)))) != 0u));
}

/* @provenance RVA 0x096a5c sub_96A5C */
int32_t phy_rf_fn_096a5c(int16_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    loc_0 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) & 0x3FFu)))) | 0x10C00u)));
    acib_mmio_write32(0x52618020u, (uint32_t)(loc_0));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x096a8c sub_96A8C */
int32_t phy_rf_fn_096a8c(uint32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint32_t loc_1 = 0;
    int32_t loc_2 = 0;
    if (arg_0 < 7u) {
        acib_mmio_write32((0x52638000u + 4u * (arg_0 + 0x54u)), (uint32_t)((((uint32_t)(int32_t)(arg_1)) + (acib_mmio_read32((0x52638000u + 4u * (arg_0 + 0x54u))) & 0xFFFF0000u))));
        loc_1 = (uint32_t)(((uint32_t)((0x52638000u + 4u * (0xCu * arg_0)))));
        for (loc_2 = (int32_t)(((int32_t)0u)); ((uint32_t)(int32_t)(loc_2)) != 0xCu; loc_2 = (int32_t)((uint32_t)loc_2 + 1u)) {
            acib_mem_write32(((uint32_t)((loc_1 + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_2))))))))), (uint32_t)(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_2)) + ((uint32_t)(int32_t)(((int32_t)(4u * ((uint32_t)(int32_t)(loc_2)))))))))))));
        }
        return (int32_t)(((int32_t)0u));
    } else {
        sub_740FE(0x91800u, 0x835Au, arg_0);
        return (int32_t)(((int32_t)0x11u));
    }
    return (int32_t)0;
}

/* @provenance RVA 0x096ba0 sub_96BA0 */
int32_t phy_rf_fn_096ba0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    acib_mem_write32(0x10027910u, (uint32_t)(arg_0));
    acib_mem_write32(0x10027914u, (uint32_t)(arg_1));
    acib_mem_write16(0x10027938u, (uint16_t)(((int16_t)0u)));
    acib_mem_write32(0x10027918u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x1002791Cu, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10027920u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10027924u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10027928u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x1002792Cu, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10027930u, (uint32_t)(((int32_t)0u)));
    acib_mem_write32(0x10027934u, (uint32_t)(((int32_t)0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x096e18 sub_96E18 */
int32_t phy_rf_fn_096e18(void)
{
    if (((uint32_t)(int32_t)(acib_mem_read_s32((0x10027910u + 4u * 0u)))) != 0u) {
        return (int32_t)(((int32_t)9u));
    }
    phy_rf_fn_0952b0();
    phy_rf_fn_09667c();
    phy_rf_fn_0966b4();
    acib_mem_write32((0x10027910u + 4u * 0u), (uint32_t)(((int32_t)1u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x09717c sub_9717C */
int32_t phy_rf_fn_09717c(int16_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint16_t)(((int32_t)(0xBu * ((uint32_t)(int32_t)(arg_0)))))));
}

/* @provenance RVA 0x09722c sub_9722C */
int32_t phy_rf_fn_09722c(void)
{
    phy_rf_fn_096a8c((uint32_t)(((int32_t)0u)), (int32_t)(((int32_t)0x48u)), (int32_t)(((int32_t)(0x000F7294u))));
    phy_rf_fn_096a8c((uint32_t)(1u), (int32_t)(((int32_t)0u)), (int32_t)(((int32_t)(0x000F72C4u))));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x09725c sub_9725C */
int32_t phy_rf_fn_09725c(int32_t arg_0)
{
    (void)arg_0;
    phy_rf_fn_0969e0();
    phy_rf_fn_0969f8();
    phy_rf_fn_0969c4((int32_t)(arg_0));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x097274 sub_97274 */
int32_t phy_rf_fn_097274(void)
{
    phy_rf_fn_0969e0();
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x097280 sub_97280 */
int32_t phy_rf_fn_097280(int16_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_rf_fn_096a5c((int16_t)(arg_0)));
}

/* @provenance RVA 0x097690 sub_97690 */
int32_t phy_rf_fn_097690(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(((uint16_t)(acib_mem_read_s16((0x10001D8Cu + 2u * ((uint32_t)(int32_t)(arg_0)))))));
}

/* @provenance RVA 0x0976a0 sub_976A0 */
int32_t phy_rf_fn_0976a0(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(acib_mem_read8((0x10001D80u + ((uint32_t)(int32_t)(arg_0)))));
}

/* @provenance RVA 0x0976ac sub_976AC */
int32_t phy_rf_fn_0976ac(int32_t arg_0)
{
    (void)arg_0;
    acib_mmio_write32((0x520E4168u + 4u * 0x22Eu), (uint32_t)(arg_0));
    return (int32_t)(arg_0);
}

/* @provenance RVA 0x0978ec sub_978EC */
int32_t phy_rf_fn_0978ec(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    int32_t loc_1 = 0;
    loc_1 = (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(((int32_t)(acib_mem_read32(((uint32_t)((0x10027958u + 2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 4u)))))))) & 0x3Fu)))))))) | 1u)));
    sub_96458(0x1Bu, ((uint32_t)(loc_1)));
    phy_rf_fn_096444((int32_t)(((int32_t)0x1Bu)), (int32_t)(acib_mem_read8(arg_1)));
    sub_964AC(((uint32_t)(loc_1)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x09796c sub_9796C */
int32_t phy_rf_fn_09796c(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    sub_96458(0x1Eu, ((uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read16((0x10027958u + 2u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 4u)))))))) & 0x3Fu)))))))) | 4u)))));
    acib_mem_write16(0x10027A0Cu, (uint16_t)(acib_mem_read_s16(arg_1)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x097f14 sub_97F14 */
int32_t phy_rf_fn_097f14(void)
{
    return (int32_t)(((int32_t)2u));
}

/* @provenance RVA 0x097f18 sub_97F18 */
int32_t phy_rf_fn_097f18(void)
{
    return (int32_t)(((int32_t)3u));
}

/* @provenance RVA 0x097f1c sub_97F1C */
int32_t phy_rf_fn_097f1c(void)
{
    return (int32_t)(((int32_t)4u));
}

/* @provenance RVA 0x097f20 sub_97F20 */
int32_t phy_rf_fn_097f20(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint8_t loc_0 = 0;
    loc_0 = (uint8_t)(acib_mem_read8((0x10001D44u + ((uint32_t)(int32_t)(arg_1)))));
    if (((uint32_t)(int32_t)(phy_mdm_fn_07b328())) == 1u) {
        loc_0 = (uint8_t)((uint32_t)loc_0 - 1u);
    }
    return (int32_t)(((int32_t)(((uint32_t)(loc_0)) + 2u)));
}

/* @provenance RVA 0x097f40 sub_97F40 */
int32_t phy_rf_fn_097f40(int32_t arg_0, int32_t arg_1, uint32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    uint8_t loc_0 = 0;
    loc_0 = (uint8_t)(acib_mem_read8((0x10001D50u + ((uint32_t)(int32_t)(arg_1)))));
    if (((uint32_t)(int32_t)(phy_mdm_fn_07b328())) == 1u) {
        loc_0 = (uint8_t)((uint32_t)loc_0 - 1u);
    }
    return (int32_t)(((((uint32_t)(loc_0)) + acib_arm_lsr32(((uint32_t)(arg_2)), 1u)) + 2u));
}

/* @provenance RVA 0x097f64 sub_97F64 */
int32_t phy_rf_fn_097f64(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x10001D5Cu + ((uint32_t)(int32_t)(arg_1)))))) + ((uint32_t)(acib_mem_read8((0x10001DA4u + ((uint32_t)(int32_t)(arg_1)))))))));
}

/* @provenance RVA 0x097f7c sub_97F7C */
int32_t phy_rf_fn_097f7c(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(((uint32_t)(acib_mem_read8((0x10001D5Cu + ((uint32_t)(int32_t)(arg_1)))))) - ((uint32_t)(acib_mem_read8((0x10001D74u + ((uint32_t)(int32_t)(arg_1)))))))))) + ((uint32_t)(acib_mem_read8((0x10001DA4u + ((uint32_t)(int32_t)(arg_1)))))))));
}

/* @provenance RVA 0x097fa0 sub_97FA0 */
int32_t phy_rf_fn_097fa0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    return (int32_t)(acib_mem_read8((0x10027948u + ((uint32_t)(int32_t)(arg_1)))));
}

/* @provenance RVA 0x097fac sub_97FAC */
uint32_t phy_rf_fn_097fac(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_1 = 0;
    uint32_t loc_2 = 0;
    uint32_t loc_3 = 0;
    uint32_t loc_4 = 0;
    uint32_t loc_5 = 0;
    loc_1 = (uint32_t)(arg_1);
    switch (arg_0) {
    case 0:
    case 6: {
        loc_1 = (uint32_t)((arg_1 - acib_arm_lsr32(((uint32_t)(arg_1)), 1u)));
        break;
    }
    case 1: {
        loc_4 = (uint32_t)((((uint64_t)((((uint64_t)(0xAAAAAAABu)) * ((uint64_t)(((uint64_t)(arg_1))))))) >> 32));
        loc_5 = (uint32_t)(((uint32_t)acib_arm_udiv32(arg_1, 3u)));
        if (((uint32_t)acib_arm_umod32(loc_1, 3u)) != 0u) {
            loc_5 = (uint32_t)((acib_arm_lsr32(((uint32_t)(loc_4)), 1u) + 1u));
        }
        loc_1 = (uint32_t)(loc_5);
        break;
    }
    case 2:
    case 5: {
        return (uint32_t)(loc_1);
    }
    case 3:
    case 9: {
        loc_3 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_1)), 2u));
        if (acib_arm_lsl32(loc_1, 0x1Eu) != 0u) {
            loc_3 = (uint32_t)((acib_arm_lsr32(((uint32_t)(loc_1)), 2u) + 1u));
        }
        loc_1 = (uint32_t)(loc_3);
        break;
    }
    case 4:
    case 10: {
        loc_2 = (uint32_t)(acib_arm_lsr32(((uint32_t)(arg_1)), 3u));
        if (acib_arm_lsl32(loc_1, 0x1Du) != 0u) {
            loc_2 = (uint32_t)((acib_arm_lsr32(((uint32_t)(loc_1)), 3u) + 1u));
        }
        loc_1 = (uint32_t)(loc_2);
        break;
    }
    case 7: {
        loc_1 = (uint32_t)((2u * arg_1));
        break;
    }
    case 8: {
        loc_1 = (uint32_t)((8u * arg_1));
        break;
    }
    case 11: {
        loc_1 = (uint32_t)((4u * arg_1));
        break;
    }
    default: {
        sub_740FE(0x9600Cu, 0x8375u, ((uint32_t)(arg_0)));
        loc_1 = (uint32_t)(((int32_t)0u));
        break;
    }
    }
    return (uint32_t)(loc_1);
}

/* @provenance RVA 0x098088 sub_98088 */
int32_t phy_rf_fn_098088(uint32_t arg_0)
{
    (void)arg_0;
    int32_t loc_0 = 0;
    acib_mem_write8(0x10027A0Eu, (uint8_t)(((int8_t)1u)));
    loc_0 = (int32_t)(acib_mem_read16(arg_0));
    acib_mem_write16(0x10027A0Fu, (uint16_t)(loc_0));
    return (int32_t)(loc_0);
}

/* @provenance RVA 0x0980dc sub_980DC */
int32_t phy_rf_fn_0980dc(uint32_t arg_0)
{
    (void)arg_0;
    int16_t loc_0 = 0;
    loc_0 = (int16_t)(acib_mmio_read32((0x520E4168u + 4u * 0x22Fu)));
    acib_mem_write8(arg_0, (uint8_t)((((int32_t)(loc_0)) < 0)));
    return (int32_t)(((int8_t)(loc_0)));
}

/* @provenance RVA 0x0980f8 sub_980F8 */
int32_t phy_rf_fn_0980f8(uint32_t arg_0)
{
    (void)arg_0;
    int16_t loc_0 = 0;
    loc_0 = (int16_t)(acib_mmio_read32((0x520E4168u + 4u * 0x22Fu)));
    acib_mem_write8(arg_0, (uint8_t)((((int32_t)(loc_0)) < 0)));
    return (int32_t)(((uint8_t)(loc_0)));
}

/* @provenance RVA 0x098138 sub_98138 */
int32_t phy_rf_fn_098138(uint32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0981b8 sub_981B8 */
int32_t phy_rf_fn_0981b8(uint32_t arg_0)
{
    (void)arg_0;
    acib_mem_write32(arg_0, (uint32_t)(((int32_t)0u)));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0983b0 sub_983B0 */
int32_t phy_rf_fn_0983b0(int32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_2 = 0;
    int32_t loc_3 = 0;
    int32_t loc_4 = 0;
    bool loc_5 = false;
    if (acib_mem_read32((0x10027A98u + 4u * ((uint32_t)(int32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(2u * ((uint32_t)(int32_t)(arg_0)))))) + 2u)))))) == 0u) {
        sub_740A8(0x98002u, 0x8378u);
    }
    phy_rf_fn_09696c((int32_t)(arg_0));
    phy_rf_fn_096954((int32_t)(arg_0), (int32_t)(arg_1));
    loc_2 = (uint32_t)(acib_mmio_read32(0x520D51F8u));
    loc_3 = (int32_t)((loc_2 & 0x7FFFFFFFu));
    loc_4 = (int32_t)(phy_mdm_fn_08501c((int32_t)(arg_1), (int32_t)((loc_2 & 0x7FFFFFFFu))));
    loc_5 = (((!(loc_4 != 0))) != 0);
    if ((loc_4) != 0) {
        loc_5 = (((((uint32_t)(int32_t)(arg_1)) == ((uint32_t)(int32_t)(loc_3)))) != 0);
    }
    if ((!(loc_5 != 0)) || (((uint32_t)(int32_t)(phy_rf_fn_096998((int32_t)(arg_0)))) == 0u)) {
        return (int32_t)(((int32_t)0u));
    }
    phy_rf_fn_09696c((int32_t)(arg_0));
    return (int32_t)(((int32_t)0x22u));
}

/* @provenance RVA 0x098428 sub_98428 */
int32_t phy_rf_fn_098428(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_rf_fn_09696c((int32_t)(arg_0)));
}

/* @provenance RVA 0x098474 sub_98474 */
int32_t phy_rf_fn_098474(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x09876c sub_9876C */
int32_t phy_rf_fn_09876c(int32_t arg_0, uint32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    if (((uint32_t)(int32_t)(acib_mem_read_s32(0x10027B14u))) == 0u) {
        return (int32_t)(((int32_t)0xAu));
    }
    acib_mem_write32(arg_1, (uint32_t)(phy_rf_fn_097fac((int32_t)(((int32_t)6u)), (uint32_t)(acib_mem_read_s32(0x10027B14u)))));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x0987f8 sub_987F8 */
int32_t phy_rf_fn_0987f8(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_1));
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x098842 sub_98842 */
uint32_t phy_rf_fn_098842(int32_t arg_0)
{
    (void)arg_0;
    return (uint32_t)(phy_rf_fn_097fac((int32_t)(((int32_t)6u)), (uint32_t)(((int32_t)(((uint32_t)(int32_t)(((int32_t)(8u * ((uint32_t)(int32_t)(arg_0)))))) + 0x70u)))));
}

/* @provenance RVA 0x098d3c sub_98D3C */
int32_t phy_rf_fn_098d3c(int32_t arg_0)
{
    (void)arg_0;
    return (int32_t)(phy_rf_fn_09717c((int16_t)(acib_mem_read32(acib_mem_read32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 0xCu)))))))));
}

/* @provenance RVA 0x098d4c sub_98D4C */
int32_t phy_rf_fn_098d4c(uint32_t arg_0, int32_t arg_1)
{
    (void)arg_0;
    (void)arg_1;
    uint32_t loc_0 = 0;
    int16_t loc_1 = 0;
    int32_t loc_2 = 0;
    int16_t loc_3 = 0;
    uint32_t loc_4 = 0;
    int32_t loc_5 = 0;
    uint32_t loc_6 = 0;
    int32_t loc_7 = 0;
    uint32_t loc_8 = 0;
    loc_0 = (uint32_t)(acib_mem_read32(arg_0));
    loc_1 = (int16_t)((acib_mem_read32((arg_0 + 4u * 1u)) & 0x3Fu));
    loc_2 = (int32_t)(((int32_t)(((uint32_t)(((uint8_t)((acib_mem_read32(arg_0) >> 24) & 0xFFu)))) & 0xFu)));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 1u)))), (uint8_t)(acib_mem_read32(arg_0)));
    loc_3 = (int16_t)(((int32_t)(((uint32_t)(int32_t)(loc_2)) | ((uint32_t)(int32_t)(((int32_t)(0x10u * ((uint32_t)(int32_t)(loc_1)))))))));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + 0x2000u))));
    acib_mem_write8(((uint32_t)(arg_1)), (uint8_t)(((int8_t)0x49u)));
    loc_4 = (uint32_t)(((uint16_t)((loc_0 >> 16) & 0xFFFFu)));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 2u)))), (uint8_t)(((uint8_t)((loc_0 >> 8) & 0xFFu))));
    loc_5 = (int32_t)(acib_mem_read8((((uint32_t)(arg_0)) + 5u)));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 3u)))), (uint8_t)(loc_4));
    if (((uint32_t)(int32_t)(loc_5)) == 1u) {
        acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 4u)))), (uint16_t)(((int32_t)(((uint32_t)(int32_t)(loc_3)) + 0x4000u))));
    }
    loc_6 = (uint32_t)(((uint32_t)(acib_mem_read32((arg_0 + 4u * 2u)))));
    loc_7 = (int32_t)(acib_mem_read_s32(loc_6));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0xAu)))), (uint32_t)(acib_mem_read_s32((loc_6 + 4u * 1u))));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 6u)))), (uint32_t)(loc_7));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0xEu)))), (uint8_t)(((int8_t)0u)));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0x10u)))), (uint16_t)(((int16_t)0xE00u)));
    loc_6 = (uint32_t)(((uint32_t)loc_6 & ~0xFFu) | (((uint32_t)(acib_mem_read8((((uint32_t)(arg_0)) + 0xCu)))) & 0xFFu));
    loc_8 = (uint32_t)(((uint32_t)(acib_mem_read32((arg_0 + 4u * 4u)))));
    acib_mem_write8(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0xFu)))), (uint8_t)(((uint8_t)(loc_6))));
    acib_mem_write16(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0x12u)))), (uint16_t)(acib_mem_read16(loc_8)));
    (void)memset(acib_addr_ptr(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_1)) + 0x14u))))), 0, 0xCu);
    return (int32_t)(((int32_t)0u));
}

/* @provenance RVA 0x098e60 sub_98E60 */
int32_t phy_rf_fn_098e60(int32_t arg_0, int32_t arg_1, int32_t arg_2)
{
    (void)arg_0;
    (void)arg_1;
    (void)arg_2;
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 8u)))), (uint32_t)(arg_2));
    acib_mem_write32(((uint32_t)(((int32_t)(((uint32_t)(int32_t)(arg_0)) + 4u)))), (uint32_t)(arg_1));
    return (int32_t)(((int32_t)0u));
}
