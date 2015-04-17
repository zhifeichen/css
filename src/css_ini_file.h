/*
 * css_ini_file.h
 *
 *  Created on: 2014?7?29?
 *      Author: zj14
 */

#ifndef CSS_INI_FILE_H_
#define CSS_INI_FILE_H_

#include "css_db.h"
#include "css_disk_manager.h"

#define SECTION_DB "db"
#define SECTION_DISK "disk"

#define DEFAULT_CONFIG_FILE "../config.ini"
#define TEST_CONFIG_FILE "../test/test_config.ini"

#define CHECK_LOAD_INI_FILE() if(m_css_config == NULL)	\
								{	\
									printf("please load the config file!! \n");	\
									return -1;	\
								}

typedef struct {
	char* keyName;
	char* value;
} css_config_env;

typedef struct {
	char* setctionName;
	int   keyCount;
	css_config_env* config;
} css_config;

static css_db_config default_db_config = {
		 "localhost", 	//host
		 3306,			//port
		 "root",		//user
		 "root",		//passwd
		 "test",		//dbName
		 DBTYPE_MYSQL,	//dbType
		 0				//init_flag
};

static css_disk_config default_disk_config = {
		"",		//disk_paths
		0,		//max_per
		1,		//clear_file_day
		0,		//init_flag
		NULL	//myLoop
};

int css_load_ini_file(const char* file_path);
void css_destory_ini_file();
int css_get_env(char* secName,char* keyName,char* defaultValue,char** value);
int css_get_db_config(css_db_config* myDbConfig);
int css_get_disk_config(css_disk_config* myDiskConfig);


#endif /* CSS_INI_FILE_H_ */
