/* Private layout shared by RTKit fault and scheduler entry veneers. */
#ifndef RTKIT_EXCEPTION_FRAME_H
#define RTKIT_EXCEPTION_FRAME_H

/* Stacked register area above the frame header: R4-R11, plus S16-S31. */
#define RTK_FRAME_CORE_REGS         0x20u
#define RTK_FRAME_FPU_REGS          0x40u

#endif /* RTKIT_EXCEPTION_FRAME_H */
