#include "css_db.h"
#include <assert.h>
#include "css_db_mysql.h"
#include "css_db_mongo.h"
#include "css_util.h"

static css_db_config* m_dbConfig = NULL;

int db_char2enum(char* dbType) {
	int i = 0;
	for (; i < 3; i++) {
		if ((strcmp(dbType, css_db_type_char[i])) == 0) {
			return i;
		}
	}
	printf("db type not defined! \n");
	return -1;
}

char* db_enum2char(css_db_type dbType) {
	return (char*) css_db_type_char[dbType];
}

int css_init_db(css_db_config* mydbConfig) {
	CHECK_INIT(m_dbConfig, css_db_config);
	printf("init css_db_config ... \n");
	CHECK_CONFIG(mydbConfig);
	m_dbConfig = (css_db_config*) malloc(sizeof(css_db_config));
	memcpy(m_dbConfig, mydbConfig, sizeof(css_db_config));
	m_dbConfig->init_flag = 1;
	return 0;
}

int css_connect_db() {
	DB_FUN(connect_db, m_dbConfig->dbType, m_dbConfig);
}
int css_close_db() {
	DB_FUN(close_db, m_dbConfig->dbType);
}

void css_destroy_db() {
	printf("destroy css_db_config!!! \n");
	FREE(m_dbConfig);
}

int css_db_insert_video_file(tbl_videoFile* myVideoFile) {
	DB_FUN(insert_video_file, m_dbConfig->dbType, m_dbConfig->dbName,
			myVideoFile);
}

int css_db_update_video_file(tbl_videoFile* myVideoFile) {
	DB_FUN(update_video_file, m_dbConfig->dbType, m_dbConfig->dbName,
			myVideoFile);
}
//int css_db_delete_video_file_by_id(int64_t fileID) {
int css_db_delete_video_file_by_id(css_file_record_info* file_info) {
	DB_FUN(delete_video_file_by_id, m_dbConfig->dbType, m_dbConfig->dbName,
			file_info);
}

int css_db_select_video_file_by_begin_time(char beginTime[24], QUEUE* fileList) {
	DB_FUN(select_video_file_by_begin_time, m_dbConfig->dbType,
			m_dbConfig->dbName, beginTime, fileList);
}

int css_db_select_video_file_by_begin_time_and_volume(char beginTime[24],
		char* volume, QUEUE* fileList) {
	DB_FUN(select_video_file_by_begin_time_and_volume, m_dbConfig->dbType,
			m_dbConfig->dbName, beginTime, volume, fileList);
}

int css_db_select_video_file_by_conditions(css_search_conditions_t* conds, QUEUE* fileList) {
	DB_FUN(select_video_file_by_conditions, m_dbConfig->dbType,
			m_dbConfig->dbName, conds, fileList);
}

int css_db_insert_warning(tbl_warning* myWarning) {
	DB_FUN(insert_warning, m_dbConfig->dbType, m_dbConfig->dbName, myWarning);
}

int css_db_update_warning(tbl_warning* myWarning) {
	DB_FUN(update_warning, m_dbConfig->dbType, m_dbConfig->dbName, myWarning);
}

int css_db_delete_warning_by_id(css_alarm_info* myAlarmRecordInfo) {
	DB_FUN(delete_warning_by_id, m_dbConfig->dbType, m_dbConfig->dbName, myAlarmRecordInfo);
}

int css_db_select_warning_by_begin_time(char beginTime[24], QUEUE* alarmList) {
	DB_FUN(select_warning_by_begin_time, m_dbConfig->dbType, m_dbConfig->dbName,
			beginTime, alarmList);
}

