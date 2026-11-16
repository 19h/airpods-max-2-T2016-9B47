#include "rkof/coproc.h"
#include "rkof/memmap.h"
#include "rkof/phy.h"
#include "rkof/rtkit.h"
#include "rkof/shared.h"

extern int sub_15830(void);
extern void sub_1A5A8(uint32_t coproc_id);
extern void sub_13328(uint32_t coproc_slot);
extern void sub_141B0(void *log_pack);
extern void sub_1A646(uintptr_t log_token, const void *log_pack);
extern int sub_1BA50(void);
extern int sub_1BA72(void);
extern int sub_22F14(uint32_t ep_or_evt, uintptr_t cb_fn, uint32_t arg);
extern void sub_F1C94(void);
extern void sub_F1D1C(void);

/* @provenance RVA 0x15b20 sub_15B20 */
int sub_15B20(void)
{
    int rc = sub_15830();
    if (rc != 0) {
        return rc;
    }

    uintptr_t *btm_obj = RKOF_PTR(uintptr_t *, RKOF_DATA_BTM_DEV_OBJ_ADDR);
    const uintptr_t *vtable = RKOF_PTR(const uintptr_t *, *btm_obj);
    typedef int (*btm_enable_fn)(void *self, int enable, int arg2, int arg3);
    btm_enable_fn fn = (btm_enable_fn)vtable[0];
    return fn((void *)btm_obj, 1, 0, 0);
}

/* @provenance RVA 0x15b78 sub_15B78 */
int sub_15B78(void)
{
    struct rkof_coproc_grant *grant = RKOF_PTR(
        struct rkof_coproc_grant *,
        RKOF_DATA_COPROC_GRANT_A_ADDR
    );
    const struct rkof_carveout_info *cv = RKOF_PTR(
        const struct rkof_carveout_info *,
        STATE_CARVEOUT_BASE
    );

    grant->grant_flags = 0u;
    grant->grant_meta = cv->shared_meta;
    grant->grant_vma = (cv->region_a_base != 0u && cv->region_a_size != 0u)
        ? cv->shared_vma
        : 0u;
    return 0;
}

/* @provenance RVA 0x15b98 sub_15B98 */
int sub_15B98(void)
{
    sub_1A5A8(2u);
    const uint8_t *desc = RKOF_PTR(const uint8_t *, RKOF_DATA_COPROC_DESC_A_ADDR);
    sub_13328((uint32_t)desc[8]);
    return 0;
}

/* @provenance RVA 0x15bac sub_15BAC */
int sub_15BAC(void)
{
    uint8_t *btp_desc = RKOF_PTR(uint8_t *, RKOF_DATA_BTP_DESC_ADDR);
    const struct rkof_carveout_info *cv = RKOF_PTR(
        const struct rkof_carveout_info *,
        STATE_CARVEOUT_BASE
    );

    *(uint32_t *)(void *)(btp_desc + 0x4Cu) = 0u;
    *(uint32_t *)(void *)(btp_desc + 0x54u) = cv->shared_meta;

    const uintptr_t *mgr_root = RKOF_PTR(const uintptr_t *, RKOF_DATA_BTP_MGR_PTR_ADDR);
    uint8_t *mgr_obj = RKOF_PTR(uint8_t *, mgr_root[0x10u / sizeof(uintptr_t)]);
    *(uint32_t *)(void *)(mgr_obj + 0x7Cu) = 0u;
#if defined(RKOF_HOST_BUILD)
    *(uintptr_t *)(void *)(mgr_obj + 0x78u) = (uintptr_t)bt_phy_otp_init;
#else
    /* Preserve exact firmware callback address 0x000FD7C1 in BTP manager slot +0x78 */
    *(uint32_t *)(void *)(mgr_obj + 0x78u) = 0x000FD7C1u;
#endif

    uint32_t grant_vma = (cv->region_a_base != 0u && cv->region_a_size != 0u)
        ? cv->shared_vma
        : 0u;
    *(uint32_t *)(void *)(btp_desc + 0x48u) = grant_vma;

    uint16_t log_pack[4] = { 0 };
    sub_141B0(log_pack);
    sub_1A646(0x200E6C1Eu, log_pack);
    return 0;
}

/* @provenance RVA 0x15c1c sub_15C1C */
uint32_t sub_15C1C(void)
{
    uint8_t *refcnt = RKOF_PTR(uint8_t *, STATE_COPROC_REF_BASE);
    uint8_t cur = *refcnt;
    if (cur > 1u) {
        return 0x8000000Bu;
    }

    *refcnt = (uint8_t)(cur + 1u);
    if (cur == 0u) {
        int rc = sub_1BA50();
        if (rc != 0) {
            *refcnt = (uint8_t)(*refcnt - 1u);
            return (uint32_t)rc;
        }
    }
    return 0u;
}

