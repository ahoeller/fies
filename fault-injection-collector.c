/*
 * fault-injection.c
 *
 *  Created on: 05.08.2014
 *      Author: Gerhard Schoenfelder
 */

#include <stdlib.h>
#include <stdint.h>
#include "fault-injection-collector.h"

/**
 * The file, where the data collector writes
 * his information to.
 */
 FILE *data_collector;

 /**
  * A flag, which defines if the fault-collector should write his content
  * to the specified file or not.
  */
 static int do_fault_injection = 0;

 /**
  * Appends the content to the opened collector-file.
  *
  * @param[in] buf - the text, which should be written.
  */
void data_collector_write(const char* buf)
{
    if (do_fault_injection)
    {
 	    data_collector = fopen("fies.log", "a+");
    	if (data_collector == NULL)
       	{
    		fprintf(stderr, "File  does not exists!\n");
    	    exit(1);
    	}
    	fprintf(data_collector,  "%s\n", (const uint8_t *) buf);
    	fclose(data_collector);
    }
}

/**
 * Sets the flag, which decides if the collector should write
 * his content to the specified file or not. This flag is set in
 * the main-function if the argument-vector (argv) contains
 * the paramter "-fi".
 *
 * @param[in] flag - the value, which should be written to
 *                             do_fault_injection flag.
 */
void set_do_fault_injection(int flag)
{
	do_fault_injection = flag;
	//error_report("Set do fault injection");
}

/**
 * Sets the flag, which decides if the collector should write
 * his content to the specified file or not. This flag is set in
 * the main-function if the argument-vector (argv) contains
 * the paramter "-fi".
 *
 * @param[out] - the value, of the do_fault_injection flag.
 */
int get_do_fault_injection(void)
{
	return do_fault_injection;
}
