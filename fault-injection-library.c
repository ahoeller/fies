/*
 * fault-injection-library.c
 *
 *  Created on: 07.08.2014
 *      Author: Gerhard Schoenfelder
 */

#include "monitor/monitor.h"
#include "fault-injection-library.h"
#include "fault-injection-controller.h"
#include "fault-injection-data-analyzer.h"
#include <unistd.h>
#include <libxml/xmlreader.h>
#include "profiler.h"
/**
 * Linked list pointer to the first entry
 */
static FaultList *head = NULL;
/**
 * Linked list pointer to the current entry
 */
static FaultList *curr = NULL;
/**
 * num_list_elements contains the number of the
 * stored entries in the linked list.
 */
static int num_list_elements = 0;

/**
 * Allocates the size for the first entry in the linked list and parses the elements to it.
 *
 * @param[in] fault - all necessary elements for defining a fault, which are held
 *                              by the struct "Fault"
 * @param[out] ptr - pointer to the linked list entry (added element)
 */
static FaultList* create_fault_list(struct Fault *fault)
{
    FaultList *ptr = (FaultList*) malloc( sizeof(struct Fault) );
    if(ptr == NULL)
    {
        fprintf(stderr, "Node creation failed\n");
        return NULL;
    }

    ptr->id = fault->id;
	ptr->component = fault->component;
	ptr->target = fault->target;
	ptr->mode = fault->mode;
	ptr->trigger = fault->trigger;
	ptr->timer = fault->timer;
	ptr->type = fault->type;
	ptr->duration = fault->duration;
	ptr->interval = fault->interval;
	ptr->params.address = fault->params.address;
	ptr->params.cf_address = fault->params.cf_address;
	ptr->params.mask = fault->params.mask;
	ptr->params.instruction = fault->params.instruction;
	ptr->params.set_bit = fault->params.set_bit;
	ptr->is_active = 0;
    ptr->next = NULL;

    head = curr = ptr;

    num_list_elements++;

    return ptr;
}

/**
 * Allocates the size for a new entry in the linked list and parses the elements to it.
 *
 * @param[in] fault - all necessary elements for defining a fault, which are held
 *                              by the struct "Fault"
 * @param[out] ptr - pointer to the linked list entry (added element)
 */
static FaultList* add_to_fault_list(struct Fault *fault)
{
    if(head == NULL)
        return create_fault_list(fault);

    FaultList *ptr = (FaultList*) malloc( sizeof(struct Fault) );
    if(NULL == ptr)
    {
        fprintf(stderr, "Node creation failed\n");
        return NULL;
    }

    ptr->id = fault->id;
	ptr->component = fault->component;
	ptr->target = fault->target;
	ptr->mode = fault->mode;
	ptr->trigger = fault->trigger;
	ptr->timer = fault->timer;
	ptr->type = fault->type;
	ptr->duration = fault->duration;
	ptr->interval = fault->interval;
	ptr->params.address = fault->params.address;
	ptr->params.cf_address = fault->params.cf_address;
	ptr->params.mask = fault->params.mask;
	ptr->params.instruction = fault->params.instruction;
	ptr->params.set_bit = fault->params.set_bit;
	ptr->is_active = 0;
    ptr->next = NULL;

    curr->next = ptr;
    curr = ptr;

    num_list_elements++;

    return ptr;
}

#if defined(DEBUG_FAULT_LIST)
/**
 * Prints all entries in the linked list to the standard  out - only for debug purpose.
 */
static void print_fault_list(void)
{
	FaultList *ptr = head;

    printf("\n -------Printing list Start------- \n");
    while(ptr != NULL)
    {
        printf("id [%d] \n",ptr->id);
        printf("component [%s] \n",ptr->component);
        printf("target [%s] \n",ptr->target);
        printf("mode [%s] \n",ptr->mode);
        printf("trigger [%s] \n",ptr->trigger);
        printf("timer [%s] \n",ptr->timer);
        printf("type [%s] \n",ptr->type);
        printf("duration [%s] \n",ptr->duration);
        printf("interval [%s] \n",ptr->interval);
        printf("params.address [%x] \n",ptr->params.address);
        printf("params.cf_address [%x] \n",ptr->params.cf_address);
        printf("params.mask [%x] \n",ptr->params.mask);
        printf("params.instruction [%x] \n",ptr->params.instruction);
        printf("params.set_bit [%x] \n",ptr->params.set_bit);
        printf("is_active [%d] \n",ptr->is_active);
        ptr = ptr->next;
        printf("\n");
    }
    printf("\n -------Printing list End------- \n");

    return;
}
#endif

