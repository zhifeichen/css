#include "css_capture.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "css_protocol_package.h"
#include "css_disk_manager.h"
#include "css_db.h"
#include "css_logger.h"

/*
 * TODO:错误处理，包括需要发送报警的错误
 */

/* session manage singleton */

#define MAX_SESSION_MANAGER 1
static css_session_manager_t gSessionMng[MAX_SESSION_MANAGER] = {0};
static uint32_t session_manager_index = 0;

static uv_mutex_t gMutex;
static struct{
	uv_interface_address_t* interfaces; /* local bind interface */
	int count;
	char(*ips)[][16];
}gInterfaces = {0};

/* static function */
static void service_thread(void* arg);
static void default_timer_cb(uv_timer_t* handle);
static void walk_cb(uv_handle_t* handle, void* arg);

static int get_local_interfaces(void);
static void release_local_interfaces(void);

static int get_local_interfaces(void)
{
	int ret;
	int i;
	int real_ip_cnt = 0;

	ret = uv_interface_addresses(&gInterfaces.interfaces, &gInterfaces.count);
	if(ret){
		return ret;
	}

	gInterfaces.ips = (char (*)[][16])calloc(gInterfaces.count, 16);

	if(!gInterfaces.ips){
		uv_free_interface_addresses(gInterfaces.interfaces, gInterfaces.count);
		gInterfaces.count = 0;
		gInterfaces.interfaces = NULL;
		return UV_ENOMEM;
	}

	for (i = 0; i < gInterfaces.count; i++){
		if(gInterfaces.interfaces[i].is_internal == 0 && gInterfaces.interfaces[i].address.address4.sin_family == AF_INET){
			uv_ip4_name(&gInterfaces.interfaces[i].address.address4, (*gInterfaces.ips)[real_ip_cnt], 16);
			CL_DEBUG("get local ip: %s\n",(*gInterfaces.ips)[real_ip_cnt]);
			real_ip_cnt++;
		}
	}

	gInterfaces.count = real_ip_cnt;
	return 0;
}

static void release_local_interfaces(void)
{
	if(gInterfaces.interfaces){
		uv_free_interface_addresses(gInterfaces.interfaces, gInterfaces.count);
		gInterfaces.interfaces = NULL;
		gInterfaces.count = 0;
	}
	if(gInterfaces.ips){
		free(gInterfaces.ips);
		gInterfaces.ips = NULL;
	}
}

static void default_timer_cb(uv_timer_t* handle)
{
	/* printf("\n"); // do nothing */
	int i ;
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		if(!QUEUE_EMPTY(&gSessionMng[i].session_queue)){
			printf("gSessionMng[%d] session queue not empty\n", i);
		}
	}
}

static void service_thread(void* arg)
{
	css_session_manager_t* mng = (css_session_manager_t*)arg;
	//uv_timer_t timer;
	CL_INFO("start session manager:%ld.\n",(long)mng->tid);
	//uv_timer_init(mng->loop, &mng->timer);
	uv_timer_start(&mng->timer, default_timer_cb, 1000, 60*1000);

	uv_run(mng->loop, UV_RUN_DEFAULT);
	
	uv_walk(mng->loop, walk_cb, 0);
	//uv_close(&mng->timer, NULL);
}

static void walk_cb(uv_handle_t* handle, void* arg)
{
	uv_close(handle, NULL);
}

static void async_close_cb(uv_handle_t* h)
{
	free(h);
}

static void capture_service_async_cb(uv_async_t* async)
{
	uv_close((uv_handle_t*)async, async_close_cb);
}

/* capture service function */
int capture_service_init(void)
{
	int i;
	CL_INFO("init capture service.\n");
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		if(gSessionMng[i].status) return 0;
		gSessionMng[i].loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
		if(gSessionMng[i].loop == NULL){
			return UV_EAI_MEMORY;
		}
		uv_loop_init(gSessionMng[i].loop);
		uv_timer_init(gSessionMng[i].loop, &gSessionMng[i].timer);
		QUEUE_INIT(&gSessionMng[i].session_queue);
		gSessionMng[i].status = 1;
	}

	uv_mutex_init(&gMutex);
	return 0;
}

int capture_service_start(void)
{
	int32_t ret = 0;
	int i;
	CL_INFO("start capture service.\n");

	ret = get_local_interfaces();
	if(ret){
		CL_ERROR("get_local_interfaces error: \n", ret);
		return ret;
	}

	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		if(gSessionMng[i].status != 1) return -1;

		ret = uv_thread_create(&gSessionMng[i].tid, service_thread, &gSessionMng[i]);
		if(ret == 0){
			gSessionMng[i].status = 2;
		}
	}
	return ret;
}

int capture_service_stop(void)
{
	int i;
	CL_INFO("stop capture service.\n");
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		uv_async_t* async;
		if(gSessionMng[i].status != 2) continue;

		//uv_walk(gSessionMng.loop, walk_cb, 0);
		//uv_stop(gSessionMng.loop);
		uv_unref((uv_handle_t*)&gSessionMng[i].timer);
		async = (uv_async_t*)malloc(sizeof(uv_async_t));
		if(async){
			uv_async_init(gSessionMng[i].loop, async, capture_service_async_cb);
			uv_async_send(async);
		}

		uv_thread_join(&gSessionMng[i].tid);
		gSessionMng[i].status = 1;
	}
	release_local_interfaces();
	return 0;
}

int capture_service_clean(void)
{
	int i;
	CL_INFO("clean capture service.\n");
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		if(gSessionMng[i].status == 0) return 0;
		if(gSessionMng[i].status == 2){
			capture_service_stop();
		}
		if(gSessionMng[i].loop) {
			uv_loop_close(gSessionMng[i].loop);
			free(gSessionMng[i].loop);
			gSessionMng[i].loop = NULL;
			gSessionMng[i].status = 0;
		}
	}

	uv_mutex_destroy(&gMutex);
	return 0;
}

/* static function */
static int capture_session_init(css_capture_session_t* session);
static int capture_session_add(css_capture_session_t* session);
static int capture_session_remove(css_capture_session_t* session);
static void capture_session_close(css_capture_session_t* session);

static int capture_session_set_info(css_capture_session_t* session, 
	css_record_info_t* recordInfo, css_sms_info_t* smsInfo); 
static int capture_session_start_record(css_capture_session_t* session);
static int capture_session_stop_record(css_capture_session_t* session);

static int capture_session_stop_receiv(css_capture_session_t* session);
static int capture_session_disconnect(css_capture_session_t* session);

static int capture_session_insert_db(css_capture_session_t* session);
static int capture_session_update_db(css_capture_session_t* session);

static int capture_session_open_file(css_capture_session_t* session);
static int capture_session_close_file(css_capture_session_t* session);

static void capture_session_cifiiffile_write_cb(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt);

static int capture_session_write_file(css_capture_session_t* session);


