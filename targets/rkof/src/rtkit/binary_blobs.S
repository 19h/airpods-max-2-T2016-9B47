/*
 * Binary-backed firmware data slices from raw ../../../rkof:
 *   - .rtkit.header : file 0x0000C0, size 0x000048 (VMA 0x000000C0)
 *   - .rodata.image : file 0x1BFA00, size 0x033E9A (VMA 0x001BFA00)
 *   - .data.image   : file 0x200000, size 0x00F138 (VMA 0x20000000)
 */

    .syntax unified
    .thumb

    .section .rtkit.header, "a", %progbits
    .global g_rkof_uuid_header_blob
g_rkof_uuid_header_blob:
    .incbin "../../../rkof", 0x0000C0, 0x000048

    .section .rodata.image, "a", %progbits
    .global g_rkof_rodata_blob
g_rkof_rodata_blob:
    .incbin "../../../rkof", 0x1BFA00, 0x033E9A

    .section .data.image, "aw", %progbits
    .global g_rkof_data_blob
g_rkof_data_blob:
    .incbin "../../../rkof", 0x200000, 0x00F138
