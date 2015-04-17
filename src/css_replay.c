#include "css_replay.h"
#include "css_protocol_package.h"
#include "css_search_record.h"

#define DCL_CMD_HANDLER(cmd) \
	static void do_##cmd(css_stream_t* stream, char* package, ssize_t length);

#define IMP_CMD_HANDLER(cmd) \
	static void do_##cmd(css_stream_t* stream, char* package, ssize_t length)

#define DSP_CMD_HANDLER(cmd) \
	case cmd:				 \
	do_##cmd(stream, package, length);\
	break;

#define CSS_RT_GET_CMD(cmd) css_proto_package_decode(package, length, (JNetCmd_Header*) &cmd)
#define CSS_RT_FREE_PACKAGE() free(package)
#define CSS_RT_CLOSE_STREAM() css_stream_close(stream, css_cmd_delete_stream)

static css_replay_session_t* css_replay_session_create(css_stream_t* stream, uv_buf_t fileName);
static css_replay_session_t* css_replay_session_get(css_stream_t* stream);
static int css_replay_session_release(css_replay_session_t* session);
static int css_replay_session_play(css_replay_session_t* session);
static int css_replay_session_start(css_replay_session_t* session);
static int css_replay_session_send(css_replay_session_t* session);
static int css_replay_session_pause(css_replay_session_t* session);
static int css_replay_session_continue(css_replay_session_t* session);
static int css_replay_session_set_speed(css_replay_session_t* session, int speed);
static int css_replay_session_set_position(css_replay_session_t* session, int position);
static int css_replay_session_stop(css_replay_session_t* session);


static int css_replay_session_update_time(css_replay_session_t* session);
static int css_replay_session_calcul_next_send_time(css_replay_session_t* session);

static void css_replay_session_send_timer_cb(uv_timer_t* t);

static void css__replay_session_open_cb(css_replay_frame_getter_t* getter);
static void css__replay_session_close_cb(css_replay_frame_getter_t* getter);
static void css__replay_session_get_cb(css_replay_frame_getter_t* getter);
static void css__replay_session_set_position_cb(css_replay_frame_getter_t* getter);

static int css_replay_add_session(css_replay_session_t* session);
static int css_replay_remove_session(css_replay_session_t* session);
static int css_replay_stop_all(void);

static css_replay_sessions_manager_t g_sessions_mng = {0};

/* uv_once intialization guards */
static uv_once_t g_mng_init_guard = UV_ONCE_INIT;

static void css_replay_init(void)
{
	QUEUE_INIT(&g_sessions_mng.active_sessions);
}

static void css_replay_once_init(void)
{
	uv_once(&g_mng_init_guard, css_replay_init);
}

static int css_replay_add_session(css_replay_session_t* session)
{
	int ret = 0;
	QUEUE_INSERT_TAIL(&g_sessions_mng.active_sessions, &session->q);
	return ret;
}

static int css_replay_remove_session(css_replay_session_t* session)
{
	int ret = 0;
	QUEUE_REMOVE(&session->q);
	return ret;
}

static int css_replay_stop_all(void)
{
	int ret = 0;
	return ret;
}

static void css_cmd_delete_stream(css_stream_t* stream);
static int send_video_frame(css_stream_t* stream, css_video_frame_t* frame);
static int on_play_reply(css_stream_t* stream, int status);
static int on_commom_reply(css_stream_t* stream, int status);
static int css_send_reply(css_stream_t* stream, char* p, ssize_t len);
static void on_stream_write(css_write_req_t* req, int status);

DCL_CMD_HANDLER(sv_cmd_replay_search_record)
DCL_CMD_HANDLER(sv_cmd_replay_play_file)
DCL_CMD_HANDLER(sv_cmd_replay_set_speed)
DCL_CMD_HANDLER(sv_cmd_replay_set_postion)
DCL_CMD_HANDLER(sv_cmd_replay_pause)
DCL_CMD_HANDLER(sv_cmd_replay_continue)
DCL_CMD_HANDLER(sv_cmd_replay_shutdown)

static void on_data_cb(css_stream_t* stream, char* package, ssize_t status)
{
	
	int ret = -1;

	JNetCmd_Header header;

	/* stop read first, start read later if necessary */
	css_stream_read_stop(stream);

	if (status <= 0) {
		if (package)
			free(package);
		css_replay_session_stop(css_replay_session_get(stream));
		return;
	}

	ret = css_proto_package_header_decode(package, status, &header);
	if(ret <= 0){
		free(package);
		css_replay_session_stop(css_replay_session_get(stream));
		return;
	}
	css_replay_cmd_dispatch(header.I_CmdId, stream, package, status, 1);
}