int css_db_insert_warn_file(tbl_warningFile* myWarningFile) {
	DB_FUN(insert_warn_file, m_dbConfig->dbType, m_dbConfig->dbName,myWarningFile);
}
int css_db_delete_warn_file_by_alarmid(char* alarmEventId) {
	DB_FUN(delete_warn_file_by_alarmid, m_dbConfig->dbType, m_dbConfig->dbName,alarmEventId);
}
int css_db_select_warn_file_by_alarmid(char* alarmEventId,tbl_warningFile** myWarningFile) {
	DB_FUN(select_warn_file_by_alarmid, m_dbConfig->dbType, m_dbConfig->dbName,alarmEventId,myWarningFile);
}

int css_free_file_list(QUEUE* fileList) {
	QUEUE* q;
	css_file_record_info* myFileRecordInfo;
	while (!QUEUE_EMPTY(fileList)) {
		q = QUEUE_HEAD(fileList);
		myFileRecordInfo = (css_file_record_info*) QUEUE_DATA(q,
				css_file_record_info, queue);
		QUEUE_REMOVE(q);
		FREE(myFileRecordInfo->fields.fileName);
		FREE(myFileRecordInfo->fields.volumeName);
		FREE(myFileRecordInfo->fields.srcPathName);
		FREE(myFileRecordInfo);
	}
	return 0;
}

int css_free_alarm_list(QUEUE* alarmList) {
	QUEUE* q;
	css_alarm_info* myAlarmRecordInfo;
	while (!QUEUE_EMPTY(alarmList)) {
		q = QUEUE_HEAD(alarmList);
		myAlarmRecordInfo = (css_alarm_info*) QUEUE_DATA(q, css_alarm_info,
				queue);
		QUEUE_REMOVE(q);
		//FREE(myAlarmRecordInfo->alarmeventid);
		FREE(myAlarmRecordInfo);
	}
	return 0;
}

/*
 * TEST:
 */
#ifdef CSS_TEST

static QUEUE gtest_file_list;
static QUEUE gtest_alarm_list;

#include "css_ini_file.h"

static tbl_videoFile test_videoFile = {1,2,"3","C:","5","2014-05-23 17:55:07.750","",8,0,9};
static int tmp_count = 3000;
struct timeval st;

void update_db_test_cb(uv_work_t* req){
	int ret = css_db_update_video_file(&test_videoFile);
	assert(ret == 0);
}

void after_update_db_test_cb(uv_work_t* req,int status){
	tmp_count--;
	printf("update db [ok],count:%d\n",tmp_count);
	if(tmp_count ==0){
		struct timeval tt;
		gettimeofday(&tt,NULL);

		printf("over!used %ld ms\n",difftimeval(&tt,&st));
	}
	FREE(req);
}

void insert_db_test_cb(uv_work_t* req)
{
	int ret = css_db_insert_video_file(&test_videoFile);
	assert(ret == 0);
}

void after_insert_db_test_cb(uv_work_t* req,int status)
{
	//	FREE(req);
	int ret;
	uv_work_t* w = (uv_work_t*)malloc(sizeof(uv_work_t));
	assert(w != NULL);
	ret = uv_queue_work(uv_default_loop(),w,update_db_test_cb,after_update_db_test_cb);
	assert(ret == 0);
}

void loop_work(int count){
	uv_work_t* w = (uv_work_t*)malloc(count*sizeof(uv_work_t));
	int ret;
	while(--count){
		ret = uv_queue_work(uv_default_loop(),&(w[count]),insert_db_test_cb,after_insert_db_test_cb);
		assert(ret == 0);
	}
}

void test_db_mutil_threads(void)
{
	int tmp_count0 = tmp_count;
	css_db_config myDBConfig = {0};
	assert(0 == css_load_ini_file(DEFAULT_CONFIG_FILE));
	css_get_db_config(&myDBConfig);
	css_destory_ini_file();

	//test init DB
	assert(m_dbConfig == NULL);
	assert(css_init_db(&myDBConfig) == 0);
	strcpy(m_dbConfig->host,"192.168.8.215");
	strcpy(m_dbConfig->dbName,"test");
	assert(m_dbConfig->port == 27017);
	assert(m_dbConfig->dbType == DBTYPE_MONGO);
	assert(css_connect_db() == 0);
	gettimeofday(&st,NULL);
//	tmp_count = 2;
	loop_work(tmp_count0);
	uv_run(uv_default_loop(),0);
	css_close_db();
	css_destroy_db();
}

