#include "fault-injection-controller.h"
#include "fault-injection-library.h"
#include "fault-injection-injector.h"
#include "fault-injection-data-analyzer.h"
#include "fault-injection-config.h"
#include "profiler.h"

#include "qemu/timer.h"
#include "include/monitor/monitor.h"
#include "hmp.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

//#define DEBUG_FAULT_INJECTION

/**
 * State of the different CPUs
 */
static CPUState *next_cpu;

static Monitor *qemu_serial_monitor;
//unsigned int sbst_cycle_count_address = 0;
//unsigned int fault_counter_address = 0;
unsigned int file_input_to_use = 0;
unsigned int file_input_to_use_address = 0;
char *fault_library_name;

/**
 * Maybe useless
 */
static hwaddr address_in_use = UINT64_MAX;

/**
 * The timer value, which controls the time-triggered
 * fault injection experiments.
 */
static int64_t timer_value = 0;

/**
 * Array, which stores the previous
 * memory cell operations for
 * dynamic faults.
 */
static int **ops_on_memory_cell;

/**
 * Array, which stores the previous
 * register cell operations for
 * dynamic faults.
 */
static int **ops_on_register_cell;

/**
 * Declares the different types of previous
 * cell operations for dynamic faults.
 */
typedef enum
{
	OPs_0w0,
	OPs_0w1,
	OPs_1w0,
	OPs_1w1,
}CellOps;

/**
 * Allocates and initializes the ops_on_memory_cell- and
 * ops_on_register_cell-array.
 *
 * @param[in] ids - the maximal id number.
 */
void init_ops_on_cell(int ids)
{
	int i = 0, j = 0;

	ops_on_memory_cell = malloc(ids * sizeof(int *));
	ops_on_register_cell = malloc(ids * sizeof(int *));

	for (i = 0; i < ids; i++)
	{
		ops_on_memory_cell[i] = malloc(MEMORY_WIDTH * sizeof(int *));
		ops_on_register_cell[i] = malloc(MEMORY_WIDTH * sizeof(int *));
	}

	for (i = 0; i < ids; i++)
	{
		for (j = 0; j < MEMORY_WIDTH; j++)
		{
			ops_on_memory_cell[i][j] = -1;
			ops_on_register_cell[i][j] = -1;
		}
	}

}

/**
 * Deletes the ops_on_memory_cell- and
 * ops_on_register_cell-array.
 */
void destroy_ops_on_cell(void)
{
	int i = 0;

	for(i = 0; i < getMaxIDInFaultList(); i++)
	{
	    free(ops_on_memory_cell[i]);
	 	free(ops_on_register_cell[i]);
	}

	free(ops_on_memory_cell);
	free(ops_on_register_cell);
}

/**
 * Reads the content of a specified register.
 *
 * @param[in] env - the information of the CPU-state.
 * @param[in] regno - the register address
 * @param[out] - the content of  the specified register.
 */
static uint32_t read_cpu_register(CPUArchState *env, hwaddr regno)
{
#if defined(TARGET_ARM)
	return ((CPUARMState *)env)->regs[(int)regno];
#else
	#error unsupported target CPU
#endif
}

/**
 * Compares the ending of a string with a given ending.
 *
 * @param[in] string - the whole string
 * @param[in] ending - the string containing the postfix
 * @param[out] - 1 if the string contains the  given ending, 0 otherwise
 */
int ends_with(const char *string, const char *ending)
{
	int string_len = strlen(string);
	int ending_len = strlen(ending);

	if ( ending_len > string_len)
		return 0;

	return !strcmp(&string[string_len - ending_len], ending);
}

/**
 * Extracts the ending of the given string and converts
 * the result to an interger value.
 *
 * @param[in] string - the given string
 * @param[out] - the timer value as integer
 */
int timer_to_int(const char *string)
{
	int string_len = strlen(string);
	char timer_string[string_len-1];

	if ( string_len < 3)
		return 0;

	memset(timer_string, '\0', sizeof(timer_string));
	strncpy(timer_string, string, string_len-2);

	return (int) strtol(timer_string, NULL, 10);
}

/**
 * Returns the elapsed time after loading a fault-config file.
 *
 * @param[out] - the elapsed time as int64
 */
int64_t fault_injection_controller_getTimer(void)
{
	return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - timer_value;
}

/**
 * Initializes the timer value after loading a fault-config file (new
 * fault injection experiment) to the current virtual time in QEMU.
 */
void fault_injection_controller_initTimer(void)
{
	timer_value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
}

/**
 * Normalizes the timer value to a uniform value (ns).
 *
 * @param[in] fault - pointer to the linked list entry
 * @param[in] start_time - the start time of the fault injection
 *                                       experiment as int64
 * @param[in] stop_time - the stop time of the fault injection
 *                                       experiment as int64
 * @param[in] interval - the interval time of the fault injection
 *                                   experiment as int64 (could be zero in
 *                                   case of no usage).
 */
static void time_normalization(FaultList *fault, int64_t *start_time,
														int64_t *stop_time, int64_t *interval)
{
	*start_time = (int64_t) timer_to_int(fault->timer);
	*stop_time =  (int64_t) timer_to_int(fault->duration);

	if (interval != NULL)
		*interval = (int64_t) timer_to_int(fault->interval);

	if (fault->timer && ends_with(fault->timer, "MS"))
	{
		*start_time *= SCALE_MS;
		*stop_time *=  SCALE_MS;
		if (interval != 0)
			*interval *=  SCALE_MS;
	}
	else if (fault->timer && ends_with(fault->timer, "US"))
	{
		*start_time *= SCALE_US;
		*stop_time *= SCALE_US;
		if (interval != 0)
			*interval *=  SCALE_MS;
	}
	else if (fault->timer && ends_with(fault->timer, "NS"))
	{
		*start_time *= SCALE_NS;
		*stop_time *= SCALE_NS;
		if (interval != 0)
			*interval *=  SCALE_MS;
	}
	else
	{
		return;
	}
}

/**
 * Sets bit-flip faults active for the different triggering-methods, extract the necessary
 * information (e.g. set bits in the fault mask), calls the appropriate functions in the
 * fault-injector module and increments the counter for the single fault types in the
 * analyzer-module.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address or the buffer, where the fault is injected.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] pc - pc-value, when a fault should be triggered for pc-triggered faults
 *                          (could be zero in case of no usage).
 */
static void fault_injection_controller_bf(CPUArchState *env, hwaddr *addr,
																		FaultList *fault, FaultInjectionInfo fi_info,
																		uint32_t pc)
{
    int64_t current_timer_value = 0, start_time = 0, stop_time = 0;
    int64_t interval = 0;
	int mask = fault->params.mask, set_bit = 0;

 	fi_info.bit_flip = 1;

    if (fault->trigger && !strcmp(fault->trigger, "PC"))
	{
		if (pc == fault->params.address)
		{
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);
        		do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
    else if (fault->type && !strcmp(fault->type, "TRANSIENT"))
	{
		time_normalization(fault, &start_time, &stop_time, NULL);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time)
		{
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);
        		do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "INTERMITTEND"))
	{
		time_normalization(fault, &start_time, &stop_time, &interval);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time
			&& (current_timer_value / interval) % 2 == 0 )
		{
    			/**
    			 * search the set bits in mask (integer)
    			 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);
    			do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "PERMANENT"))
	{
		/**
		 * search the set bits in mask (integer)
		 */
		while (mask)
		{
    		/**
    		 * extract least significant bit of 2s complement
    		 */
			set_bit = mask & -mask;

    		/**
    		 * toggle the bit off
    		 */
			mask ^= set_bit ;

    		/**
    		 * determine the position of the set bit
    		 */
			fi_info.injected_bit =  log2(set_bit);
			do_inject_memory_register(env, addr, fi_info);

			if (fi_info.fault_on_register)
				incr_num_injected_faults(fault->id, "reg perm");
			else
				incr_num_injected_faults(fault->id, "ram perm");
		}
		fault->is_active = 1;
	}
	else
	{
		return;
	}
}

/**
 * Sets faults active for the different triggering-methods and increments the
 * counter for the single fault types in the analyzer-module.
 *
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fault_component - the name of the  fault injection component (cpu, ram or reg).
 * @param[in] pc - pc-value, when a fault should be triggered for pc-triggered faults
 *                          (could be zero in case of no usage).
 */
static void fault_injection_controller_set_fault_active(	FaultList *fault, const char *fault_component,
																								unsigned int pc)
{
    int64_t current_timer_value = 0, start_time = 0, stop_time = 0;
    int64_t interval = 0;
    int str_len = strlen(fault_component);
    char* fault_type = NULL;

    if (fault->trigger && !strcmp(fault->trigger, "PC") && (pc == fault->params.address))
	{
		fault_type = (char*) malloc((str_len + 7) * sizeof(char*));
		strcpy(fault_type, fault_component);
		strcat(fault_type, " trans");
		incr_num_injected_faults(fault->id, fault_type);
		fault->is_active = 1;
	}
	else if (fault->type && !strcmp(fault->type, "TRANSIENT"))
	{
		time_normalization(fault, &start_time, &stop_time, NULL);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time)
		{
			fault_type = (char*) malloc((str_len + 7) * sizeof(char*));
			strcpy(fault_type, fault_component);
			strcat(fault_type, " trans");
			incr_num_injected_faults(fault->id, fault_type);
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "INTERMITTEND"))
	{
		time_normalization(fault, &start_time, &stop_time, &interval);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time
			&& (current_timer_value / interval) % 2 == 0 )
		{
			fault_type = (char*) malloc((str_len + 7) * sizeof(char*));
			strcpy(fault_type, fault_component);
			strcat(fault_type, " trans");
			incr_num_injected_faults(fault->id, fault_type);
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "PERMANENT"))
	{
		fault_type = (char*) malloc((str_len + 6) * sizeof(char*));
		strcpy(fault_type, fault_component);
		strcat(fault_type, " perm");
		incr_num_injected_faults(fault->id, fault_type);
		fault->is_active = 1;
	}
	else
		return;

	if (fault_type)
		free(fault_type);
}

/**
 * Sets new-value faults active for the different triggering-methods, prepares the necessary
 * information (e.g. copy new-value to bit_value), calls the appropriate functions in the
 * fault-injector module and increments the counter for the single fault types in the
 * analyzer-module.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address or the buffer, where the fault is injected.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] pc - pc-value, when a fault should be triggered for pc-triggered faults
 *                          (could be zero in case of no usage).
 */
