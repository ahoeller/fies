/*
 * fault-injection-injector.c
 *
 *  Created on: 17.08.2014
 *      Author: Gerhard Schoenfelder
 */

#include "fault-injection-injector.h"
#include "fault-injection-config.h"

/**
 * Performs a bit-flip on the content of a specified general-purpose register or
 * on the content of the CPSR-register.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] register_num - register number, on which a bit-flip should
 *                                           be performed (0-15 for general-purpose register
 *                                           r0 to r15 and  cpsr-register otherwise.)
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_register_arm_bf(CPUARMState *env, int register_num,
																FaultInjectionInfo fi_info)
{
	int cpsr_value = 0;

    if (register_num < 16)
    {
    	/* bit-flip in general purpose registers*/
        if ( (env->regs[register_num] >> fi_info.injected_bit) & 0x1)
            env->regs[register_num] &= ~(1 << fi_info.injected_bit);
        else
            env->regs[register_num] |= (1 << fi_info.injected_bit);
    }
    else
    {
    	/* bit-flip in CPSR*/
    	cpsr_value =  cpsr_read(env);

        if ( (cpsr_read(env) >> fi_info.injected_bit) & 0x1)
        {
        	cpsr_value &= ~(1 << fi_info.injected_bit);
            cpsr_write(env, cpsr_value, 0xFFFFFFFF);
        }
        else
        {
        	cpsr_value |= (1 << fi_info.injected_bit);
            cpsr_write(env, cpsr_value, 0xFFFFFFFF);
        }
    }
}

/**
 * Sets or resets a specified bit in the content of a specified general-purpose register or
 * CPSR-register.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] register_num - register number, on which a bit-flip should
 *                                           be performed (0-15 for general-purpose register
 *                                           r0 to r15 and  cpsr-register otherwise.)
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_register_arm_rs(CPUARMState *env, int register_num,
													FaultInjectionInfo fi_info)
{
	int cpsr_value = 0;

    if (register_num < 16)
    {
    	if (fi_info.bit_value)
    		env->regs[register_num] |= (1 << fi_info.injected_bit);
    	else
    		env->regs[register_num] &= ~(1 << fi_info.injected_bit);
    }
    else
    {
    	/* bit-flip in CPSR*/
    	cpsr_value =  cpsr_read(env);

    	if (fi_info.bit_value)
    	{
    		cpsr_value |= (1 << fi_info.injected_bit);
    	    cpsr_write(env, cpsr_value, 0xFFFFFFFF);
    	}
    	else
    	{
    		cpsr_value &= ~(1 << fi_info.injected_bit);
    	    cpsr_write(env, cpsr_value, 0xFFFFFFFF);
    	}
    }
}

/**
 * Writes a new value to a specified general-purpose register or
 * CPSR-register.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] register_num - register number, on which a bit-flip should
 *                                           be performed (0-15 for general-purpose register
 *                                           r0 to r15 and  cpsr-register otherwise.)
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_new_register_value_arm(CPUARMState *env, int register_num,
																				FaultInjectionInfo fi_info)
{
    if (register_num < 16)
   		env->regs[register_num] = fi_info.bit_value;
    else
   	    cpsr_write(env, fi_info.bit_value, 0xFFFFFFFF);
}

/**
 * Decides, based on the information held by fi_info, which function should be called.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - containing the register number.
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_register_arm(CPUARMState *env, hwaddr *addr,
															FaultInjectionInfo fi_info)
{
	if (fi_info.bit_flip)
		do_inject_register_arm_bf(env, (int)*addr, fi_info);
	else if (!fi_info.bit_flip && !fi_info.new_value)
		do_inject_register_arm_rs(env,  (int)*addr, fi_info);
	else if (!fi_info.bit_flip && fi_info.new_value)
		do_inject_new_register_value_arm(env,  (int)*addr, fi_info);
}

/**
 * Overwrites the program counter to perform an look-up error.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] lockup_instruction - the PC-value for the new instruction.
 */
static void do_inject_look_up_error_arm(CPUARMState *env,
											unsigned lockup_instruction)
{
    static unsigned lockup_pc = 0;
    static unsigned saved_insn = 0;

    CPUState *cpu = ENV_GET_CPU(env);

    if (!saved_insn)
    {
        unsigned memword;
        uint8_t *membytes = (uint8_t *)&memword;

        // Change the instruction pointed to by PC and replace with
        // lockup instruction
        cpu_memory_rw_debug(cpu, env->regs[15], membytes, 4, 0);

        saved_insn = memword;
        memword = lockup_instruction;
        cpu_memory_rw_debug(cpu, env->regs[15], membytes, 4, 1);
        lockup_pc = env->regs[15];
    }
    else
    {
        // Restore PC
        if (env->regs[15] != lockup_pc)
        {
            uint8_t *membytes = (uint8_t *)&saved_insn;
            cpu_memory_rw_debug(cpu, lockup_pc, membytes, 4, 1);
            saved_insn = 0;
        }
    }
}

