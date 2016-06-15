#include "fault-injection-data-analyzer.h"
#include "fault-injection-controller.h"
#include "fault-injection-config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

 /**
  * The variables for counting injected faults at different
  * fault components and types.
  */
 static int num_injected_faults = 0;
 static int num_injected_faults_ram_trans = 0;
 static int num_injected_faults_ram_perm = 0;
 static int num_injected_faults_cpu_trans = 0;
 static int num_injected_faults_cpu_perm = 0;
 static int num_injected_faults_register_trans = 0;
 static int num_injected_faults_register_perm = 0;

 static CPUState *next_cpu;
 /**
  * The id array decides, if the number of fault should be
  * incremented or not. This is necessary, because otherwise a transient
  * fault is active many times but the SBST only detect it one or a few times.
  * Hence the diagnostic coverage is distorted by incrementing the injected
  * fault number.
  */
 static int *id_array;

 /**
  * Increments a specified fault type (e.g. transient ram faults)
  *
  * @param[in] id - the id of the fault
  * @param[out] fault_type - the type of the fault (e.g. ram trans
  * for transient ram faults, cpu perm for permanent cpu faults or
  * reg trans  for transient register faults.
  */
void incr_num_injected_faults(int id, const char* fault_type)
{
	id -= 1;

	if (id_array[id])
		return;

	num_injected_faults++;

	if (!strcmp(fault_type, "ram trans"))
		num_injected_faults_ram_trans++;
	else if (!strcmp(fault_type, "ram perm"))
		num_injected_faults_ram_perm++;
	else if (!strcmp(fault_type, "cpu trans"))
		num_injected_faults_cpu_trans++;
	else if (!strcmp(fault_type, "cpu perm"))
		num_injected_faults_cpu_perm++;
	else if (!strcmp(fault_type, "reg trans"))
		num_injected_faults_register_trans++;
	else if (!strcmp(fault_type, "reg perm"))
		num_injected_faults_register_perm++;
	else
	{
		fprintf(stderr, "error: unknown fault type: %s\n", fault_type);
		return;
	}

	id_array[id] = 1;
}

/**
 * Allocates the id array.
 *
 * @param[in] size - the maximal number of faults (max_id)
 */
void init_id_array(int size)
{
	int i = 0;

	id_array = (int*) malloc(size * sizeof(int*));
	for (i = 0; i < size; i++)
		id_array[i] = 0;
}

/**
 * Deletes the allocated array.
 *
 */
void destroy_id_array(void)
{
	if (id_array)
		free(id_array);
}

/**
 * Sets the number of injected faults to a specified value (is
 * used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults
 */
void set_num_injected_faults(int num)
{
	num_injected_faults = num;
}

/**
 * Returns the  number of  injected  faults.
 *
 * @param[out] the number of injected faults
 */
int get_num_injected_faults(void)
{
	return num_injected_faults;
}

/**
 * Returns the  number of  detected  faults.
 *
 * @param[out] the number of detected faults.
 */
int get_num_detected_faults(void)
{
	/*
	unsigned memword = 0;
	uint8_t *membytes = (uint8_t *)&memword;

    if (next_cpu == NULL)
        next_cpu = first_cpu;

    for (; next_cpu != NULL; next_cpu = CPU_NEXT(next_cpu))
    {
        CPUState *cpu = next_cpu;

        if (fault_counter_address)
        	cpu_memory_rw_debug(cpu, fault_counter_address, membytes, 4, 0);
        else
        	cpu_memory_rw_debug(cpu, FAULT_COUNTER_ADDRESS, membytes, 4, 0);
	}

	return memword;*/
	return 0;
}

/**
 * Sets the number of detected faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_detected_faults
 */
void set_num_detected_faults(int num)
{
	/*uint8_t *membytes = (uint8_t *)&num;

    if (next_cpu == NULL)
        next_cpu = first_cpu;

    for (; next_cpu != NULL; next_cpu = CPU_NEXT(next_cpu))
    {
        CPUState *cpu = next_cpu;

        if (fault_counter_address)
        	cpu_memory_rw_debug(cpu, fault_counter_address, membytes, 4, 1);
        else
        	cpu_memory_rw_debug(cpu, FAULT_COUNTER_ADDRESS, membytes, 4, 1);
	}*/
}

