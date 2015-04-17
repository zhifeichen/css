#include "css_db_mongo.h"
#include <string.h>
#include "css_logger.h"
#include "bson.h"

//static mongoc_client_t* m_mg_client;
//static mongoc_collection_t* videofile_cllc;
//static mongoc_collection_t* warning_cllc;
//static mongoc_collection_t* warnflie_cllc;
static char* db_name;
static mongoc_client_pool_t* pool;

int connect_db_mongo(css_db_config* mydbConfig) {
	char mongo_addr[MAX_PATH];
	mongoc_uri_t* uri;
	mongoc_init();
	sprintf(mongo_addr, "mongodb://%s:%d/?minPoolSize=16", mydbConfig->host,
			mydbConfig->port);
	//sprintf(mongo_addr,"mongodb://%s:%s@%s:%d/?authSource=%s",mydbConfig->user,mydbConfig->passwd,mydbConfig->host,mydbConfig->port,mydbConfig->dbName);

	COPY_STR(db_name, mydbConfig->dbName);
	uri = mongoc_uri_new(mongo_addr);
	pool = mongoc_client_pool_new(uri);
	mongoc_uri_destroy(uri);

//	m_mg_client = mongoc_client_new(mongo_addr);
//	videofile_cllc = mongoc_client_get_collection(m_mg_client,
//			mydbConfig->dbName, VIDEO_TABLE);
//	warning_cllc = mongoc_client_get_collection(m_mg_client, mydbConfig->dbName,
//	WARN_TABLE);
//	warnflie_cllc = mongoc_client_get_collection(m_mg_client,
//			mydbConfig->dbName, WARN_FILE_TABLE);
	return 0;
}

int close_db_mongo() {
//	mongoc_collection_destroy(videofile_cllc);
//	mongoc_collection_destroy(warnflie_cllc);
//	mongoc_collection_destroy(warning_cllc);
//	mongoc_client_destroy(m_mg_client);
	mongoc_client_pool_destroy(pool);
	mongoc_cleanup();
	return 0;
}

static int64_t get_max_file_id();

static int create_insert_video_bson_doc(tbl_videoFile* myVideoFile, bson_t* doc) {
	int64_t maxid = 0;
	if (myVideoFile == NULL) {
		return -1;
	}

	if ((maxid = get_max_file_id()) <= 0) {
		CL_ERROR("get max file id error!!\n");
		return -1;
	}

	BSON_APPEND_INT64(doc, "fileId", maxid);
	BSON_APPEND_INT64(doc, "dvrEquipId", myVideoFile->dvrEquipId);
	BSON_APPEND_INT32(doc, "channelNo", (int32_t )(myVideoFile->channelNo));
	BSON_APPEND_UTF8(doc, "fileName", myVideoFile->fileName);
	BSON_APPEND_UTF8(doc, "volumeName", myVideoFile->volumeName);
	BSON_APPEND_UTF8(doc, "srcPathName", myVideoFile->srcPathName);
	BSON_APPEND_UTF8(doc, "beginTime", myVideoFile->beginTime);
	BSON_APPEND_UTF8(doc, "endTime", myVideoFile->endTime);
	BSON_APPEND_INT32(doc, "cssEquipId", 0);
	BSON_APPEND_INT64(doc, "playLength", myVideoFile->playLength);
	BSON_APPEND_INT32(doc, "fileType", myVideoFile->fileType);
	BSON_APPEND_INT32(doc, "available", myVideoFile->available);
	return 0;
}
static int create_warn_bson_doc(tbl_warning* myWarning, bson_t* doc) {
	if (myWarning == NULL) {
		return -1;
	}
	BSON_APPEND_INT64(doc, "dvrEquipId", myWarning->dvrEquipId);
	BSON_APPEND_INT32(doc, "channelNo", (int32_t )(myWarning->channelNo));
	BSON_APPEND_UTF8(doc, "alarmEventId", myWarning->alarmeventid);
	BSON_APPEND_UTF8(doc, "beginTime", myWarning->beginTime);
	BSON_APPEND_UTF8(doc, "endTime", myWarning->endTime);
	return 0;
}

