/*
 * Exact binary-backed read-only and initialized-RAM sections extracted from
 * raw acib firmware image by tools/extract_blobs.py.
 */

    .section .rtkit.header, "a", %progbits
    .global g_acib_rtkit_header_blob
g_acib_rtkit_header_blob:
    .incbin BLOB_RTKIT_HDR

    .section .vectors.secondary, "a", %progbits
    .global g_acib_secondary_vectors_blob
g_acib_secondary_vectors_blob:
    .incbin BLOB_SEC_VECTORS

    .section .rom.rodata, "a", %progbits
    .global g_acib_rom_rodata_blob
g_acib_rom_rodata_blob:
    .incbin BLOB_ROM_RODATA

    .section .ram.data, "aw", %progbits
    .global g_acib_ram_data_blob
g_acib_ram_data_blob:
    .incbin BLOB_RAM_DATA