static void capture_session_package_add(css_capture_session_t* session, css_capture_package_t* cp);
static void capture_session_package_remove_header(css_capture_session_t* session, css_capture_package_t** cp, int need_free);

static int capture_session_add_openfile_package(css_capture_session_t* session);
static int capture_session_add_closefile_package(css_capture_session_t* session);
static int capture_session_add_closesession_package(css_capture_session_t* session);

static int capture_session_clone_package(css_capture_package_t* dst, css_capture_package_t* src);
static int capture_session_add_sysheader_package(css_capture_session_t* session);

static void capture_session_do_package_if_necessary(css_capture_session_t* session);

/* stream callback */
static void on_stream_connect_cb(css_stream_t* stream, int status);
static void on_stream_close_cb(css_stream_t* stream);
static void on_stream_write_cb(css_write_req_t* req, int status);
static void on_stream_data_cb(css_stream_t* stream, char* package, ssize_t status);

static void do_response(css_stream_t* stream, char* package, int status);
static void do_preview_frame(css_stream_t* stream, char* package, ssize_t status);


static int capture_session_init(css_capture_session_t* session)
{
	QUEUE_INIT(&session->queue);
	session->mng = &gSessionMng[session_manager_index % MAX_SESSION_MANAGER];
	session_manager_index++;
	session->package_header = session->package_tail = NULL;
	session->package_cnt = 0;
	session->id = session_manager_index;
	session->db_result = 0;
	session->recordFile_offset = 0;
	session->cifFile_offset = 0;
	session->fileIncisionTime = DEFAULT_INCISIONTIME;
	session->recordFile.data = session->cifFile.data = session->iifFile.data = session;
	session->recordFile.loop = session->cifFile.loop = session->iifFile.loop = session->mng->loop;
	session->cifBuf.buf.base = session->iifBuf.buf.base = NULL;
	session->cifBuf.buf.len = session->iifBuf.buf.len = 0;
	session->cifBuf.max_size = session->iifBuf.max_size = 0;
	session->status = SESSION_STATUS_TYPE_UNINIT;
	memset(&session->package_sysheader, 0, sizeof(css_capture_package_t));
	return 0;
}

static int capture_session_add(css_capture_session_t* session)
{
	CL_DEBUG("add session:%d.\n",session->id);
	QUEUE_INSERT_TAIL(&session->mng->session_queue, &session->queue);
	return 0;
}

static int capture_session_remove(css_capture_session_t* session)
{
	CL_DEBUG("remove session:%d.\n",session->id);
	QUEUE_REMOVE(&session->queue);
	return 0;
}

static int capture_session_set_info(css_capture_session_t* session, 
	css_record_info_t* recordInfo, css_sms_info_t* smsInfo)
{
	if(!session || !recordInfo || !smsInfo)
		return -1;

	strcpy(session->recordInfo.timestamp.eventId, recordInfo->timestamp.eventId);
	session->recordInfo.timestamp.timestamp = recordInfo->timestamp.timestamp;
	session->recordInfo.timestamp.type = recordInfo->timestamp.type;
	session->recordInfo.dvrEquipId = recordInfo->dvrEquipId;
	session->recordInfo.channelNo = recordInfo->channelNo;
	session->recordInfo.flowType = recordInfo->flowType;

	session->smsInfo.smsEquipId = smsInfo->smsEquipId;
	memcpy(&session->smsInfo.smsAddr, &smsInfo->smsAddr, sizeof(struct sockaddr_in));
	return 0;
}

static int capture_session_clone_package(css_capture_package_t* dst, css_capture_package_t* src)
{
	if(src->buf.len > 0){
		dst->buf = uv_buf_init((char*)malloc(src->buf.len), src->buf.len);
		if(dst->buf.base == NULL){
			return UV_EAI_MEMORY;
		}
		memcpy(dst->buf.base, src->buf.base, dst->buf.len);
	}
	/*
	if(src->frame.frame.len > 0){
		dst->frame.frame = uv_buf_init((char*)malloc(src->frame.frame.len), src->frame.frame.len);
		if(dst->frame.frame.base == NULL){
			free(dst->frame.frame.base);
			return UV_EAI_MEMORY;
		}
		memcpy(dst->frame.frame.base, src->frame.frame.base, dst->frame.frame.len);
	}
	*/
	dst->frame.frame.base = dst->buf.base + (src->frame.frame.base - src->buf.base);
	dst->frame.frame.len = src->frame.frame.len;

	dst->type = src->type;
	dst->timestamp = src->timestamp;
	dst->frame.frameType = src->frame.frameType;
	dst->status = 0;
	dst->next = NULL;
	return 0;
}

static int capture_session_add_sysheader_package(css_capture_session_t* session)
{
	int ret;
	css_capture_package_t* cp;
	if(session->package_sysheader.frame.frame.base == NULL){
		return -1; /* error: no sys header had received */
	}
	cp = (css_capture_package_t*)malloc(sizeof(css_capture_package_t));
	if(!cp){
		return UV_EAI_MEMORY;
	}

	ret = capture_session_clone_package(cp, &session->package_sysheader);
	if(ret < 0){
		free(cp);
		return ret;
	}
	capture_session_package_add(session, cp);
	return 0;
}

static void capture_session_iifFile_open_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	/*css_capture_package_t* cp;*/

	if(writer->result < 0){
		CL_ERROR("open iiffile:%s error:%s.\n",writer->path,uv_strerror((int)writer->result));
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
		return;
	}

	session->status |= SESSION_STATUS_TYPE_FILE_OPENED;
	/*capture_session_package_remove_header(session, &cp);
	free(cp);*/
	capture_session_insert_db(session);/* after insert then remove the open package */
	//capture_session_do_package_if_necessary(session);
}

static void capture_session_cifFile_open_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	int ret;

	if(writer->result < 0){
		CL_ERROR("open ciffile:%s error:%s.\n",writer->path,uv_strerror((int)writer->result));
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
		return;
	}

	ret = css_file_writer_open(&session->iifFile, capture_session_iifFile_open_cb);
	if(ret < 0){
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
	}
}

static void capture_session_recordFile_open_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	int ret;

	if(writer->result < 0){
		CL_ERROR("open recordfile:%s error:%s.\n",writer->path,uv_strerror((int)writer->result));
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
		return;
	}

	ret = css_file_writer_open(&session->cifFile, capture_session_cifFile_open_cb);
	if(ret < 0){
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
	}
}

