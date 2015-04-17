#ifndef __CSS_DB_MONGO_H__
#define __CSS_DB_MONGO_H__

#include "mongoc.h"
#include "bson.h"
#include "css_db.h"

typedef enum {
	TABLE_VIDEA,
	TABLE_WARN,
	TABLE_WARNFILE
} css_mongo_tables;

int connect_db_mongo(css_db_config* mydbConfig);
int close_db_mongo();

int insert_video_file_mongo(const char* dbName,tbl_videoFile* myVideoFile);
int update_video_file_mongo(const char* dbName,tbl_videoFile* myVideoFile);
int delete_video_file_by_id_mongo(const char* dbName,css_file_record_info* file_info);
int select_video_file_by_begin_time_mongo(const char* dbName,char beginTime[24],QUEUE* fileList);
int select_video_file_by_begin_time_and_volume_mongo(const char* dbName,char beginTime[24],char* volume,QUEUE* fileList);
int select_video_file_by_file_name_mongo(const char* dbName,char* fileName,css_file_record_info** myVideoFile);
int select_video_file_by_conditions_mongo(const char* dbName,css_search_conditions_t* conds,QUEUE* fileList);

int insert_warning_mongo(const char* dbName,tbl_warning* myWarning);
int update_warning_mongo(const char* dbName,tbl_warning* myWarning);
int delete_warning_by_id_mongo(const char* dbName,css_alarm_info* myAlarmInfo);
int select_warning_by_begin_time_mongo(const char* dbName,char beginTime[24],QUEUE* alarmList);
int select_warning_by_alarmid_mongo(const char* dbName,char* alarmEventId,tbl_warning** myWarning);

int insert_warn_file_mongo(const char* dbName,tbl_warningFile* myWarningFile);
int delete_warn_file_by_alarmid_mongo(const char* dbName,char* alarmEventId);
int select_warn_file_by_alarmid_mongo(const char* dbName,char* alarmEventId,tbl_warningFile** myWarningFile);

#endif /* __CSS_DB_MONGO_H__ */