static char* get_table_name(css_mongo_tables table) {
	switch (table) {
	case TABLE_VIDEA:
		return VIDEO_TABLE;
	case TABLE_WARN:
		return WARN_TABLE;
	case TABLE_WARNFILE:
		return WARN_FILE_TABLE;
	default:
		CL_ERROR("not found this table!!\n");
	}
	return NULL;
}

static int mongo_insert(bson_t* doc, css_mongo_tables table) {
	int ret = -1;
	bson_error_t error;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	char* table_name;

	client = mongoc_client_pool_pop(pool);

	if ((table_name = get_table_name(table)) != NULL) {
		CL_INFO("table_name:%s,xj\n", table_name);
		cllc = mongoc_client_get_collection(client, db_name, table_name);
		ret = mongoc_collection_insert(cllc, MONGOC_INSERT_NONE, doc,
		NULL, &error);
		mongoc_collection_destroy(cllc);
		mongoc_client_pool_push(pool, client);
	} else {
		return ret;
	}

	if (!ret) {
		CL_ERROR("%d,%s\n", error.code, error.message);
	}
//bool to int,true=1
	return (ret - 1);
}
static int mongo_update(bson_t* doc, bson_t* query, css_mongo_tables table) {
	int ret = -1;
	bson_error_t error;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	char* table_name;

	client = mongoc_client_pool_pop(pool);

	if ((table_name = get_table_name(table)) != NULL) {
		CL_INFO("table_name:%s,xj\n", table_name);
		cllc = mongoc_client_get_collection(client, db_name, table_name);
		ret = mongoc_collection_update(cllc, MONGOC_UPDATE_NONE, query, doc,
		NULL, &error);
		mongoc_collection_destroy(cllc);
		mongoc_client_pool_push(pool, client);
	} else {
		return ret;
	}

	if (!ret) {
		CL_ERROR("%s\n", error.message);
	}
//bool to int,true=1
	return (ret - 1);
}
static int mongo_delete(bson_t* doc, css_mongo_tables table) {
	int ret = -1;
	bson_error_t error;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	char* table_name;

	client = mongoc_client_pool_pop(pool);

	if ((table_name = get_table_name(table)) != NULL) {
		CL_INFO("table_name:%s,xj\n", table_name);
		cllc = mongoc_client_get_collection(client, db_name, table_name);
		ret = mongoc_collection_remove(cllc, MONGOC_REMOVE_SINGLE_REMOVE, doc,
		NULL, &error);
		mongoc_collection_destroy(cllc);
		mongoc_client_pool_push(pool, client);
	} else {
		return ret;
	}

	if (!ret) {
		CL_ERROR("%s\n", error.message);
	}
//bool to int,true=1
	return (ret - 1);
}

int insert_video_file_mongo(const char* dbName, tbl_videoFile* myVideoFile) {
	bson_t* doc;
	int ret = -1;

	doc = bson_new();
	do {
		CHECK_FALSE_TO_BREAK(create_insert_video_bson_doc(myVideoFile, doc));
		ret = mongo_insert(doc, TABLE_VIDEA);
	} while (0);
	bson_destroy(doc);
	return ret;
}

int update_video_file_mongo(const char* dbName, tbl_videoFile* myVideoFile) {
	bson_t* doc;
	bson_t* query;
	int ret = -1;

//	doc = bson_new();
//	create_update_video_bson_doc(myVideoFile, doc);
	doc = BCON_NEW("$set", "{", "endTime", BCON_UTF8(myVideoFile->endTime),
			"playLength", BCON_INT64(myVideoFile->playLength), "available",
			BCON_INT32(myVideoFile->available), "}");

	query = BCON_NEW("fileName", BCON_UTF8(myVideoFile->fileName));
	ret = mongo_update(doc, query, TABLE_VIDEA);
	bson_destroy(doc);
	bson_destroy(query);
	return ret;
}