static int capture_session_open_file(css_capture_session_t* session)
{
	int ret = 0;
	char path[MAX_PATH];/* TODO: get the path*/
	char filename[MAX_PATH];
	int volume_len;

	ret = timeval_to_filename(&session->package_header->timestamp, filename, MAX_PATH);
	if(ret < 0){
		CL_ERROR("timeval_to_filename return error: %d\n", ret);
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
		return ret;
	}
	if(session->recordInfo.timestamp.type == RECORD_TYPE_SCHEDULE){
		ret = css_get_disk_path(session->recordInfo.dvrEquipId, session->recordInfo.channelNo, path, MAX_PATH, &volume_len);
	}
	else{
		ret = css_get_disk_warn_path(session->recordInfo.dvrEquipId, session->recordInfo.channelNo, path, MAX_PATH, &volume_len);
	}
	if(ret < 0){
		CL_ERROR("css_get_disk_path return error: %d\n", ret);
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
		return ret;
	}
	sprintf(session->recordFile.path, "%s%s.mp4", path, filename);
	sprintf(session->iifFile.path, "%s%s.iif", path, filename);
	sprintf(session->cifFile.path, "%s%s.cif", path, filename);

	session->recordFile.volume_length = session->iifFile.volume_length = session->cifFile.volume_length = volume_len;

	session->fileStartTime = session->package_header->timestamp;
	session->fileEndTime = session->package_header->timestamp;

	ret = css_file_writer_open(&session->recordFile, capture_session_recordFile_open_cb);
	if(ret < 0){
		CL_ERROR("css_file_writer_open return error: %d\n", ret);
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_stop_record(session);
	}
	return ret;
}

/* useless, close file immediate */
static void capture_session_iifFile_close_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	//capture_session_package_remove_header(session, NULL, 1);
	capture_session_update_db(session);
	//capture_session_do_package_if_necessary(session);
}

/* useless, close file immediate */
static void capture_session_cifFile_close_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	css_file_writer_close(&session->iifFile, capture_session_iifFile_close_cb);
}

/* useless, close file immediate */
static void capture_session_recordFile_close_cb(css_file_writer_t* writer)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	css_file_writer_close(&session->cifFile, capture_session_cifFile_close_cb);
}

static int capture_session_close_file(css_capture_session_t* session)
{
	int ret = 0;
	struct timeval t0,t1;
	gettimeofday(&t0, NULL);
	if(session->status & SESSION_STATUS_TYPE_FILE_OPENED){
		session->status &= ~SESSION_STATUS_TYPE_FILE_OPENED;
		css_file_writer_write(&session->cifFile, &session->cifBuf.buf, 1, -1, NULL/*capture_session_cifiiffile_write_cb*/);
		free(session->cifBuf.buf.base);
		session->cifBuf.buf.base = NULL;
		session->cifBuf.buf.len = 0;
		css_file_writer_write(&session->iifFile, &session->iifBuf.buf, 1, -1, NULL/*capture_session_cifiiffile_write_cb*/);
		free(session->iifBuf.buf.base);
		session->iifBuf.buf.base = NULL;
		session->iifBuf.buf.len = 0;

		session->recordFile_offset = 0;
		session->cifFile_offset = 0;
		session->fileEndTime = session->package_header->timestamp;
		/*capture_session_update_db(session);*/

		ret = css_file_writer_close(&session->recordFile, NULL/*capture_session_recordFile_close_cb*/);
		ret = css_file_writer_close(&session->cifFile, NULL);
		ret = css_file_writer_close(&session->iifFile, NULL);
	}
	
	/*capture_session_package_remove_header(session, &cp);
	free(cp);*/
	capture_session_update_db(session);/* after update db, then remove close file package */
	gettimeofday(&t1, NULL);
	CL_DEBUG("close file cost %ld\n", difftimeval(&t1, &t0));
	return ret;
}

static void capture_session_cifiiffile_write_cb(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;

	if(writer->result < 0){
		capture_session_stop_record(session);
	}
	free(buf[0].base);
}

static int capture_session_write_iif(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp = session->package_header;
	sv_time_t sv;

	if(session->iifBuf.buf.base == NULL){
		session->iifBuf.buf = uv_buf_init((char*)malloc(IIF_BUF_LEN), IIF_BUF_LEN);
		if(session->iifBuf.buf.base == NULL){
			return UV_EAI_MEMORY;
		}
		session->iifBuf.buf.len = 0;
		session->iifBuf.max_size = IIF_BUF_LEN;
	}

	if(session->iifBuf.max_size - session->iifBuf.buf.len < 12){
		ret = css_file_writer_write(&session->iifFile, &session->iifBuf.buf, 1, -1, capture_session_cifiiffile_write_cb);
		if(ret < 0){
			free(session->iifBuf.buf.base);
			session->iifBuf.buf.base = NULL;
			session->iifBuf.buf.len = 0;
			session->iifBuf.max_size = 0;
			return ret;
		}

		session->iifBuf.buf = uv_buf_init((char*)malloc(IIF_BUF_LEN), IIF_BUF_LEN);
		if(session->iifBuf.buf.base == NULL){
			return UV_EAI_MEMORY;
		}
		session->iifBuf.buf.len = 0;
		session->iifBuf.max_size = IIF_BUF_LEN;
	}

	timeval_to_svtime(&sv, &cp->timestamp);
	css_int32_encode(session->iifBuf.buf.base+session->iifBuf.buf.len, session->cifFile_offset);
	session->iifBuf.buf.len += 4;
	css_uint16_encode(session->iifBuf.buf.base+session->iifBuf.buf.len, sv.year);
	session->iifBuf.buf.len += 2;
	css_uint8_encode(session->iifBuf.buf.base+session->iifBuf.buf.len, sv.month);
	session->iifBuf.buf.len += 1;
	css_uint8_encode(session->iifBuf.buf.base+session->iifBuf.buf.len, sv.day);
	session->iifBuf.buf.len += 1;
	css_int32_encode(session->iifBuf.buf.base+session->iifBuf.buf.len, sv.milliSeconds);
	session->iifBuf.buf.len += 4;

	/*session->cifFile_offset += 12;*/
	return ret;
}

static int capture_session_write_cif(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp = session->package_header;
	sv_time_t sv;

	if(session->cifBuf.buf.base == NULL){
		session->cifBuf.buf = uv_buf_init((char*)malloc(CIF_BUF_LEN), CIF_BUF_LEN);
		if(session->cifBuf.buf.base == NULL){
			return UV_EAI_MEMORY;
		}
		session->cifBuf.buf.len = 0;
		session->cifBuf.max_size = CIF_BUF_LEN;
	}

	if(session->cifBuf.max_size - session->cifBuf.buf.len < 20){
		ret = css_file_writer_write(&session->cifFile, &session->cifBuf.buf, 1, -1, capture_session_cifiiffile_write_cb);
		if(ret < 0){
			free(session->cifBuf.buf.base);
			session->cifBuf.buf.base = NULL;
			session->cifBuf.buf.len = 0;
			session->cifBuf.max_size = 0;
			return ret;
		}

		session->cifBuf.buf = uv_buf_init((char*)malloc(CIF_BUF_LEN), CIF_BUF_LEN);
		if(session->cifBuf.buf.base == NULL){
			return UV_EAI_MEMORY;
		}
		session->cifBuf.buf.len = 0;
		session->cifBuf.max_size = CIF_BUF_LEN;
	}

	timeval_to_svtime(&sv, &cp->timestamp);
	css_int32_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, session->recordFile_offset);
	session->cifBuf.buf.len += 4;
	css_int32_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, cp->frame.frame.len);
	session->cifBuf.buf.len += 4;
	css_int32_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, cp->frame.frameType);
	session->cifBuf.buf.len += 4;
	css_uint16_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, sv.year);
	session->cifBuf.buf.len += 2;
	css_uint8_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, sv.month);
	session->cifBuf.buf.len += 1;
	css_uint8_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, sv.day);
	session->cifBuf.buf.len += 1;
	css_int32_encode(session->cifBuf.buf.base+session->cifBuf.buf.len, sv.milliSeconds);
	session->cifBuf.buf.len += 4;

	session->cifFile_offset += 20;
	return ret;
}

