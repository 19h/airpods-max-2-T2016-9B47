/* Recovered readonly storage between separately pinned objects.
 * These are heterogeneous storage aggregates, not five original firmware structs.
 * See RODATA_RECOVERY.md for consumers, overlapping views and confidence limits.
 * uint32_t address fields encode the 32-bit wire/ROM value, not host pointers.
 * Section names retain the existing linker bindings and all original bytes.
 * These are 9A348 bytes: 8E251/8E258 differ in two build paths and version
 * strings; the longer 9A348 RTKit string shifts later strings and 13 table
 * pointers here by +3 (and 144 literal-pool words in ROM text by +3).
 */
#include "btp/btp.h"

#pragma pack(push, 1)
struct rtk_stack_constant_tail {
    uint32_t psp_top;
    uint32_t psp_bytes;
    uint32_t msp_base;
    uint32_t msp_top;
    uint32_t msp_bytes;
};

struct exception_and_band_constants {
    uint8_t product_string_padding[2];
    uint32_t exception_name_addresses[9];
    uint8_t frs_band_spans[8];
};

/* The two RX bandwidth views overlap trailing zero bytes: the alternate-rate
 * view begins at 0x6C68, the packet-type view at 0x6C6F. Their eight/sixteen
 * byte consumer views reach into following storage; original array extents
 * cannot be inferred just from this physical layout. */
struct phy_rom_lookup_storage {
    uint8_t frs_channel_counts[8]; /* 0x6BDD */
    uint8_t align_status_pointers[3]; /* 0x6BE5 */
    uint32_t frs_status_name_addresses[4]; /* 0x6BE8 */
    uint32_t frs_maxgain_defaults[8][2]; /* 0x6BF8 */
    uint8_t frs_fixed_gain_indices[13]; /* 0x6C38 */
    uint8_t align_gain_codes[1]; /* 0x6C45 */
    uint16_t frs_fixed_gain_codes[13]; /* 0x6C46 */
    int16_t frs_status_samples[4]; /* 0x6C60 */
    uint8_t rx_bandwidth_lookup_storage[21]; /* 0x6C68 */
    uint8_t rx_maxgain_band_offsets[8]; /* 0x6C7D */
    uint8_t tx_alternate_power_types[8]; /* 0x6C85 */
    uint8_t tx_packet_power_types[16]; /* 0x6C8D */
    uint8_t unclassified_zero_fill[4099]; /* 0x6C9D */
    uint32_t exception_class_by_number[14]; /* 0x7CA0 */
    uint64_t stack_paint_pattern; /* 0x7CD8 */
    char format_hex_lower[16]; /* 0x7CE0 */
    char format_hex_upper[16]; /* 0x7CF0 */
};

#define ROM_STRING(field, address, literal) char field[sizeof(literal)];
struct exception_rom_strings {
#include "rom_exception_strings.def"
};
struct diagnostic_rom_strings {
#include "rom_diagnostic_strings.def"
};
#undef ROM_STRING
#pragma pack(pop)

_Static_assert(sizeof(struct rtk_stack_constant_tail) == 20, "stack constants extent");
_Static_assert(offsetof(struct rtk_stack_constant_tail, psp_top) == 0, "psp_top offset");
_Static_assert(offsetof(struct rtk_stack_constant_tail, psp_bytes) == 4, "psp_bytes offset");
_Static_assert(offsetof(struct rtk_stack_constant_tail, msp_base) == 8, "msp_base offset");
_Static_assert(offsetof(struct rtk_stack_constant_tail, msp_top) == 12, "msp_top offset");
_Static_assert(offsetof(struct rtk_stack_constant_tail, msp_bytes) == 16, "msp_bytes offset");
_Static_assert(sizeof(struct exception_and_band_constants) == 46, "exception/band extent");
_Static_assert(offsetof(struct exception_and_band_constants, product_string_padding) == 0, "product padding offset");
_Static_assert(offsetof(struct exception_and_band_constants, frs_band_spans) == 38, "band spans offset");
_Static_assert(offsetof(struct exception_and_band_constants, exception_name_addresses) == 2,
               "unaligned start of exception pointer table");