static void fault_injection_controller_new_value(CPUArchState *env, hwaddr *addr,
																					FaultList *fault, FaultInjectionInfo fi_info,
																					uint32_t pc)
{
    int64_t current_timer_value = 0, start_time = 0, stop_time = 0;
    int64_t interval = 0;

	fi_info.bit_flip = 0;
	fi_info.new_value = 1;

    if (fault->trigger && !strcmp(fault->trigger, "PC"))
	{
		if (pc == fault->params.address)
		{
			/**
			 * copy the new value, which is stored in the mask-variable of
			 * the linked list, to the bit_value  variable of the FaultInjectionInfo
			 * struct, which will be written by the appropriate function from
			 * fault-injector module.
			 */
	  		fi_info.bit_value = fault->params.mask;
	   		do_inject_memory_register(env, addr, fi_info);

			if (fi_info.fault_on_register)
				incr_num_injected_faults(fault->id, "reg trans");
			else
				incr_num_injected_faults(fault->id, "ram trans");

			fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
    else if (fault->type && !strcmp(fault->type, "TRANSIENT"))
	{
		time_normalization(fault, &start_time, &stop_time, NULL);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time)
		{
			/**
			 * copy the new value, which is stored in the mask-variable of
			 * the linked list, to the bit_value  variable of the FaultInjectionInfo
			 * struct, which will be written by the appropriate function from
			 * fault-injector module.
			 */
  			fi_info.bit_value = fault->params.mask;
   			do_inject_memory_register(env, addr, fi_info);

			if (fi_info.fault_on_register)
				incr_num_injected_faults(fault->id, "reg trans");
			else
				incr_num_injected_faults(fault->id, "ram trans");

    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "INTERMITTEND"))
	{
		time_normalization(fault, &start_time, &stop_time, &interval);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time
			&& (current_timer_value / interval) % 2 == 0 )
		{
			/**
			 * copy the new value, which is stored in the mask-variable of
			 * the linked list, to the bit_value  variable of the FaultInjectionInfo
			 * struct, which will be written by the appropriate function from
			 * fault-injector module.
			 */
  			fi_info.bit_value = fault->params.mask;
   			do_inject_memory_register(env, addr, fi_info);

			if (fi_info.fault_on_register)
				incr_num_injected_faults(fault->id, "reg trans");
			else
				incr_num_injected_faults(fault->id, "ram trans");

    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "PERMANENT"))
	{
		/**
		 * copy the new value, which is stored in the mask-variable of
		 * the linked list, to the bit_value  variable of the FaultInjectionInfo
		 * struct, which will be written by the appropriate function from
		 * fault-injector module.
		 */

		fi_info.bit_value = fault->params.mask;
		do_inject_memory_register(env, addr, fi_info);

		if (fi_info.fault_on_register)
			incr_num_injected_faults(fault->id, "reg perm");
		else
			incr_num_injected_faults(fault->id, "ram perm");

		fault->is_active = 1;
	}
	else
	{
		return;
	}
}

/**
 * Sets State Faults (SFs) active for the different triggering-methods, extracts the necessary
 * information (e.g. single, set bits in the mask), calls the appropriate functions in the
 * fault-injector module and increments the counter for the single fault types in the
 * analyzer-module.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address or the buffer, where the fault is injected.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] pc - pc-value, when a fault should be triggered for pc-triggered faults
 *                          (could be zero in case of no usage).
 */
static void fault_injection_controller_rs(CPUArchState *env, hwaddr *addr,
																		FaultList *fault, FaultInjectionInfo fi_info,
																		uint32_t pc)
{
    int64_t current_timer_value = 0, start_time = 0, stop_time = 0;
    int64_t interval = 0;
    int mask = fault->params.mask, set_bit = 0;

	fi_info.bit_flip = 0;

    if (fault->trigger && !strcmp(fault->trigger, "PC"))
	{
		if (pc == fault->params.address)
		{
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);

    			/**
    			 * copy the information, if a bit should be set or reset, which is stored
    			 * in the set_bit-variable of the linked list to the bit-value-variable of
    			 * the FaultInjectionInfo struct, if the mask is set at this position.
    			 * The double negation (!!) is used to convert an integer to a single
    			 * logical value (0 or 1).
    			 */
    			fi_info.bit_value = !!(fault->params.set_bit & set_bit);
       			do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
    else if (fault->type && !strcmp(fault->type, "TRANSIENT"))
	{
		time_normalization(fault, &start_time, &stop_time, NULL);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time)
		{
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);

    			/**
    			 * copy the information, if a bit should be set or reset, which is stored
    			 * in the set_bit-variable of the linked list to the bit-value-variable of
    			 * the FaultInjectionInfo struct, if the mask is set at this position.
    			 * The double negation (!!) is used to convert an integer to a single
    			 * logical value (0 or 1).
    			 */
    			fi_info.bit_value = !!(fault->params.set_bit & set_bit);
       			do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "INTERMITTEND"))
	{
		time_normalization(fault, &start_time, &stop_time, &interval);

		current_timer_value = fault_injection_controller_getTimer();
		if (current_timer_value > start_time
			&& current_timer_value < stop_time
			&& (current_timer_value / interval) % 2 == 0 )
		{
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

        		/**
        		 * determine the position of the set bit
        		 */
    			fi_info.injected_bit =  log2(set_bit);

    			/**
    			 * copy the information, if a bit should be set or reset, which is stored
    			 * in the set_bit-variable of the linked list to the bit-value-variable of
    			 * the FaultInjectionInfo struct, if the mask is set at this position.
    			 * The double negation (!!) is used to convert an integer to a single
    			 * logical value (0 or 1).
    			 */
    			fi_info.bit_value = !!(fault->params.set_bit & set_bit);
        		do_inject_memory_register(env, addr, fi_info);

    			if (fi_info.fault_on_register)
    				incr_num_injected_faults(fault->id, "reg trans");
    			else
    				incr_num_injected_faults(fault->id, "ram trans");
    		}
    		fault->is_active = 1;
		}
		else
		{
			fault->is_active = 0;
		}
	}
	else if (fault->type && !strcmp(fault->type, "PERMANENT"))
	{
		/* search the set bits in mask (integer) */
		while (mask)
		{
			set_bit = mask & -mask;  // extract least significant bit of 2s complement
			mask ^= set_bit ;  // toggle the bit off

			fi_info.injected_bit =  (uint32_t) log2(set_bit); // determine the position of the set bit
			fi_info.bit_value =  !!(fault->params.set_bit & set_bit); // determine if bit should be set or reset
			do_inject_memory_register(env, addr, fi_info);

			if (fi_info.fault_on_register)
				incr_num_injected_faults(fault->id, "reg perm");
			else
				incr_num_injected_faults(fault->id, "ram perm");
		}
		fault->is_active = 1;
	}
	else
	{
		return;
	}
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a fault in the address decoder of the main memory (RAM) and if the accessed
 * address is defined in the XML-file.
 *
 * @param[in] addr - the address or the buffer, where the fault is injected.
 */