static void capture_session_recordfile_write_cb(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt)
{
	css_capture_session_t* session = (css_capture_session_t*)writer->data;
	int ret = 0;
	css_capture_package_t* cp;
	int i;

	if(!session){
		return;
	}
	cp = session->package_header;
	//1. write record file fisrt.
	if(writer->result < 0){
		CL_ERROR("write file error: %d\n", writer->result);
		for(i = 0; i < buf_cnt; i++){
			capture_session_package_remove_header(session, &cp, 0);
			free(cp->buf.base);
			free(cp);
		}
		capture_session_stop_record(session);
		return;
	}
	//2. write cif and iif file.
	for(i = 0; i < buf_cnt; i++){
		if(cp->frame.frameType == FRAME_TYPE_I_Frame){
			ret = capture_session_write_iif(session);
		}
		if(ret >= 0){
			ret = capture_session_write_cif(session);
		}
		if(ret < 0){
			CL_ERROR("write cif or iif file error:%s.\n",uv_strerror(ret));
			for(; i < buf_cnt; i++){
				capture_session_package_remove_header(session, &cp, 0);
				free(cp->buf.base);
				free(cp);
			}
			capture_session_stop_record(session);
			break;
		}

		session->recordFile_offset += buf[i].len;

		capture_session_package_remove_header(session, &cp, 0);
		free(cp->buf.base);
		free(cp);
	}
	capture_session_do_package_if_necessary(session);
	return ;
}

static int capture_session_write_file(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp = session->package_header;

	if((session->status & SESSION_STATUS_TYPE_FILE_OPENED) == 0){
		CL_ERROR("session->status:%d error, file is not opened\n",session->status);
		do{
			capture_session_package_remove_header(session, &cp, 0);
			free(cp->buf.base);
			free(cp);
		}while(session->package_header && session->package_header->type == PACKAGE_TYPE_FRAME);
		capture_session_stop_record(session);
		return -1;/* wrong status */
	}

	ret = css_file_writer_write(&session->recordFile, 
								&session->package_header->frame.frame, 1, session->recordFile_offset, 
								capture_session_recordfile_write_cb);
	if(ret < 0){
		CL_ERROR("write record file error:%d(%s)\n", ret,uv_strerror(ret));
		do{
			capture_session_package_remove_header(session, &cp, 0);
			free(cp->buf.base);
			free(cp);
		}while(session->package_header && session->package_header->type == PACKAGE_TYPE_FRAME);
		capture_session_stop_record(session);
	}

	return ret;
}

static void capture_session_do_package_if_necessary(css_capture_session_t* session)
{
	css_capture_package_t* cp = session->package_header;
	if(!cp || cp->status > 0){
		return ;
	}
	cp->status = 1;
	switch(cp->type){
	case PACKAGE_TYPE_OPENFILE:
		{
			capture_session_open_file(session);
			break;
		}
	case PACKAGE_TYPE_FRAME:
		{
			capture_session_write_file(session);
			break;
		}
	case PACKAGE_TYPE_CLOSEFILE:
		{
			capture_session_close_file(session);
			break;
		}
	case PACKAGE_TYPE_CLOSESESSION:
		{
			capture_session_close(session);
			break;
		}
	default:
		{
			/* should not happen */
			CL_WARN("unknow packet type:%d\n",cp->type);
			capture_session_package_remove_header(session, NULL, 1);
			capture_session_do_package_if_necessary(session);
			break;
		}
	}
}



static void on_stream_close_cb(css_stream_t* stream)
{
	int ret;
	css_capture_session_t* session = (css_capture_session_t*)stream->data;
	CL_DEBUG("session:%d stream closed\n",session->id);
	if(session->status & SESSION_STATUS_TYPE_FILE_OPENED){
		CL_DEBUG("session:%d stream closed, add close file package\n",session->id);
		ret = capture_session_add_closefile_package(session);
	}
	ret = capture_session_add_closesession_package(session);
	if(ret < 0){
		/* close && free immediate*/
		CL_ERROR("capture_session_add_closesession_package return error: %d\n", ret);
		capture_session_close(session);
	}
}

static void capture_session_package_add(css_capture_session_t* session, css_capture_package_t* cp)
{
	if(session->package_cnt > MAX_CAPTURE_FRAME_LIST_LEN){
		CL_ERROR("session:%d packet cnt:%d>max_frames_len:%d,ignore packet type:%d.\n",
				session->id,session->package_cnt,MAX_CAPTURE_FRAME_LIST_LEN,cp->type);
		return ;
	}
	cp->next = NULL;
	if(session->package_tail){
		session->package_tail->next = cp;
		session->package_tail = cp;
	}
	else{
		session->package_header = session->package_tail = cp;
	}
	session->package_cnt += 1;
}

static void capture_session_package_remove_header(css_capture_session_t* session, css_capture_package_t** cp, int need_free)
{
	css_capture_package_t* cp0 = session->package_header;
	if(cp){
		*cp = cp0;
	}
	if(cp0 == NULL){
		return ;
	}
	if(session->package_tail == session->package_header) session->package_tail = NULL;
	session->package_header = session->package_header->next;
	session->package_cnt -= 1;
	if(need_free){
		free(cp0); /* in this case , cp == NULL OR *cp should not use after the function return */
	}
}


static int capture_session_add_openfile_package(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp;

	cp = (css_capture_package_t*)malloc(sizeof(css_capture_package_t));
	if(!cp){
		return UV_EAI_MEMORY;
	}
	memset(cp, 0 ,sizeof(css_capture_package_t));
	cp->type = PACKAGE_TYPE_OPENFILE;
	cp->status = 0;
	gettimeofday(&cp->timestamp, NULL);
	capture_session_package_add(session, cp);
	capture_session_do_package_if_necessary(session);
	return ret;
}

