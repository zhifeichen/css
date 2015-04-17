#ifndef CSS_DISK_MANAGER_H_
#define CSS_DISK_MANAGER_H_

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "uv/uv.h"
#include "css_util.h"

#ifdef _WIN32
#include <Windows.h>
#include <WinBase.h>
//#define MKDIR(path) CreateDirectory((path), NULL)
#else
#include "sys/stat.h"
#include "sys/statvfs.h"
#include <sys/types.h>
#include <errno.h>
#endif

#define CSS_VIDEO_FILE_PATH 0
#define CSS_WARN_FILE_PATH 1

#define FILE_PATH "%s/dvr_data/%lld/%hd/"
#define WARN_FILE_PATH "%s/warn_data/%lld/%hd/"

#define CHECK_COPY_LEN(x,y) if((x) > (y))	\
							{	\
								printf("buf too small! \n");	\
								return -1;	\
							}

typedef struct
{
	char* disk_paths;//存盘盘符名，分号分隔，例：Win-> D:;E:;H:	\ Unix系-> /mnt/sda;/mnt/sdb
	int32_t max_per;	//清盘阈值，例：85
	int32_t clear_file_day;	//例：30，即从当前日期之前30天开始清盘
	int32_t init_flag;	//是否已初始化，1 TRUE,0 FALSE
	uv_loop_t* myLoop;
} css_disk_config;

typedef struct
{
	char* disk_name;	//盘符名，例：Win-> E: \ Unix系-> /mnt/sda
	uint64_t disk_unused_size;	//可用容量 单位 MB
	uint64_t disk_total_size;	//总容量 单位 MB
	int32_t used_per;          //已使用百分比
	uint32_t weight;            //权重
	int32_t err_tag;		   //该磁盘是否故障, 1 TRUE,0 FALSE
	int32_t clr_tag;		   //该磁盘是否清盘中，1 TRUE,0 FALSE
	int32_t clear_file_day;
	int32_t max_per;
} disk_status;

int css_init_disk_manager(css_disk_config* myDiskConfig);

/*
volumePostion = strlen("C:") or strlen("/mnt/sda")
*/
int css_get_disk_path(uint64_t dvrEquipId, uint16_t channelNo, char* filePath,
		int len, int* volumePostion);
/*
volumePostion = strlen("C:") or strlen("/mnt/sda")
*/
int css_get_disk_warn_path(uint64_t dvrEquipId, uint16_t channelNo, char* filePath,
	int len, int* volumePostion);

void css_destroy_disk_manager();

int css_get_disk_status(disk_status** myDiskStatus);

#endif /* CSS_DISK_MANAGER_H_ */
