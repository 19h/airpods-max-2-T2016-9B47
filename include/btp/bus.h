/*
 * bus.h -- access to objects that live at fixed physical addresses.
 *
 * The W3 PHY is an absolute-address image: the memory-mapped peripherals, the
 * MAC<->PHY shared-memory regions and the RTKit crash/trace records all have
 * addresses that are part of the hardware or host contract and cannot move.
 *
 * On the target these macros collapse to plain volatile dereferences.  For the
 * host-side regression build (-DBTP_HOST) the same macros resolve through a
 * sparse address model (tests/host_bus.c) so the firmware's bus traffic can be
 * driven and observed from ordinary C tests.
 */
#ifndef BTP_BUS_H
#define BTP_BUS_H

#include <stdint.h>

#if defined(BTP_HOST)

void    *btp_bus_ptr(uint32_t address, uint32_t size);
void     btp_bus_reset(void);

#define BTP_PTR(address)              btp_bus_ptr((uint32_t)(address), 1u)
#define BTP_REF(type, address)        (*(type *)btp_bus_ptr((uint32_t)(address), \
                                                            (uint32_t)sizeof(type)))
#define BTP_ARR(type, count, address) (*(type (*)[count])btp_bus_ptr((uint32_t)(address), \
                                                            (uint32_t)sizeof(type[count])))
#else

#define BTP_PTR(address)              ((void *)(uintptr_t)(address))
#define BTP_REF(type, address)        (*(type *)(uintptr_t)(address))
#define BTP_ARR(type, count, address) (*(type (*)[count])(uintptr_t)(address))

#endif

/* Memory-mapped register of the given width at an absolute address. */
#define MMIO8(address)    BTP_REF(volatile uint8_t,  (address))
#define MMIO16(address)   BTP_REF(volatile uint16_t, (address))
#define MMIO32(address)   BTP_REF(volatile uint32_t, (address))

#endif /* BTP_BUS_H */