static int capture_session_add_closefile_package(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp;

	cp = (css_capture_package_t*)malloc(sizeof(css_capture_package_t));
	if(!cp){
		return UV_EAI_MEMORY;
	}
	memset(cp, 0 ,sizeof(css_capture_package_t));
	cp->type = PACKAGE_TYPE_CLOSEFILE;
	cp->status = 0;
	gettimeofday(&cp->timestamp, NULL);
	capture_session_package_add(session, cp);
	capture_session_do_package_if_necessary(session);
	return ret;
}

static int capture_session_add_closesession_package(css_capture_session_t* session)
{
	int ret = 0;
	css_capture_package_t* cp;

	cp = (css_capture_package_t*)malloc(sizeof(css_capture_package_t));
	if(!cp){
		return UV_EAI_MEMORY;
	}
	memset(cp, 0 ,sizeof(css_capture_package_t));
	cp->type = PACKAGE_TYPE_CLOSESESSION;
	cp->status = 0;
	gettimeofday(&cp->timestamp, NULL);
	capture_session_package_add(session, cp);
	CL_DEBUG("added close session package and header package type: %d\n", session->package_header->type);
	capture_session_do_package_if_necessary(session);
	return ret;
}

static void do_preview_response(css_stream_t* stream, char* package, ssize_t status)
{
	css_capture_session_t* session = (css_capture_session_t*)stream->data;
	int ret = 0;
	JNetCmd_PreviewConnect_Resp resp;

	ret = css_proto_package_decode(package, status, (JNetCmd_Header*)&resp);
	free(package); /* can free anyway */
	if(ret <= 0 || resp.State ){
		CL_ERROR("decode preview response packet error:%d.\n",ret);
		capture_session_stop_record(session);
		return ;
	}

	ret = capture_session_add_openfile_package(session);
	if(ret){
		CL_ERROR("add openfile packet error:%d(%s).\n",ret,uv_strerror(ret));
		capture_session_stop_record(session);
		return ;
	}
}

static void do_preview_frame(css_stream_t* stream, char* package, ssize_t status)
{
	css_capture_session_t* session = (css_capture_session_t*)stream->data;
	int ret = 0;
	JNetCmd_Preview_Frame frame;
	css_capture_package_t* cp = NULL;

	ret = css_proto_package_decode(package, status, (JNetCmd_Header*)&frame);
	if(ret <= 0){
		CL_ERROR("decode preview frame error:%d.\n",ret);
		free(package);
		capture_session_stop_record(session);
		return ;
	}
	/* TODO: COMPLETE */
	cp = (css_capture_package_t*)malloc(sizeof(css_capture_package_t));
	if(!cp){
		free(package);
		capture_session_stop_record(session);
		return ;
	}
	memset(cp, 0 ,sizeof(css_capture_package_t));

	cp->type = PACKAGE_TYPE_FRAME;
	cp->status = 0;
	cp->buf.base = package;
	cp->buf.len = status;
	cp->frame.frameType = frame.FrameType;
	cp->frame.frame = frame.FrameData;
	gettimeofday(&cp->timestamp, NULL);

	/* sys header */
	if(cp->frame.frameType == FRAME_TYPE_SysHeader){
		capture_session_clone_package(&session->package_sysheader, cp);
	}

	/* incision file */
	if(cp->frame.frameType == FRAME_TYPE_I_Frame 
		&& difftimeval(&cp->timestamp, &session->fileStartTime) >= session->fileIncisionTime){
			capture_session_add_closefile_package(session);
			capture_session_add_openfile_package(session);
			capture_session_add_sysheader_package(session);
	}
	capture_session_package_add(session, cp);
	capture_session_do_package_if_necessary(session);
}

static void on_stream_data_cb(css_stream_t* stream, char* package, ssize_t status)
{
	JNetCmd_Header header;
	struct timeval tv;
	css_capture_session_t* session = (css_capture_session_t*)stream->data;

	if(status <= 0){
		CL_ERROR("session:%d receive data error!, error: %d(%s).\n",session->id, status,uv_strerror((int)status));
		if(package) free(package);
		capture_session_stop_record(session);
		return ;
	}

	if(!package){ /* should not happened */
		CL_ERROR("status: %d, but package is NULL\n", status);
		return ;
	}
	gettimeofday(&tv, NULL);
	if(difftimeval(&tv, &session->sessionStartTime) >= (session->recordInfo.timestamp.timestamp/1000 + APPEND_TIME)){
		CL_DEBUG("session:%d record timeout.\n",session->id);
		if(package) free(package);
		capture_session_stop_record(session);
		return ;
	}

	css_proto_package_header_decode(package, status, &header);

	switch(header.I_CmdId){
	case sv_cmd_preview_connect_resp:
		do_preview_response(stream, package, status);
		break;
	case sv_cmd_preview_frame:
		do_preview_frame(stream, package, status);
		break;
	default:
		free(package);
		break;
	}
}

static void on_stream_write_cb(css_write_req_t* req, int status)
{
	css_stream_t* stream = req->stream;
	css_capture_session_t* session = (css_capture_session_t*)stream->data;
	int ret = 0;

	free(req->buf.base);
	free(req);

	if(status < 0){
		CL_ERROR("write to stream error:%d(%s).\n",status,uv_strerror(status));
		capture_session_stop_record(session);
		return ;
	}

	ret = css_stream_read_start(stream, on_stream_data_cb);
	if(ret){
		CL_ERROR("start read from stream error:%d(%s).\n",ret,uv_strerror(ret));
		capture_session_stop_record(session);
		return ;
	}

	session->status |= SESSION_STATUS_TYPE_READING;
}

static void on_stream_connect_cb(css_stream_t* stream, int status)
{
	JNetCmd_PreviewConnect_Ex msg;
	uv_buf_t buf;
	ssize_t len;
	css_write_req_t* req;
	css_capture_session_t* session = (css_capture_session_t*)stream->data;
	int ret;

	session->status |= SESSION_STATUS_TYPE_CONNECTED;
	gettimeofday(&session->sessionStartTime, NULL);

	if(status){
		CL_ERROR("stream connnect error:%d(%s).\n",status,uv_strerror(status));
		capture_session_stop_record(session);
		return ;
	}

	JNetCmd_PreviewConnect_Ex_init(&msg);
	msg.EquipId = session->recordInfo.dvrEquipId;
	msg.ChannelNo = session->recordInfo.channelNo;
	msg.CodingFlowType = session->recordInfo.flowType;

	len = css_proto_package_calculate_buf_len((JNetCmd_Header *)&msg);
	buf.base = (char*)malloc(len);
	if(!buf.base){
		capture_session_stop_record(session);
		return ;
	}
	buf.len = len;

	css_proto_package_encode(buf.base, len, (JNetCmd_Header *)&msg);
	req = (css_write_req_t*)malloc(sizeof(css_write_req_t));
	if(!req){
		CL_ERROR("error:out of memory.\n");
		free(buf.base);
		capture_session_stop_record(session);
		return ;
	}

	ret = css_stream_write(stream, req, buf, on_stream_write_cb);
	if(ret < 0){
		CL_ERROR("write to stream error:%d(%s).\n",ret,uv_strerror(ret));
		free(buf.base);
		free(req);
		capture_session_stop_record(session);
	}
}

