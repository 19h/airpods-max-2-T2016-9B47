/*
 * memmap.h -- fixed addresses of the W3 PHY image.
 *
 * Two kinds of object have addresses that are part of a contract and therefore
 * appear here:
 *
 *   * regions shared with the host MAC (mailbox, status/telemetry page, the
 *     RTKit trace ring and crash record, the NVV calibration blob);
 *   * the module state blocks, whose placement in the recovered image is
 *     reproduced by link/phyb.ld so that crash dumps taken from the device can
 *     be read against this source.
 *
 * Peripheral register blocks live in the regs_*.h headers.
 */
#ifndef BTP_MEMMAP_H
#define BTP_MEMMAP_H

/* ---- ROM ----------------------------------------------------------------- */
#define ROM_BASE                    0x00000000u
/* 9A348's header word at ROM 0xF0 and RAM word at 0x4D2898A0 are 0x88D4;
 * both are 0x88D0 in 8E251/8E258. These equal the respective logical ROM
 * ends (0x88D2 / 0x88CF) rounded up to four bytes. The loader field contract
 * is not independently established; do not confuse this with file offset
 * 0x88E0, where the initialized RAM payload begins in all three raw files. */
#define ROM_SIZE                    0x000088D4u
#define ROM_IMAGE_HEADER            0x000000C0u    /* RTKit 'uuid' header, 0x48 bytes */

/* ---- shared with the host -------------------------------------------------- */
#define MAC_MAILBOX_BASE            0x4D280000u    /* struct mac_mailbox */
#define NVV_MDMI_BASE               0x4D281000u    /* modem NVV blob, 0x4000 bytes */
#define NVV_MDMI_SIZE               0x00004000u
#define TXPWR_TABLE_MARKER          0x4D284000u    /* written with TXPWR_TABLE_MARKER_VALUE */
#define TXPWR_TABLE_MARKER_VALUE    0xCAFEBABAu
#define PHY_STATUS_BASE             0x4D285000u    /* struct phy_status_region */
#define TRACE_RING_BASE             0x4D289000u    /* struct trace_ring, 0x200 bytes */
#define CRASH_RECORD_BASE           0x4D289200u    /* struct crash_record, 0x4C bytes */

/* ---- RAM image ------------------------------------------------------------ */
#define RAM_DATA_BASE               0x4D289298u    /* start of .data */
#define RAM_BSS_BASE                0x4D2898BDu
#define RAM_END                     0x4D28D178u    /* RTSZ metadata record */

/* Exception stack (MSP) and thread stack (PSP), 1 KiB each, painted with
 * "RTKSTACK" at reset. */
#define PSP_STACK_BASE              0x4D289AC8u
#define PSP_STACK_TOP               0x4D289EC8u
#define MSP_STACK_BASE              0x4D289EC8u
#define MSP_STACK_TOP               0x4D28A2C8u
#define STACK_SIZE                  0x400u
#define STACK_PAINT_PATTERN         0x4B434154534B5452ull   /* "RTKSTACK" */

/* Initialized image objects whose addresses are used in the reference. */
#ifdef BTP_HOST
#define BTP_DATA(name)
#define BTP_ROM(name)
#else
#define BTP_DATA(name) __attribute__((section(".image.data." #name), used))
#define BTP_ROM(name) __attribute__((section(".image.rom." #name), used))
#endif

/* Module state blocks (see link/phyb.ld and tests/layout.c). */
#define STATE_CLOCK_BASE            0x4D289900u
#define STATE_MAC_BASE              0x4D28990Cu
#define STATE_RX_OVERRIDES_BASE     0x4D289910u
#define STATE_TX_OVERRIDES_BASE     0x4D289930u
#define STATE_TXPWR_MODE_BASE       0x4D289948u
#define STATE_LOOPBACK_BASE         0x4D28996Cu
#define STATE_WARM_BOOT_BASE        0x4D2899ACu
#define STATE_FORMAT_BASE           0x4D2899B4u
#define STATE_NVV_PRESENCE_BASE     0x4D289A38u
#define STATE_RTK_BASE              0x4D289A40u
#define STATE_DIAG_BASE             0x4D28A2C8u
#define STATE_FRS_BASE              0x4D28A300u
#define STATE_RX_BASE               0x4D28A9C8u
#define STATE_DPD_BASE              0x4D28AA88u
#define STATE_TX_BASE               0x4D28B674u
#define STATE_TXPWR_BASE            0x4D28B700u
#define STATE_SYSCFG_BASE           0x4D28CB24u
#define STATE_LOOPBACK_SAVED_BASE   0x4D28CF1Cu
#define STATE_RADIO_REV_BASE        0x4D28CF44u
#define STATE_NVV_TABLES_BASE       0x4D28CF48u
#define STATE_MPU_BASE              0x4D28CFE0u
#define STATE_RTK_IRQ_BASE          0x4D28D170u

/*
 * Placement attribute for a module state block.  On the target each block
 * gets its own section so the linker script can pin it; the host build keeps
 * them as ordinary globals.
 */
#if defined(__arm__) && !defined(BTP_HOST)
#define BTP_STATE(name)             __attribute__((section(".state." #name)))
#else
#define BTP_STATE(name)
#endif

#endif /* BTP_MEMMAP_H */
