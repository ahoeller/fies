/*
 * fault-injection-injector.h
 *
 *  Created on: 17.08.2014
 *      Author: Gerhard Schoenfelder
 */

#ifndef FAULT_INJECTION_INJECTOR_H_
#define FAULT_INJECTION_INJECTOR_H_

#include "config.h"
#include "cpu.h"

/**
 * The declaration of parameters, which decides
 * which function in the injector-component is
 * called.
 */
typedef struct
{
	/**
	 * Is set for performing faults, which are targeted
	 * on a memory or register address - otherwise it
	 * is targeted at the content of a cell.
	 */
	uint32_t fault_on_address;

	/**
	 * Is set for performing faults, which are access-
	 * triggered - otherwise it is a time- or pc-triggered
	 * fault.
	 */
	uint32_t access_triggered_content_fault;

	/**
	 * Is set for performing faults, which are targeted
	 * to a register - otherwise it is targeted to a
	 * memory.
	 */
	uint32_t fault_on_register;

	/**
	 * Defines the bit, on which a fault should be injected
	 * (e.g. 0x1 defines the first bit, 0x2 defines the second
	 * bit and 0x3 defines the first two bits and so on)
	 */
	uint32_t injected_bit;

	/**
	 * Defines, if a bit-flip is performed as fault injection
	 */
	uint32_t bit_flip;

	/**
	 * Defines, if the bit should been reset or set for
	 * the State Faults (SFs) or contains the new value
	 * which should be written to the target.
	 */
	uint32_t bit_value;

	/**
	 * Defines, if a new value should been written to
	 * the specified target.
	 */
	uint32_t new_value;
}FaultInjectionInfo;

/**
 * see corresponding c-file for documentation
 */
void do_inject_look_up_error(CPUArchState *env, unsigned lockup_instruction);
void do_inject_condition_flags(CPUARMState *env, const char* src_flag_name,
																				int new_flag_value);
void do_inject_insn(unsigned int *orig_insn, unsigned int repl_insn);
void do_inject_memory_register(CPUArchState *env, hwaddr *addr, FaultInjectionInfo fi_info);

#endif /* FAULT_INJECTION_INJECTOR_H_ */