static int capture_session_stop_receiv(css_capture_session_t* session)
{
	int ret = 0;
	if(session->status & SESSION_STATUS_TYPE_READING){
		ret = css_stream_read_stop(&session->client);
		session->status &= ~SESSION_STATUS_TYPE_READING;
	}
	return ret;
}


static int capture_session_disconnect(css_capture_session_t* session)
{
	int ret = -1;
	if(session->status & SESSION_STATUS_TYPE_CONNECTED){
		session->status &= ~SESSION_STATUS_TYPE_CONNECTED;
		ret = css_stream_close(&session->client, on_stream_close_cb);
		if(ret < 0){
			CL_ERROR("css_stream_close return error: %d\n", ret);
		}
	}
	return ret;
}

static int capture_session_start_record(css_capture_session_t* session)
{
	int ret;

	ret = css_stream_init(session->mng->loop, &session->client);
	if(ret < 0){
		return ret;
	}
	session->status |= SESSION_STATUS_TYPE_INIT;
	session->client.data = (void*)session;

	if(gInterfaces.count){
		int mod = session->id % gInterfaces.count;
		ret = css_stream_bind(&session->client, (*gInterfaces.ips)[mod], 0);
		if(ret < 0){
			CL_ERROR("client bind %s return %d\n", (*gInterfaces.ips)[mod], ret);
			return ret;
		}
	}
	ret = css_stream_connect(&session->client, 
							(const struct sockaddr*)&session->smsInfo.smsAddr, 
							 on_stream_connect_cb);
	return ret;
}

static int capture_session_stop_record(css_capture_session_t* session)
{
	int ret = -1;
	QUEUE *q;
	css_capture_session_t* s;

	QUEUE_FOREACH(q, &session->mng->session_queue){
		s = QUEUE_DATA(q, css_capture_session_t, queue);
		if(s == session){/* find the session */
			ret = 0;
		}
	}

	if(ret < 0){
		CL_ERROR("capture_session_stop_record can't find the session\n");
		return ret;
	}

	session->status |= SESSION_STATUS_TYPE_CLOSING;
	capture_session_stop_receiv(session);
	ret = capture_session_disconnect(session);
	if(ret < 0){
		CL_ERROR("capture_session_disconnect return error: %d\n", ret);
		capture_session_close(session);
	}
	return ret;
}

static void capture_session_close(css_capture_session_t* session)
{
	css_capture_package_t* cp;
	capture_session_package_remove_header(session, &cp, 0);
	while(cp){
		free(cp->buf.base);
		free(cp);
		capture_session_package_remove_header(session, &cp, 0);
	}

	if(session->status & SESSION_STATUS_TYPE_FILE_OPENED){
		session->status &= ~SESSION_STATUS_TYPE_FILE_OPENED;
		css_file_writer_write(&session->cifFile, &session->cifBuf.buf, 1, -1, NULL/*capture_session_cifiiffile_write_cb*/);
		free(session->cifBuf.buf.base);
		session->cifBuf.buf.base = NULL;
		session->cifBuf.buf.len = 0;
		css_file_writer_write(&session->iifFile, &session->iifBuf.buf, 1, -1, NULL/*capture_session_cifiiffile_write_cb*/);
		free(session->iifBuf.buf.base);
		session->iifBuf.buf.base = NULL;
		session->iifBuf.buf.len = 0;

		session->recordFile_offset = 0;
		session->cifFile_offset = 0;
		/* don't care */
		/*session->fileEndTime = session->package_header->timestamp;*/
		/*capture_session_update_db(session);*/

		css_file_writer_close(&session->recordFile, NULL/*capture_session_recordFile_close_cb*/);
		css_file_writer_close(&session->cifFile, NULL);
		css_file_writer_close(&session->iifFile, NULL);
	}

	free(session->package_sysheader.buf.base);

	capture_session_remove(session);
	CL_INFO("stop record dvrid:%lld,channelno:%u, type:%u.\n",
			session->recordInfo.dvrEquipId,session->recordInfo.channelNo,
			session->recordInfo.timestamp.type);
	free(session);

	CL_DEBUG("session closed\n");
}

static int capture_session_check_record_info(css_record_info_t* record_info)
{
	int ret = 0;
	QUEUE *q;
	css_capture_session_t* session;
	struct timeval tv;
	int i;
	int64_t dif;
	int64_t rest;
	gettimeofday(&tv, NULL);
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		QUEUE_FOREACH(q, &gSessionMng[i].session_queue){
			session = QUEUE_DATA(q, css_capture_session_t, queue);
			if(strcmp(session->recordInfo.timestamp.eventId, record_info->timestamp.eventId) == 0){
				/* if exist, recalculate timestamp */
				//if(session->recordInfo.timestamp.type == 0){
				if(session->status & SESSION_STATUS_TYPE_FILE_OPENED){
					dif = (int64_t)session->recordInfo.timestamp.timestamp - difftimeval(&tv, &session->sessionStartTime)*1000;
					rest = record_info->timestamp.timestamp - dif;
					if(0 < rest){
						session->recordInfo.timestamp.timestamp += rest; 
					}
				}
				else if(session->status & SESSION_STATUS_TYPE_CLOSING){
					return 0;
				}
				else{
					session->recordInfo.timestamp.timestamp += record_info->timestamp.timestamp;
				}
				//}
				//else if(session->recordInfo.timestamp.type == 1){
				//}
				ret = -2; /* no error, but ok */

				return ret;
			}
		}
	}
	return ret;
}

/* db operation function */
typedef struct css_capture_work_data_s
{
	css_capture_session_t* session;
	tbl_videoFile* video_file;
}css_capture_work_data_t;

static int capture_session_job_insert_db(css_capture_session_t* session);
static int capture_session_job_update_db(css_capture_session_t* session);

void insert_db_work(uv_work_t* req);
void after_insert_db_work(uv_work_t* req, int status);

void update_db_work(uv_work_t* req);
void after_update_db_work(uv_work_t* req, int status);

static int capture_session_insert_db(css_capture_session_t* session)
{
	if(session->recordInfo.timestamp.type == 1){ /* job record */
		return capture_session_job_insert_db(session);
	}/* TODO: WARNING RECORD */
	else{
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_do_package_if_necessary(session);
		return 0;
	}
}

static int capture_session_update_db(css_capture_session_t* session)
{
	if(session->recordInfo.timestamp.type == 1){ /* JOB RECORD */
		return capture_session_job_update_db(session);
	}/* TODO: WARNING RECORD */
	else{
		capture_session_package_remove_header(session, NULL, 1);
		capture_session_do_package_if_necessary(session);
		return 0;
	}
}