static void fault_injection_controller_memory_address(CPUArchState *env, hwaddr *addr)
{
    FaultList *fault;
    int element = 0;
    FaultInjectionInfo fi_info = {0, 0, 0, 0, 0, 0, 0};

    for (element = 0; element < getNumFaultListElements(); element++)
    {
    	fault = getFaultListElement(element);

		#if defined(DEBUG_FAULT_CONTROLLER_TLB_FLUSH)
			printf("flushing tlb address %x\n", (int)*addr);
		#endif
		tlb_flush_page(env, (target_ulong)*addr);

		

		/*
		 * accessed address is not the defined fault address or the trigger is set to
		 * time- or pc-triggering.
		 */
    	if ( fault->params.address != (int)*addr || strcmp(fault->trigger, "ACCESS") )
    		continue;

    	if (!strcmp(fault->component, "RAM") && !strcmp(fault->target, "ADDRESS DECODER"))
    	{
    		/*
    		 * set/reset values
    		 */
    		fi_info.access_triggered_content_fault = 1;
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
    		fi_info.fault_on_address = 1;
    		fi_info.fault_on_register = 0;

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("memory address before: 0x%08x\n", (uint32_t) *addr);
#endif

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    			fault_injection_controller_bf(env, addr, fault, fi_info, 0);
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    			fault_injection_controller_new_value(env, addr, fault, fi_info, 0);
    		else if (!strcmp(fault->mode, "SF"))
    			fault_injection_controller_rs(env, addr, fault, fi_info, 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("memory address after: 0x%08x\n", (uint32_t) *addr);
#endif
    	}

//		tlb_flush_page(env, (target_ulong)fault->params.address);
//		tlb_flush_page(env, (target_ulong)fault->params.cf_address);
    }
}

/**
 * Implements the functionality of injecting a transition fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_tf(CPUArchState *env, FaultList *fault,
																	FaultInjectionInfo fi_info,
																	hwaddr *addr, uint32_t *value,
																	AccessType access_type)
{
	unsigned memword = 0, mask = 0, fault_value = 0;
	uint8_t *membytes = (uint8_t *)&memword;
	CPUState *cpu = ENV_GET_CPU(env);

	/**
	 * only a write operation can trigger this fault
	 */
	if (access_type == read_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	if (fi_info.fault_on_register)
		memword = read_cpu_register(env, *addr);
	else
		cpu_memory_rw_debug(cpu, *addr, membytes, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("cell content before write: 0x%08x\n", memword);
#endif

	mask = fault->params.mask;

	if (fault->mode[2] == '0')
	 	fault_value = memword | *value;
	else if (fault->mode[2] == '1')
	   	fault_value = memword & *value;
	else
	{
		address_in_use = 0;
		return;
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * This function overwrites the value, which should been written
	 * to the defined cell address.
	 */
	uint64_t value64 = *value;
	fault_injection_controller_new_value(env,  &value64, fault, fi_info, 0);
	*value = value64;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a read disturb fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_rdf(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *value,
																							AccessType access_type)
{
	unsigned mask = 0, fault_value = 0;
	uint64_t temp;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	mask = fault->params.mask;

    if (fault->mode[3] == '0')
    	fault_value = UINT32_MAX;
    else if (fault->mode[3] == '1')
    	fault_value = 0;
    else
    {
		address_in_use = 0;
    	return;
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *value;
	fault_injection_controller_new_value(env, &temp, fault, fi_info, 0);
	*value = (uint32_t)temp;

	/**
	 * Write the faulty value back to the register or memory cell.
	 */
	if (fault->is_active)
	{
		fi_info.access_triggered_content_fault = 0;
		fi_info.new_value = 1;
		fi_info.bit_value = fault->params.mask;

		do_inject_memory_register(env, addr, fi_info);
	}

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("value after fault injection: 0x%08x\n", *value);
#endif
}

/**
 * Implements the functionality of injecting a write disturb fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_wdf(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *value,
																							AccessType access_type)
{
	unsigned memword = 0, mask = 0, fault_value = 0;
	uint8_t *membytes = (uint8_t *)&memword;
	CPUState *cpu = ENV_GET_CPU(env);

	/**
	 * only a write operation can trigger this fault
	 */
	if (access_type == read_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	if (fi_info.fault_on_register)
		memword = read_cpu_register(env, *addr);
	else
		cpu_memory_rw_debug(cpu, *addr, membytes, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("cell content before write: 0x%08x\n", memword);
#endif

	mask = fault->params.mask;

	if (fault->mode[3] == '0')
	   	fault_value = ~(memword & ~(*value));
	else if (fault->mode[3] == '1')
	   	fault_value = ~memword & *value;
	else
	{
		address_in_use = 0;
		return;
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * This function overwrites the value, which should been written
	 * to the defined cell address.
	 */

	uint64_t value64 = *value;
	fault_injection_controller_new_value(env,  &value64, fault, fi_info, 0);
	*value = value64;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting an incorrect read fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_irf(CPUArchState *env, FaultList *fault,
																						FaultInjectionInfo fi_info,
																						hwaddr *addr, uint32_t *value,
																						AccessType access_type)
{
	unsigned mask = 0, fault_value = 0;
	uint64_t temp;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	mask = fault->params.mask;

	if (fault->mode[3] == '0')
	   	fault_value = UINT32_MAX;
	else if (fault->mode[3] == '1')
	   	fault_value = 0;
	else
	{
		address_in_use = 0;
		return;
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *value;
	fault_injection_controller_new_value(env,  &temp, fault, fi_info, 0);
	*value = (uint32_t)temp;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a deceptive read disturb
 * fault to a register or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_drdf(CPUArchState *env, FaultList *fault,
																						FaultInjectionInfo fi_info,
																						hwaddr *addr, uint32_t *value,
																						AccessType access_type)
{
	unsigned mask = 0, fault_value = 0;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	mask = fault->params.mask;

	 if (fault->mode[4] == '0')
	   	fault_value = UINT32_MAX;
	 else if (fault->mode[4] == '1')
	   	fault_value = 0;
	 else
	{
		address_in_use = 0;
		return;
	}

	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * Write the faulty value back to the register or memory cell
	 * and set the fault active. Do not call the function, in the
	 * same way as in rdf or irf. In this function the fault is activated
	 * by the previous function call, which overwrites the value.
	 */
	fi_info.access_triggered_content_fault = 0;
	fault_injection_controller_new_value(env, addr, fault, fi_info, 0);

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a dynamic read disturb
 * fault to a register or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_rdf_dyn(CPUArchState *env, FaultList *fault,
																						FaultInjectionInfo fi_info,
																						hwaddr *addr, uint32_t *value,
																						AccessType access_type)
{
	unsigned mask = 0, fault_value = 0, id = 0;
	int **ops_on_cell;
    int i = 0;
	uint64_t temp;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	/**
	 * set the ops_on_cell-array to the register- or
	 * memory array (depends on the accessed
	 * address)
	 */
	if (fi_info.fault_on_register)
		ops_on_cell = ops_on_register_cell;
	else
		ops_on_cell = ops_on_memory_cell;

	mask = fault->params.mask;

	id = fault->id - 1;

	/**
	 * Searches in the ops_on_cell-array, if the previous access-operations
	 * conditions are fulfilled to trigger the fault (for every bit).
	 */
	if (fault->mode[3] == '0' && fault->mode[4] == '0')
	{
		for (i = 0; i < MEMORY_WIDTH; i++)
		{
		   if (ops_on_cell[id][i] == OPs_0w0)
			   fault_value |= (1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
		}
	}
	else if (fault->mode[3] == '0' && fault->mode[4] == '1')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_0w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
   }
   else if (fault->mode[3] == '1' && fault->mode[4] == '0')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_1w0)
			   fault_value |= (1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
   }
   else if (fault->mode[3] == '1' && fault->mode[4] == '1')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_1w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
    }
	else
	{
		address_in_use = 0;
		return;
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *value;
	fault_injection_controller_new_value(env,  &temp, fault, fi_info, 0);
	*value = (uint32_t)temp;

	/**
	 * Write the faulty value back to the register or memory cell.
	 */
	if (fault->is_active)
	{
		fi_info.access_triggered_content_fault = 0;
		fi_info.new_value = 1;
		fi_info.bit_value = fault->params.mask;

		do_inject_memory_register(env, addr, fi_info);
	}

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a dynamic incorrect read
 * fault to a register or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_irf_dyn(CPUArchState *env, FaultList *fault,
																						FaultInjectionInfo fi_info,
																						hwaddr *addr, uint32_t *value,
																						AccessType access_type)
{
	unsigned mask = 0, fault_value = 0, id = 0;
	int **ops_on_cell;
    int i = 0;
	uint64_t temp;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	/**
	 * set the ops_on_cell-array to the register- or
	 * memory array (depends on the accessed
	 * address)
	 */
	if (fi_info.fault_on_register)
		ops_on_cell = ops_on_register_cell;
	else
		ops_on_cell = ops_on_memory_cell;

   mask = fault->params.mask;

   id = fault->id - 1;

	/**
	 * Searches in the ops_on_cell-array, if the previous access-operations
	 * conditions are fulfilled to trigger the fault (for every bit).
	 */
   if (fault->mode[3] == '0' && fault->mode[4] == '0')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_0w0)
			   fault_value |= (1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
    }
    else if (fault->mode[3] == '0' && fault->mode[4] == '1')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_0w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
    }
    else if (fault->mode[3] == '1' && fault->mode[4] == '0')
   {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_1w0)
			   fault_value |= (1 << i);
		   else
		   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
    }
    else if (fault->mode[3] == '1' && fault->mode[4] == '1')
	{
		for (i = 0; i < MEMORY_WIDTH; i++)
		{
		   if (ops_on_cell[id][i] == OPs_1w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
	}
    else
    {
		address_in_use = 0;
		return;
    }

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
    fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *value;
	fault_injection_controller_new_value(env,  &temp, fault, fi_info, 0);
	*value = (uint32_t)temp;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a dynamic deceptive read disturb
 * fault to a register or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_drdf_dyn(CPUArchState *env, FaultList *fault,
																						FaultInjectionInfo fi_info,
																						hwaddr *addr, uint32_t *value,
																						AccessType access_type)
{
	unsigned mask = 0, fault_value = 0, id = 0;
	int **ops_on_cell;
    int i = 0;

	/**
	 * only a read operation can trigger this fault
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	address_in_use = *addr;

	/**
	 * set the ops_on_cell-array to the register- or
	 * memory array (depends on the accessed
	 * address)
	 */
	if (fi_info.fault_on_register)
		ops_on_cell = ops_on_register_cell;
	else
		ops_on_cell = ops_on_memory_cell;

   mask = fault->params.mask;

   id = fault->id - 1;

	/**
	 * Searches in the ops_on_cell-array, if the previous access-operations
	 * conditions are fulfilled to trigger the fault (for every bit).
	 */
   if (fault->mode[4] == '0' && fault->mode[5] == '0')
  {
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_0w0)
			   fault_value |= (1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
    }
    else if (fault->mode[4] == '0' && fault->mode[5] == '1')
	{
    	for (i = 0; i < MEMORY_WIDTH; i++)
		{
		   if (ops_on_cell[id][i] == OPs_0w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
		}
	}
    else if (fault->mode[4] == '1' && fault->mode[5] == '0')
	{
	   for (i = 0; i < MEMORY_WIDTH; i++)
	   {
		   if (ops_on_cell[id][i] == OPs_1w0)
			   fault_value |= (1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
	}
    else if (fault->mode[4] == '1' && fault->mode[5] == '1')
	{
	   for (i = 0; i < MEMORY_WIDTH; i++)
	  {
		   if (ops_on_cell[id][i] == OPs_1w1)
			   fault_value &= ~(1 << i);
		   else
			   fault_value |= (*value & ((uint32_t) pow(2, i)));
	   }
	}
    else
    {
		address_in_use = 0;
		return;
   }

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
    fault->params.mask = (fault_value & mask) | (*value & ~mask);

	/**
	 * Write the faulty value back to the register or memory cell
	 * and set the fault active. Do not call the function, in the
	 * same way as in rdf or irf. In this function the fault is activated
	 * by the previous function call, which overwrites the value.
	 */
	fi_info.access_triggered_content_fault = 0;
	fault_injection_controller_new_value(env, addr, fault, fi_info, 0);

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Stores the previous access-operations of a defined fault memory address. This information
 * is used for deciding, if a dynamic fault should be triggered or not.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void log_cell_operations_memory(CPUArchState *env, FaultList *fault, hwaddr *addr,
																		uint32_t *value, AccessType access_type)
{
	unsigned memword = 0, mask = 0, set_bit, bit_pos = 0, id = 0;

	/**
	 * only a write access can trigger a dynamic fault
	 */
   	if (access_type == write_access_type)
   	{
   		uint8_t *membytes = (uint8_t *)&memword;
		CPUState *cpu = ENV_GET_CPU(env);
		cpu_memory_rw_debug(cpu, *addr, membytes, (MEMORY_WIDTH / 8), 0);

	    mask = fault->params.mask;

		/**
		 * search the set bits in mask (integer)
		 */
		while (mask)
		{
    		/**
    		 * extract least significant bit of 2s complement
    		 */
			set_bit = mask & -mask;

    		/**
    		 * toggle the bit off
    		 */
			mask ^= set_bit ;

    		/**
    		 * determine the position of the set bit
    		 */
			bit_pos = (uint32_t) log2(set_bit);
			id = fault->id - 1;

			if (!!(memword & set_bit) == 0 && !!(*value & set_bit) == 0)
				ops_on_memory_cell[id][bit_pos] = OPs_0w0;
			else if (!!(memword & set_bit) == 0 && !!(*value & set_bit) == 1)
				ops_on_memory_cell[id][bit_pos] = OPs_0w1;
			else if (!!(memword & set_bit) == 1 && !!(*value & set_bit) == 0)
				ops_on_memory_cell[id][bit_pos] = OPs_1w0;
			else if (!!(memword & set_bit) == 1 && !!(*value & set_bit) == 1)
				ops_on_memory_cell[id][bit_pos] = OPs_1w1;
			else
				ops_on_memory_cell[id][bit_pos] = -1;
		}
   	}
}

/**
 * Implements the functionality of injecting a state coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] aggressor_value -  the value of the victim or aggressor cell of the
 *                                                 register or memory (depends on direction_agressor_to_victim)
 * @param[in] access_type - if the access-operation is a write, read or execute.
 * @param[in] direction_agressor_to_victim - indicates, if the accessed cell is the victim
 *                                                                   or aggressor cell.
 */
static void fault_injection_controller_cfst(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *aggressor_value,
																							AccessType access_type,
																							int direction_agressor_to_victim)
{
	unsigned memword_aggressor = *aggressor_value, memword_victim = 0;
	unsigned mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_victim = (uint8_t *)&memword_victim;
	uint8_t *membytes_aggressor = (uint8_t *)&memword_aggressor;
	CPUState *cpu = ENV_GET_CPU(env);

    /**
     *  Inverse direction: Have to swap victim with aggressor
     *  value and address.
     */
    if (!direction_agressor_to_victim)
    {
    	address_in_use = fault->params.address;

    	if (fi_info.fault_on_register)
    		memword_aggressor = read_cpu_register(env, address_in_use);
    	else
    		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);
    }

    /**
     *  Read the content of coupling address
     */
	address_in_use = fault->params.cf_address;
	if (fi_info.fault_on_register)
		memword_victim = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_victim, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
	printf("victim cell content: 0x%08x\n", memword_victim);
#endif

	mask = fault->params.mask;
	/**
	 * Intra-coupling fault, if address and cf_address  are the same,
	 * otherwise it is a inter-coupling fault
	 *
	 */
	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = memword_aggressor;

		if (fault->mode[4] == '0' && fault->mode[5] == '0')
		{
			/**
			 * aggressor-bit is not zero and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & memword_aggressor)
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (fault->mode[4] == '0' && fault->mode[5] == '1')
		{
			/**
			 * aggressor-bit is not zero and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & memword_aggressor)
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else if (fault->mode[4] == '1' && fault->mode[5] == '0')
		{
			/**
			 * aggressor-bit is not one and hence the this fault can not
			 * been triggered
			 */
			if (!(fault->params.set_bit & memword_aggressor))
			{
				address_in_use = 0;
				return;
			}
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (fault->mode[4] == '1' && fault->mode[5] == '1')
		{
			/**
			 * aggressor-bit is not one and hence the this fault can not
			 * been triggered
			 */
			if (!(fault->params.set_bit & memword_aggressor))
			{
				address_in_use = 0;
				return;
			}
    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (fault->mode[4] == '0' && fault->mode[5] == '0')
			fault_value = ~(memword_aggressor & ~memword_victim);
		else if (fault->mode[4] == '0' && fault->mode[5] == '1')
			fault_value = memword_aggressor & memword_victim;
		else if (fault->mode[4] == '1' && fault->mode[5] == '0')
			fault_value = memword_aggressor | memword_victim;
		else if (fault->mode[4] == '1' && fault->mode[5] == '1')
			fault_value = ~memword_aggressor & memword_victim;
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (memword_victim & ~mask);

    /**
     *  Inverse direction: Have to swap victim with aggressor
     *  value and address.
     */
    if (!direction_agressor_to_victim)
    {
    	/**
    	 * cell content will be overwritten after returning - accessed cell is the
    	 * coupled cell - just modify the value.
    	 */
		fi_info.access_triggered_content_fault = 1;

		uint64_t aggressor_value64 = *aggressor_value;
		fault_injection_controller_new_value(env, &aggressor_value64, fault, fi_info, 0);
		*aggressor_value = aggressor_value64;
    }
    else
    {
		/**
		 *  write cell content immediately - accessed cell is not the coupled cell.
		 */
		fi_info.access_triggered_content_fault = 0;
		fault_injection_controller_new_value(env,  &address_in_use, fault, fi_info, 0);
    }

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}


/**
 * Implements the functionality of injecting a disturb coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] aggressor_value -  the value of the aggressor cell, which should be written to the
 *                                                 register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cfds(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *aggressor_value,
																							AccessType access_type)
{
	unsigned aggressor_value_before = 0, memword_victim = 0;
	unsigned mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_victim = (uint8_t *)&memword_victim;
	uint8_t *membytes_aggressor = (uint8_t *)&aggressor_value_before;
	CPUState *cpu = ENV_GET_CPU(env);

	address_in_use = *addr;
	if (fi_info.fault_on_register)
		aggressor_value_before = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

    /**
     *  Read the content of coupling address
     */
	address_in_use = fault->params.cf_address;
	if (fi_info.fault_on_register)
		memword_victim = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_victim, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("aggressor cell content before write: 0x%08x\n", aggressor_value_before);
	printf("aggressor cell content to write: 0x%08x\n", *aggressor_value);
	printf("victim cell content: 0x%08x\n", memword_victim);
#endif

	mask = fault->params.mask;

	/**
	 * Intra-coupling fault, if address and cf_address  are the same,
	 * otherwise it is a inter-coupling fault
	 *
	 */
	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = memword_victim;

		if (!strcmp(fault->mode, "CFDS0W00") || !strcmp(fault->mode, "CFDS0R00"))
		{
			/**
			 * aggressor-bit before and after write/read are not zero and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & (aggressor_value_before | *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_victim))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFDS0W01") || !strcmp(fault->mode, "CFDS0R01"))
		{
			/**
			 * aggressor-bit before and after write/read are not zero and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & (aggressor_value_before | *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_victim)
    				fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS1W10") || !strcmp(fault->mode, "CFDS1R10"))
		{
			/**
			 * aggressor-bit before and after write/read are not one and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & ~(aggressor_value_before & *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_victim))
    				fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS1W11") || !strcmp(fault->mode, "CFDS1R11"))
		{
			/**
			 * aggressor-bit before and after write/read are not one and hence the this fault can not
			 * been triggered
			 */
			if (fault->params.set_bit & ~(aggressor_value_before & *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_victim)
    				fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS0W10"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			/**
			 * aggressor-bit before is not zero and aggressor-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(~aggressor_value_before & *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_victim))
    				fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS0W11"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			/**
			 * aggressor-bit before is not zero and aggressor-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(~aggressor_value_before & *aggressor_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_victim)
    				fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS1W00"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			/**
			 * aggressor-bit before is not zero and aggressor-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(aggressor_value_before & ~(*aggressor_value)))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_victim))
    				fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDS1W01"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			/**
			 * aggressor-bit before is not zero and aggressor-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(aggressor_value_before & ~(*aggressor_value)))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_victim)
    				fault_value &= ~set_bit;
			}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFDS0W00") || !strcmp(fault->mode, "CFDS0R00"))
		{
			fault_value = (~aggressor_value_before & ~(*aggressor_value) & ~memword_victim)
								   | memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS0W01") || !strcmp(fault->mode, "CFDS0R01"))
		{
			fault_value =  ~(~aggressor_value_before & ~(*aggressor_value) & memword_victim)
								   & memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS1W10") || !strcmp(fault->mode, "CFDS1R10"))
		{
			fault_value = (aggressor_value_before & *aggressor_value & ~memword_victim)
								   | memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS1W11") || !strcmp(fault->mode, "CFDS1R11"))
		{
			fault_value = ~(aggressor_value_before & *aggressor_value & memword_victim)
									& memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS0W10"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			fault_value = (~aggressor_value_before & *aggressor_value & ~memword_victim)
								   | memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS0W11"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			fault_value =  ~(~aggressor_value_before & *aggressor_value & memword_victim)
									& memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS1W00"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			fault_value =  (aggressor_value_before & ~(*aggressor_value) & ~memword_victim)
									| memword_victim;
		}
		else if (!strcmp(fault->mode, "CFDS1W01"))
		{
			if (access_type == read_access_type)
			{
				address_in_use = 0;
				return;
			}

			fault_value =  ~(aggressor_value_before & ~(*aggressor_value) & memword_victim)
									 & memword_victim;
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (memword_victim & ~mask);

	/**
	 *  write cell content immediately - accessed cell is not the coupling cell.
	 */
	fi_info.access_triggered_content_fault = 0;
	fault_injection_controller_new_value(env,  &address_in_use, fault, fi_info, 0);

	/**
	 * write faulty value to victim and aggressor cell -> they are the same address,
	 * because of intra-cfs.
	 */
	if (fault->params.address == fault->params.cf_address)
	{
		fi_info.access_triggered_content_fault = 1;

		uint64_t aggressor_value64 = *aggressor_value;
		fault_injection_controller_new_value(env, &aggressor_value64, fault, fi_info, 0);
		*aggressor_value = aggressor_value64;
	}

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a transition coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] victim_value -  the value of the victim cell, which should be written to the
 *                                                 register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cftr(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *victim_value,
																							AccessType access_type)
{
	unsigned victim_value_before = 0, memword_aggressor = 0;
	unsigned mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_aggressor= (uint8_t *)&memword_aggressor;
	uint8_t *membytes_victim = (uint8_t *)&victim_value_before;
	CPUState *cpu = ENV_GET_CPU(env);

	/**
	 * only write operation can trigger this fault.
	 */
	if (access_type == read_access_type)
	{
		address_in_use = 0;
		return;
	}

    /**
     *  Read the content of the aggressor cell
     */
	address_in_use = fault->params.address;
	if (fi_info.fault_on_register)
		memword_aggressor = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

    /**
     *  Read the content of the victim cell
     */
	address_in_use = fault->params.cf_address;
	if (fi_info.fault_on_register)
		victim_value_before = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_victim, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("victim cell content before write: 0x%08x\n", victim_value_before);
	printf("victim cell content to write: 0x%08x\n", *victim_value);
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
#endif

	mask = fault->params.mask;

	/**
	 * Intra-coupling fault, if address and cf_address  are the same,
	 * otherwise it is a inter-coupling fault
	 *
	 */
	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = *victim_value;

		if (!strcmp(fault->mode, "CFTR01"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(~victim_value_before & *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value &= ~set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFTR00"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(victim_value_before & ~(*victim_value)))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * setting the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFTR11"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(~victim_value_before & *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFTR10"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(victim_value_before & ~(*victim_value)))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value |= set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFTR01"))
		{
			fault_value = ~(~memword_aggressor & ~victim_value_before & (*victim_value))
									 & (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFTR00"))
		{
			fault_value =  (~memword_aggressor & victim_value_before & ~(*victim_value))
									| (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFTR11"))
		{
			fault_value = ~(memword_aggressor & ~victim_value_before & (*victim_value))
									 & (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFTR10"))
		{
			fault_value =  (memword_aggressor & victim_value_before & ~(*victim_value))
									| (*victim_value);
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*victim_value & ~mask);

	uint64_t victim_value64 = *victim_value;
	fault_injection_controller_new_value(env, &victim_value64, fault, fi_info, 0);
	*victim_value = victim_value64;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a write disturb coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] victim_value -  the value of the vcitim cell, which sould be written to the
 *                                            register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cfwd(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *victim_value,
																							AccessType access_type)
{
	unsigned victim_value_before = 0, memword_aggressor = 0;
	unsigned mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_aggressor= (uint8_t *)&memword_aggressor;
	uint8_t *membytes_victim = (uint8_t *)&victim_value_before;
	CPUState *cpu = ENV_GET_CPU(env);

	/**
	 * only write operation can trigger this fault.
	 */
	if (access_type == read_access_type)
	{
		address_in_use = 0;
		return;
	}

	/**
	 * read the content of the aggressor cell.
	 */
	address_in_use = fault->params.address;
	if (fi_info.fault_on_register)
		memword_aggressor = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

	/**
	 * read the content of the victim cell.
	 */
	address_in_use = fault->params.cf_address;
	if (fi_info.fault_on_register)
		victim_value_before = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_victim, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("victim cell content before write: 0x%08x\n", victim_value_before);
	printf("victim cell content to write: 0x%08x\n", *victim_value);
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
#endif

	mask = fault->params.mask;

	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = *victim_value;

		if (!strcmp(fault->mode, "CFWD00"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & (victim_value_before | *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFWD01"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(victim_value_before & *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value &= ~set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFWD10"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & (victim_value_before | *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFWD11"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(victim_value_before & *victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFWD00"))
		{
			fault_value = (~memword_aggressor & ~victim_value_before & ~(*victim_value))
								   | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFWD01"))
		{
			fault_value =  ~(~memword_aggressor & victim_value_before & *victim_value)
									& (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFWD10"))
		{
			fault_value = (memword_aggressor & ~victim_value_before & ~(*victim_value))
								  | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFWD11"))
		{
			fault_value =  ~(memword_aggressor & victim_value_before & *victim_value)
									& (*victim_value);
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*victim_value & ~mask);
	uint64_t victim_value64 = *victim_value;
	fault_injection_controller_new_value(env, &victim_value64, fault, fi_info, 0);
	*victim_value = victim_value64;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a read disturb coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] victim_value -  the value of the vcitim cell, which sould be written to the
 *                                            register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cfrd(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *victim_value,
																							AccessType access_type)
{
	unsigned memword_aggressor = 0, mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_aggressor= (uint8_t *)&memword_aggressor;
	CPUState *cpu = ENV_GET_CPU(env);
	uint64_t temp;

	/**
	 * only read operation can trigger this fault.
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	/**
	 * read the content of the aggressor cell.
	 */
	address_in_use = fault->params.address;
	if (fi_info.fault_on_register)
		memword_aggressor = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("victim cell value:  0x%08x\n", *victim_value);
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
#endif

	mask = fault->params.mask;

	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = *victim_value;

		if (!strcmp(fault->mode, "CFRD00"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit &  *victim_value)
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFRD01"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit &  *victim_value)
			{
				address_in_use = 0;
				return;
			}

			/**
			* search the set bits in mask (integer)
			*/
			while (mask)
			{
			   /**
			   * extract least significant bit of 2s complement
			   */
			   set_bit = mask & -mask;

			   /**
			   * toggle the bit off
			   */
			   mask ^= set_bit ;

			   /**
			   * skipping: aggressor-bit flag and not a victim bit
			   */
			   if (set_bit & fault->params.set_bit)
   			  {
   				  address_in_use = 0;
   				  continue;
   			  }

			   /**
			   * clearing the victim bit at mask-position to one
			   */
			   if (set_bit & memword_aggressor)
				   fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFRD10"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

				/**
				 * setting the victim bit at mask-position to one
				 */
				if (!(set_bit & memword_aggressor))
					fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFRD11"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFRD00"))
		{
			fault_value = (~memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFRD01"))
		{
			fault_value = ~(~memword_aggressor & *victim_value) & (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFRD10"))
		{
			fault_value = (memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFRD11"))
		{
			fault_value = ~(memword_aggressor & *victim_value) & (*victim_value);
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*victim_value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *victim_value;
	fault_injection_controller_new_value(env,  &temp, fault, fi_info, 0);
	*victim_value = (uint32_t)temp;

	/**
	 *  write cell content immediately
	 */
	address_in_use = fault->params.cf_address;
	fi_info.access_triggered_content_fault = 0;
	fault_injection_controller_new_value(env, &address_in_use, fault, fi_info, 0);

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a incorrect read coupling fault to a register
 * or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] victim_value -  the value of the vcitim cell, which sould be written to the
 *                                            register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cfir(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *victim_value,
																							AccessType access_type)
{
	unsigned memword_aggressor = 0, mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_aggressor= (uint8_t *)&memword_aggressor;
	CPUState *cpu = ENV_GET_CPU(env);
	uint64_t temp;

	/**
	 * only read operation can trigger this fault.
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	/**
	 * read the content of the aggressor cell.
	 */
	address_in_use = fault->params.address;
	if (fi_info.fault_on_register)
		memword_aggressor = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("victim cell value:  0x%08x\n", *victim_value);
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
#endif

	mask = fault->params.mask;

	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = *victim_value;

		if (!strcmp(fault->mode, "CFIR00"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit &  *victim_value)
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFIR01"))
		{
			/**
			* victim-bit before is not zero and victim-bit after write is not one
		    * and hence the this fault can not been triggered.
			*/
			if (fault->params.set_bit & *victim_value)
			{
				address_in_use = 0;
				return;
			}

			/**
			* search the set bits in mask (integer)
			*/
			while (mask)
			{
			   /**
			   * extract least significant bit of 2s complement
			   */
			   set_bit = mask & -mask;

			   /**
			   * toggle the bit off
			   */
			   mask ^= set_bit ;

			   /**
			   * skipping: aggressor-bit flag and not a victim bit
			   */
			   if (set_bit & fault->params.set_bit)
   			   {
   				  address_in_use = 0;
   				  continue;
   			   }

			   /**
			   * clearing the victim bit at mask-position to one
			   */
			   if (set_bit & memword_aggressor)
				   fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFIR10"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

				/**
				 * setting the victim bit at mask-position to one
				 */
				if (!(set_bit & memword_aggressor))
					fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFIR11"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFIR00"))
		{
			fault_value = (~memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFIR01"))
		{
			fault_value = ~(~memword_aggressor & *victim_value) & (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFIR10"))
		{
			fault_value = (memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFIR11"))
		{
			fault_value = ~(memword_aggressor & *victim_value) & (*victim_value);
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}
	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*victim_value & ~mask);

	/**
	 * value and passed argument of the function have different
	 * data types. A cast in a read-operation will crash the system.
	 * This function overwrites the value, read by the cell address.
	 */
	temp = *victim_value;
	fault_injection_controller_new_value(env,  &temp, fault, fi_info, 0);
	*victim_value = (uint32_t)temp;

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Implements the functionality of injecting a deceptive read disturb coupling fault
 * to a register or memory cell.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] fi_info - information for performing faults.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] victim_value -  the value of the vcitim cell, which sould be written to the
 *                                            register or memory.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_cfdr(CPUArchState *env, FaultList *fault,
																							FaultInjectionInfo fi_info,
																							hwaddr *addr, uint32_t *victim_value,
																							AccessType access_type)
{
	unsigned memword_aggressor = 0, mask = 0, fault_value = 0, set_bit = 0;
	uint8_t *membytes_aggressor= (uint8_t *)&memword_aggressor;
	CPUState *cpu = ENV_GET_CPU(env);

	/**
	 * only read operation can trigger this fault.
	 */
	if (access_type == write_access_type)
	{
		address_in_use = 0;
		return;
	}

	/**
	 * read the content of the aggressor cell.
	 */
	address_in_use = fault->params.address;
	if (fi_info.fault_on_register)
		memword_aggressor = read_cpu_register(env, address_in_use);
	else
		cpu_memory_rw_debug(cpu, address_in_use, membytes_aggressor, (MEMORY_WIDTH / 8), 0);

#if defined(DEBUG_FAULT_CONTROLLER)
	printf("victim cell value:  0x%08x\n", *victim_value);
	printf("aggressor cell content: 0x%08x\n", memword_aggressor);
#endif

	mask = fault->params.mask;

	if (fault->params.address == fault->params.cf_address)
	{
		fault_value = *victim_value;

		if (!strcmp(fault->mode, "CFDR00"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit &  *victim_value)
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (!(set_bit & memword_aggressor))
    				fault_value |= set_bit;
    		}
		}
		else if (!strcmp(fault->mode, "CFDR01"))
		{
			/**
			* victim-bit before is not zero and victim-bit after write is not one
		    * and hence the this fault can not been triggered.
			*/
			if (fault->params.set_bit & *victim_value)
			{
				address_in_use = 0;
				return;
			}

			/**
			* search the set bits in mask (integer)
			*/
			while (mask)
			{
			   /**
			   * extract least significant bit of 2s complement
			   */
			   set_bit = mask & -mask;

			   /**
			   * toggle the bit off
			   */
			   mask ^= set_bit ;

			   /**
			   * skipping: aggressor-bit flag and not a victim bit
			   */
			   if (set_bit & fault->params.set_bit)
   			   {
   				  address_in_use = 0;
   				  continue;
   			   }

			   /**
			   * clearing the victim bit at mask-position to one
			   */
			   if (set_bit & memword_aggressor)
				   fault_value &= ~set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDR10"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

			/**
			 * search the set bits in mask (integer)
			 */
			while (mask)
			{
				/**
				 * extract least significant bit of 2s complement
				 */
				set_bit = mask & -mask;

				/**
				 * toggle the bit off
				 */
				mask ^= set_bit ;

				/**
				 * skipping: aggressor-bit flag and not a victim bit
				 */
				if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

				/**
				 * setting the victim bit at mask-position to one
				 */
				if (!(set_bit & memword_aggressor))
					fault_value |= set_bit;
			}
		}
		else if (!strcmp(fault->mode, "CFDR11"))
		{
			/**
			 * victim-bit before is not zero and victim-bit after write is not one
			 * and hence the this fault can not been triggered.
			 */
			if (fault->params.set_bit & ~(*victim_value))
			{
				address_in_use = 0;
				return;
			}

    		/**
    		 * search the set bits in mask (integer)
    		 */
    		while (mask)
    		{
        		/**
        		 * extract least significant bit of 2s complement
        		 */
    			set_bit = mask & -mask;

        		/**
        		 * toggle the bit off
        		 */
    			mask ^= set_bit ;

    			/**
    			 * skipping: aggressor-bit flag and not a victim bit
    			 */
    			if (set_bit & fault->params.set_bit)
    			{
    				address_in_use = 0;
    				continue;
    			}

    			/**
    			 * clearing the victim bit at mask-position to one
    			 */
    			if (set_bit & memword_aggressor)
    				fault_value &= ~set_bit;
    		}
		}
		else
		{
			address_in_use = 0;
			return;
		}

		mask = fault->params.mask;
	}
	else
	{
		if (!strcmp(fault->mode, "CFDR00"))
		{
			fault_value = (~memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFDR01"))
		{
			fault_value = ~(~memword_aggressor & *victim_value) & (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFDR10"))
		{
			fault_value = (memword_aggressor & ~(*victim_value)) | (*victim_value);
		}
		else if (!strcmp(fault->mode, "CFDR11"))
		{
			fault_value = ~(memword_aggressor & *victim_value) & (*victim_value);
		}
		else
		{
			address_in_use = 0;
			return;
		}
	}

	/**
	 * set the faulty value to the bits, where the mask is set, otherwise
	 * the original value, which should been written to memory or register,
	 * is set.
	 */
	fault->params.mask = (fault_value & mask) | (*victim_value & ~mask);

	/**
	 *  write cell content immediately
	 */
	address_in_use = fault->params.cf_address;
	fi_info.access_triggered_content_fault = 0;
	fault_injection_controller_new_value(env, &address_in_use, fault, fi_info, 0);

	/**
	 * Restores the mask-variable for the correct visualization in the monitor module.
	 */
	fault->params.mask = mask;
	address_in_use = 0;
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a fault in the memory cells of the main memory (RAM) and if the accessed
 * address is defined in the XML-file.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_memory_content(CPUArchState *env, hwaddr *addr,
																					uint32_t *value, AccessType access_type)
{
    FaultList *fault;
    int element = 0;
    FaultInjectionInfo fi_info = {0, 0, 0, 0, 0, 0, 0};

    for (element = 0; element < getNumFaultListElements(); element++)
    {
    	fault = getFaultListElement(element);
	

    	/*
    	 * accessed address is not the defined fault address or the trigger is set to
    	 * time- or pc-triggering.
    	 */
    	if ( (fault->params.address != (int)*addr
    		&& fault->params.cf_address != (int)*addr)
    		|| !strcmp(fault->trigger, "TIME")
    		|| !strcmp(fault->trigger, "PC"))
    	{
    		continue;
    	}

		#if defined(DEBUG_FAULT_CONTROLLER_TLB_FLUSH)
			printf("flushing tlb address %x\n", (int)*addr);
		#endif
		tlb_flush_page(env, (target_ulong)*addr);

    	/*
    	 * strcmp does not check a null-pointer - system will crash
    	 * in the case of a null-pointer.
    	 */
    	if (!fault->component || !fault->target || !fault->mode)
    		continue;

    	if (!strcmp(fault->component, "RAM")
    		&& (!strcmp(fault->target, "MEMORY CELL") || !strcmp(fault->target, "R/W LOGIC")))
    	{
#if defined(DEBUG_FAULT_CONTROLLER)
    		printf("FAULT INJECTED TRIGGERED TO %x with addr %x",(int)*addr, fault->params.address);
#endif
    		/* set/reset values */
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
   			fi_info.fault_on_address = 0;
   			fi_info.access_triggered_content_fault = 1;
   			fi_info.fault_on_register = 0;

   			log_cell_operations_memory(env, fault, addr, value, access_type);

#if defined(DEBUG_FAULT_CONTROLLER)
   		    printf("-----------------------START-------------------------------\n");
   			if (access_type == read_access_type)
   				printf("value read from cell before fault injection: 0x%08x\n", *value);
   			else if (access_type == write_access_type)
   				printf("value to write into cell before fault injection: 0x%08x\n", *value);
#endif

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			uint64_t value64 = *value;
    			fault_injection_controller_bf(env, &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			uint64_t value64 = *value;
    			fault_injection_controller_new_value(env,  &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "SF"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
					continue;
    			}
    			uint64_t value64 = *value;
    			fault_injection_controller_rs(env, &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "TF1") || !strcmp(fault->mode, "TF0") )
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_tf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "RDF1") || !strcmp(fault->mode, "RDF0"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_rdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "WDF0") || !strcmp(fault->mode, "WDF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_wdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "IRF0") || !strcmp(fault->mode, "IRF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_irf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "DRDF0") || !strcmp(fault->mode, "DRDF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_drdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "RDF00") || !strcmp(fault->mode, "RDF01")
    					|| !strcmp(fault->mode, "RDF10") || !strcmp(fault->mode, "RDF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_rdf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "IRF00") || !strcmp(fault->mode, "IRF01")
    					|| !strcmp(fault->mode, "IRF10") || !strcmp(fault->mode, "IRF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_irf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "DRDF00") || !strcmp(fault->mode, "DRDF01")
    					|| !strcmp(fault->mode, "DRDF10") || !strcmp(fault->mode, "DRDF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_drdf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFST00") || !strcmp(fault->mode, "CFST01")
    				    || !strcmp(fault->mode, "CFST10") || !strcmp(fault->mode, "CFST11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    				fault_injection_controller_cfst(env, fault, fi_info, addr, value, access_type, 0);
    			else if ((fault->params.address == (int)*addr))
    				fault_injection_controller_cfst(env, fault, fi_info, addr, value, access_type, 1);
    		}
    		else if (!strcmp(fault->mode, "CFDS0W00") || !strcmp(fault->mode, "CFDS0W01")
    				    || !strcmp(fault->mode, "CFDS1W10") || !strcmp(fault->mode, "CFDS1W11")
    				    || !strcmp(fault->mode, "CFDS0W10") || !strcmp(fault->mode, "CFDS0W11")
    				 	|| !strcmp(fault->mode, "CFDS1W00") || !strcmp(fault->mode, "CFDS1W01")
    				  	|| !strcmp(fault->mode, "CFDS0R00") || !strcmp(fault->mode, "CFDS0R01")
    					|| !strcmp(fault->mode, "CFDS1R10") || !strcmp(fault->mode, "CFDS1R11"))
    		{
    			if (*addr == fault->params.cf_address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
   				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
    				continue;
    			}
    			fault_injection_controller_cfds(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFTR00") || !strcmp(fault->mode, "CFTR01")
    					 || !strcmp(fault->mode, "CFTR10") || !strcmp(fault->mode, "CFTR11"))
    		{
    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
   				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
    				continue;
    			}
    			fault_injection_controller_cftr(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFWD00") || !strcmp(fault->mode, "CFWD01")
    					 || !strcmp(fault->mode, "CFWD10") || !strcmp(fault->mode, "CFWD11"))
    		{
    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
   				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
    				continue;
    			}
    			fault_injection_controller_cfwd(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFRD00") || !strcmp(fault->mode, "CFRD01")
    					 || !strcmp(fault->mode, "CFRD10") || !strcmp(fault->mode, "CFRD11"))
    		{
    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
			    {
#if defined(DEBUG_FAULT_CONTROLLER)
				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
				   continue;
			    }
    			fault_injection_controller_cfrd(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFIR00") || !strcmp(fault->mode, "CFIR01")
    					 || !strcmp(fault->mode, "CFIR10") || !strcmp(fault->mode, "CFIR11"))
    		{
    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
   				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
    				continue;
    			}
    			fault_injection_controller_cfir(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFDR00") || !strcmp(fault->mode, "CFDR01")
    					 || !strcmp(fault->mode, "CFDR10") || !strcmp(fault->mode, "CFDR11"))
    		{
    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
   				   printf("Accessed address is the aggressor cell and not the victim - Fault cannot be triggered!\n");
#endif
    				continue;
    			}
    			fault_injection_controller_cfdr(env, fault, fi_info, addr, value, access_type);
    		}
        	else
        	{
        		continue;
        	}

#if defined(DEBUG_FAULT_CONTROLLER)
   			unsigned memword = 0;
			uint8_t *membytes = (uint8_t *)&memword;
  			CPUState *cpu = ENV_GET_CPU(env);

    		if (fault->params.cf_address != -1)
    		{
       			cpu_memory_rw_debug(cpu, fault->params.cf_address, membytes, (MEMORY_WIDTH / 8), 0);
     			printf("coupled cell content after fault injection: 0x%08x\n", memword);
    		}

   			if (access_type == read_access_type)
   				printf("value read from cell after fault injection: 0x%08x\n", *value);
   			else if (access_type == write_access_type)
   				printf("value to write into cell after fault injection: 0x%08x\n", *value);

    	    cpu_memory_rw_debug(cpu, *addr, membytes, (MEMORY_WIDTH / 8), 0);
   		    printf("cell content after fault injection: 0x%08x\n", memword);
   		    printf("-----------------------END---------------------------------\n");
#endif
    	}
    	else
    	{
    		continue;
    	}
//
//		tlb_flush_page(env, (target_ulong)fault->params.address);
//		tlb_flush_page(env, (target_ulong)fault->params.cf_address);
    }
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a fault in the instruction decoder or instruction execution of the CPU
 * and if the accessed address is defined in the XML-file.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the instruction number.
 */
static void fault_injection_controller_insn(CPUArchState *env, hwaddr *addr)
{
	FaultList *fault;
	int element = 0;
	unsigned int insn;

	//printf("---------------------------HARTL------------------------------------------\n");
  	//printf("instruction number before fault injection: 0x%08x\n", (unsigned int)*addr);

	for (element = 0; element < getNumFaultListElements(); element++)
	{
		fault = getFaultListElement(element);

		//printf("---------------------------HARTL2------------------------------------------\n");
  		//printf("fault->params.address: 0x%08x\n", (unsigned int)fault->params.address);

    	/**
    	 * accessed address is not the defined fault address or the trigger is set to
    	 * time- or pc-triggering.
    	 */
	    if ( fault->params.address != (int)*addr || strcmp(fault->trigger, "ACCESS") )
	    	continue;

//printf("---------------------------HARTL3------------------------------------------\n");


    	/**
    	 * strcmp does not check a null-pointer - system will crash
    	 * in the case of a null-pointer.
    	 */
    	if (!fault->component || !fault->target || !fault->mode)
    		continue;

#if defined(DEBUG_FAULT_CONTROLLER)
		    printf("---------------------------START------------------------------------------\n");
   		    printf("instruction number before fault injection: 0x%08x\n", (unsigned int)*addr);
#endif

	    if (!strcmp(fault->component, "CPU") && !strcmp(fault->target, "INSTRUCTION DECODER"))
	    {
    		if (strcmp(fault->mode, "NEW VALUE"))
    		{
	   			fprintf(stderr, "error: only mode=\"NEW VALUE\" supported (fault id: %d)\n", fault->id);
	   			continue;
    		}

	   		if (fault->params.address == -1 || fault->params.instruction == -1)
	   		{
	   			fprintf(stderr, "error: address or instruction not defined (fault id: %d)\n", fault->id);
	   			continue;
	   		}

	   		fault_injection_controller_set_fault_active(fault, "cpu", 0);
	   		if (!fault->is_active)
	   			continue;

	    	/**
	    	 * different data types sizes - cast will crash the system!
	    	 */
	   		insn = (unsigned int)*addr;
	   		do_inject_insn(&insn, fault->params.instruction);
	   		*addr = insn;

	    }
	    else if (!strcmp(fault->component, "CPU") && !strcmp(fault->target, "INSTRUCTION EXECUTION"))
	    {
//printf("HARTL 4 enter INSTRUCTION EXECUTION");
    		if (strcmp(fault->mode, "NEW VALUE"))
    		{
	   			fprintf(stderr, "error: only mode=\"NEW VALUE\" supported (fault id: %d)\n", fault->id);
	   			continue;
    		}

	   		if (fault->params.address == -1 || fault->params.instruction != 0xDEADBEEF)
	   		{
	   			fprintf(stderr, "error: address or instruction (NOP = 0xDEADBEEF) "
	   					"for access-triggered faults not defined (fault id: %d)\n", fault->id);
	   			continue;
	   		}

	   		fault_injection_controller_set_fault_active(fault, "cpu", 0);
	   		if (!fault->is_active)
	   			continue;

	    	/**
	    	 * different data types sizes - cast will crash the system!
	    	 */
	   		insn = (unsigned long)*addr;
	    	/**
	    	 * the instruction number 0xE1A08008 defines a
	    	 * MOV R8, R8 is assembler syntax and defines a
	    	 * NOP-operation, which simulates a "no execution"
	    	 */
	   		do_inject_insn(&insn, 0xE1A08008);
	   		*addr = insn;
	    }

#if defined(DEBUG_FAULT_CONTROLLER)
   		    printf("instruction number after fault injection: 0x%08x\n", (unsigned int)*addr);
		    printf("---------------------------END--------------------------------------------\n");
#endif
	}
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a time- or pc-triggered fault and if the accessed address is defined in the XML-file.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the instruction number.
* @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_time(CPUArchState *env, hwaddr *addr,
																	int access_type)
{
	FaultList *fault;
	int element = 0;
	unsigned int pc = (unsigned long)*addr;
	hwaddr reg_mem_addr = 0;
    FaultInjectionInfo fi_info = {0, 0, 0, 0, 0, 0, 0};

	for (element = 0; element < getNumFaultListElements(); element++)
	{
		fault = getFaultListElement(element);

    	/**
    	 * trigger is not set to time- or pc-triggering.
    	 */
		if (!strcmp(fault->trigger, "ACCESS"))
			continue;

    	/**
    	 * strcmp does not check a null-pointer - system will crash
    	 * in the case of a null-pointer.
    	 */
    	if (!fault->component || !fault->target || !fault->mode)
    		continue;

	#if defined(DEBUG_FAULT_CONTROLLER)
	    printf("---------------------------START------------------------------------------\n");
		printf("pc before fault injection: 0x%08x\n", (unsigned int)*addr);
	#endif

		if (!strcmp(fault->component, "CPU") && !strcmp(fault->target, "CONDITION FLAGS"))
		{
			fault_injection_controller_set_fault_active(fault, "cpu", pc);

			if (!fault->is_active)
				continue;

			do_inject_condition_flags(env, fault->mode, fault->params.set_bit);
		}
		else if (!strcmp(fault->component, "CPU") && (!strcmp(fault->target, "INSTRUCTION DECODER")
																			|| !strcmp(fault->target, "INSTRUCTION EXECUTION")))
	    {
    		if (strcmp(fault->mode, "NEW VALUE"))
    		{
	   			fprintf(stderr, "error: only mode=\"NEW VALUE\" supported (fault id: %d)\n", fault->id);
	   			continue;
    		}

	   		if (fault->params.address == -1 && fault->params.instruction == -1)
	   		{
	   			fprintf(stderr, "error: PC- or instruction-address not defined (fault id: %d)\n", fault->id);
	   			continue;
	   		}

	   		fault_injection_controller_set_fault_active(fault, "cpu", pc);
	   		if (!fault->is_active)
	   			continue;

	   		/**
	   		 * overwrites the pc directly in the CPUArchState.
	   		 * This is needed, because the pc is not accessed
	   		 * at this time (time- triggering).
	   		 */
	   		do_inject_look_up_error(env, fault->params.instruction);
	    }
		else if (!strcmp(fault->component, "REGISTER")
		    		&& !strcmp(fault->target, "REGISTER CELL"))
		{
	   		/**
	   		 * overwrites the value in register or memory directly
	   		 * through CPUArchState. This is needed, because
	   		 * the value is not accessed at this time (time-
	   		 * or pc-triggering).
	   		 */
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
   			fi_info.fault_on_address = 0;
   			fi_info.access_triggered_content_fault = 0;
   			fi_info.fault_on_register = 1;

   			/**
   			 * accessed memory or register address is stored
   			 * it the instruction-variable, because the address
   			 * variable contains the pc-value.
   			 */
   			reg_mem_addr = fault->params.instruction;
	#if defined(DEBUG_FAULT_CONTROLLER)
   			unsigned memword = 0;
    	    memword = read_cpu_register(env, reg_mem_addr);
			printf("injecting fault on register %d with initial content 0x%08x\n", fault->params.instruction, memword);
	#endif

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_bf(env, &reg_mem_addr, fault, fi_info, pc);
    		}
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_new_value(env, &reg_mem_addr, fault, fi_info, pc);
    		}
    		else if (!strcmp(fault->mode, "SF"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
					continue;
    			}
    			fault_injection_controller_rs(env, &reg_mem_addr, fault, fi_info, pc);
    		}
#if defined(DEBUG_FAULT_CONTROLLER)
			printf("fault status: %d (1-active, 0-inactive)\n", fault->is_active);

    	    memword = read_cpu_register(env, reg_mem_addr);
   		    printf("cell content after fault injection: 0x%08x\n", memword);
#endif
		}
		else if (!strcmp(fault->component, "RAM")
		    && (!strcmp(fault->target, "MEMORY CELL") || !strcmp(fault->target, "R/W LOGIC")))
    	{
    		/**
    		 * set/reset values
    		 */
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
   			fi_info.fault_on_address = 0;
   			fi_info.access_triggered_content_fault = 0;
   			fi_info.fault_on_register = 0;

   			reg_mem_addr = fault->params.instruction;
	#if defined(DEBUG_FAULT_CONTROLLER)
   			unsigned memword = 0;
			uint8_t *membytes = (uint8_t *)&memword;
  			CPUState *cpu = ENV_GET_CPU(env);

    	    cpu_memory_rw_debug(cpu, reg_mem_addr, membytes, (MEMORY_WIDTH / 8), 0);
   			printf("injecting fault on memory cell: 0x%08x with initial content 0x%08x\n",
   					fault->params.instruction, memword);
	#endif

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_bf(env, &reg_mem_addr, fault, fi_info, pc);
    		}
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_new_value(env, &reg_mem_addr, fault, fi_info, pc);
    		}
    		else if (!strcmp(fault->mode, "SF"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
					continue;
    			}
    			fault_injection_controller_rs(env, &reg_mem_addr, fault, fi_info, pc);
    		}
#if defined(DEBUG_FAULT_CONTROLLER)
			printf("fault status: %d (1-active, 0-inactive)\n", fault->is_active);

    		memword = 0;
			membytes = (uint8_t *)&memword;

    	    cpu_memory_rw_debug(cpu, reg_mem_addr, membytes, (MEMORY_WIDTH / 8), 0);
   		    printf("cell content after fault injection: 0x%08x\n", memword);
#endif
    	}
	#if defined(DEBUG_FAULT_CONTROLLER)
				printf("pc after fault injection: 0x%08x\n", (unsigned int)*addr);
			printf("---------------------------END--------------------------------------------\n");
	#endif
	}
}

/**
 * Stores the previous access-operations of a defined fault register address. This information
 * is used for deciding, if a dynamic fault should be triggered or not.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] fault - pointer to the linked list entry.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void log_cell_operations_register(CPUArchState *env, FaultList *fault, hwaddr *addr,
																		uint32_t *value, AccessType access_type)
{
	unsigned memword = 0, mask = 0, set_bit, bit_pos = 0, id = 0;

	/**
	 * only a write access can trigger a dynamic fault
	 */
   	if (access_type == write_access_type)
   	{
   		memword = read_cpu_register(env, *addr);
   		mask = fault->params.mask;

		/**
		 * search the set bits in mask (integer)
		 */
		while (mask)
		{
    		/**
    		 * extract least significant bit of 2s complement
    		 */
			set_bit = mask & -mask;

    		/**
    		 * toggle the bit off
    		 */
			mask ^= set_bit ;

    		/**
    		 * determine the position of the set bit
    		 */
			bit_pos = (uint32_t) log2(set_bit);
			id = fault->id - 1;

			if (!!(memword & set_bit) == 0 && !!(*value & set_bit) == 0)
				ops_on_register_cell[id][bit_pos] = OPs_0w0;
			else if (!!(memword & set_bit) == 0 && !!(*value & set_bit) == 1)
				ops_on_register_cell[id][bit_pos] = OPs_0w1;
			else if (!!(memword & set_bit) == 1 && !!(*value & set_bit) == 0)
				ops_on_register_cell[id][bit_pos] = OPs_1w0;
			else if (!!(memword & set_bit) == 1 && !!(*value & set_bit) == 1)
				ops_on_register_cell[id][bit_pos] = OPs_1w1;
			else
				ops_on_register_cell[id][bit_pos] = -1;
		}
   	}
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a fault in the register cells of a register bank and if the accessed
 * address is defined in the XML-file.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 */
static void fault_injection_controller_register_content(CPUArchState *env, hwaddr *addr,
																		uint32_t *value, AccessType access_type)
{
    FaultList *fault;
    int element = 0;
    FaultInjectionInfo fi_info;

    for (element = 0; element < getNumFaultListElements(); element++)
    {
    	fault = getFaultListElement(element);

    	/**
    	 * accessed address is not the defined fault address or the trigger is set to
    	 * time- or pc-triggering.
    	 */
    	if ( (fault->params.address != (int)*addr
    		&& fault->params.cf_address != (int)*addr)
    		|| !strcmp(fault->trigger, "TIME")
    		|| !strcmp(fault->trigger, "PC"))
    	{
    		continue;
    	}

    	/**
    	 * strcmp does not check a null-pointer - system will crash
    	 * in the case of a null-pointer.
    	 */
    	if (!fault->component || !fault->target || !fault->mode)
    		continue;

    	if (!strcmp(fault->component, "REGISTER")
    		&& !strcmp(fault->target, "REGISTER CELL"))
    	{
    		/**
    		 *  set/reset values
    		 */
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
   			fi_info.fault_on_address = 0;
   			fi_info.access_triggered_content_fault = 1;
   			fi_info.fault_on_register = 1;

   			log_cell_operations_register(env, fault, addr, value, access_type);

#if defined(DEBUG_FAULT_CONTROLLER)
   		    printf("-----------------------START-------------------------------\n");
   			if (access_type == read_access_type)
   				printf("value read from cell before fault injection: 0x%08x\n", *value);
   			else if (access_type == write_access_type)
   				printf("value to write into cell before fault injection: 0x%08x\n", *value);
#endif

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			uint64_t value64 = *value;
    			fault_injection_controller_bf(env, &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			uint64_t value64 = *value;
    			fault_injection_controller_new_value(env,  &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "SF"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
					continue;
    			}

    			uint64_t value64 = *value;
    			fault_injection_controller_rs(env, &value64, fault, fi_info, 0);
    			*value = value64;
    		}
    		else if (!strcmp(fault->mode, "TF1") || !strcmp(fault->mode, "TF0") )
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_tf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "RDF1") || !strcmp(fault->mode, "RDF0"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_rdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "WDF0") || !strcmp(fault->mode, "WDF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_wdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "IRF0") || !strcmp(fault->mode, "IRF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_irf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "DRDF0") || !strcmp(fault->mode, "DRDF1"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_drdf(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "RDF00") || !strcmp(fault->mode, "RDF01")
    					|| !strcmp(fault->mode, "RDF10") || !strcmp(fault->mode, "RDF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_rdf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "IRF00") || !strcmp(fault->mode, "IRF01")
    					|| !strcmp(fault->mode, "IRF10") || !strcmp(fault->mode, "IRF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_irf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "DRDF00") || !strcmp(fault->mode, "DRDF01")
    					|| !strcmp(fault->mode, "DRDF10") || !strcmp(fault->mode, "DRDF11"))
    		{
    			if (fault->params.cf_address == (int)*addr)
    			{
    				fprintf(stderr, "error: CF address defined without CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}
    			fault_injection_controller_drdf_dyn(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFST00") || !strcmp(fault->mode, "CFST01")
    				    || !strcmp(fault->mode, "CFST10") || !strcmp(fault->mode, "CFST11"))
    		{
    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			if (fault->params.cf_address == (int)*addr)
    				fault_injection_controller_cfst(env, fault, fi_info, addr, value, access_type, 0);
    			else if ((fault->params.address == (int)*addr))
    				fault_injection_controller_cfst(env, fault, fi_info, addr, value, access_type, 1);
    		}
    		else if (!strcmp(fault->mode, "CFDS0W00") || !strcmp(fault->mode, "CFDS0W01")
    				    || !strcmp(fault->mode, "CFDS1W10") || !strcmp(fault->mode, "CFDS1W11")
    				    || !strcmp(fault->mode, "CFDS0W10") || !strcmp(fault->mode, "CFDS0W11")
    				 	|| !strcmp(fault->mode, "CFDS1W00") || !strcmp(fault->mode, "CFDS1W01")
    				  	|| !strcmp(fault->mode, "CFDS0R00") || !strcmp(fault->mode, "CFDS0R01")
    					|| !strcmp(fault->mode, "CFDS1R10") || !strcmp(fault->mode, "CFDS1R11"))
    		{
    			/* only an operation on the aggressor cell can change
    			 * the content of the victim cell */
    			if (fault->params.cf_address == (int)*addr &&  fault->params.address != fault->params.cf_address)
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing victim cell - only operation on aggressor cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cfds(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFTR00") || !strcmp(fault->mode, "CFTR01")
    					 || !strcmp(fault->mode, "CFTR10") || !strcmp(fault->mode, "CFTR11"))
    		{
    			/* only an operation on the victim cell can change
    			 * the content of the aggressor cell */
    			if (fault->params.address == (int)*addr &&  fault->params.address != fault->params.cf_address)
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing aggressor cell - only operation on victim cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cftr(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFWD00") || !strcmp(fault->mode, "CFWD01")
    					 || !strcmp(fault->mode, "CFWD10") || !strcmp(fault->mode, "CFWD11"))
    		{
    			/* only an operation on the victim cell can change
    			 * the content of the aggressor cell */
    			if (fault->params.address == (int)*addr &&  fault->params.address != fault->params.cf_address)
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing aggressor cell - only operation on victim cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cfwd(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFRD00") || !strcmp(fault->mode, "CFRD01")
    					 || !strcmp(fault->mode, "CFRD10") || !strcmp(fault->mode, "CFRD11"))
    		{
    			/**
    			 * only an operation on the victim cell can change
    			 * the content of the aggressor cell
    			 */

    			if (*addr == fault->params.address &&  fault->params.address != fault->params.cf_address )
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing aggressor cell - only operation on victim cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cfrd(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFIR00") || !strcmp(fault->mode, "CFIR01")
    					 || !strcmp(fault->mode, "CFIR10") || !strcmp(fault->mode, "CFIR11"))
    		{
    			/*
    			 * only an operation on the victim cell can change
    			 * the content of the aggressor cell
    			 */
    			if (fault->params.address == (int)*addr &&  fault->params.address != fault->params.cf_address)
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing aggressor cell - only operation on victim cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cfir(env, fault, fi_info, addr, value, access_type);
    		}
    		else if (!strcmp(fault->mode, "CFDR00") || !strcmp(fault->mode, "CFDR01")
    					 || !strcmp(fault->mode, "CFDR10") || !strcmp(fault->mode, "CFDR11"))
    		{
    			/*
    			 * only an operation on the victim cell can change
    			 * the content of the aggressor cell
    			 */
    			if (fault->params.address == (int)*addr &&  fault->params.address != fault->params.cf_address)
    			{
#if defined(DEBUG_FAULT_CONTROLLER)
    				printf("Accessing aggressor cell - only operation on victim cell can change content - Skipping!\n");
#endif
    				continue;
    			}

    			if (fault->params.cf_address == -1)
    			{
    				fprintf(stderr, "error: CF address not defined for CF-mode (fault id: %d)\n", fault->id);
    				continue;
    			}

    			fault_injection_controller_cfdr(env, fault, fi_info, addr, value, access_type);
    		}
        	else
        	{
        		continue;
        	}

#if defined(DEBUG_FAULT_CONTROLLER)
			printf("fault status: %d (1-active, 0-inactive)\n", fault->is_active);

   			unsigned memword = 0;

    		if (fault->params.cf_address != -1)
    		{
        	    memword = read_cpu_register(env, fault->params.cf_address);
     			printf("coupled cell content after fault injection: 0x%08x\n", memword);
    		}

   			if (access_type == read_access_type)
   				printf("value read from cell after fault injection: 0x%08x\n", *value);
   			else if (access_type == write_access_type)
   				printf("value to write into cell after fault injection: 0x%08x\n", *value);

    	    memword = read_cpu_register(env, *addr);
   		    printf("cell content after fault injection: 0x%08x\n", memword);
   		    printf("-----------------------END---------------------------------\n");
#endif
    	}
    	else
    	{
    		continue;
    	}
    }
}

/**
 * Iterates through the linked list and checks if an entry or element belongs to
 * a fault in the address decoder of the register banks and if the accessed
 * address is defined in the XML-file.
 *
 * @param[in] addr - the address of the register, where the fault is injected.
 */
static void fault_injection_controller_register_address(CPUArchState *env, hwaddr *addr)
{
    FaultList *fault;
    int element = 0;
    FaultInjectionInfo fi_info = {0, 0, 0, 0, 0, 0, 0};

    for (element = 0; element < getNumFaultListElements(); element++)
    {
    	fault = getFaultListElement(element);

		/**
		 * accessed address is not the defined fault address or the trigger is set to
		 * time- or pc-triggering.
		 */
    	if ( fault->params.address != (int)*addr || strcmp(fault->trigger, "ACCESS") )
    		continue;

    	/**
    	 * strcmp does not check a null-pointer - system will crash
    	 * in the case of a null-pointer.
    	 */
    	if (!fault->component || !fault->target || !fault->mode)
    		continue;

    	if (!strcmp(fault->component, "REGISTER") && !strcmp(fault->target, "ADDRESS DECODER"))
    	{
#if defined(DEBUG_FAULT_CONTROLLER)
   		    printf("-----------------------START-------------------------------\n");
			printf("register address before fault injection: 0x%08x\n", (unsigned int) *addr);
#endif
    		/**
    		 *  set/reset values
    		 */
    		fi_info.new_value = 0;
    		fi_info.bit_flip = 0;
    		fi_info.fault_on_address = 1;
    		fi_info.fault_on_register = 1;

    		if (!strcmp(fault->mode, "BIT-FLIP"))
    			fault_injection_controller_bf(env, addr, fault, fi_info, 0);
    		else if (!strcmp(fault->mode, "NEW VALUE"))
    			fault_injection_controller_new_value(env, addr, fault, fi_info, 0);
    		else if (!strcmp(fault->mode, "SF"))
    			fault_injection_controller_rs(env, addr, fault, fi_info, 0);

#if defined(DEBUG_FAULT_CONTROLLER)
			printf("fault status: %d (1-active, 0-inactive)\n", fault->is_active);
			printf("register address before fault injection: 0x%08x\n", (unsigned int) *addr);
   		    printf("-----------------------END-------------------------------\n");
#endif
    	}
    }
}

void start_automatic_test_process(CPUArchState *env)
{
	static int already_set = 0, shutting_down = 0;
	//int sbst_cycle_count_value = 0;
	//int file_input_to_use_value = 0;
	//uint8_t *membytes = (uint8_t *)&sbst_cycle_count_value;
	//uint8_t *membytes_file_input = (uint8_t *)&file_input_to_use_value;
	CPUState *cpu;

	//error_report("start automatic test process\n");
	

	// GET SBSCT_CYCLE_COUNT
	
	


	//if (!sbst_cycle_count_address || !file_input_to_use_address)
	//	return;

	//if (env)
	//{
		//cpu = ENV_GET_CPU(env);
		//cpu_memory_rw_debug(cpu, sbst_cycle_count_address, membytes, 4, 0);
		//cpu_memory_rw_debug(cpu, file_input_to_use_address, membytes_file_input, 4, 0);
	//}

		
	// SET INPUT ID

	/*if (file_input_to_use_value == -1 && !file_input_already_set){

		file_input_already_set=1;
		set_input_file_to_use(file_input_to_use);
	}*/


	if (!already_set)
	{
		already_set = 1;
		hmp_fault_reload(qemu_serial_monitor, NULL);
	}


	//if (sbst_cycle_count_value > SBST_CYCLES_BEFORE_EXIT && !shutting_down)
	if (false && !shutting_down)
	{
		shutting_down = 1;
		fclose(outfile);

		hmp_info_faults(qemu_serial_monitor, NULL);

		if (env)
		{
			cpu = ENV_GET_CPU(env);
			cpu_dump_state(cpu, (FILE *)qemu_serial_monitor, (fprintf_function) monitor_printf, CPU_DUMP_FPU);
		}

	    qmp_quit(NULL);
	}
}

/**
 * Implements the interface to the appropriate controller functions.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - the address of the accessed cell.
 * @param[in] value -  the value or buffer, which should be written to register or memory.
 * @param[in] injection_mode - defines the location, where the function is called from.
 * @param[in] access_type - if the access-operation is a write, read or execute.
 *
 */
void fault_injection_controller_init(CPUArchState *env, hwaddr *addr,
												uint32_t *value, InjectionMode injection_mode,
												AccessType access_type)
{
    FaultList *fault;
    int element = 0;

    profiler_log(env, addr, value, access_type);

	if (*addr == address_in_use)
		return;

	if (injection_mode == FI_MEMORY_ADDR)
	{
		fault_injection_controller_memory_address(env, addr);
	}
	else if (injection_mode == FI_MEMORY_CONTENT)
	{
		if (env)
		{
			fault_injection_controller_memory_content(env, addr, value, access_type);
			return;
		}

		/**
		 * get the CPUArchState of the current CPU (if not defined)
		 */
		if (next_cpu == NULL)
			next_cpu = first_cpu;

		for (; next_cpu != NULL && !exit_request; next_cpu = CPU_NEXT(next_cpu))
		{
			CPUState *cpu = next_cpu;
			CPUArchState *env = cpu->env_ptr;

			fault_injection_controller_memory_content(env, addr, value, access_type);
		}

	}
	else if (injection_mode == FI_INSN)
	{
		fault_injection_controller_insn(env, addr);
	}
	else if (injection_mode == FI_REGISTER_ADDR)
	{
		fault_injection_controller_register_address(env, addr);
	}
	else if (injection_mode == FI_REGISTER_CONTENT)
	{
		fault_injection_controller_register_content(env, addr, value, access_type);
	}
	else if (injection_mode == FI_TIME)
	{
	    for (element = 0; element < getNumFaultListElements(); element++)
	    {
	    	fault = getFaultListElement(element);

    		#if defined(DEBUG_FAULT_CONTROLLER_TLB_FLUSH)
	    		printf("flushing tlb address %x\n", fault->params.address);
			#endif
    		tlb_flush_page(env, (target_ulong)fault->params.address);
    		tlb_flush_page(env, (target_ulong)fault->params.cf_address);
	    }

		fault_injection_controller_time(env, addr, access_type);
	}
	else
		fprintf(stderr, "Unknown fault injection target!\n");
}

void setMonitor(Monitor *mon)
{
	qemu_serial_monitor = mon;
}