// replay api
void css_replay_cmd_dispatch(uint32_t cmdid, css_stream_t* stream, char* package, ssize_t length, int internal)
{
	switch (cmdid) {
	DSP_CMD_HANDLER(sv_cmd_replay_search_record)
	DSP_CMD_HANDLER(sv_cmd_replay_play_file)
	DSP_CMD_HANDLER(sv_cmd_replay_set_speed)
	DSP_CMD_HANDLER(sv_cmd_replay_set_postion)
	DSP_CMD_HANDLER(sv_cmd_replay_pause)
	DSP_CMD_HANDLER(sv_cmd_replay_continue)
	DSP_CMD_HANDLER(sv_cmd_replay_shutdown)
	}
	if(!internal){
		int ret;
		ret = css_stream_read_start(stream, on_data_cb);
		if(ret && ret != UV_EALREADY){
			CSS_RT_CLOSE_STREAM();
		}
	}
}

static uint64_t css_get_mstime(void)
{
	struct timeval t;
	int ret = gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

static int css_replay_session_update_time(css_replay_session_t* session)
{
	int ret = 0;
	session->now = css_get_mstime();
	session->last_send_time = session->next_send_time;
	if(session->last_send_time == 0){
		session->last_send_time = session->now;
		session->last_timestemp = session->getter.frame->millisecond;
	}
	session->next_send_time = session->last_send_time + session->getter.frame->millisecond - session->last_timestemp;
	session->last_timestemp = session->getter.frame->millisecond;
	return ret;
}

static int css_replay_session_calcul_next_send_time(css_replay_session_t* session)
{
	int ret = 0;
	int64_t ts;
	ts = session->next_send_time - session->now;
	if(session->rate == 0){
		session->rate = 1;
	}
	if(ts > 1000){
		ts = 1000;
		session->next_send_time = session->now + ts;
	}
	ret = ts / session->rate;
	return ret;
}

static void css_replay_session_send_timer_cb(uv_timer_t* t)
{
	css_replay_session_t* s = (css_replay_session_t*)t->data;
	css_replay_session_send(s);
}

/* call back function */
static void css__replay_session_open_cb(css_replay_frame_getter_t* getter)
{
	css_replay_session_t* s = (css_replay_session_t*)getter->data;
	if(getter->result < 0){ /* open error */
		css_replay_session_stop(s);
	}
	else{
		css_replay_session_start(s);
	}
}

static void css__replay_session_close_cb(css_replay_frame_getter_t* getter)
{
	css_replay_session_t* s = (css_replay_session_t*)getter->data;
	css_stream_close(s->stream, css_cmd_delete_stream);
}

static void css__replay_session_get_cb(css_replay_frame_getter_t* getter)
{
	int ret;
	css_replay_session_t* s = (css_replay_session_t*)getter->data;
	if(getter->result < 0 || (!getter->frame || getter->frame->state < 0)){
		css_replay_session_stop(s);
		return;
	}
	if(s->state == CSS_REPLAY_PAUSE){
		css_replay_session_send(s);
		return;
	}
	ret = css_replay_session_update_time(s);
	ret = css_replay_session_calcul_next_send_time(s);
	if(ret <= 0){
		css_replay_session_send(s);
	}
	else{
		uv_timer_start(s->timer, css_replay_session_send_timer_cb, ret, 0);
	}
}

static void css__replay_session_set_position_cb(css_replay_frame_getter_t* getter)
{
	css_replay_session_t* s = (css_replay_session_t*)getter->data;
}

// session operation handler

static int css_replay_session_init(css_replay_session_t* s)
{
	int ret = 0;

	QUEUE_INIT(&s->q);
	uv_timer_init(s->stream->loop, s->timer);
	s->getter.loop = s->stream->loop;
	s->getter.data = s;
	s->stream->data = s;
	s->timer->data = s;
	s->rate = 1;
	return ret;
}

static css_replay_session_t* css_replay_session_create(css_stream_t* stream, uv_buf_t fileName)
{
	css_replay_session_t* session = NULL;
	int ret = 0;

	if(fileName.len > MAX_PATH){
		ret = UV_ENAMETOOLONG;
		return NULL;
	}

	css_replay_once_init();

	session = (css_replay_session_t*)malloc(sizeof(css_replay_session_t));
	if(!session){
		return session;
	}
	memset(session, 0, sizeof(css_replay_session_t));
	session->timer = (uv_timer_t*)malloc(sizeof(uv_timer_t));
	if(session->timer == NULL){
		free(session);
		return NULL;
	}
	memcpy(session->file_name, fileName.base, fileName.len);
	session->stream = stream;
	ret = css_replay_session_init(session);
	if(ret < 0){
		free(session);
		session = NULL;
	}
	return session;
}

static css_replay_session_t* css_replay_session_get(css_stream_t* stream)
{
	css_replay_session_t* session = (css_replay_session_t*)stream->data;
	return session;
}

static int css_replay_session_release(css_replay_session_t* session)
{
	if(session){
		css_replay_remove_session(session);
		free(session);
	}
	return 0;
}

static int css_replay_session_play(css_replay_session_t* session)
{
	int ret = 0;

	ret = css_replay_frame_getter_open(&session->getter, session->file_name, css__replay_session_open_cb);
	if(ret < 0){
	}
	else{
		session->state = CSS_REPLAY_INITED;
		css_replay_add_session(session);
	}
	return ret;
}

static int css_replay_session_start(css_replay_session_t* session)
{
	int ret = 0;
	if(session->state == CSS_REPLAY_PLAYNG){
		return ret;
	}

	session->state = CSS_REPLAY_PLAYNG;
	ret = css_replay_frame_getter_get_next_frame(&session->getter, css__replay_session_get_cb);
	if(ret < 0){
		css_replay_session_stop(session);
	}
	return ret;
}

static int css_replay_session_send(css_replay_session_t* session)
{
	int ret;
	ret = send_video_frame(session->stream, session->getter.frame);
	if(ret < 0){
		css_replay_session_stop(session);
		return ret;
	}

	if(session->state == CSS_REPLAY_PLAYNG){
		ret = css_replay_frame_getter_get_next_frame(&session->getter, css__replay_session_get_cb);
		if(ret < 0){
			css_replay_session_stop(session);
		}
	}
	return 0;
}

static int css_replay_session_pause(css_replay_session_t* session)
{
	int ret = 0;
	session->state = CSS_REPLAY_PAUSE;
	session->next_send_time = 0;
	return ret;
}

static int css_replay_session_continue(css_replay_session_t* session)
{
	int ret = 0;
	ret = css_replay_session_start(session);
	return ret;
}

static int css_replay_session_set_speed(css_replay_session_t* session, int speed)
{
	int ret = 0;
	if(speed > 0 && speed < 9){
		session->rate = speed;
	}
	else{
		ret = -1;
		/*session->rate = 1;*/
	}
	return ret;
}

static int css_replay_session_set_position(css_replay_session_t* session, int position)
{
	int ret = 0;
	return ret;
}

static void css_replay_session_timer_close(uv_handle_t* h)
{
	free(h);
}

static int css_replay_session_stop(css_replay_session_t* session)
{
	int ret = 0;
	if(session->state != CSS_REPLAY_SHUTDW){
		session->state = CSS_REPLAY_SHUTDW;
		uv_close((uv_handle_t*)session->timer, css_replay_session_timer_close);
		ret = css_replay_frame_getter_close(&session->getter, css__replay_session_close_cb);
		if(ret < 0){
			css_stream_close(session->stream, css_cmd_delete_stream);
			
		}
	}
	return ret;
}


// replay cmd handler
IMP_CMD_HANDLER(sv_cmd_replay_search_record)
{
	JNetCmd_Replay_Search_Record cmd;
	int ret = -1;
	css_search_record_t *search_req = NULL;
	css_search_conditions_t *conds = NULL;
	uv_work_t* req = NULL;

	do {
		if (CSS_RT_GET_CMD(cmd) <= 0) {
			ret = -1;
			break;
		}

		search_req = (css_search_record_t*)malloc(sizeof(css_search_record_t));
		req = (uv_work_t*)malloc(sizeof(uv_work_t));
		if(!search_req || !req) {
			ret = -1;
			break;
		}

		memset(search_req, 0, sizeof(css_search_record_t));
		conds = &(search_req->conds);

		conds->dvrEquipId = cmd.dvrEquipId;
		conds->channelNo = cmd.channelNo;
		conds->fileType = cmd.recordType;
		memcpy(conds->beginTime, cmd.startTime.base, cmd.startTime.len);
		memcpy(conds->endTime, cmd.endTime.base, cmd.endTime.len);

		search_req->stream = stream;

		req->data = search_req;
		ret = uv_queue_work(stream->loop, req, search_record, after_search_record);
	} while(0);

	if(ret < 0) {
		FREE(search_req)
		FREE(req)
		CSS_RT_CLOSE_STREAM();
	}

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_play_file)
{
	int ret = -1;
	JNetCmd_Replay_Play_File cmd;
	css_replay_session_t* session = NULL;

	do {
		if (CSS_RT_GET_CMD(cmd) <= 0) {
			ret = -1;
			break;
		}

		session = css_replay_session_create(stream, cmd.fileName);
		if (!session) {
			ret = -1;
			break;
		}

		ret = css_replay_session_play(session);
	} while(0);

	if (ret < 0){
		FREE(session);
	}
	on_play_reply(stream, ret);

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_set_speed)
{
	int ret = -1;
	JNetCmd_Replay_Set_Speed cmd;
	css_replay_session_t* session = NULL;

	ret = CSS_RT_GET_CMD(cmd);
	if (ret > 0) {
		session = css_replay_session_get(stream);
		css_replay_session_set_speed(session, cmd.speed);
	}

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_set_postion)
{
	int ret = -1;
	JNetCmd_Replay_Set_Postion cmd;
	css_replay_session_t* session = NULL;

	ret = CSS_RT_GET_CMD(cmd);
	if (ret > 0) {
		session = css_replay_session_get(stream);
		css_replay_session_set_position(session, cmd.postion);
	}

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_pause)
{
	css_replay_session_t* session = NULL;

	session = css_replay_session_get(stream);
	css_replay_session_pause(session);

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_continue)
{
	css_replay_session_t* session = NULL;

	session = css_replay_session_get(stream);
	css_replay_session_continue(session);

	CSS_RT_FREE_PACKAGE();
}

IMP_CMD_HANDLER(sv_cmd_replay_shutdown)
{
	css_replay_session_t* session = NULL;

	session = css_replay_session_get(stream);
	css_replay_session_stop(session);

	CSS_RT_FREE_PACKAGE();
}


static void css_cmd_delete_stream(css_stream_t* stream)
{
	if(stream){
		css_replay_session_release(css_replay_session_get(stream));
		free(stream);
	}
}

/* TODO: change to no memcpy */
static int send_video_frame(css_stream_t* stream, css_video_frame_t* frame)
{
	int ret = -1;
	JNetCmd_Replay_Send_Frame cmd;
	ssize_t len = 0;
	char *buf = NULL;

	JNetCmd_Replay_Send_Frame_init(&cmd);
	cmd.frameType = frame->frameType;
	cmd.sendType = frame->sendType;
	cmd.frameData = frame->frameData;
	cmd.replayDataLen = frame->frameData.len;

	len = css_proto_package_calculate_buf_len((JNetCmd_Header*)&cmd);
	buf = (char *)malloc(len);
	if (buf == NULL)
		return -1;

	css_proto_package_encode(buf, len, (JNetCmd_Header*)&cmd);
	css_replay_frame_getter_release_frame(&((css_replay_session_t*)stream->data)->getter, frame);
	return css_send_reply(stream, buf, len);
}

static int on_play_reply(css_stream_t* stream, int status)
{
	JNetCmd_Replay_Play_File_Resp cmd;
	ssize_t len = 0;
	char *buf = NULL;

	JNetCmd_Replay_Play_File_Resp_init(&cmd);
	cmd.state = status;

	len = css_proto_package_calculate_buf_len((JNetCmd_Header*)&cmd);
	buf = (char *)malloc(len);
	if (buf == NULL)
		return -1;

	css_proto_package_encode(buf, len, (JNetCmd_Header*)&cmd);
	return css_send_reply(stream, buf, len);
}

static int on_commom_reply(css_stream_t* stream, int status)
{
	JNetCmd_Replay_Common_Resp cmd;
	ssize_t len = 0;
	char *buf = NULL;

	JNetCmd_Replay_Common_Resp_init(&cmd);
	cmd.state = status;

	len = css_proto_package_calculate_buf_len((JNetCmd_Header*)&cmd);
	buf = (char *)malloc(len);
	if (buf == NULL)
		return -1;

	css_proto_package_encode(buf, len, (JNetCmd_Header*)&cmd);
	return css_send_reply(stream, buf, len);
}

static int css_send_reply(css_stream_t* stream, char* p, ssize_t len)
{
	int ret = -1;
	css_write_req_t *req = NULL;
	uv_buf_t buf = {0};

	req = (css_write_req_t *)malloc(sizeof(css_write_req_t));
	if (req != NULL) {
		buf.base = p;
		buf.len = len;
		ret = css_stream_write(stream, req, buf, on_stream_write);
		if (ret == 0)
			return 0;
	}

	FREE(p)
	FREE(req)
	return -1;
}

static void on_stream_write(css_write_req_t* req, int status)
{
	css_replay_session_t* session = NULL;

	if (status < 0) {
		session = css_replay_session_get(req->stream);
		css_replay_session_stop(session);
	}

	FREE(req->buf.base)
	FREE(req)
}