int delete_video_file_by_id_mongo(const char* dbName,
		css_file_record_info* file_info) {
	bson_t* doc;
	int ret = -1;

	doc = BCON_NEW("_id", BCON_OID(bson_iter_oid(&(file_info->fileOID))));
//	doc = BCON_NEW ("_id",bson_iter_oid(&(file_info->fileOID)));
	ret = mongo_delete(doc, TABLE_VIDEA);
	bson_destroy(doc);
	return ret;
}

static int get_bson_value_uint64(const bson_t* bson, char* key, uint64_t* value) {
	int ret = -1;
	bson_iter_t baz;
	bson_iter_t iter;
	if (bson_iter_init(&iter, bson)&&bson_iter_find_descendant(&iter, key,
			&baz) && BSON_ITER_HOLDS_INT64(&baz)) {
		*value = bson_iter_int64(&baz);
		ret = 0;
	}
	return ret;
}
static int get_bson_value_int32(const bson_t* bson, char* key, int32_t* value) {
	int ret = -1;
	bson_iter_t baz;
	bson_iter_t iter;
	if (bson_iter_init(&iter, bson)&&bson_iter_find_descendant(&iter, key,
			&baz) && BSON_ITER_HOLDS_INT32(&baz)) {
		*value = bson_iter_int32(&baz);
		ret = 0;
	}
	return ret;
}
static int get_bson_value_utf8(const bson_t* bson, char* key, char* value) {
	int ret = -1;
	bson_iter_t baz;
	bson_iter_t iter;
	if (bson_iter_init(&iter,
			bson)&&bson_iter_find_descendant(&iter, key, &baz) && BSON_ITER_HOLDS_UTF8(&baz)) {
		strcpy(value, bson_iter_utf8(&baz, &(baz.len)));
		ret = 0;
	}
	return ret;
}
static int get_bson_value_oid(const bson_t* bson, char* key, bson_iter_t* value) {
	int ret = -1;
	bson_iter_t iter;
	if (bson_iter_init(&iter, bson)&&bson_iter_find_descendant(&iter, key,
			value) && BSON_ITER_HOLDS_OID(value)) {
		ret = 0;
	}
	return ret;
}
static int get_video_bson_all_value(const bson_t* bson,
		css_file_record_info** videofile) {
	int ret = -1;
//	bson_iter_t iter;
	int tmp = 0;
	char value[MAX_PATH];
//	if (!bson_iter_init(&iter, bson))
//		return ret;
//printf("%s\n",bson_as_json(bson,NULL));
	do {
		css_file_record_info* my_videoFile = (css_file_record_info*) malloc(
				sizeof(css_file_record_info));
		tbl_videoFile* mytbl_videoFile = &(my_videoFile->fields);

		CHECK_FALSE_TO_BREAK(
				get_bson_value_oid(bson, "_id", &(my_videoFile->fileOID)));

		CHECK_FALSE_TO_BREAK(
				get_bson_value_uint64(bson, "fileId", &(my_videoFile->fileID)));

		CHECK_FALSE_TO_BREAK(
				get_bson_value_uint64(bson, "dvrEquipId",
						&(mytbl_videoFile->dvrEquipId)));
		CHECK_FALSE_TO_BREAK(get_bson_value_int32(bson, "channelNo", &tmp));
		mytbl_videoFile->channelNo = (uint16_t) tmp;

		CHECK_FALSE_TO_BREAK(get_bson_value_utf8(bson, "fileName", value));
		COPY_STR(mytbl_videoFile->fileName, value);
		memset(value, 0, MAX_PATH);

		CHECK_FALSE_TO_BREAK(get_bson_value_utf8(bson, "volumeName", value));
		COPY_STR(mytbl_videoFile->volumeName, value);
		memset(value, 0, MAX_PATH);

		CHECK_FALSE_TO_BREAK(get_bson_value_utf8(bson, "srcPathName", value));
		COPY_STR(mytbl_videoFile->srcPathName, value);
		memset(value, 0, MAX_PATH);

		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "beginTime",
						mytbl_videoFile->beginTime));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "endTime", mytbl_videoFile->endTime));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_uint64(bson, "playLength",
						&(mytbl_videoFile->playLength)));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_int32(bson, "fileType",
						&(mytbl_videoFile->fileType)));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_int32(bson, "available",
						&(mytbl_videoFile->available)));

		*videofile = my_videoFile;
		ret = 0;
	} while (0);
	return ret;
}
static int get_warn_bson_all_value(const bson_t* bson, tbl_warning** warning) {
	int ret = -1;
//	bson_iter_t iter;
	int tmp = 0;
//	if (!bson_iter_init(&iter, bson))
//		return ret;
//printf("%s\n",bson_as_json(bson,NULL));
	do {
		tbl_warning* mytbl_warning = (tbl_warning*) malloc(sizeof(tbl_warning));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_uint64(bson, "dvrEquipId",
						&(mytbl_warning->dvrEquipId)));
		CHECK_FALSE_TO_BREAK(get_bson_value_int32(bson, "channelNo", &tmp));
		mytbl_warning->channelNo = (uint16_t) tmp;

		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "alarmEventId",
						mytbl_warning->alarmeventid));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "beginTime",
						mytbl_warning->beginTime));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "endTime", mytbl_warning->endTime));
		*warning = mytbl_warning;
		ret = 0;
	} while (0);
	return ret;
}
static int get_warnfile_bson_all_value(const bson_t* bson,
		tbl_warningFile** warnfile) {
	int ret = -1;
//	bson_iter_t iter;
	char value[MAX_PATH];
//	if (!bson_iter_init(&iter, bson))
//		return ret;
//printf("%s\n",bson_as_json(bson,NULL));
	do {
		tbl_warningFile* mytbl_warnfile = (tbl_warningFile*) malloc(
				sizeof(tbl_warningFile));
		CHECK_FALSE_TO_BREAK(
				get_bson_value_utf8(bson, "alarmEventId",
						mytbl_warnfile->alarmeventid));
		CHECK_FALSE_TO_BREAK(get_bson_value_utf8(bson, "fileName", value));
		COPY_STR(mytbl_warnfile->fileName, value);
		*warnfile = mytbl_warnfile;
		ret = 0;
	} while (0);
	return ret;
}

