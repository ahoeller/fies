/*
 * fault-injection-library.h
 *
 *  Created on: 07.08.2014
 *      Author: Gerhard Schoenfelder
 */

#ifndef FAULT_INJECTION_LIBRARY_H_
#define FAULT_INJECTION_LIBRARY_H_

#include "qmp-commands.h"

/**
 * The declaration of the linked list, which contains
 *  the fault parameters
 */
struct parameters
{
	/**
	 * The address, where a fault should been injected.
	 * It could be a memory, register or instruction address.
	 * In case of a pc-triggered fault, this variables
	 * stores the pc-value, at which a fault should be
	 * injected.
	 */
	int address;

	/**
	 * The coupling address, defines the second cell,
	 * which is involved (aggressor or victim cell).
	 * It could be a memory or register address.
	 * It should been only defined if the defined
	 * fault mode is a kind of Coupling Fault (CFxx).
	 */
	int cf_address;

	/**
	 * The mask contains the position where a fault
	 * should been injected at a specified address or
	 * content (e.g. a mask = 0x2 defines that only
	 * the second bit should be modified.
	 * In case, that the  fault mode is "NEW VALUE",
	 * the mask contains the new value, which should
	 * be  written to a specified target.
	 */
	int mask;

	/**
	 * The instruction contains the instruction number
	 * which should be replaced in a CPU decoding or
	 * execution fault. If the  instruction is defined as
	 * 0xDEADBEEF a NOP instruction is injected, to
	 * implement a "no execution fault".
	 * In the case, that the fault is pc-triggered and
	 * the address-variable contains the pc-value,
	 * the address for the memory or register cell
	 * is defined in the instruction variable.
	 */
	int instruction;

	/**
	 * This variable defines, if a specified bit (by mask) is
	 * set or rest at that position (is only used for State
	 * Faults or Condition Flag Faults).
	 */
	int set_bit;
};

struct Fault
{
	/**
	 * Stores the fault id.
	 */
	int id;

	/**
	 * Defines, the component of a fault. Should be a string containing
	 * the keywords CPU, RAM or REGISTER.
	 */
	char *component;

	/**
	 * Defines, the target of a fault. Should be a string containing
	 * the appropriate keywords.
	 */
	char *target;

	/**
	 * Defines, the fault mode. Should be a string containing
	 * the appropriate keywords.
	 */
	char *mode;

	/**
	 * Defines, how a  fault should been triggered.
	 * Should be a string containing the keyword access,
	 * pc or time.
	 */
	char *trigger;

	/**
	 * The time, where the fault should been active.
	 * Should be a positive, real number containing a
	 * time period in ms, us or ns.
	 */
	char *timer;

	/**
	 * The fault type for access- or time-triggered faults.
	 * Should be a string containing the keyword transient,
	 * permanent or intermittend.
	 */
	char *type;

	/**
	 * The duration of transient or intermittend faults.
	 * Should be a positive, real number containing a
	 * time period in ms, us or ns.
	 */
	char *duration;

	/**
	 * The interval of intermittend faults. Should be  a
	 * positive, real number containing a time period in ms,
	 * us or ns.
	 */
	char *interval;

	/**
	 * Visualizes if a fault is active (set) or not (reset)
	 */
	int is_active;

	/**
	 * struct, which contains important parameters
	 */
	struct parameters params;

	/**
	 * Pointer to the next entry in the linked list.
	 */
    struct Fault *next;
};

typedef struct Fault FaultList;

/**
 * see corresponding c-file for documentation
 */
int getNumFaultListElements(void);
FaultList* getFaultListElement(int element);
void qmp_fault_reload(Monitor *mon, const char *filename, Error **errp);
void delete_fault_list(void);
int getMaxIDInFaultList(void);

#endif /* FAULT_INJECTION_LIBRARY_H_ */
