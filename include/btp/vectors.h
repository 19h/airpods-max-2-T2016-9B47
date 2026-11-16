/*
 * vectors.h -- interrupt entry points referenced by the vector table.
 */
#ifndef BTP_VECTORS_H
#define BTP_VECTORS_H

void IRQ0_Handler(void);
void IRQ1_Handler(void);
void IRQ2_Handler(void);
void IRQ3_Handler(void);
void IRQ4_Handler(void);
void IRQ6_Handler(void);
void IRQ12_Handler(void);
void IRQ15_Handler(void);
void IRQ16_Handler(void);
void IRQ19_Handler(void);
void IRQ20_Handler(void);
void IRQ_Unused_Handler(void);

#endif /* BTP_VECTORS_H */
