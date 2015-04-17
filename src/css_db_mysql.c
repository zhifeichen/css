#include "css_db.h"
#include "css_db_mysql.h"
#include "css_util.h"
#include <assert.h>
#include "css_logger.h"

#define CSS_POOL_SIZE 10

typedef struct css_mysql_s
{
	int		in_using;
	MYSQL	mysql;
}css_mysql_t;

struct css_mysql_pool_s
{
	uv_mutex_t	mutex;
	uv_sem_t	sem;
	css_mysql_t	mysql[CSS_POOL_SIZE];
};

static struct css_mysql_pool_s the_mysql_pool;
static MYSQL* m_sql = NULL;

static int css_mysql_init(css_db_config* mydbConfig);
static void css_mysql_fini(void);
static css_mysql_t*	css_mysql_get(void);
static void css_mysql_release(css_mysql_t* t);

static int css_mysql_init(css_db_config* mydbConfig)
{
	int value = 1;
	int ret = 0;
	MYSQL* S;
	int i = 0;

	uv_sem_init(&the_mysql_pool.sem, CSS_POOL_SIZE);
	uv_mutex_init(&the_mysql_pool.mutex);
	for(i = 0; i < CSS_POOL_SIZE; i++){
		mysql_init(&the_mysql_pool.mysql[i].mysql);
		S = mysql_real_connect(&(the_mysql_pool.mysql[i].mysql),mydbConfig->host,mydbConfig->user,mydbConfig->passwd,mydbConfig->dbName,mydbConfig->port,NULL,0);
		assert(S);
		mysql_options(&the_mysql_pool.mysql[i].mysql, MYSQL_OPT_RECONNECT, &value);
		the_mysql_pool.mysql[i].in_using = 0;
	}
	return ret;
}

static void css_mysql_fini(void)
{
	int i = 0;
	for(i = 0; i < CSS_POOL_SIZE; i++){
		mysql_close(&the_mysql_pool.mysql[i].mysql);
		the_mysql_pool.mysql[i].in_using = -1;
	}
	uv_sem_destroy(&the_mysql_pool.sem);
	uv_mutex_destroy(&the_mysql_pool.mutex);
}

static css_mysql_t*	css_mysql_get(void)
{
	int i = 0;
	css_mysql_t* t = NULL;
	uv_sem_wait(&the_mysql_pool.sem);
	uv_mutex_lock(&the_mysql_pool.mutex);
	for(i = 0; i < CSS_POOL_SIZE; i++){
		if(the_mysql_pool.mysql[i].in_using == 0){
			t = &the_mysql_pool.mysql[i];
			the_mysql_pool.mysql[i].in_using = 1;
			break;
		}
	}
	uv_mutex_unlock(&the_mysql_pool.mutex);
	return t;
}

static void css_mysql_release(css_mysql_t* t)
{
	if(!t){
		return ;
	}

	t->in_using = 0;
	uv_sem_post(&the_mysql_pool.sem);
}

int connect_db_mysql(css_db_config* mydbConfig)
{
	char value = 1;
	MYSQL* mysql = (MYSQL*)malloc(sizeof(MYSQL));
	mysql_library_init(0,NULL,NULL);
	mysql_init(mysql);
	if (!mysql_real_connect(mysql,mydbConfig->host,mydbConfig->user,mydbConfig->passwd,mydbConfig->dbName,mydbConfig->port,NULL,0))
	{
		printf( "Error connecting to mysql database: %s \n",mysql_error(mysql));
		return -1;
	}
	else printf("mysql Connected... \n");
	mysql_options(mysql, MYSQL_OPT_RECONNECT, &value);
	m_sql = mysql;

	return css_mysql_init(mydbConfig);
	return 0;
}

int close_db_mysql()
{
	if (m_sql != NULL)
	{
		mysql_close(m_sql);
		FREE(m_sql);
		mysql_library_end();
	}
	css_mysql_fini();
	return 0;
}

int insert_video_file_mysql(const char* dbName,tbl_videoFile* myVideoFile)
{
	char s[1024] = {0};
	int ret;

	css_mysql_t* t;
	t = css_mysql_get();
	if(!t){
		printf("all sql connection are busy...\n");
		return -1;
	}
	CHECK_MYSQL(m_sql);
	sprintf(s,
		INSERT_VIDEOFILE_QUERY,
		dbName,
		myVideoFile->dvrEquipId,  \
		myVideoFile->channelNo,   \
		myVideoFile->fileName,    \
		myVideoFile->volumeName,  \
		myVideoFile->srcPathName, \
		myVideoFile->beginTime,   \
		myVideoFile->endTime,     \
		0,  \
		myVideoFile->playLength,  \
		myVideoFile->available);
	//ret = mysql_real_query(m_sql,s,(uint32_t)strlen(s));
	ret = mysql_real_query(&t->mysql,s,(uint32_t)strlen(s));
	if(ret){
		printf("insert_video_file_mysql-> mysql_real_query return %d\n", ret);
	}

	css_mysql_release(t);
	return ret;
}

