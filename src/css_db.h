#ifndef __CSS_DB_H__
#define __CSS_DB_H__

#include "css.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "queue.h"
#include "bson.h"
#include "css_logger.h"

#define WARN_TABLE "tbl_cs_warning"
#define VIDEO_TABLE "tbl_cs_videofile"
#define WARN_FILE_TABLE "tbl_cs_warningtovideofile"

#define NORMAL_VIDEO 0
#define WARNING_VIDEO 1

#define AVAILABLE_VIDEO_FILE 1
#define UNAVAILABLE_VIDEO_FILE 0

#define TOSTRING(x) #x

#define CHECK_DBCONFIG() 							\
	if ( m_dbConfig == NULL) {						\
		CL_ERROR("please init DB ... \n");			\
		return -1;									\
	}

#ifdef X64
#define DB_FUN(funName,type,...)					\
	switch(type) {   								\
		case DBTYPE_MYSQL:  						\
			return funName##_##mysql(__VA_ARGS__);  \
		case DBTYPE_MONGO:  						\
			return funName##_##mongo(__VA_ARGS__);  \
		default:    								\
			CL_ERROR("dbType undefined! \n"); 		\
			return -1;  							\
	}
#else
#define DB_FUN(funName,type,...) 					\
	switch(type) {   								\
		case DBTYPE_MYSQL:  						\
			return funName##_##mysql(__VA_ARGS__);  \
		default:    								\
			CL_ERROR("dbType undefined! \n"); 		\
			return -1;  							\
	}
#endif

typedef enum {
	DBTYPE_MYSQL, DBTYPE_MONGO, DBTYPE_SQLITE
} css_db_type;

static const char* css_db_type_char[] = { "mysql", "mongo", "sqlite" };
int db_char2enum(char* dbType);
char* db_enum2char(css_db_type dbType);

typedef struct {
	char* host;
	uint32_t port;
	char* user;
	char* passwd;
	char* dbName;
	css_db_type dbType;
	int32_t init_flag;			// 0:false(uninit) ; 1:ture(inited)
} css_db_config;

typedef struct {
	uint64_t dvrEquipId;		// DVR Id
	uint16_t channelNo;			// channel number
	char* fileName;				// like:"D:/dvrdat/11/20234.mp4"
	char* volumeName;			// volume name  Win-> D:	\ Unixϵ-> /mnt/sda
	char* srcPathName;			// same as fileName , for ATM to use
	char beginTime[24];			// start time , like:"2014-04-03 07:55:07.750"
	char endTime[24];			// end time   , like:"2014-04-23 17:55:07.750"
	uint64_t playLength;		// play length (ms)
	int32_t fileType;			// 0:normal file 1:alarm file
	int32_t available;			// 0:false ; 1:true
} tbl_videoFile;
 
typedef struct {
	int64_t fileID;				// fileID in DB
	bson_iter_t fileOID; 		// fileOID in mongoDB
 	tbl_videoFile fields;
	void* queue[2];
} css_file_record_info;

typedef struct {
	uint64_t dvrEquipId;		// DVR Id
	uint32_t channelNo;			// channel number
	int32_t fileType;			// 0:normal file 1:alarm file 
	char beginTime[24];			// start time , like:"2014-04-03 07:55:07.750"
	char endTime[24];			// end time   , like:"2014-04-23 17:55:07.750"
} css_search_conditions_t;

typedef struct {
	uint64_t dvrEquipId;		// DVR Id
	uint16_t channelNo;			// channel number
	char alarmeventid[128];		// alarm event id
	char beginTime[24];			// alarm start time , like:"2014-04-03 07:55:07.750"
	char endTime[24];			// alarm end time , like:"2014-04-03 07:55:07.750"
} tbl_warning;

typedef struct {
	int64_t id;					// alarm id in mysql db
	bson_iter_t oid;			// alarm OID in mongo db
	char alarmeventid[128];		// alarm event id
	void* queue[2];
} css_alarm_info;

typedef struct {
	char alarmeventid[128];		// alarm event id
	char* fileName;				// video file name
} tbl_warningFile;

int css_init_db(css_db_config* mydbConfig);
int css_connect_db();
int css_close_db();
void css_destroy_db();

int css_db_insert_video_file(tbl_videoFile* myVideoFile);
int css_db_update_video_file(tbl_videoFile* myVideoFile);
int css_db_delete_video_file_by_id(css_file_record_info* file_info);
/*
 use css_free_file_list API to FREE the fileList
 */
int css_db_select_video_file_by_begin_time(char beginTime[24], QUEUE* fileList);
/*
 use css_free_file_list API to FREE the fileList
 */
int css_db_select_video_file_by_begin_time_and_volume(char beginTime[24],
		char* volume, QUEUE* fileList);
/*
 use css_free_file_list API to FREE the fileList
 */
int css_db_select_video_file_by_conditions(css_search_conditions_t*,
		QUEUE* fileList);
int css_free_file_list(QUEUE* fileList);

int css_db_insert_warning(tbl_warning* myWarning);
int css_db_update_warning(tbl_warning* myWarning);
int css_db_delete_warning_by_id(css_alarm_info* myAlarmRecordInfo);
/*
 use css_free_alarm_list API to FREE the alarmList
 */
int css_db_select_warning_by_begin_time(char beginTime[24], QUEUE* alarmList);
int css_free_alarm_list(QUEUE* alarmList);

int css_db_insert_warn_file(tbl_warningFile* myWarningFile);
int css_db_delete_warn_file_by_alarmid(char* alarmEventId);
int css_db_select_warn_file_by_alarmid(char* alarmEventId,
		tbl_warningFile** myWarningFile);

#endif /* __CSS_DB_H__ */