/**
 * Calls the appropriate function for the used CPU-model.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] lockup_instruction - the PC-value for the new instruction.
 */
void do_inject_look_up_error(CPUArchState *env, unsigned lockup_instruction)
{
	#if defined(TARGET_ARM)
		do_inject_look_up_error_arm(env, lockup_instruction);
	#else
		#error unsupported target CPU
	#endif
}

/**
 * Sets or resets the value of a specified condition flag.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] src_flag_name - the name of the condition flag on which a
 *                                             fault should be injected.
 * @param[in] new_flag_value - 0 for reseting, 1 for setting the condition flag
 */
static void do_inject_condition_flags_arm(CPUARMState *env,
													const char* src_flag_name, int new_flag_value)
{
	if (new_flag_value != 0 && new_flag_value != 1)
		return;

	if ( !strcmp("CF", src_flag_name) )
	{
		/* Carry flag */
		//env->CF = new_flag_value;
		if (new_flag_value)
			cpsr_write(env, (1 << 29), (1 << 29));
		else
			cpsr_write(env, 0,  (1 << 29));
	}
	else if ( !strcmp("NF", src_flag_name) )
	{
		/* Negative or Less than */
		//env->NF = new_flag_value;
		if (new_flag_value)
			cpsr_write(env, (1 << 29), (1 << 29));
		else
			cpsr_write(env, 0,  (1 << 29));
	}
	else if ( !strcmp("QF", src_flag_name) )
	{
		/* Sticky overflow */
		//env->QF = new_flag_value;
		if (new_flag_value)
			cpsr_write(env, (1 << 27), (1 << 27));
		else
			cpsr_write(env, 0,  (1 << 27));
	}
	else if ( !strcmp("VF", src_flag_name) )
	{
		/* Overflow */
		//env->VF = new_flag_value;
		if (new_flag_value)
			cpsr_write(env, (1 << 28), (1 << 28));
		else
			cpsr_write(env, 0,  (1 << 28));
	}
	else if ( !strcmp("ZF", src_flag_name) )
	{
		/* Zero flag */
		//env->ZF = new_flag_value;
		if (new_flag_value)
			cpsr_write(env, (1 << 30), (1 << 30));
		else
			cpsr_write(env, 0,  (1 << 30));
	}
	else
	{
		fprintf(stderr, "error: unknown condition flag\n");
		return;
	}
}

/**
 * Calls the appropriate function for the used CPU-model.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] src_flag_name - the name of the condition flag on which a
 *                                             fault should be injected.
 * @param[in] new_flag_value - 0 for reseting, 1 for setting the condition flag
 */
void do_inject_condition_flags(CPUArchState *env,
								const char* src_flag_name, int new_flag_value)
{
	#if defined(TARGET_ARM)
		do_inject_condition_flags_arm(env, src_flag_name, new_flag_value);
	#else
		#error unsupported target CPU
	#endif
}

/**
 * Replaces the current instruction number with a new one.
 *
 * @param[in] orig_insn - reference to the current instruction number.
 * @param[in] repl_insn - value of the new instruction  number.
 */
static void do_inject_insn_arm(unsigned int *orig_insn,
														unsigned int repl_insn)
{
	*orig_insn = repl_insn;
}

/**
 * Calls the appropriate function the the used CPU-model.
 *
 * @param[in] orig_insn - reference to the current instruction number.
 * @param[in] repl_insn - value of the new instruction  number.
 */
void do_inject_insn(unsigned int *orig_insn,
									unsigned int repl_insn)
{
	#if defined(TARGET_ARM)
		do_inject_insn_arm(orig_insn, repl_insn);
	#else
		#error unsupported target CPU
	#endif
}

/**
 * Performs a bit-flip on the specified address or buffer value at a specified bit-
 * position.
 *
 * @param[in] value - the address or the buffer, where the fault is injected.
 * @param[in] injected_bit - the position of the affected bit.
 */
static void do_inject_memory_buffer_arm_bf(hwaddr *value,
																				int injected_bit)
{
    if ( (*value >> injected_bit) & 0x1)
    	*value &= ~(1 << injected_bit);
    else
        *value |= (1 << injected_bit);
}

