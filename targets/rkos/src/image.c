#include "rkos/rkos.h"

/*
 * Exact firmware read-only and initialized-RAM image segments preserved for
 * byte-accurate load-image verification (`tools/verify_load_image.py`) and
 * runtime table lookup (runtime VTOR at 0x13200, CRC32 table at 0x14A74,
 * SHA-256/512 K constants at 0x16180, RTKit TLV metadata at 0x1714C, and
 * initialized RAM_DATA + SRAM trampoline payload at 0x20000000..0x20000BA4).
 */

#ifdef RKOS_HOST
#define RKOS_SECT_HDR
#define RKOS_SECT_RODATA
#define RKOS_SECT_DATA
#else
#define RKOS_SECT_HDR    __attribute__((section(".rtkit.header"), aligned(4), used))
#define RKOS_SECT_RODATA __attribute__((section(".rodata.firmware"), aligned(4), used))
#define RKOS_SECT_DATA   __attribute__((section(".data.firmware"), aligned(4), used))
#endif

RKOS_SECT_HDR const uint8_t g_rkos_rtkit_hdr[RKOS_RTKIT_HDR_SIZE] = {
#include "image_rtkit_hdr.inc"
};

RKOS_SECT_RODATA const uint8_t g_rkos_rodata[RKOS_ROM_RODATA_SIZE] = {
#include "image_rodata.inc"
};

RKOS_SECT_DATA uint8_t g_rkos_data[RKOS_RAM_DATA_SIZE] = {
#include "image_data.inc"
};
