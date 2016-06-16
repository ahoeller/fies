/*
 * fault-injection-controller.h
 *
 *  Created on: 17.08.2014
 *      Author: Gerhard Schoenfelder
 */

#ifndef FAULT_INJECTION_CONTROLLER_H_
#define FAULT_INJECTION_CONTROLLER_H_

#include "config.h"
#include "cpu.h"

/**
 * The declaration of the InjectionMode, which specifies,
 * if the controller-function is called from softmmu (for
 * access-triggered memory address or content faults)
 * or from register-access-function (for access-triggered
 * register address or content faults) or from
 * decode-cpu-function (for access-triggered instruction
 * faults) or fromtb_find_fast-function for time-triggered
 * faults.
 */
typedef enum
{
	FI_MEMORY_ADDR,
	FI_MEMORY_CONTENT,
	FI_REGISTER_ADDR,
	FI_REGISTER_CONTENT,
   FI_INSN,
   FI_TIME
}InjectionMode;

/**
 * The declaration of the AccessType, which specifies
 * a read-, write- or execution-access
 */
typedef enum
{
	read_access_type,
	write_access_type,
	exec_access_type,
}AccessType;

/**
 * see corresponding c-file for documentation
 */
void fault_injection_controller_init(CPUArchState *env, hwaddr *addr,
												uint32_t *value, InjectionMode injection_mode,
												AccessType access_type);
int64_t fault_injection_controller_getTimer(void);
void fault_injection_controller_initTimer(void);
void init_ops_on_cell(int size);
void destroy_ops_on_cell(void);
int ends_with(const char *string, const char *ending);
int timer_to_int(const char *string);
void setMonitor(Monitor *mon);
void start_automatic_test_process(CPUArchState *env);
//void fault_reload_arg();

#endif /* FAULT_INJECTION_CONTROLLER_H_ */
