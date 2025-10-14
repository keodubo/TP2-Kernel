#ifndef INTERRUPS_H_
#define INTERRUPS_H_

#include <stdint.h>

void interrupt_keyboardHandler(void);
void irq_timer_handler(void);
void interrupt_systemCall(void);
void exception_invalidOp(void);
void exception_zeroDiv(void);

void _cli(void);

void _sti(void);

void _hlt(void);

void picMasterMask(uint8_t mask);

void picSlaveMask(uint8_t mask);

// Termina la ejecución de la cpu.
void haltcpu(void);

#endif /* INTERRUPS_H_ */