_Static_assert(sizeof(struct phy_rom_lookup_storage) == 4387, "lookup storage extent");
_Static_assert(sizeof(struct exception_rom_strings) == 300, "exception string extent");
_Static_assert(sizeof(struct diagnostic_rom_strings) == 2655, "diagnostic string extent");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_channel_counts) == 0x0, "frs_channel_counts offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, align_status_pointers) == 0x8, "align_status_pointers offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_status_name_addresses) == 0xB, "frs_status_name_addresses offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_maxgain_defaults) == 0x1B, "frs_maxgain_defaults offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_fixed_gain_indices) == 0x5B, "frs_fixed_gain_indices offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, align_gain_codes) == 0x68, "align_gain_codes offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_fixed_gain_codes) == 0x69, "frs_fixed_gain_codes offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, frs_status_samples) == 0x83, "frs_status_samples offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, rx_bandwidth_lookup_storage) == 0x8B, "rx_bandwidth_lookup_storage offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, rx_maxgain_band_offsets) == 0xA0, "rx_maxgain_band_offsets offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, tx_alternate_power_types) == 0xA8, "tx_alternate_power_types offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, tx_packet_power_types) == 0xB0, "tx_packet_power_types offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, unclassified_zero_fill) == 0xC0, "unclassified_zero_fill offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, exception_class_by_number) == 0x10C3, "exception_class_by_number offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, stack_paint_pattern) == 0x10FB, "stack_paint_pattern offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, format_hex_lower) == 0x1103, "format_hex_lower offset");
_Static_assert(offsetof(struct phy_rom_lookup_storage, format_hex_upper) == 0x1113, "format_hex_upper offset");

#define ROM_STRING(field, address, literal) \
    _Static_assert(offsetof(struct exception_rom_strings, field) == (address) - 0x7D1C, #field " offset");
#include "rom_exception_strings.def"
#undef ROM_STRING
#define ROM_STRING(field, address, literal) \
    _Static_assert(offsetof(struct diagnostic_rom_strings, field) == (address) - 0x7E73, #field " offset");
#include "rom_diagnostic_strings.def"
#undef ROM_STRING

#ifndef BTP_HOST
static const struct rtk_stack_constant_tail rom_stack_constants BTP_ROM(rom_rodata_6b4c) = {
    .psp_top = PSP_STACK_TOP, .psp_bytes = STACK_SIZE,
    .msp_base = MSP_STACK_BASE, .msp_top = MSP_STACK_TOP, .msp_bytes = STACK_SIZE,
};

static const struct exception_and_band_constants rom_exception_and_band_constants
    BTP_ROM(rom_rodata_6b76) = {
    .product_string_padding = {0, 0},
    .exception_name_addresses = {0x7D21, 0x7D27, 0x7D2D, 0x7D31, 0x7D40,
                                 0x7D4B, 0x7D4F, 0x7D53, 0x7D57},
    .frs_band_spans = {79, 101, 126, 76, 126, 125, 125, 125},
};

static const struct phy_rom_lookup_storage rom_phy_lookups BTP_ROM(rom_rodata_6bdd) = {
    .frs_channel_counts = {5, 7, 8, 5, 8, 8, 8, 8},
    .align_status_pointers = {0},
    .frs_status_name_addresses = {0x00007E73, 0x00007E77, 0x00007E7B, 0x00007E7F},
    .frs_maxgain_defaults = {
        {0x339D0B4D, 0x2E6BBEFA},
        {0x354D7768, 0x2EFBF70D},
        {0x339D0F4E, 0x2FAC1712},
        {0x335CFF4B, 0x2F9C1711},
        {0x331CEF47, 0x2F7C0B0E},
        {0x32DCDF44, 0x2F7C0B0D},
        {0x328CCF3F, 0x2F4BFB0A},
        {0x323CBB3A, 0x2F3BF707},
    },
    .frs_fixed_gain_indices = {65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5},
    .align_gain_codes = {0},
    .frs_fixed_gain_codes = {0x00FE, 0x00EA, 0x00CE, 0x00CD, 0x008D, 0x0094, 0x40C0, 0x401C, 0x4008, 0x500C, 0x5800, 0x6808, 0x7318},
    .frs_status_samples = {0x7FFF, 0x7FFE, 0x7FFD, 0x7FFC},
    .rx_bandwidth_lookup_storage = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 3, 2, 2, 2},
    .rx_maxgain_band_offsets = {0, 0, 1, 1, 1, 2, 2, 2},
    .tx_alternate_power_types = {3, 3, 4, 3, 3, 3, 3, 3},
    .tx_packet_power_types = {2, 2, 2, 2, 2, 2, 0, 1, 5, 6, 2, 2, 7, 8, 2, 2},
    .unclassified_zero_fill = {0},
    .exception_class_by_number = {0x00000000, 0x00000007, 0x00000008, 0x00000004, 0x00000004, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000000, 0x00000000, 0x00000002},
    .stack_paint_pattern = STACK_PAINT_PATTERN,
    .format_hex_lower = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'},
    .format_hex_upper = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'},
};

#define ROM_STRING(field, address, literal) .field = literal,
static const struct exception_rom_strings rom_exception_messages BTP_ROM(rom_rodata_7d1c) = {
#include "rom_exception_strings.def"
};
static const struct diagnostic_rom_strings rom_diagnostic_messages BTP_ROM(rom_rodata_7e73) = {
#include "rom_diagnostic_strings.def"
};
#undef ROM_STRING
#endif
