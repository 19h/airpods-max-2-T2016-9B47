/*
 * host_compat.h -- force-included into every host translation unit.
 *
 * The image provides its own stack-protector guard and failure handler
 * (src/rtkit/boot.c).  On the host those names belong to the C runtime, so
 * they are renamed here to keep the firmware's definitions from shadowing
 * the system's.
 */
#ifndef BTP_HOST_COMPAT_H
#define BTP_HOST_COMPAT_H

#define __stack_chk_guard   btp_host_stack_chk_guard
#define __stack_chk_fail    btp_host_stack_chk_fail

#endif /* BTP_HOST_COMPAT_H */