static int capture_session_job_insert_db(css_capture_session_t* session)
{
	int ret = UV_EAI_MEMORY;
	tbl_videoFile* vf = NULL;
	uv_work_t* req = NULL;
	css_capture_work_data_t* data = NULL;

	char* filename = NULL;
	char* volume_name = NULL;

	if((filename = (char*)malloc(strlen(session->recordFile.path)+1)) 
		&& (volume_name = (char*)malloc(session->recordFile.volume_length + 1))
		&& (data = (css_capture_work_data_t*)malloc(sizeof(css_capture_work_data_t)))
		&& (req = (uv_work_t*)malloc(sizeof(uv_work_t)))
		&& (vf = (tbl_videoFile*)malloc(sizeof(tbl_videoFile)))){
			vf->dvrEquipId		= session->recordInfo.dvrEquipId;	//dvr的设备Id
			vf->channelNo		= session->recordInfo.channelNo;		//通道号
			strcpy(filename, session->recordFile.path);
			vf->fileName		= filename;		//录像文件名
			memset(volume_name, 0, session->recordFile.volume_length+1); 
			strncpy(volume_name, session->recordFile.path, session->recordFile.volume_length);
			vf->volumeName		= volume_name;		//录像文件所在的卷名,"E:"
			vf->srcPathName		= filename;	//录像文件的原路径,给atm机使用
			timeval_to_svtime_string(&session->fileStartTime, vf->beginTime, 24);		//录像开始时间,"2014-04-03 07:55:07.750"
			timeval_to_svtime_string(&session->fileStartTime, vf->endTime, 24);			//录像结束时间,"2014-04-23 17:55:07.750"
			vf->playLength = 0;		//录像文件的播放长度(毫秒)
			vf->available = UNAVAILABLE_VIDEO_FILE;
			vf->fileType = NORMAL_VIDEO;

			data->session = session;
			data->video_file = vf;
			req->data = data;
			ret = uv_queue_work(session->mng->loop, req, insert_db_work, after_insert_db_work);
	}

	if(ret < 0){
		FREE(filename);
		FREE(volume_name);
		FREE(data);
		FREE(req);
		FREE(vf);
	}
	return ret;
}

static int capture_session_job_update_db(css_capture_session_t* session)
{
	int ret = UV_EAI_MEMORY;
	tbl_videoFile* vf = NULL;
	uv_work_t* req = NULL;
	css_capture_work_data_t* data = NULL;

	char* filename = NULL;
	char* volume_name = NULL;

	if((filename = (char*)malloc(strlen(session->recordFile.path)+1)) 
		&& (volume_name = (char*)malloc(session->recordFile.volume_length + 1))
		&& (data = (css_capture_work_data_t*)malloc(sizeof(css_capture_work_data_t)))
		&& (req = (uv_work_t*)malloc(sizeof(uv_work_t)))
		&& (vf = (tbl_videoFile*)malloc(sizeof(tbl_videoFile)))){
			vf->dvrEquipId		= session->recordInfo.dvrEquipId;	//dvr的设备Id
			vf->channelNo		= session->recordInfo.channelNo;		//通道号
			strcpy(filename, session->recordFile.path);
			vf->fileName		= filename;		//录像文件名
			memset(volume_name, 0, session->recordFile.volume_length+1); 
			strncpy(volume_name, session->recordFile.path, session->recordFile.volume_length);
			vf->volumeName		= volume_name;		//录像文件所在的卷名,"E:"
			vf->srcPathName		= filename;	//录像文件的原路径,给atm机使用
			timeval_to_svtime_string(&session->fileStartTime, vf->beginTime, 24);		//录像开始时间,"2014-04-03 07:55:07.750"
			timeval_to_svtime_string(&session->fileEndTime, vf->endTime, 24);		//录像结束时间,"2014-04-23 17:55:07.750"
			vf->playLength = difftimeval(&session->fileEndTime, &session->fileStartTime);		//录像文件的播放长度(毫秒)
			vf->available = AVAILABLE_VIDEO_FILE;
			vf->fileType = NORMAL_VIDEO;

			data->session = session;
			data->video_file = vf;
			req->data = data;
			ret = uv_queue_work(session->mng->loop, req, update_db_work, after_update_db_work);
	}
	if(ret < 0){
		FREE(filename);
		FREE(volume_name);
		FREE(data);
		FREE(req);
		FREE(vf);
	}
	return ret;
}

void insert_db_work(uv_work_t* req)
{
	css_capture_work_data_t* data = (css_capture_work_data_t*)req->data;
	css_capture_session_t* s = data->session;
	tbl_videoFile* vf = data->video_file;

	s->db_result = css_db_insert_video_file(vf);
}

void after_insert_db_work(uv_work_t* req, int status)
{
	css_capture_work_data_t* data = (css_capture_work_data_t*)req->data;
	css_capture_session_t* s = data->session;
	tbl_videoFile* vf = data->video_file;
	
	free(vf->fileName);
	free(vf->volumeName);
	free(data);
	free(vf);
	free(req);

	/* now free the open file package */
	capture_session_package_remove_header(s, NULL, 1);

	if(status < 0 || s->db_result < 0){
		CL_ERROR("insert db error,status:%d,db_result:%d.\n",status,s->db_result);
		capture_session_stop_record(s);
		return ;
	}
	capture_session_do_package_if_necessary(s);
}

void update_db_work(uv_work_t* req)
{
	css_capture_work_data_t* data = (css_capture_work_data_t*)req->data;
	css_capture_session_t* s = data->session;
	tbl_videoFile* vf = data->video_file;

	s->db_result = css_db_update_video_file(vf);
}

void after_update_db_work(uv_work_t* req, int status)
{
	css_capture_work_data_t* data = (css_capture_work_data_t*)req->data;
	css_capture_session_t* s = data->session;
	tbl_videoFile* vf = data->video_file;

	free(vf->fileName);
	free(vf->volumeName);
	free(data);
	free(vf);
	free(req);

	/* now free the close file package */
	capture_session_package_remove_header(s, NULL, 1);

	if(status < 0 || s->db_result < 0){
		CL_ERROR("update db error,status:%d,db_result:%d.\n",status,s->db_result);
		capture_session_stop_record(s);
		return ;
	}
	capture_session_do_package_if_necessary(s);
}


/* record function */
int capture_start_record(css_record_info_t* record_info, css_sms_info_t* sms)  /* sms == NULL ,use default sms*/
{
	css_capture_session_t* session;
	css_sms_info_t* sms_info;
	int ret;

	if(!record_info)
		return -1;
	ret = capture_session_check_record_info(record_info);
	if(ret == -2){
		return 0;
	}
	else if(ret == -1){
		return ret;
	}

	session = (css_capture_session_t*)malloc(sizeof(css_capture_session_t));
	if(session == NULL){
		return UV_EAI_MEMORY;
	}

	if(sms == NULL){
		sms_info = css_server_get_default_sms();
	}
	else{
		sms_info = sms;
	}
	CL_INFO("start channel record dvrid:%lld,channelno:%u,type:%u.\n",
				record_info->dvrEquipId,record_info->channelNo,record_info->timestamp.type);
	uv_mutex_lock(&gMutex);
	ret = capture_session_init(session);
	ret = capture_session_set_info(session, record_info, sms_info);
	ret = capture_session_start_record(session);
	ret = capture_session_add(session);
	uv_mutex_unlock(&gMutex);
	return ret;
}

