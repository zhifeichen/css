/*
 * css_clear_disk.c
 *
 *  Created on: 2014?8?4?
 *      Author: zj14
 */

#include "css_db.h"
#include "css_util.h"
#include "css_clear_disk.h"
#include "css_logger.h"
#include "css_disk_manager.h"
#include "css.h"

static int change_filename(char* filename,char* filetype){
	int i;
	if ((i=(int)strlen(filename))==0)
	{
		return -1;
	}
	while ((--i)>=0)
	{
		if (filename[i] == '.')
		{
			filename[i+1]=0;
			break;
		}
	}
	strcat(filename,filetype);
	return 0;
}

static int clear_disk(disk_status* myDiskStatus,int time){
	QUEUE clear_file_list;
	QUEUE *q;
	char clear_file_begin_time[24];
	css_file_record_info* myFileRecordInfo;
	struct timeval tv;
	char tmpName[MAX_PATH];
	QUEUE_INIT(&clear_file_list);
	
	while ((myDiskStatus->used_per > myDiskStatus->max_per)
		&& (time >= CSS_CLEAR_MIN_HOUR)) {
			gettimeofday(&tv, NULL);
			tv.tv_sec -= (time * 3600);
			timeval_to_svtime_string(&tv, clear_file_begin_time, 24);

			css_db_select_video_file_by_begin_time_and_volume(clear_file_begin_time,
				myDiskStatus->disk_name, &clear_file_list);
			QUEUE_FOREACH(q, &clear_file_list)
			{
				myFileRecordInfo = QUEUE_DATA(q, css_file_record_info, queue);
				unlink(myFileRecordInfo->fields.fileName);
				strcpy(tmpName,myFileRecordInfo->fields.fileName);
				change_filename(tmpName,"iif");
				unlink(tmpName);
				change_filename(tmpName,"cif");
				unlink(tmpName);
				css_db_delete_video_file_by_id(myFileRecordInfo);
			}
			if (time > 24)
			{
				time -= 24;
			} 
			else
			{
				time --;
			}
			css_get_disk_status(&myDiskStatus);
	}

	if (time < CSS_CLEAR_MIN_HOUR) {
		CL_WARN("clear over,but disk: %s  have no unused size !!! \n",
			myDiskStatus->disk_name);
		//myDiskStatus->err_tag = 1;
		return -1;
	}
	return 0;
}

int css_clear_disk(disk_status* myDiskStatus) {
	return clear_disk(myDiskStatus,myDiskStatus->clear_file_day*24);
}

#ifdef CSS_TEST

#include "assert.h"
#include "css_ini_file.h"
#include "stdio.h"
#include "stdlib.h"

void test_clear_disk(void)
{
	FILE* fp;
	css_db_config myDBConfig =
	{	0};
#ifdef _WIN32
	disk_status diskStatus =
	{	"C:",0,1,80,0,0,1,30,80};
	tbl_videoFile videoFile1 =
	{	1,1,"C:/11.dat","C:","5","2014-05-23 17:55:07.750","",8,9};
	char iifFile[] = "C:/11.iif";
	char cifFile[] = "C:/11.cif";
#else
	disk_status diskStatus =
	{	".",0,1,80,0,0,1,30,80};
	tbl_videoFile videoFile1 =
	{	1,1,"./11.dat",".","5","2014-05-23 17:55:07.750","",8,9};
	char iifFile[] = "./11.iif";
	char cifFile[] = "./11.cif";
#endif
	printf("clear disk test start ... \n");
	css_load_ini_file(DEFAULT_CONFIG_FILE);
	css_get_db_config(&myDBConfig);
	css_destory_ini_file();
	assert(css_init_db(&myDBConfig) == 0);
	assert(css_connect_db() == 0);
	assert(css_db_insert_video_file(&videoFile1) == 0);
	fp = fopen(iifFile,"w");
	fclose(fp);
	fp = fopen(cifFile,"w");
	fclose(fp);
	fp = fopen(videoFile1.fileName,"w");
	fclose(fp);
	diskStatus.used_per = 100;
	css_clear_disk(&diskStatus);
	printf("clear disk test start ...%s \n",videoFile1.fileName);
	assert( NULL == fopen(videoFile1.fileName,"r"));
	assert( NULL == fopen(iifFile,"r"));
	assert( NULL == fopen(cifFile,"r"));

	printf("clear disk test end! \n");
}

void test_clear_disk_suite()
{
	test_clear_disk();
}

#endif
