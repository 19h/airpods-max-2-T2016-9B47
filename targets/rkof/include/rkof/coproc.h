#ifndef RKOF_COPROC_H
#define RKOF_COPROC_H

#include <stdint.h>

/*
 * Coprocessor Supervisor & GTB Capture Registration (src/coproc/supervisor.c)
 * Manages phyb (W3 PHY), acib (BT MAC), aopf (AOP), and radio register capture.
 */

int sub_15B20(void);
int sub_15B78(void);
int sub_15B98(void);
int sub_15BAC(void);
uint32_t sub_15C1C(void);
uint32_t sub_15C4C(void);
void *sub_17F5C(uint32_t coproc_mask);
int sub_F1B90(void *unused_self, uint32_t gtb_ctx_val);

#endif /* RKOF_COPROC_H */