static int select_video_file(const char* dbName, bson_t* query, QUEUE* fileList) {
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	int ret = -1;
	css_file_record_info* myFileRecordInfo = NULL;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	cllc = mongoc_client_get_collection(client, dbName, VIDEO_TABLE);
	cursor = mongoc_collection_find(cllc, MONGOC_QUERY_NONE, 0, 0, 0, query,
	NULL, NULL);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	while (mongoc_cursor_next(cursor, &doc)) {

		if (!get_video_bson_all_value(doc, &myFileRecordInfo)) {
			QUEUE_INIT(&(myFileRecordInfo->queue));
			QUEUE_INSERT_TAIL(fileList, &(myFileRecordInfo->queue));
		}
	}
	ret = 0;
	bson_destroy(query);
	mongoc_cursor_destroy(cursor);
	return ret;
}

int select_video_file_by_conditions_mongo(const char* dbName,
		css_search_conditions_t* conds, QUEUE* fileList) {
	bson_t *query;

	query = BCON_NEW("$and", "[", "{", "dvrEquipId",
			BCON_INT64(conds->dvrEquipId), "}", "{", "channelNo",
			BCON_INT32(conds->channelNo), "}", "{", "fileType",
			BCON_INT32(conds->fileType), "}", "{", "available",
			BCON_INT32(AVAILABLE_VIDEO_FILE), "}", "{", "$nor", "[", "{",
			"beginTime", "{", "$gt", BCON_UTF8(conds->endTime), "}", "}", "{",
			"endTime", "{", "$lt", BCON_UTF8(conds->beginTime), "}", "}", "]",
			"}", "]");
//	printf("%s\n",bson_as_json(query,NULL));
	return select_video_file(dbName, query, fileList);
}

