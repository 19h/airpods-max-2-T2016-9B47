/*
 * btp.h -- common definitions for the W3 Bluetooth PHY firmware ("BTP").
 */
#ifndef BTP_H
#define BTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "bus.h"
#include "cpu.h"
#include "regs.h"
#include "memmap.h"
#include "shared.h"
#include "libc.h"

/* Build comparison: this reconstruction targets 9A348 (14 Sep 2026), PHY
 * build 185. 8E251 (31 Mar) and 8E258 (5 May 2026) both carry PHY build 183;
 * their complete images differ only in identity bytes and two build paths.
 * Release identifiers/dates are supplied metadata, not decoded PHY versions. */
#define BTP_BUILD_STRING            "185{T:w3}{C:B0}.debug"
#define BTP_FIRMWARE_VERSION        185

/* Status codes returned to the MAC and used internally. */
typedef int btp_status_t;
enum {
    BTP_OK              = 0,
    BTP_ERR_NOT_ACTIVE  = 7,        /* RTKit kRTKErrorNotReady */
    BTP_ERR_INVALID     = 11,       /* RTKit kRTKErrorInvalidArgument */
};

/* Panic / halt codes handed to the RTKit layer. */
enum {
    BTP_PANIC_INVALID_REQUEST   = 11,
    BTP_PANIC_WARM_BOOT_STATUS  = 7,
    BTP_HALT_FATAL              = 5,
};

/* Radio bands (SEQ_PKT_CMD0_BAND). */
#define BTP_BAND_COUNT              8

/* Packet types (SEQ_PKT_CMD1_PKT_TYPE). */
#define BTP_PKT_TYPE_COUNT          16
#define BTP_PKT_TYPE_FRS_SCAN       0xC
#define BTP_PKT_TYPE_FRS_WIDE       0xD

#define ARRAY_SIZE(a)               (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)                   ((a) < (b) ? (a) : (b))
#define MAX(a, b)                   ((a) > (b) ? (a) : (b))

/*
 * Assertion support.  RTKit prints the base name of __FILE__; the image only
 * carries the two files that actually assert (mac.c, exception.c).  Line
 * numbers are those of the original build so the crash messages stay
 * byte-identical; see btp_panic_at().
 */
#define BTP_FILENAME \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#endif /* BTP_H */
