/*
 * fault-injection.h
 *
 *  Created on: 05.08.2014
 *      Author: Gerhard Schoenfelder
 */

#ifndef FAULT_INJECTION_H_
#define FAULT_INJECTION_H_

#include <stdio.h>

/**
 * The file is opened at the init-function of the QEMU-monitor
 * but is held by the fault-collector.
 */
extern FILE *data_collector;

/**
 * see corresponding c-file for documentation
 */
void data_collector_write(const char* buf);
void set_do_fault_injection(int flag);
int get_do_fault_injection(void);

#endif /* FAULT_INJECTION_H_ */
