/*
 * css_test.h
 *
 *  Created on: 上午11:04:31
 *      Author: ss
 */

#ifndef CSS_TEST_H_
#define CSS_TEST_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "css_util.h"


typedef void (*test_case)(void);

typedef enum
{
	TCASE_INACTIVE = 0, TCASE_ACTIVE
} test_case_active_t;

/**
 * add test func here!   func name must start with 'test_'
 * XX(name,active) like XX(name,0) or XX(name,TCASE_ACTIVE)
 */
#define CSS_TEST_CASES(XX)  			\
	XX(protocol_packege_suite, 0)		\
	XX(file_writer, 0)				\
    XX(file_reader,0)                \
	XX(reader,0)             \
	XX(stream, 0)						\
	XX(capture, 0)						\
	XX(db_suite, 1)						\
	XX(capture_package, 0)				\
	XX(capture_open_file, 0)			\
	XX(record, 0)						\
	XX(stream_server, 0)				\
	XX(disk_manager_suite, 0)			\
	XX(read_inifile_suite, 0)			\
	XX(clear_disk_suite, 0)				\
	XX(css_linked_list_suite, 0)		\
	XX(css_util_suite, 0)				\
	XX(css_job_trigger_suite,0)         \
	XX(css_logger_suite,0)				\
	XX(css_alarm_suite,0)				\
	XX(css_job_suite,0)					\
//	XX(test_case,TCASE_ACTIVE)		\
//	XX(test_case,0)						\



/**
 * private
 */
typedef struct css_test_case_s
{
	char *name;
	test_case main;
	int active;
} css_test_case_t;

/**
 * declare all test case func.
 */
#define CTEST_DECLARE(name,_) \
	void test_##name(void);
CSS_TEST_CASES(CTEST_DECLARE)

/**
 * add test case to suite.
 */
#define CTEST_ENTITY(name,active) \
	{ #name, &test_##name, active },

css_test_case_t suite[] = {
CSS_TEST_CASES(CTEST_ENTITY) { 0, 0, 0 } };

#define PRINTF_CSS_TEST_CAST_HEADER(name)	\
	printf("Running test_case: test_%s\n---------------------------------------------------\n",(name))

#define PRINTF_CSS_TEST_CASE_TAIL(name,cost)		\
		printf("Test end of test case:%s , cost:%ldms.\n---------------------------------------------------\n\n",(name),(cost))

#define PRINTF_CSS_DO_TEST_HEADER()				 \
do {																	\
	printf("---------------------------------------------------\n");	\
	printf("   R U N N I N G    T E S T S \n");							\
	printf("---------------------------------------------------\n\n");	\
}while(0)

#define PRINTF_CSS_DO_TEST_TAIL(t,active,ignore,total)					\
do{																			\
	printf("\nTotal %d test case, active:%d, skipped:%d, total cost:%ldms.\n", (t), (active), (ignore), (total));  \
}while(0)

void css_do_test_by(char *name);
void css_do_test_by(char *name)
{
	int i = 0, ignore = 0, active = 0;
	struct timeval st, et;
	long cost, total_cost = 0;
	PRINTF_CSS_DO_TEST_HEADER()
	;
	while (suite[i].name != NULL) {
		if (strcmp(name, suite[i].name) == 0) {
			gettimeofday(&st, NULL);
			PRINTF_CSS_TEST_CAST_HEADER(suite[i].name);
			suite[i].main();
			gettimeofday(&et, NULL);
			cost = difftimeval(&et, &st);
			total_cost += cost;
			PRINTF_CSS_TEST_CASE_TAIL(suite[i].name, cost);
			active++;
			break;
		}
		i++;
	}
	PRINTF_CSS_DO_TEST_TAIL(1, active, 1 - active, total_cost);
}

void css_do_test_all();
void css_do_test_all()
{
	int i = 0, ignore = 0, active = 0;
	struct timeval st, et;
	long cost, total_cost = 0;
	PRINTF_CSS_DO_TEST_HEADER()
	;
	while (suite[i].main != NULL) {
		if (suite[i].active) {
			gettimeofday(&st, NULL);
			PRINTF_CSS_TEST_CAST_HEADER(suite[i].name);
			suite[i].main();
			gettimeofday(&et, NULL);
			cost = difftimeval(&et, &st);
			total_cost += cost;
			PRINTF_CSS_TEST_CASE_TAIL(suite[i].name, cost);
			active++;
		} else {
			ignore++;
		}
		i++;
	}
	PRINTF_CSS_DO_TEST_TAIL(i, active, ignore, total_cost);
}

void test_test_case(void)
{
	printf("start test case test body.\n");
	printf("start test case test body.\n");
	printf("start test case test body.\n");
	printf("start test case test body.\n");
}

#endif /* CSS_TEST_H_ */