int update_video_file_mysql(const char* dbName,tbl_videoFile* myVideoFile)
{
	char s[1024];
	int ret;

	css_mysql_t* t;
	t = css_mysql_get();
	if(!t){
		printf("all sql connection are busy...\n");
		return -1;
	}

	CHECK_MYSQL(m_sql);
	sprintf(s,
		//UPDATE_VIDEOFILE_QUERY,
		"UPDATE `%s`.`tbl_cs_videofile` SET `endTime`='%s', `playLength`='%lld', `available`='%d' WHERE `fileName`='%s';",
		dbName,
		myVideoFile->endTime,     \
		myVideoFile->playLength,  \
		myVideoFile->available,   \
		myVideoFile->fileName);
	//ret = mysql_real_query(m_sql,s,(uint32_t)strlen(s));
	ret = mysql_real_query(&t->mysql,s,(uint32_t)strlen(s));
	if(ret){
		printf("update_video_file_mysql-> mysql_real_query return %d\n", ret);
	}

	css_mysql_release(t);
	return ret;
}

int delete_video_file_by_id_mysql(const char* dbName,css_file_record_info* file_info)
{
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,DELETE_VIDEOFILE_QUERY,dbName,file_info->fileID);
	return mysql_real_query(m_sql,s,(uint32_t)strlen(s));
}

int select_video_file_by_begin_time_mysql(const char* dbName,char beginTime[24],QUEUE* fileList)
{
	MYSQL_RES* results;
	MYSQL_ROW record;
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,SELECT_VIDEOFILE_BY_BENGINTIME_QUERY,dbName,beginTime);
	if (mysql_real_query(m_sql,s,(uint32_t)strlen(s)) == 0)
	{
		results = mysql_store_result(m_sql);
		CHECK_RESULTS(results);
		while((record = mysql_fetch_row(results))) {
			css_file_record_info* myFileRecordInfo = (css_file_record_info*)malloc(sizeof(css_file_record_info));
			memset(myFileRecordInfo, 0, sizeof(css_file_record_info));
			QUEUE_INIT(&(myFileRecordInfo->queue));
			myFileRecordInfo->fileID = ATOLL(record[0]);
			COPY_STR(myFileRecordInfo->fields.fileName,record[1]);
			QUEUE_INSERT_TAIL(fileList, &(myFileRecordInfo->queue));
//			printf("id:%lld - fileName:%s \n", myFileRecordInfo->fileID, myFileRecordInfo->fileName);
		}
		mysql_free_result(results);
		return 0;
	}else
	{
		printf("select error : %s \n",mysql_error(m_sql));
		return -1;
	}
}

int select_video_file_by_begin_time_and_volume_mysql(const char* dbName,char beginTime[24],char* volume,QUEUE* fileList)
{
	MYSQL_RES* results;
	MYSQL_ROW record;
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,SELECT_VIDEOFILE_BY_BENGINTIME_AND_VOLUME_QUERY,dbName,beginTime,volume);
	if (mysql_real_query(m_sql,s,(uint32_t)strlen(s)) == 0)
	{
		results = mysql_store_result(m_sql);
		CHECK_RESULTS(results);
		while((record = mysql_fetch_row(results))) {
			css_file_record_info* myFileRecordInfo = (css_file_record_info*)malloc(sizeof(css_file_record_info));
			memset(myFileRecordInfo, 0, sizeof(css_file_record_info));
			QUEUE_INIT(&(myFileRecordInfo->queue));
			myFileRecordInfo->fileID = ATOLL(record[0]);
			COPY_STR(myFileRecordInfo->fields.fileName,record[1]);
			QUEUE_INSERT_TAIL(fileList, &(myFileRecordInfo->queue));
//			printf("id:%lld - fileName:%s \n", myFileRecordInfo->fileID, myFileRecordInfo->fileName);
		}
		mysql_free_result(results);
		return 0;
	}else
	{
		printf("select error : %s \n",mysql_error(m_sql));
		return -1;
	}
}

/*
this API just to test
*/
int select_video_file_by_file_name_mysql(const char* dbName,char* fileName,css_file_record_info** videoFile)
{
	MYSQL_RES* results;
	MYSQL_ROW record;
	char s[1024];
	css_file_record_info* my_videoFile = NULL;
	tbl_videoFile* mytbl_videoFile = NULL;
	CHECK_MYSQL(m_sql);
	sprintf(s,SELECT_VIDEOFILE_BY_FILENAME_QUERY,dbName,fileName);
	if (mysql_real_query(m_sql,s,(uint32_t)strlen(s)) == 0)
	{
		results = mysql_store_result(m_sql);
		CHECK_RESULTS(results);
		while((record = mysql_fetch_row(results))) {
			my_videoFile = (css_file_record_info*)malloc(sizeof(css_file_record_info));
			memset(my_videoFile, 0, sizeof(css_file_record_info));
			mytbl_videoFile = &(my_videoFile->fields);
			//dvrEquipId,channelNo,fileName,volumeName,srcPathName,beginTime,endTime,cssEquipId,playLength,available
			mytbl_videoFile->dvrEquipId = ATOLL(record[0]);
			mytbl_videoFile->channelNo = (uint16_t)atoi(record[1]);
			COPY_STR(mytbl_videoFile->fileName,record[2]);
			COPY_STR(mytbl_videoFile->volumeName,record[3]);
			COPY_STR(mytbl_videoFile->srcPathName,record[4]);
			strcpy(mytbl_videoFile->beginTime,record[5]);
			strcpy(mytbl_videoFile->endTime,record[6]);
			//cssEquipId always 0 ,record[7]
			mytbl_videoFile->playLength = ATOLL(record[8]);
			mytbl_videoFile->available = atoi(record[9]);			
		}
		mysql_free_result(results);
		*videoFile = my_videoFile;
		return 0;
	}else
	{
		printf("select error : %s \n",mysql_error(m_sql));
		return -1;
	}
}