int select_video_file_by_begin_time_mongo(const char* dbName,
		char beginTime[24], QUEUE* fileList) {
	bson_t *query;
	query = BCON_NEW("beginTime", "{", "$lt", BCON_UTF8(beginTime), "}");
	return select_video_file(dbName, query, fileList);
}

int select_video_file_by_begin_time_and_volume_mongo(const char* dbName,
		char beginTime[24], char* volume, QUEUE* fileList) {
	bson_t *query;
	query = BCON_NEW("beginTime", "{", "$lt", BCON_UTF8(beginTime), "}",
			"volumeName", BCON_UTF8(volume));
	return select_video_file(dbName, query, fileList);
}

/*
 this API just to test
 */
int select_video_file_by_file_name_mongo(const char* dbName, char* fileName,
		css_file_record_info** myVideoFile) {
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_t *query;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	int ret = -1;

	query = BCON_NEW("fileName", BCON_UTF8(fileName));
	cllc = mongoc_client_get_collection(client, dbName, VIDEO_TABLE);
	cursor = mongoc_collection_find(cllc, MONGOC_QUERY_NONE, 0, 0, 0, query,
	NULL, NULL);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	while (mongoc_cursor_next(cursor, &doc)) {
		if (get_video_bson_all_value(doc, myVideoFile) == 0) {
			ret = 0;
		}
	}
	return ret;
}

int insert_warning_mongo(const char* dbName, tbl_warning* myWarning) {
	bson_t* doc;
	int ret = -1;

	doc = bson_new();
	create_warn_bson_doc(myWarning, doc);
	ret = mongo_insert(doc, TABLE_WARN);
	bson_destroy(doc);
	return ret;
}

int update_warning_mongo(const char* dbName, tbl_warning* myWarning) {
	bson_t* doc;
	bson_t* query;
	int ret = -1;

	doc = bson_new();
	create_warn_bson_doc(myWarning, doc);
	query = BCON_NEW("alarmEventId", BCON_UTF8(myWarning->alarmeventid));
	ret = mongo_update(doc, query, TABLE_WARN);
	bson_destroy(doc);
	bson_destroy(query);
	return ret;
}

int delete_warning_by_id_mongo(const char* dbName,
		css_alarm_info* myAlarmRecordInfo) {
	bson_t* doc;
	int ret = -1;

	doc = BCON_NEW("_id", BCON_OID(bson_iter_oid(&(myAlarmRecordInfo->oid))));
	ret = mongo_delete(doc, TABLE_WARN);
	bson_destroy(doc);
	return ret;
}

static int select_warn(bson_t* query, char beginTime[24], QUEUE* alarmList) {
	mongoc_cursor_t *cursor;
	const bson_t *doc;
//bson_iter_t baz;
	int ret = -1;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;
	client = mongoc_client_pool_pop(pool);
	cllc = mongoc_client_get_collection(client, db_name, WARN_TABLE);
	cursor = mongoc_collection_find(cllc, MONGOC_QUERY_NONE, 0, 0, 0, query,
	NULL, NULL);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	while (mongoc_cursor_next(cursor, &doc)) {
		css_alarm_info* myAlarmRecordInfo = (css_alarm_info*) malloc(
				sizeof(css_alarm_info));
		QUEUE_INIT(&(myAlarmRecordInfo->queue));
//		if (bson_iter_init(&baz, doc)) {
		get_bson_value_oid(doc, "_id", &(myAlarmRecordInfo->oid));
		get_bson_value_utf8(doc, "alarmEventId",
				myAlarmRecordInfo->alarmeventid);
		QUEUE_INSERT_TAIL(alarmList, &(myAlarmRecordInfo->queue));
//		}
	}
	ret = 0;
	bson_destroy(query);
	mongoc_cursor_destroy(cursor);
	return ret;
}

int select_warning_by_begin_time_mongo(const char* dbName, char beginTime[24],
		QUEUE* alarmList) {
	bson_t *query;
	query = BCON_NEW("beginTime", "{", "$lt", BCON_UTF8(beginTime), "}");
	return select_warn(query, beginTime, alarmList);
}