/**
 * Performs a set or reset on the specified address or buffer value at a specified bit-
 * position.
 *
 * @param[in] value - the address or the buffer, where the fault is injected.
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_memory_buffer_arm_rs(hwaddr *value,
																			FaultInjectionInfo fi_info)
{
	if (fi_info.bit_value)
		*value |= (1 << fi_info.injected_bit);
	else
		*value &= ~(1 << fi_info.injected_bit);
}

/**
 * Writes a new value to a specified address or buffer value.
 *
 * @param[in] orig_value - the address or the buffer, where the fault is injected.
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_new_memory_value_buffer_arm(hwaddr *orig_value,
																					FaultInjectionInfo fi_info)
{
	*orig_value = fi_info.bit_value;
}

/**
 * Performs a bit-flip on the memory content of a specified address.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] inject_address - the address of the memory cell, where
 *                                             the fault should be injected.
 * @param[in] injected_bit - the position of the affected bit.
 */
static void do_inject_memory_arm_bf(CPUARMState *env, hwaddr inject_address,
																	int injected_bit)
{
     CPUState *cpu = ENV_GET_CPU(env);

     unsigned memword;
     uint8_t *membytes = (uint8_t *)&memword;

     // Read memory
     cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 0);

     // Flip bit and write back
     if ((memword  >> injected_bit) & 0x1)
     {
         memword &= ~(1 << injected_bit);
         cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 1);
     }
     else
     {
         memword |= (1 << injected_bit);
         cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 1);
     }
}

/**
 * Performs a set or reset on the content of a specified memory address.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] inject_address - the address of the memory cell, where
 *                                             the fault should be injected.
 * @param[in] injected_bit - the position of the affected bit.
 */
static void do_inject_memory_arm_rs(CPUARMState *env, hwaddr inject_address,
																	FaultInjectionInfo fi_info)
{
    CPUState *cpu = ENV_GET_CPU(env);

    unsigned memword;
    uint8_t *membytes = (uint8_t *)&memword;

    // Read memory
    cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 0);

    if (fi_info.bit_value)
    	memword |= (1 << fi_info.injected_bit);
    else
    	memword &= ~(1 << fi_info.injected_bit);

    cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 1);
}

/**
 * Writes a new value to the content of a specified memory address.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] inject_address - the address of the memory cell, where
 *                                             the fault should be injected.
 * @param[in] injected_bit - the position of the affected bit.
 */
static void do_inject_new_memory_value_arm(CPUARMState *env,  hwaddr inject_address,
																				FaultInjectionInfo fi_info)
{
    CPUState *cpu = ENV_GET_CPU(env);
    uint8_t *membytes = (uint8_t *)&fi_info.bit_value;

    cpu_memory_rw_debug(cpu, inject_address, membytes, (MEMORY_WIDTH / 8), 1);
}

/**
 * Decides, based on the information held by fi_info, which function should be called.
 *
 * @param[in] addr - containing the memory address.
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_memory_buffer_arm(hwaddr *addr,
																FaultInjectionInfo fi_info)
{
	if (fi_info.bit_flip)
		do_inject_memory_buffer_arm_bf(addr, fi_info.injected_bit);
	else if (!fi_info.bit_flip && !fi_info.new_value)
		do_inject_memory_buffer_arm_rs(addr, fi_info);
	else if (!fi_info.bit_flip && fi_info.new_value)
		do_inject_new_memory_value_buffer_arm(addr, fi_info);
}


/**
 * Decides, based on the information held by fi_info, which function should be called.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - containing the memory address.
 * @param[in] fi_info - information for performing faults.
 */
static void do_inject_memory_arm(CPUARMState *env, hwaddr *addr,
															FaultInjectionInfo fi_info)
{
	if (fi_info.bit_flip)
		do_inject_memory_arm_bf(env, *addr, fi_info.injected_bit);
	else if (!fi_info.bit_flip && !fi_info.new_value)
		do_inject_memory_arm_rs(env, *addr, fi_info);
	else if (!fi_info.bit_flip && fi_info.new_value)
		do_inject_new_memory_value_arm(env, *addr, fi_info);
}

/**
 * Calls the appropriate function for the used CPU-model and decides,
 * based on the information held by fi_info, if the fault injection is
 * performed on a register or a memory.
 *
 * @param[in] env - Reference to the information of the CPU state.
 * @param[in] addr - containing the memory address.
 * @param[in] fi_info - information for performing faults.
 */
void do_inject_memory_register(CPUArchState *env, hwaddr *addr,
														FaultInjectionInfo fi_info)
{
	#if defined(TARGET_ARM)
		if (fi_info.fault_on_register)
		{
			if (fi_info.fault_on_address || fi_info.access_triggered_content_fault)
				do_inject_memory_buffer_arm(addr, fi_info);
			else
				do_inject_register_arm(env, addr, fi_info);
		}
		else
		{
			if (fi_info.fault_on_address || fi_info.access_triggered_content_fault)
				do_inject_memory_buffer_arm(addr, fi_info);
			else
				do_inject_memory_arm(env, addr, fi_info);
		}
	#else
		#error unsupported target CPU
	#endif
}