/**
 * Deletes the linked list and all included elements
 */
void delete_fault_list(void)
{
	FaultList *ptr;

	while ( (ptr = head) )
	{
		head = ptr->next;
		if (ptr)
			free(ptr);
	}

    num_list_elements = 0;
}

/**
 * Returns the size of the linked list (included elements)
 *
 * @param[out] num_list_elements - number of included elements
 */
int getNumFaultListElements(void)
{
	return num_list_elements;
}

/**
 * Returns the corresponding FaultList entry to the linked list.
 *
 * @param[in] element - defines  the index of the desired FaultList entry
 * @param[out] fault_element - corresponding pointer to the entry in the linked list.
 */
FaultList* getFaultListElement(int element)
{
	FaultList *ptr = head, *fault_element = NULL;
    int index = 0;

    while (ptr != NULL)
    {
    	if (element == index)
    		fault_element =  ptr;

    	index++;
    	ptr = ptr->next;
	}

    return fault_element;
}

/**
 * Searches the maximal fault id number in the linked list.
 *
 * @param[out] max_id - the maximal id number
 */
int getMaxIDInFaultList(void)
{
	FaultList *ptr = head;
    int max_id = 0;

    while (ptr != NULL)
    {
    	if (ptr->id > max_id)
        	max_id = ptr->id;

    	ptr = ptr->next;
	}

    return max_id;
}

/**
 * Checks the data types and the content of the parsed XML-parameters
 * for correctness. IMPORTANT: it does not check, if all necessary parameters
 * are defined.
 */