/*
 this API just to test
 */
int select_warning_by_alarmid_mongo(const char* dbName, char* alarmEventId,
		tbl_warning** myWarning) {
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_t *query;
	int ret = -1;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;

	query = BCON_NEW("alarmEventId", BCON_UTF8(alarmEventId));
	client = mongoc_client_pool_pop(pool);
	cllc = mongoc_client_get_collection(client, db_name, WARN_FILE_TABLE);
	cursor = mongoc_collection_find(cllc, MONGOC_QUERY_NONE, 0, 0, 0, query,
	NULL, NULL);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	while (mongoc_cursor_next(cursor, &doc)) {
		if (get_warn_bson_all_value(doc, myWarning) == 0) {
			ret = 0;
		}
	}
	return ret;
}

int insert_warn_file_mongo(const char* dbName, tbl_warningFile* myWarningFile) {
	bson_t* doc;
	int ret = -1;

	doc = bson_new();
	BSON_APPEND_UTF8(doc, "alarmEventId", myWarningFile->alarmeventid);
	BSON_APPEND_UTF8(doc, "fileName", myWarningFile->fileName);
	ret = mongo_insert(doc, TABLE_WARNFILE);
	bson_destroy(doc);
	return ret;
}
int delete_warn_file_by_alarmid_mongo(const char* dbName, char* alarmeventid) {
	bson_t* doc;
	int ret = -1;

	doc = BCON_NEW("alarmEventId", BCON_UTF8(alarmeventid));

	ret = mongo_delete(doc, TABLE_WARNFILE);
	bson_destroy(doc);
	return ret;
}
int select_warn_file_by_alarmid_mongo(const char* dbName, char* alarmEventId,
		tbl_warningFile** myWarningFile) {
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_t *query;
	int ret = -1;
	tbl_warningFile* warnfile;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;

	query = BCON_NEW("alarmEventId", BCON_UTF8(alarmEventId));
	client = mongoc_client_pool_pop(pool);
	cllc = mongoc_client_get_collection(client, db_name, WARN_FILE_TABLE);
	cursor = mongoc_collection_find(cllc, MONGOC_QUERY_NONE, 0, 0, 0, query,
	NULL, NULL);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	while (mongoc_cursor_next(cursor, &doc)) {
		warnfile = (tbl_warningFile*) malloc(sizeof(tbl_warningFile));
		if (get_warnfile_bson_all_value(doc, myWarningFile) == 0) {
			ret = 0;
		}
	}
	return ret;
}

int64_t get_max_file_id() {
	int64_t fileID = 0;
	int ret = -1;
	bson_error_t error;
	bson_t reply;
//bson_iter_t iter;
	mongoc_client_t *client;
	mongoc_collection_t* cllc;

	bson_t *query;
	bson_t *update;
	query = BCON_NEW("name", BCON_UTF8("maxFileId"));
	update = BCON_NEW("$inc", "{", "id", BCON_INT64(1), "}");

	client = mongoc_client_pool_pop(pool);
	cllc = mongoc_client_get_collection(client, db_name, VIDEO_TABLE);
	ret = mongoc_collection_find_and_modify(cllc, query, NULL, update,
	NULL, false, true, true, &reply, &error);
	mongoc_collection_destroy(cllc);
	mongoc_client_pool_push(pool, client);

	if (!ret) {
		CL_ERROR("%d,%s\n", error.code, error.message);
	} else {
		bson_iter_t iter;
		bson_iter_init(&iter, &reply);
		if (bson_iter_find(&iter, "value")) {
			const uint8_t *buf;
			uint32_t len;
			bson_t rec;

			bson_iter_document(&iter, &len, &buf);
			bson_init_static(&rec, buf, len);

//			bson_iter_init(&iter, &rec);
			ret = get_bson_value_uint64(&rec, "id", &fileID);
//			printf("id= %d \n", ret);
			bson_destroy(&rec);
		}
	}
	return fileID;
}