void test_db_loop(void)
{
	css_db_config myDBConfig = {0};
	assert(0 == css_load_ini_file(DEFAULT_CONFIG_FILE));
	css_get_db_config(&myDBConfig);
	css_destory_ini_file();

	//test init DB
	assert(m_dbConfig == NULL);
	assert(css_init_db(&myDBConfig) == 0);
	assert(css_connect_db() == 0);
	CL_INFO("loop test start!\n");
	while(tmp_count--){
		assert(css_db_insert_video_file(&test_videoFile) == 0);
	}
	CL_INFO("loop test over!\n");
}


void test_table_videofile(void)
{
	QUEUE *q;
	css_file_record_info* myFileRecordInfo;
	css_file_record_info* test_videoFile = NULL;
	tbl_videoFile m_tab_videoFile = {1,2,"3","C:","5","2014-05-23 17:55:07.750","",8,0,9};
	//css_db_config myDBConfig = {"192.168.8.217",3306,"root","root","test",DBTYPE_MYSQL};

	QUEUE_INIT(&gtest_file_list);
	
	//test delete all videofile
	printf("	db test tbl_videofile ... \n");
	css_db_select_video_file_by_begin_time("2099-06-23 17:55:07.750",&gtest_file_list);
	QUEUE_FOREACH(q, &gtest_file_list)
	{
		myFileRecordInfo = QUEUE_DATA(q, css_file_record_info, queue);
		css_db_delete_video_file_by_id(myFileRecordInfo);
	}
	assert(css_free_file_list(&gtest_file_list) == 0);

	//test insert videofile
	assert(css_db_insert_video_file(&m_tab_videoFile) == 0);

	//test update videofile
	memcpy(m_tab_videoFile.endTime,"2014-05-23 17:55:07.750",24);
	m_tab_videoFile.playLength = 200;
	m_tab_videoFile.available = 1;
	assert(css_db_update_video_file(&m_tab_videoFile) == 0);
	if (m_dbConfig->dbType == DBTYPE_MYSQL)
	{
		assert(select_video_file_by_file_name_mysql(m_dbConfig->dbName,"3",&test_videoFile) == 0);
	} 
	else
	{
#ifdef X64
		assert(select_video_file_by_file_name_mongo(m_dbConfig->dbName,"3",&test_videoFile) == 0);
//		printf("fileid is %lld\n",test_videoFile->fileID);
		assert(test_videoFile->fileID > 0);
#endif
	}
	assert(test_videoFile->fields.playLength == 200);
	assert(test_videoFile->fields.fileType == 0);
	assert(test_videoFile->fields.available == AVAILABLE_VIDEO_FILE);

	//test select videofile false 
	assert(css_db_select_video_file_by_begin_time("2014-04-23 17:55:07.750",&gtest_file_list) == 0);
	assert(QUEUE_EMPTY(&gtest_file_list));

	assert(css_db_select_video_file_by_begin_time_and_volume("2014-04-23 17:55:07.750","D:",&gtest_file_list) == 0);
	assert(QUEUE_EMPTY(&gtest_file_list));

	//test select videofile true
	assert(css_db_select_video_file_by_begin_time_and_volume("2015-04-23 17:55:07.750","C:",&gtest_file_list) == 0);
	assert(!QUEUE_EMPTY(&gtest_file_list));
	assert(css_free_file_list(&gtest_file_list) == 0);

	assert(QUEUE_EMPTY(&gtest_file_list));
	assert(css_db_select_video_file_by_begin_time("2014-06-23 17:55:07.750",&gtest_file_list) == 0);
	assert(!QUEUE_EMPTY(&gtest_file_list));
	QUEUE_FOREACH(q, &gtest_file_list)
	{
		myFileRecordInfo = QUEUE_DATA(q, css_file_record_info, queue);
		//test delete
		assert(css_db_delete_video_file_by_id(myFileRecordInfo) == 0 );
	}

	//test free fileList
	assert(css_free_file_list(&gtest_file_list) == 0);
	assert(QUEUE_EMPTY(&gtest_file_list));

	//test select by conditions
	assert(test_table_videofile_conditions() == 0);
}