static void validateXMLInput(void)
{
	FaultList *ptr = head;

    while (ptr != NULL)
    {
    	if (!ptr->id || ptr->id == -1)
    	{
    		fprintf(stderr, "fault id is not a positive, real number\n");
    	}

    	if (!ptr->component || !ptr->target || !ptr->mode)
    	{
    		fprintf(stderr, "component, target or mode is not defined (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->component
    		&& strcmp(ptr->component, "CPU")
    		&& strcmp(ptr->component, "RAM")
    		&& strcmp(ptr->component, "REGISTER") )
    	{
    		fprintf(stderr, "component has to be \"CPU, REGISTER or RAM\" (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->target
    		&& strcmp(ptr->target, "REGISTER CELL")
    		&& strcmp(ptr->target, "MEMORY CELL")
    		&& strcmp(ptr->target, "CONDITION FLAGS")
    		&& strcmp(ptr->target, "INSTRUCTION EXECUTION")
    		&& strcmp(ptr->target, "INSTRUCTION DECODER")
    		&& strcmp(ptr->target, "ADDRESS DECODER")
		&& strcmp(ptr->target, "PRINT ADDRESSES TO FILE"))
    	{
    		fprintf(stderr, "target has to be \"REGISTER CELL, MEMORY CELL, "
    				"CONDITION FLAGS, INSTRUCTION EXECUTION, INSTRUCTION DECODER, "
    				"or ADDRESS DECODER\" (fault id: %d)\n", ptr->id);
    	}

	if (ptr->target && !strcmp(ptr->target, "PRINT ADDRESSES TO FILE")){
		profile_ram_addresses = 1;
}

    	if (ptr->mode
    		&& strcmp(ptr->mode, "NEW VALUE")
    		&& strcmp(ptr->mode, "ZF") && strcmp(ptr->mode, "CF")
    		&& strcmp(ptr->mode, "NF") && strcmp(ptr->mode, "QF")
    		&& strcmp(ptr->mode, "VF") && strcmp(ptr->mode, "SF")
    		&& strcmp(ptr->mode, "BIT-FLIP") && strcmp(ptr->mode, "VF")
    		&& strcmp(ptr->mode, "TF0") && strcmp(ptr->mode, "TF1")
    		&& strcmp(ptr->mode, "WDF0") && strcmp(ptr->mode, "WDF1")
    		&& strcmp(ptr->mode, "RDF0") && strcmp(ptr->mode, "RDF1")
    		&& strcmp(ptr->mode, "IRF0") && strcmp(ptr->mode, "IRF1")
    		&& strcmp(ptr->mode, "DRDF0") && strcmp(ptr->mode, "DRDF1")
    		&& strcmp(ptr->mode, "RDF00") && strcmp(ptr->mode, "RDF01")
    		&& strcmp(ptr->mode, "RDF10") && strcmp(ptr->mode, "RDF11")
    		&& strcmp(ptr->mode, "IRF00") && strcmp(ptr->mode, "IRF01")
    		&& strcmp(ptr->mode, "IRF10") && strcmp(ptr->mode, "IRF11")
    		&& strcmp(ptr->mode, "DRDF00") && strcmp(ptr->mode, "DRDF01")
    		&& strcmp(ptr->mode, "DRDF10") && strcmp(ptr->mode, "DRDF11")
    		&& strcmp(ptr->mode, "CFST00") && strcmp(ptr->mode, "CFST01")
    		&& strcmp(ptr->mode, "CFST10") && strcmp(ptr->mode, "CFST11")
    		&& strcmp(ptr->mode, "CFTR00") && strcmp(ptr->mode, "CFTR01")
    		&& strcmp(ptr->mode, "CFTR10") && strcmp(ptr->mode, "CFTR11")
    		&& strcmp(ptr->mode, "CFWD00") && strcmp(ptr->mode, "CFWD01")
    		&& strcmp(ptr->mode, "CFWD10") && strcmp(ptr->mode, "CFWD11")
    		&& strcmp(ptr->mode, "CFRD00") && strcmp(ptr->mode, "CFRD01")
    		&& strcmp(ptr->mode, "CFRD10") && strcmp(ptr->mode, "CFRD11")
    		&& strcmp(ptr->mode, "CFIR00") && strcmp(ptr->mode, "CFIR01")
    		&& strcmp(ptr->mode, "CFIR10") && strcmp(ptr->mode, "CFIR11")
    		&& strcmp(ptr->mode, "CFDR00") && strcmp(ptr->mode, "CFDR01")
    		&& strcmp(ptr->mode, "CFDR10") && strcmp(ptr->mode, "CFDR11")
    		&& strcmp(ptr->mode, "CFDS0W00") && strcmp(ptr->mode, "CFDS0W01")
    		&& strcmp(ptr->mode, "CFDS0W10") && strcmp(ptr->mode, "CFDS0W11")
    		&& strcmp(ptr->mode, "CFDS1W00") && strcmp(ptr->mode, "CFDS1W01")
    		&& strcmp(ptr->mode, "CFDS1W10") && strcmp(ptr->mode, "CFDS1W11")
    		&& strcmp(ptr->mode, "CFDS0R00") && strcmp(ptr->mode, "CFDS0R01")
    		&& strcmp(ptr->mode, "CFDS1R10") && strcmp(ptr->mode, "CFDS1R11"))
    	{
    		fprintf(stderr, "unknown mode (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->trigger
    		&& strcmp(ptr->trigger, "ACCESS")
    		&& strcmp(ptr->trigger, "TIME")
    		&& strcmp(ptr->trigger, "PC"))
    	{
    		fprintf(stderr, "trigger has to be \"ACCESS, TIME or PC\" (fault id: %d)\n", ptr->id);
    	}

    	if (!ptr->params.address)
    	{
    		fprintf(stderr, "fault address is not a number (fault id: %d)\n", ptr->id);
    	}

    	if (!ptr->params.cf_address)
    	{
    		fprintf(stderr, "fault coupling address is not a number (fault id: %d)\n", ptr->id);
    	}

    	if (!ptr->params.instruction)
    	{
    		fprintf(stderr, "fault instruction address is not a number (fault id: %d)\n", ptr->id);
    	}

    	if (!ptr->params.mask)
    	{
    		fprintf(stderr, "fault mask is not a number (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->trigger && (!strcmp(ptr->trigger, "TIME") || !strcmp(ptr->trigger, "ACCESS"))
    		 && ptr->type && strcmp(ptr->type, "PERMANENT")
    		 && strcmp(ptr->type, "TRANSIENT")
    		 && strcmp(ptr->type, "INTERMITTEND"))
    	{
    		fprintf(stderr, "type has to be \"PERMANENT, TRANSIENT or "
    				"INTERMITTEND\" for time- or access-triggered faults (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->trigger && !strcmp(ptr->trigger, "PC")
    		&& (ptr->params.address == -1 || !ptr->params.address))
    	{
    		fprintf(stderr, "PC-address has to be defined in the <params>->"
    				"<instruction>-tag or has  to be a positive, real number (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->timer
    		&& !ends_with(ptr->timer, "MS")
    		&& !ends_with(ptr->timer, "US")
    		&& !ends_with(ptr->timer, "NS"))
    	{
    		fprintf(stderr, "timer has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->timer
    		&& (ends_with(ptr->timer, "MS")
    		|| ends_with(ptr->timer, "US")
    		|| ends_with(ptr->timer, "NS"))
    		&& !timer_to_int(ptr->timer) )
    	{
    		fprintf(stderr, "timer has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->duration
    		&& !ends_with(ptr->duration, "MS")
    		&& !ends_with(ptr->duration, "US")
    		&& !ends_with(ptr->duration, "NS"))
    	{
    		fprintf(stderr, "duration has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->duration
    		&& (ends_with(ptr->duration, "MS")
    		|| ends_with(ptr->duration, "US")
    		|| ends_with(ptr->duration, "NS"))
    		&& !timer_to_int(ptr->duration) )
    	{
    		fprintf(stderr, "duration has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->interval
    		&& !ends_with(ptr->interval, "MS")
    		&& !ends_with(ptr->interval, "US")
    		&& !ends_with(ptr->interval, "NS"))
    	{
    		fprintf(stderr, "interval has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	if (ptr->interval
    		&& (ends_with(ptr->interval, "MS")
    		|| ends_with(ptr->interval, "US")
    		|| ends_with(ptr->interval, "NS"))
    		&& !timer_to_int(ptr->interval) )
    	{
    		fprintf(stderr, "interval has to be a positive, real number in ns, us or"
    				" ms (fault id: %d)\n", ptr->id);
    	}

    	ptr = ptr->next;
	}
}

#ifdef LIBXML_READER_ENABLED
/**
 * Parses the fault parameters from the XML file.
 *
 * @param[in] doc - A structure containing the tree created by a parsed
 *                            doc.
 * @param[in] cur - A structure containing a single node.
 */
static void parseFault(xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar *key = NULL;
	xmlNodePtr grandchild_node;
	FaultList fault = {-1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, {-1, -1, -1, -1, -1}, NULL};

	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
	    if ( !xmlStrcmp(cur->name, (const xmlChar *) "id") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.id =  (int) strtol((char *) key, NULL, 10);
			xmlFree(key);
 	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "component") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.component = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "target") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.target = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "mode") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.mode = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "trigger") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.trigger = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "timer") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.timer = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "type") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.type = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "duration") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.duration = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "interval") )
	    {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    fault.interval = strdup((char *) key);
			xmlFree(key);
	    }
	    else if ( !xmlStrcmp(cur->name, (const xmlChar *) "params") )
	    {
	    	grandchild_node =  cur->xmlChildrenNode;
	    	while (grandchild_node != NULL)
	    	{
	    		if ( !xmlStrcmp(grandchild_node->name, (const xmlChar *) "address") )
	    		{
	    		    key = xmlNodeListGetString(doc, grandchild_node->xmlChildrenNode, 1);
	    			fault.params.address = (int) strtoul((char *) key, NULL, 16);
	    			xmlFree(key);
	    		}
	    		if ( !xmlStrcmp(grandchild_node->name, (const xmlChar *) "cf_address") )
	    		{
	    		    key = xmlNodeListGetString(doc, grandchild_node->xmlChildrenNode, 1);
	    			fault.params.cf_address = (int) strtoul((char *) key, NULL, 16);
	    			xmlFree(key);
	    		}
	    		else if ( !xmlStrcmp(grandchild_node->name, (const xmlChar *) "mask") )
	    		{
	    		    key = xmlNodeListGetString(doc, grandchild_node->xmlChildrenNode, 1);
	    			fault.params.mask = (int) strtol((char *) key, NULL, 16);
	    			xmlFree(key);
	    		}
	    		if ( !xmlStrcmp(grandchild_node->name, (const xmlChar *) "instruction") )
	    		{
	    		    key = xmlNodeListGetString(doc, grandchild_node->xmlChildrenNode, 1);
	    			fault.params.instruction = (int) strtoul((char *) key, NULL, 16);
	    			xmlFree(key);
	    		}
	    		if ( !xmlStrcmp(grandchild_node->name, (const xmlChar *) "set_bit") )
	    		{
	    		    key = xmlNodeListGetString(doc, grandchild_node->xmlChildrenNode, 1);
	    		    fault.params.set_bit = (int) strtol((char *) key, NULL, 16);
	    			xmlFree(key);
	    		}

	    		grandchild_node = grandchild_node->next;
	    	}
	    }

	    cur = cur->next;
	}

	add_to_fault_list(&fault);

    return;
}

/**
 * Read the XML-file and checks the basic structure of the XML for
 * correctness. Starts the XML-parser.
 *
 * @param[in] mon - Reference to the QEMU-monitor
 * @param[in] filename - The name of the XML-file containing the fault definitions
 */
static int parseFile(Monitor *mon, const char *filename)
{
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseFile(filename);
	if (doc == NULL )
	{
		monitor_printf(mon, "Document not parsed successfully.\n");
		return -1;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL)
	{
		monitor_printf(mon, "Empty document\n");
		xmlFreeDoc(doc);
		return -1;
	}


	if (xmlStrcmp(cur->name, (const xmlChar *) "injection"))
	{
		monitor_printf(mon, "Document of the wrong type, root node != injection\n");
		xmlFreeDoc(doc);
		return -1;
	}

	/**
	 * Starting new fault injection experiment -
	 * Deleting current context
	*/
	if (head != NULL)
		delete_fault_list();

	destroy_id_array();
	destroy_ops_on_cell();

	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		if ( !xmlStrcmp(cur->name, (const xmlChar *) "fault") )
		{
			parseFault(doc, cur);
		}

	cur = cur->next;
	}

	validateXMLInput();
	xmlFreeDoc(doc);
	return 0;
}

/**
 * Read the XML-file and checks the basic structure of the XML for
 * correctness. Starts the XML-parser.
 *
 * @param[in] mon - Reference to the QEMU-monitor
 * @param[in] filename - The name of the XML-file containing the fault definitions
 * @param[in] errp - Reference for setting errors in QEMU
 */
void qmp_fault_reload(Monitor *mon, const char *filename, Error **errp)
{
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
	int max_id = 0;

	/**
	 * Starting new fault injection experiment -
	 * reset timer and statistics
	*/
	fault_injection_controller_initTimer();
	set_num_injected_faults(0);
	set_num_detected_faults(0);
	set_num_injected_faults_ram_trans(0);
	set_num_injected_faults_ram_perm(0);
	set_num_injected_faults_cpu_trans(0);
	set_num_injected_faults_cpu_perm(0);
	set_num_injected_faults_register_trans(0);
	set_num_injected_faults_register_perm(0);

    LIBXML_TEST_VERSION

    if (parseFile(mon, filename))
    	monitor_printf(mon, "Configuration file not loaded\n");
    else
    	monitor_printf(mon, "Configuration file loaded successfully\n");

#if defined(DEBUG_FAULT_LIST)
    print_fault_list();
#endif

	/**
	 * Initialize the context for a new fault injection experiment
	*/
    max_id = getMaxIDInFaultList();
    init_id_array(max_id);
    init_ops_on_cell(max_id);

    xmlCleanupParser();
}
#else
void qmp_fault_reload(Monitor *mon, const char *filename, Error **errp)
{
	error_setg(errp, "Error: Configuration file not loaded - XInclude support not compiled\n");
}
#endif

