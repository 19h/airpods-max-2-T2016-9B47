/* RTKit wire records. Byte order and packed lengths follow the firmware. */
#include "btp/rtkit.h"

#ifdef BTP_HOST
#define IMAGE_SECTION(name) __attribute__((used))
#else
#define IMAGE_SECTION(name) __attribute__((section(name), used))
#endif

/* Raw ROM 0xD0..0xDF (header offsets 0x10..0x1F), by release:
 * 8E251: 499dbd6b1d163ec98faa9ac3a3bba2e2
 * 8E258: c347230bd4473dadab888eafdd14a767
 * 9A348: 5fa9e1613a11373e968af4be63f54183 (below).
 * These are identity bytes; no UUID byte order or hash algorithm is assumed.
 * Header offset 0x30 (ROM 0xF0) changes from 0x88D0 to 0x88D4 in 9A348. */
IMAGE_SECTION(".rtkit.header")
const uint8_t btp_image_header[0x48] = {
    0x75, 0x75, 0x69, 0x64, 0x05, 0x00, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00,
    0x60, 0x6B, 0x00, 0x00, 0x5F, 0xA9, 0xE1, 0x61, 0x3A, 0x11, 0x37, 0x3E,
    0x96, 0x8A, 0xF4, 0xBE, 0x63, 0xF5, 0x41, 0x83, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x44, 0x98, 0x28, 0x4D, 0x79, 0x00, 0x00, 0x00,
    0xD4, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x60, 0x6B, 0x00, 0x00,
};

IMAGE_SECTION(".rtkit.product")
const char btp_product_build[] = BTP_BUILD_STRING;
/* 8E251/8E258 use "RTKit-3255.100.139.debug" (24 bytes excluding NUL).
 * 9A348 uses the 27-byte value below. Its +3-byte growth shifts following
 * ROM strings and their pointer values by +3 without changing code layout. */
IMAGE_SECTION(".rtkit.build")
const char btp_runtime_build[] = "RTKit_debug-3514.0.15.debug";

IMAGE_SECTION(".rtkit.guard")
uint32_t __stack_chk_guard = 0xAFF;

#define META32(a, b, c, d, value) { RTK_META_TAG(a, b, c, d), 4, value }
IMAGE_SECTION(".rtkit.meta")
struct rtk_image_metadata rtk_metadata = {
    .soc = META32('S', 'O', 'C', '_', 0xFFFFFFFF),
    .soc_rev = META32('S', 'O', 'C', 'R', 0xFFFFFFFF),
    .fpga = META32('f', 'p', 'g', 'a', 0xFFFFFFFF),
    .stack_guard = META32('S', 'T', 'K', 'G', 0xAFF),
    .ram_size = META32('R', 'T', 'S', 'Z', RAM_END),
    .ram_start = META32('R', 'R', 'S', 'T', 0),
    .ram_end = META32('R', 'R', 'E', 'D', 0),
    .text_size = META32('T', 'X', 'S', 'Z', ROM_SIZE),
    .data_reload = { RTK_META_TAG('D', 'S', 'R', 'L'), 1, 0 },
    .slid = { RTK_META_TAG('S', 'L', 'I', 'D'), 8, {0} },
};

#ifndef BTP_HOST
IMAGE_SECTION(".image.prefix")
uint8_t btp_initialized_shared_ram[0x9298] = {
#include "ram_prefix.inc"
};
#endif