int test_table_videofile_conditions()
{
	int num = 0;
	QUEUE *q = NULL;
	css_file_record_info* myFileRecordInfo = NULL;
	css_search_conditions_t conds = {111, 22, 0, "2014-10-07 08:00:00.000", "2014-10-07 18:00:00.000"};

	tbl_videoFile m_tab_videoFile1 = {111,22,"vf1","xxx","xxx","2014-10-06 18:00:00.000","2014-10-07 10:00:00.000",8,0,1};
	tbl_videoFile m_tab_videoFile2 = {111,22,"vf2","xxx","xxx","2014-10-07 11:00:00.000","2014-10-07 16:00:00.000",8,0,1};
	tbl_videoFile m_tab_videoFile3 = {111,22,"vf3","xxx","xxx","2014-10-07 17:00:00.000","2014-10-07 23:00:00.000",8,0,1};
	tbl_videoFile m_tab_videoFile4 = {111,22,"vf4","xxx","xxx","2014-10-07 17:00:00.000","2014-10-07 23:00:00.000",8,1,1};
	tbl_videoFile m_tab_videoFile5 = {111,22,"vf5","xxx","xxx","2014-10-07 17:00:00.000","2014-10-07 23:00:00.000",8,1,0};
	tbl_videoFile m_tab_videoFile6 = {111,21,"vf6","xxx","xxx","2014-10-07 17:00:00.000","2014-10-07 23:00:00.000",8,1,1};
	tbl_videoFile m_tab_videoFile7 = {112,22,"vf7","xxx","xxx","2014-10-07 17:00:00.000","2014-10-07 23:00:00.000",8,1,1};

	// insert items to mongo
	assert(css_db_insert_video_file(&m_tab_videoFile1) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile2) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile3) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile4) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile5) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile6) == 0);
	assert(css_db_insert_video_file(&m_tab_videoFile7) == 0);

	// test select video file by conditions
	QUEUE_INIT(&gtest_file_list);
	assert(css_db_select_video_file_by_conditions(&conds, &gtest_file_list) == 0);
	QUEUE_FOREACH(q, &gtest_file_list)
	{
		num++;
	}
	assert(num == 3);
	assert(css_free_file_list(&gtest_file_list) == 0);
	assert(QUEUE_EMPTY(&gtest_file_list));

	// delete all items in mongo
	QUEUE_INIT(&gtest_file_list);
	assert(css_db_select_video_file_by_begin_time("2099-06-23 17:55:07.750",&gtest_file_list) == 0);
	QUEUE_FOREACH(q, &gtest_file_list)
	{
		myFileRecordInfo = QUEUE_DATA(q, css_file_record_info, queue);
		css_db_delete_video_file_by_id(myFileRecordInfo);
	}
	assert(css_free_file_list(&gtest_file_list) == 0);
	assert(QUEUE_EMPTY(&gtest_file_list));
	return 0;
}
	
