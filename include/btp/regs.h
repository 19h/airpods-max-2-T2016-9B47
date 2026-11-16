/*
 * regs.h -- register-field helpers shared by the hardware block headers.
 *
 * A field is described by one constant that packs its bit offset and width,
 * which keeps the block headers to a single line per field and lets the
 * helpers below generate the BFI/UBFX-style read-modify-write sequences the
 * hardware expects.  Every update is a separate load/store pair; nothing here
 * coalesces consecutive writes to the same register.
 */
#ifndef BTP_REGS_H
#define BTP_REGS_H

#include <stdint.h>
#include "bus.h"

#define BIT(n)                      (1u << (n))

#define RFIELD(shift, width)        ((uint32_t)(((shift) << 8) | (width)))
#define RFIELD_SHIFT(f)             ((f) >> 8)
#define RFIELD_WIDTH(f)             ((f) & 0xFFu)
#define RFIELD_MAX(f)               ((1u << RFIELD_WIDTH(f)) - 1u)
#define RFIELD_MASK(f)              (RFIELD_MAX(f) << RFIELD_SHIFT(f))

/* Value of `f` encoded in a register image / extracted from one. */
#define FIELD_PREP(f, v)            (((uint32_t)(v) & RFIELD_MAX(f)) << RFIELD_SHIFT(f))
#define FIELD_GET(f, r)             (((uint32_t)(r) >> RFIELD_SHIFT(f)) & RFIELD_MAX(f))
#define FIELD_REPLACE(r, f, v)      (((uint32_t)(r) & ~RFIELD_MASK(f)) | FIELD_PREP(f, v))

/* Read-modify-write a field / bits of a memory-mapped register. */
#define REG_WRITE_FIELD(reg, f, v)  ((reg) = FIELD_REPLACE((reg), (f), (v)))
#define REG_SET_BITS(reg, bits)     ((reg) |= (uint32_t)(bits))
#define REG_CLEAR_BITS(reg, bits)   ((reg) &= ~(uint32_t)(bits))
#define REG_WRITE_MASKED(reg, mask, v) \
    ((reg) = ((reg) & ~(uint32_t)(mask)) | ((uint32_t)(v) & (uint32_t)(mask)))

#include "regs_radio.h"
#include "regs_txdig.h"
#include "regs_dpd.h"
#include "regs_rxdig.h"
#include "regs_seq.h"
#include "regs_clk.h"

#endif /* BTP_REGS_H */