static void css_capture_stop_work(uv_work_t* w)
{
}

static void css_capture_after_stop_work(uv_work_t* w, int status)
{
	css_capture_session_t* session = (css_capture_session_t*)w->data;
	capture_session_stop_record(session);
	free(w);
}

/* TODO: wait all stop then return? */
void capture_stop_all(void)
{

	css_capture_session_t* session;
	int i;
	QUEUE *q;
	CL_INFO("stop all capture.\n");
	uv_mutex_lock(&gMutex);

	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		QUEUE_FOREACH(q, &gSessionMng[i].session_queue){
			uv_work_t* w;
			//session = QUEUE_DATA(QUEUE_HEAD(&gSessionMng[i].session_queue), css_capture_session_t, queue);
			session = QUEUE_DATA(q, css_capture_session_t, queue);
			//capture_session_remove(session);
			//capture_session_stop_record(session);// TODO: because removed ,so can't find and stop
			w = (uv_work_t*)malloc(sizeof(uv_work_t));
			if(!w){
			}
			w->data = session;
			uv_queue_work(session->mng->loop, w, css_capture_stop_work, css_capture_after_stop_work);
		}
	}

	uv_mutex_unlock(&gMutex);
}



/*
 * TEST:
 */
#ifdef CSS_TEST

void test_capture_open_file(void)
{
	css_capture_session_t s;

	capture_service_init();
	capture_service_start();

	capture_session_init(&s);
	capture_session_close_file(&s);
	Sleep(5000);
	capture_session_open_file(&s);
	Sleep(5000);
	capture_session_close_file(&s);
	Sleep(5000);
	capture_session_close_file(&s);
	Sleep(5000);

	unlink(s.recordFile.path);
	unlink(s.cifFile.path);
	unlink(s.iifFile.path);

	capture_session_open_file(&s);
	Sleep(5000);
	capture_session_close_file(&s);
	Sleep(5000);

	unlink(s.recordFile.path);
	unlink(s.cifFile.path);
	unlink(s.iifFile.path);

	capture_session_add_openfile_package(&s);
	capture_session_add_closefile_package(&s);
	Sleep(5000);

	unlink(s.recordFile.path);
	unlink(s.cifFile.path);
	unlink(s.iifFile.path);
	
	capture_service_stop();
	capture_service_clean();
}

void test_capture_package(void)
{
	css_capture_session_t s;
	css_capture_package_t p[10], *pp;
	int i;

	capture_session_init(&s);

	for(i = 0; i < 10; i++){
		p[i].type = i;
		capture_session_package_add(&s, &p[i]);
	}
	pp = s.package_header;
	while(pp){
		printf("pp [%d]\n", pp->type);
		pp = pp->next;
	}
	do{
		capture_session_package_remove_header(&s, &pp, 0);
		if(pp)
			printf("remove pp[%d]\n", pp->type);
	}while(pp);

	for(i = 0; i < 10; i++){
		capture_session_package_add(&s, &p[i]);
	}
	pp = s.package_header;
	while(pp){
		printf("pp [%d]\n", pp->type);
		pp = pp->next;
	}
	do{
		capture_session_package_remove_header(&s, NULL/*&pp*/, 0);
		/*if(pp)
			printf("remove pp[%d]\n", pp->type);*/
	}while(s.package_header);
}

void test_capture(void)
{
	css_capture_session_t s[10], *session;
	QUEUE *q;
	int i;

	capture_service_init();

	for(i = 0; i < 10; i++){
		capture_session_init(&s[i]);
		s[i].id = i;
	}

	for(i = 0; i < 10; i++){
		capture_session_remove(&s[i]);
	}

	capture_session_add(&s[3]);
	capture_session_add(&s[6]);
	capture_session_add(&s[9]);
	capture_session_add(&s[7]);
	capture_session_add(&s[5]);
	capture_session_add(&s[1]);
	capture_session_add(&s[2]);
	capture_session_add(&s[4]);
	capture_session_add(&s[8]);
	capture_session_add(&s[0]);

	
	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		QUEUE_FOREACH(q, &gSessionMng[i].session_queue){
			session = QUEUE_DATA(q, css_capture_session_t, queue);
			printf("sessin [%d]\n", session->id);
		}
	}

	for(i = 0; i < 10; i++){
		capture_session_remove(&s[i]);
	}

	for(i = 0; i < MAX_SESSION_MANAGER; i++){
		if(QUEUE_EMPTY(&gSessionMng[i].session_queue)){
			printf("session manager %d queue is empty\n", i);
		}
	}

	capture_service_clean();
	//return ;

	capture_service_start();
	Sleep(5000);
	capture_service_stop();

	capture_service_start();
	Sleep(5000);
	capture_service_stop();

	capture_service_clean();

	/* test again*/
	capture_service_init();

	capture_service_start();
	Sleep(5000);
	capture_service_stop();

	capture_service_start();
	Sleep(5000);
	capture_service_stop();

	capture_service_clean();

	/* test any call */
	capture_service_start();
	capture_service_clean();
	capture_service_stop();
	capture_service_init();
	capture_service_init();
	capture_service_start();
	capture_service_clean();
}

#include "css_ini_file.h"
#include "css_disk_manager.h"

void test_record(void)
{
	css_sms_info_t sms = {0};
	css_record_info_t recordInfo = {0};
	css_disk_config myDiskConfig;

	css_load_ini_file(DEFAULT_CONFIG_FILE);

	
	myDiskConfig.myLoop = gSessionMng[0].loop;
	css_get_disk_config(&myDiskConfig);
	
	css_init_disk_manager(&myDiskConfig);

	uv_ip4_addr("192.168.8.211", 65005, &sms.smsAddr);
	recordInfo.dvrEquipId = 1ull;
	recordInfo.channelNo = 1;
	recordInfo.timestamp.timestamp = (10*1000*1000);

	capture_service_init();
	capture_service_start();
	
	capture_start_record(&recordInfo, &sms);
	//capture_start_record(&recordInfo, &sms);
	Sleep(10000);
	recordInfo.timestamp.timestamp += (10*1000*1000);
	capture_start_record(&recordInfo, &sms);
	Sleep(3000);
	capture_stop_all();
	//Sleep(2000);
	css_destroy_disk_manager();
	capture_service_stop();
	capture_service_clean();
}

#endif