void test_table_warning(void){
	QUEUE *q;
	css_alarm_info* myAlarmInfo;
	tbl_warning* test_warning = NULL;
	tbl_warning m_tab_warning = {1,2,"1cs2et","2014-05-23 17:55:07.750",""};

	QUEUE_INIT(&gtest_alarm_list);
	printf("	db test tbl_warning ... \n");
	//test delete all warning
	css_db_select_warning_by_begin_time("2099-06-23 17:55:07.750",&gtest_alarm_list);
	QUEUE_FOREACH(q, &gtest_alarm_list)
	{
		myAlarmInfo = QUEUE_DATA(q, css_alarm_info, queue);
		css_db_delete_warning_by_id(myAlarmInfo);
	}
	css_free_alarm_list(&gtest_alarm_list);

	//test insert warning
	assert(css_db_insert_warning(&m_tab_warning) == 0);

	//test update warning
	memcpy(m_tab_warning.endTime,"2014-05-23 17:55:07.750",24);
	assert(css_db_update_warning(&m_tab_warning) == 0);
	if (m_dbConfig->dbType == DBTYPE_MYSQL)
	{
		assert(select_warning_by_alarmid_mysql(m_dbConfig->dbName,"1cs2et",&test_warning) == 0);
	} 
	else
	{
#ifdef X64
		assert(select_warning_by_alarmid_mongo(m_dbConfig->dbName,"1cs2et",&test_warning) == 0);
#endif
	}
	assert(strcmp(test_warning->endTime,"2014-05-23 17:55:07.750") == 0);

	//test select warning false 
	assert(css_db_select_warning_by_begin_time("2014-04-23 17:55:07.750",&gtest_alarm_list) == 0);
	assert(QUEUE_EMPTY(&gtest_alarm_list));

	//test select warning true
	assert(css_db_select_warning_by_begin_time("2014-06-23 17:55:07.750",&gtest_alarm_list) == 0);
	assert(!QUEUE_EMPTY(&gtest_alarm_list));
	QUEUE_FOREACH(q, &gtest_alarm_list)
	{
		myAlarmInfo = QUEUE_DATA(q, css_alarm_info, queue);
		//test delete
		assert(css_db_delete_warning_by_id(myAlarmInfo) == 0 );
	}

	//test free alarmList
	assert(css_free_alarm_list(&gtest_alarm_list) == 0);
	assert(QUEUE_EMPTY(&gtest_alarm_list));
}

void test_table_warnfile(void){
	tbl_warningFile* test_warningFile = NULL;
	tbl_warningFile m_tbl_warningFile = {"1cs2et132dg","C:/warn_dvr/11.dat"};

	QUEUE_INIT(&gtest_alarm_list);
	printf("	db test tbl_warningFile ... \n");

	//test insert warning
	assert(css_db_insert_warn_file(&m_tbl_warningFile) == 0);

	//test select warning false 
	assert(css_db_select_warn_file_by_alarmid("111111",&test_warningFile) == -1);
	assert(NULL == test_warningFile);

	//test select warning true
	assert(css_db_select_warn_file_by_alarmid("1cs2et132dg",&test_warningFile) == 0);
	assert(0 == strcmp("C:/warn_dvr/11.dat",test_warningFile->fileName));
	
	assert(css_db_delete_warn_file_by_alarmid(test_warningFile->fileName) == 0 );
	
	//test free alarmList
	assert(css_free_alarm_list(&gtest_alarm_list) == 0);
	assert(QUEUE_EMPTY(&gtest_alarm_list));
}

void test_db_api(void)
{
	css_db_config myDBConfig = {0};
	printf("db test start ... \n");

	css_load_ini_file(DEFAULT_CONFIG_FILE);
	css_get_db_config(&myDBConfig);
	css_destory_ini_file();

	//test init DB
	assert(m_dbConfig == NULL);
	assert(css_init_db(&myDBConfig) == 0);
	if (m_dbConfig->dbType == DBTYPE_MYSQL)
	{
		assert(m_dbConfig->port == 3306);
	} 
	else if (m_dbConfig->dbType == DBTYPE_MONGO)
	{
		assert(m_dbConfig->port == 27017);
	}else{
		abort();
	}

	//test connect
	assert(css_connect_db() == 0);

	test_table_videofile();
	test_table_warning();
	test_table_warnfile();


	//test free
	css_close_db();
	css_destroy_db();
	assert(m_dbConfig == NULL);
	printf("db test end! \n");
}

void test_db_suite()
{
	test_db_mutil_threads();
//	test_db_loop();
//	test_db_api();
}
#endif
