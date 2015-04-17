#ifndef __CSS_DB_MYSQL_H__
#define __CSS_DB_MYSQL_H__

#include <mysql.h>
#include "queue.h"
#include "css_db.h"

//TODO: video_file_table
#define INSERT_VIDEOFILE_QUERY "INSERT DELAYED INTO `%s`.`tbl_cs_videofile` (`dvrEquipId`, `channelNo`, `fileName`, `volumeName`, `srcPathName`, `beginTime`, `endTime`, `cssEquipId`, `playLength`, `available`) \
			  VALUES ('%lld', '%hd', '%s', '%s', '%s', '%s', '%s', '%d', '%lld', '%d')"

#define DELETE_VIDEOFILE_QUERY "DELETE FROM `%s`.`tbl_cs_videofile` WHERE `id`='%lld'"

#define UPDATE_VIDEOFILE_QUERY "UPDATE `%s`.`tbl_cs_videofile` SET `endTime`='%s', `playLength`='%lld', `available`='%d' WHERE `fileName`='%s'"

#define SELECT_VIDEOFILE_BY_BENGINTIME_QUERY "SELECT id,fileName FROM `%s`.`tbl_cs_videofile` WHERE `beginTime`<'%s'"

#define SELECT_VIDEOFILE_BY_BENGINTIME_AND_VOLUME_QUERY "SELECT id,fileName FROM `%s`.`tbl_cs_videofile` WHERE `beginTime`<'%s' and `volumeName`='%s'"

#define SELECT_VIDEOFILE_BY_FILENAME_QUERY "SELECT dvrEquipId,channelNo,fileName,volumeName,srcPathName,beginTime,endTime,cssEquipId,playLength,available FROM `%s`.`tbl_cs_videofile` WHERE `fileName`='%s'"


//TODO: warning_table
#define INSERT_WARNING_QUERY "INSERT INTO `%s`.`tbl_cs_warning` (`dvrEquipId`, `channelNo`, `alarmeventId`, `beginTime`, `endTime`) \
			  VALUES ('%lld', '%hd', '%s', '%s', '%s')"

#define DELETE_WARNING_QUERY "DELETE FROM `%s`.`tbl_cs_warning` WHERE `id`='%lld'"

#define UPDATE_WARNING_QUERY "UPDATE `%s`.`tbl_cs_warning` SET `endTime`='%s' WHERE `alarmeventId`='%s'"

#define SELECT_WARNING_BY_BENGINTIME_QUERY "SELECT id,alarmeventId FROM `%s`.`tbl_cs_warning` WHERE `beginTime`<'%s'"

#define SELECT_WARNING_BY_ALARMEVENTID_QUERY "SELECT dvrEquipId,channelNo,alarmeventId,beginTime,endTime FROM `%s`.`tbl_cs_warning` WHERE `alarmeventId`='%s'"



#define CHECK_MYSQL(s) if( s == NULL)	\
					   { \
						printf("ERROR: mysql = NULL \n"); \
						return -1;	\
					   }

#define CHECK_RESULTS(x) if (results == NULL)	\
						{	\
							printf("selected nothing! \n");	\
							return 0;	\
						}

//TODO: mysql function
int connect_db_mysql(css_db_config* mydbConfig);
int close_db_mysql();

int insert_video_file_mysql(const char* dbName,tbl_videoFile* myVideoFile);
int update_video_file_mysql(const char* dbName,tbl_videoFile* myVideoFile);
int delete_video_file_by_id_mysql(const char* dbName,css_file_record_info* file_info);
int select_video_file_by_begin_time_mysql(const char* dbName,char beginTime[24],QUEUE* fileList);
int select_video_file_by_begin_time_and_volume_mysql(const char* dbName,char beginTime[24],char* volume,QUEUE* fileList);
int select_video_file_by_file_name_mysql(const char* dbName,char* fileName,css_file_record_info** myVideoFile);
int select_video_file_by_conditions_mysql(const char* dbName,css_search_conditions_t* conds,QUEUE* fileList);


int insert_warning_mysql(const char* dbName,tbl_warning* myWarning);
int update_warning_mysql(const char* dbName,tbl_warning* myWarning);
int delete_warning_by_id_mysql(const char* dbName,css_alarm_info* myAlarmRecordInfo);
int select_warning_by_begin_time_mysql(const char* dbName,char beginTime[24],QUEUE* alarmList);
int select_warning_by_alarmid_mysql(const char* dbName,char* alarmEventId,tbl_warning** myWarning);

int insert_warn_file_mysql(const char* dbName,tbl_warningFile* myWarningFile);
int delete_warn_file_by_alarmid_mysql(const char* dbName,char* alarmEventId);
int select_warn_file_by_alarmid_mysql(const char* dbName,char* alarmEventId,tbl_warningFile** myWarningFile);


#endif /* __CSS_DB_MYSQL_H__ */