/**
 * Sets the number of selected input to a
 * specified value.
 *
 * @param[in] num - the number, which should be written to
 *                              file_input_to_use_address
 */
void set_input_file_to_use(int num)
{

	uint8_t *membytes = (uint8_t *)&num;

    if (next_cpu == NULL)
        next_cpu = first_cpu;

    for (; next_cpu != NULL; next_cpu = CPU_NEXT(next_cpu))
    {
        CPUState *cpu = next_cpu;

        if (file_input_to_use_address){
        	cpu_memory_rw_debug(cpu, file_input_to_use_address, membytes, 4, 1);
		}

	}
}

/**
 * Sets the number of injected transient ram faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_ram_trans
 */
void set_num_injected_faults_ram_trans(int num)
{
	num_injected_faults_ram_trans = num;
}

/**
 * Sets the number of injected permanent ram faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_ram_perm
 */
void set_num_injected_faults_ram_perm(int num)
{
	num_injected_faults_ram_perm = num;
}

/**
 * Sets the number of injected transient cpu faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_cpu_trans
 */
void set_num_injected_faults_cpu_trans(int num)
{
	num_injected_faults_cpu_trans = num;
}

/**
 * Sets the number of injected permanent cpu faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_cpu_perm
 */
void set_num_injected_faults_cpu_perm(int num)
{
	num_injected_faults_cpu_perm = num;
}

/**
 * Sets the number of injected transient register faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_register_trans
 */
void set_num_injected_faults_register_trans(int num)
{
	num_injected_faults_register_trans = num;
}

/**
 * Sets the number of injected permanent register faults to a
 * specified value (is used for reseting the variable).
 *
 * @param[in] num - the number, which should be written to
 *                              num_injected_faults_register_perm
 */
void set_num_injected_faults_register_perm(int num)
{
	num_injected_faults_register_perm = num;
}

/**
 * Returns the number of injected transient ram faults.
 *
 * @param[out] the number of injected transient ram faults.
 */
int get_num_injected_faults_ram_trans(void)
{
	return num_injected_faults_ram_trans;
}

/**
 * Returns the number of injected permanent ram faults.
 *
 * @param[out] the number of injected permanent ram faults.
 */
int get_num_injected_faults_ram_perm(void)
{
	return num_injected_faults_ram_perm;
}

/**
 * Returns the number of injected transient cpu faults.
 *
 * @param[out] the number of injected transient cpu faults.
 */
int get_num_injected_faults_cpu_trans(void)
{
	return num_injected_faults_cpu_trans;
}

/**
 * Returns the number of injected permanent cpu faults.
 *
 * @param[out] the number of injected permanent cpu faults.
 */
int get_num_injected_faults_cpu_perm(void)
{
	return num_injected_faults_cpu_perm;
}

/**
 * Returns the number of injected transient register faults.
 *
 * @param[out] the number of injected transient register faults.
 */
int get_num_injected_faults_register_trans(void)
{
	return num_injected_faults_register_trans;
}

/**
 * Returns the number of injected permanent register faults.
 *
 * @param[out] the number of injected permanent register faults.
 */
int get_num_injected_faults_register_perm(void)
{
	return num_injected_faults_register_perm;
}

///**
// * Returns the number of detected transient ram faults.
// *
// * @param[out] the number of detected transient ram faults.
// */
//int get_num_detected_faults_ram_trans(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}
//
///**
// * Returns the number of detected permanent ram faults.
// *
// * @param[out] the number of detected permanent ram faults.
// */
//int get_num_detected_faults_ram_perm(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}
//
///**
// * Returns the number of detected transient cpu faults.
// *
// * @param[out] the number of detected transient cpu faults.
// */
//int get_num_detected_faults_cpu_trans(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}
//
///**
// * Returns the number of detected permanent cpu faults.
// *
// * @param[out] the number of detected permanent cpu faults.
// */
//int get_num_detected_faults_cpu_perm(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}
//
///**
// * Returns the number of detected transient register faults.
// *
// * @param[out] the number of detected transient register faults.
// */
//int get_num_detected_faults_register_trans(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}
//
///**
// * Returns the number of detected permanent register faults.
// *
// * @param[out] the number of detected permanent register faults.
// */
//int get_num_detected_faults_register_perm(void)
//{
//	// über register auslesen oder so??
//	return 1;
//}