int select_video_file_by_conditions_mysql(const char* dbName,css_search_conditions_t* conds,QUEUE* fileList){
	CL_ERROR("this fun not implement!!\n");
	return -1;
}

int insert_warning_mysql(const char* dbName,tbl_warning* myWarning)
{
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,INSERT_WARNING_QUERY,dbName,
		myWarning->dvrEquipId,  \
		myWarning->channelNo,	\
		myWarning->alarmeventid,	\
		myWarning->beginTime,	\
		myWarning->endTime);
	return mysql_real_query(m_sql,s,(uint32_t)strlen(s));
}

int update_warning_mysql(const char* dbName,tbl_warning* myWarning)
{
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,UPDATE_WARNING_QUERY,dbName,
		myWarning->endTime,	\
		myWarning->alarmeventid);
	return mysql_real_query(m_sql,s,(uint32_t)strlen(s));
}

int delete_warning_by_id_mysql(const char* dbName,css_alarm_info* myAlarmRecordInfo)
{
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,DELETE_WARNING_QUERY,dbName,myAlarmRecordInfo->id);
	return mysql_real_query(m_sql,s,(uint32_t)strlen(s));
}

int select_warning_by_begin_time_mysql(const char* dbName,char beginTime[24],QUEUE* alarmList)
{
	MYSQL_RES* results;
	MYSQL_ROW record;
	char s[1024];
	CHECK_MYSQL(m_sql);
	sprintf(s,SELECT_WARNING_BY_BENGINTIME_QUERY,dbName,beginTime);
	if (mysql_real_query(m_sql,s,(uint32_t)strlen(s)) == 0)
	{
		results = mysql_store_result(m_sql);
		CHECK_RESULTS(results);
		while((record = mysql_fetch_row(results))) {
			css_alarm_info* myAlarmRecordInfo = (css_alarm_info*)malloc(sizeof(css_alarm_info));
			QUEUE_INIT(&(myAlarmRecordInfo->queue));
			myAlarmRecordInfo->id = ATOLL(record[0]);
			strcpy(myAlarmRecordInfo->alarmeventid,record[1]);
			QUEUE_INSERT_TAIL(alarmList, &(myAlarmRecordInfo->queue));
//			printf("id:%lld - alarmeventid:%s \n", myAlarmRecordInfo->id, myAlarmRecordInfo->alarmeventid);
		}
		mysql_free_result(results);
		return 0;
	}else
	{
		printf("select error : %s \n",mysql_error(m_sql));
		return -1;
	}
}

/*
this API just to test
*/
int select_warning_by_alarmid_mysql(const char* dbName,char* alarmEventId,tbl_warning** myWarning)
{
	MYSQL_RES* results;
	MYSQL_ROW record;
	char s[1024];
	tbl_warning* mytbl_warning = NULL;
	CHECK_MYSQL(m_sql);
	sprintf(s,SELECT_WARNING_BY_ALARMEVENTID_QUERY,dbName,alarmEventId);
	if (mysql_real_query(m_sql,s,(uint32_t)strlen(s)) == 0)
	{
		results = mysql_store_result(m_sql);
		CHECK_RESULTS(results);
		while((record = mysql_fetch_row(results))) {
			mytbl_warning = (tbl_warning*)malloc(sizeof(tbl_warning));
			//dvrEquipId,channelNo,alarmeventId,beginTime,endTime
			mytbl_warning->dvrEquipId = ATOLL(record[0]);
			mytbl_warning->channelNo = (uint16_t)atoi(record[1]);
			strcpy(mytbl_warning->alarmeventid,record[2]);
			strcpy(mytbl_warning->beginTime,record[3]);
			strcpy(mytbl_warning->endTime,record[4]);
		}
		mysql_free_result(results);
		*myWarning = mytbl_warning;
		return 0;
	}else
	{
		printf("select error : %s \n",mysql_error(m_sql));
		return -1;
	}
}

int insert_warn_file_mysql(const char* dbName,tbl_warningFile* myWarningFile)
{
	return 0;
}
int delete_warn_file_by_alarmid_mysql(const char* dbName,char* alarmEventId)
{
	return 0;
}
int select_warn_file_by_alarmid_mysql(const char* dbName,char* alarmEventId,tbl_warningFile** myWarningFile)
{
	return 0;
}
