/*
 * fault-injection-data-analyzer.h
 *
 *  Created on: 07.08.2014
 *      Author: Gerhard Schoenfelder
 */

#ifndef FAULT_INJECTION_DATA_ANALYZER_H_
#define FAULT_INJECTION_DATA_ANALYZER_H_

/**
 * see corresponding c-file for documentation
 */
void incr_num_injected_faults(int id, const char* fault_type);
void set_num_injected_faults(int num);
void set_input_file_to_use(int num);
int get_num_injected_faults(void);
int get_num_detected_faults(void);
void set_num_detected_faults(int num);
void set_num_injected_faults_ram_trans(int num);
void set_num_injected_faults_ram_perm(int num);
void set_num_injected_faults_cpu_trans(int num);
void set_num_injected_faults_cpu_perm(int num);
void set_num_injected_faults_register_trans(int num);
void set_num_injected_faults_register_perm(int num);
int get_num_injected_faults_ram_trans(void);
int get_num_injected_faults_ram_perm(void);
int get_num_injected_faults_cpu_trans(void);
int get_num_injected_faults_cpu_perm(void);
int get_num_injected_faults_register_trans(void);
int get_num_injected_faults_register_perm(void);
//int get_num_detected_faults_ram_trans(void);
//int get_num_detected_faults_ram_perm(void);
//int get_num_detected_faults_cpu_trans(void);
//int get_num_detected_faults_cpu_perm(void);
//int get_num_detected_faults_register_trans(void);
//int get_num_detected_faults_register_perm(void);
void init_id_array(int size);
void destroy_id_array(void);

#endif /* FAULT_INJECTION_DATA_ANALYZER_H_ */