/* @provenance RVA 0x15c4c sub_15C4C */
uint32_t sub_15C4C(void)
{
    uint8_t *refcnt = RKOF_PTR(uint8_t *, STATE_COPROC_REF_BASE);
    uint8_t cur = *refcnt;
    if (cur == 0u) {
        return 0x8000000Bu;
    }

    uint8_t next = (uint8_t)(cur - 1u);
    *refcnt = next;
    if (next == 0u) {
        int rc = sub_1BA72();
        if (rc != 0) {
            *refcnt = (uint8_t)(*refcnt + 1u);
            return (uint32_t)rc;
        }
    }
    return 0u;
}

/* @provenance RVA 0x17f5c sub_17F5C */
void *sub_17F5C(uint32_t coproc_mask)
{
    /* Match exact uxth check on bit 1 followed by full 32-bit comparisons */
    if ((uint16_t)coproc_mask == 2u) {
        return RKOF_PTR(void *, RKOF_DATA_COPROC_MASK02_ADDR);
    }
    if (coproc_mask == 4u) {
        return RKOF_PTR(void *, RKOF_DATA_COPROC_MASK04_ADDR);
    }
    if (coproc_mask == 0x80u) {
        return RKOF_PTR(void *, RKOF_DATA_COPROC_MASK80_ADDR);
    }
    if (coproc_mask == 0x10u) {
        return RKOF_PTR(void *, RKOF_DATA_COPROC_MASK10_ADDR);
    }
    if (coproc_mask == 0x20u) {
        return RKOF_PTR(void *, STATE_COPROC_DYN_BASE);
    }
    if (coproc_mask == 0x40u) {
        return RKOF_PTR(void *, STATE_COPROC_DYN_BASE + 0x40u);
    }
    if (coproc_mask == 8u) {
        return RKOF_PTR(void *, RKOF_DATA_COPROC_MASK08_ADDR);
    }
    return NULL;
}

/* @provenance RVA 0xf1b90 sub_F1B90 */
int sub_F1B90(void *unused_self, uint32_t gtb_ctx_val)
{
    (void)unused_self;
    *RKOF_PTR(uint32_t *, STATE_GTB_CTX_BASE) = gtb_ctx_val;

#if defined(RKOF_HOST_BUILD)
    uintptr_t cb_267 = (uintptr_t)sub_F1C94;
    uintptr_t cb_1305 = (uintptr_t)sub_F1D1C;
#else
    uintptr_t cb_267 = 0x000F1C95u;
    uintptr_t cb_1305 = 0x000F1D1Du;
#endif

    (void)sub_22F14(0x267u, cb_267, 0u);
    if (sub_22F14(0x1305u, cb_1305, 0u) != 0) {
        rtkit_panic("ASSERTION FAILED: %s:%d\n", "gtb_capture_platform_rtk.cpp", 126);
    }

    uintptr_t *gtb_obj = RKOF_PTR(uintptr_t *, RKOF_DATA_GTB_CAPTURE_OBJ_ADDR);
    const uintptr_t *vtable = RKOF_PTR(const uintptr_t *, *gtb_obj);

    typedef int (*gtb_init_fn)(void *self, int mode, int arg2, int arg3);
    gtb_init_fn init_fn = (gtb_init_fn)vtable[0];
    if (init_fn((void *)gtb_obj, 1, 0, 0) != 0) {
        rtkit_panic("ASSERTION FAILED: %s:%d\n", "gtb_capture_platform_rtk.cpp", 130);
    }

    vtable = RKOF_PTR(const uintptr_t *, *gtb_obj);
    typedef int (*gtb_reg_fn)(void *self, uint32_t count, const void *regs, const void *desc);
    gtb_reg_fn reg_fn = (gtb_reg_fn)vtable[3];
    int rc = reg_fn(
        (void *)gtb_obj,
        0x24u,
        RKOF_PTR(const void *, RKOF_RODATA_GTB_REG_LIST_ADDR),
        RKOF_PTR(const void *, RKOF_RODATA_GTB_DESC_ADDR)
    );
    if (rc != 0) {
        rtkit_panic("ASSERTION FAILED: %s:%d\n", "gtb_capture_platform_rtk.cpp", 136);
    }
    return rc;
}
