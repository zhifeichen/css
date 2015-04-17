/**
 fyy
 说明：	获得的工作日历表，晦涩，结构混乱，无冲突检测，无奈
 尽量模拟获取的日历结构，然后再处理冲突，错误，合并。
 最后处理边界，空值等问题。
 注意：	1.获取不到job，使用上次获得文档
 2.无日历任务，默认为执行
 3.休息和工作冲突，以工作为重
 4.mxml透明和非透明字符串的区别（空格等）
 */

#include <time.h>
#include "uv/uv.h"
#include "css_job.h"
#include "mxml/mxml.h"
#include "css_restclient.h"
#include "css_capture.h"
#include "css_logger.h"
#include "css_ini_file.h"

#ifdef WIN32
#pragma comment(lib,"mxml1.lib")
#endif

static char* manage_ctr = "manager center";

QUEUE CHANNEL_QUEUE;
QUEUE CALENDAR_QUEUE;

static int running = 0;
static uv_mutex_t gmutex;
static uv_loop_t *loop = NULL;
static int HTTP_TIMEOUT = 5;
static char *loginName = NULL;
static char *password = NULL;
static char *centeralizeServer_addr = NULL;
static char *login_addr = NULL;
static char *channel_addr = NULL;
static char *type = NULL;
static int64_t csId;
static char *css_URL = NULL;
static int jobTriggerTime = 0;
static uv_timer_t* Jobtimer;

static int get_channel_info_from_id(CHANNEL *channel, uint64_t Id);
static int get_channel(uint64_t Id, char* token, CHANNEL *channel);
static int deal_job_xml(char* xmlstr);
static int insert_channellistByNoKeyClash(CHANNEL* chan);
static CALENDAR* css_find_calendar_inlist(int64_t calendar_id);
static int start_trigger();
static int begin_Scheduler(CHANNEL *chan, int64_t timestamp);
static int get_calendar_from_xmlptr(mxml_node_t *tree);
static int get_job_from_xmlPtr(mxml_node_t *tree);
static int trigger_mingle_two_list(TRIGGER* srcCh_listhead,
		TRIGGER* desCh_listhead);
static int trigger_mingle_to_list(TRIGGER* srcTr_listhead, TRIGGER* Tr);
static uint32_t get_today_sec_from_now();
static int deal_this_trigger(CHANNEL* channel);
typedef void (*queue_node_cb)(QUEUE* q);
static void free_queue(QUEUE* q, queue_node_cb cb);
static void free_channel_cb(QUEUE* q);
static TRIGGER* copy_trigger(TRIGGER* srcTrg);
static void delete_trigger(TRIGGER* trg);
static int css_get_config();

//http://10.3.0.224:8080/sc/centeralizeServer?_type=ws&csId=1015810
#define GEN_HTTP_GET_JOB_URL(url) sprintf(url,"%s%s?_type=%s&csId=%lld",css_URL,centeralizeServer_addr,type,csId);

//http://10.3.0.224:8080/sc/login?_type=ws&loginName=sysadmin&password=12345
#define GEN_HTTP_GET_TOKEN_URL(url) sprintf(url,"%s%s?_type=%s&loginName=%s&password=%s",css_URL,login_addr,type,loginName,password);

//http://10.3.0.224:8080/sc/channel?_type=ws&channelId=589834&token=sc47@5@10.3.0.162:1405057485937-10.3.0.224--15326665284029583095674386736650392646-33935239872011307088728051780234754770-685754
#define GEN_HTTP_GET_CHANNEL_URL(url,token,Id) sprintf(url,"%s%s?_type=%s&channelId=%lld&token=%s",css_URL,channel_addr,type,Id,token);

#ifdef CSS_TEST
static int print_begin_Scheduler(css_record_info_t* record_info, css_sms_info_t* sms);
static uint32_t get_today_sec_from_now_for_test();
#define capture_start_record(a,b) print_begin_Scheduler(a,b);
#define GET_TODAY_SEC_FROM_NOW get_today_sec_from_now_for_test();
#else
#define GET_TODAY_SEC_FROM_NOW get_today_sec_from_now();
#endif

mxml_type_t type_cb1(mxml_node_t* node)
{
	if ((!strcmp(node->value.opaque, "date"))
			|| (!strcmp(node->value.opaque, "day_of_week"))
			|| (!strcmp(node->value.opaque, "day_of_month")))
		return (MXML_OPAQUE);
	return (MXML_TEXT);
}

int httpGet(char* url, char *body)
{
	//char body[1024*512]={0};
	int r;
	if ((r = get_rest(url, body, "", HTTP_TIMEOUT)) != 200) {
		CL_ERROR("get job:%s error:%d.\n", url, r);
		return -1;
	} else
		return 0;
}

int css_get_config()
{
	char *httpTimeOut, *csid;
	int ret = 0;
	if (css_URL == NULL) {
		ret = css_get_env(manage_ctr, "wsUriPref", "", &css_URL);
		if (ret != 0)
			return -1;
		css_get_env(manage_ctr, "centeralizeServer_addr", "centeralizeServer",
				&centeralizeServer_addr);
		css_get_env(manage_ctr, "login_addr", "login", &login_addr);
		css_get_env(manage_ctr, "channel_addr", "channel", &channel_addr);
		css_get_env(manage_ctr, "type", "ws", &type);
		css_get_env(manage_ctr, "loginName", "sysadmin", &loginName);
		css_get_env(manage_ctr, "password", "12345", &password);
		css_get_env(manage_ctr, "httpTimeOut", "5", &httpTimeOut);
		ret = css_get_env(manage_ctr, "csId", "", &csid); //the same import to css_URL
		HTTP_TIMEOUT = atoi(httpTimeOut);
		free(httpTimeOut);
		csId = ATOLL(csid);
		free(csid);

		jobTriggerTime = 0;
	}
	return ret;
}

static char JOB_BODY[RESPONSE_BUFF_LEN] = { 0 };

void uv_work_cb_job(uv_work_t* req)
{

	char body[RESPONSE_BUFF_LEN] = { 0 };
	char url[256] = { 0 };
	CL_DEBUG("start get css job.\n");
	GEN_HTTP_GET_JOB_URL(url);

	//inti channel calendar queue
	QUEUE_INIT(&CHANNEL_QUEUE);
	QUEUE_INIT(&CALENDAR_QUEUE);
	//防止两次同时请求job
	uv_mutex_lock(&gmutex);
	if (!httpGet(url, body))		//sucess to get http
			{		// 保存一份，防止下次连不通
		memcpy(JOB_BODY, body, RESPONSE_BUFF_LEN);
		deal_job_xml(body);
	} else {
		deal_job_xml(JOB_BODY);
	}

	uv_mutex_unlock(&gmutex);
}

void uv_after_work_cb_job(uv_work_t* req, int status)
{
	start_trigger();
	free(req);
}

void timer_cb_for_getjob(uv_timer_t* handle)
{

	time_t nowtime;
	struct tm *begin_t;
	uv_work_t *work_forJob;
	CL_DEBUG("start get css job when timeout..\n");
	// 防止定时器不精确 就到00去取
	nowtime = time(NULL);
	begin_t = localtime(&nowtime);
	if ((begin_t->tm_min != 0) && (begin_t->tm_min > 58)) {
		//假设误差在1分钟内
		uv_timer_start(Jobtimer, timer_cb_for_getjob,
				1000 * (60 - begin_t->tm_sec), 0);
		return;
	}

	//先删了原先的队列 
	free_queue(&CHANNEL_QUEUE, free_channel_cb);
	free_queue(&CALENDAR_QUEUE, NULL);

	work_forJob = (uv_work_t*) malloc(sizeof(uv_work_t));
	uv_queue_work(loop, work_forJob, uv_work_cb_job, uv_after_work_cb_job);
}

int css_start_job(uv_loop_t *loop_in)
{

	int dec_to_nextday = 0;
	time_t nowtime;
	struct tm *begin_t;
	uv_work_t *work_forJob;

	if (running)
		return 1;
	CL_INFO("start css job.\n");
	running = 1;
	if (css_get_config() == -1) {
		CL_INFO("get config error.\n");
		return -1;
	}

	uv_mutex_init(&gmutex);
	if (!loop_in)
	{
		loop = uv_default_loop();
		*loop_in = *loop;
	}
	else
		loop = loop_in;

	//first start calculate next job_get
	nowtime = time(NULL);
	begin_t = localtime(&nowtime);
	dec_to_nextday = 24 * 3600 - begin_t->tm_hour * 3600 - begin_t->tm_min * 60
			- begin_t->tm_sec;
	dec_to_nextday = dec_to_nextday > 100 ? dec_to_nextday : 100;//至少留出100s给job获取

	Jobtimer = (uv_timer_t*) malloc(sizeof(uv_timer_t));
	uv_timer_init(loop, Jobtimer);
	uv_timer_start(Jobtimer, timer_cb_for_getjob, dec_to_nextday * 1000,
			jobTriggerTime);

	//first get job
	work_forJob = (uv_work_t*) malloc(sizeof(uv_work_t));
	uv_queue_work(loop, work_forJob, uv_work_cb_job, uv_after_work_cb_job);

	return 0;
}

void free_channel_cb(QUEUE* q)
{
	CHANNEL * channel = NULL;
	channel = QUEUE_DATA(q, CHANNEL, channel_queue);
	delete_trigger(channel->trigger);
	uv_timer_stop(&channel->channelTimer);
	uv_close((uv_handle_t*) &channel->channelTimer, NULL);
}

void free_queue(QUEUE* q, queue_node_cb cb)
{
	QUEUE* q_temp = NULL;

	QUEUE_FOREACH(q_temp,q)
	{
		if (cb)
			cb(q_temp);
		QUEUE_REMOVE(q_temp);
	}
}

void css_end_job()
{
	CL_INFO("stop css job.\n");
	if (css_URL) {
		FREE(css_URL);
		FREE(centeralizeServer_addr);
		FREE(login_addr);
		FREE(channel_addr);
		FREE(login_addr);
		FREE(loginName);
		FREE(password);
	}

	uv_mutex_destroy(&gmutex);
	//delete_L(list_CHANNEL,delete_L_cb_forTimer);
	free_queue(&CHANNEL_QUEUE, free_channel_cb);
	free_queue(&CALENDAR_QUEUE, NULL);
	uv_close((uv_handle_t*) Jobtimer, NULL);
}

int css_restart_job()
{
	css_end_job();
	return css_start_job(loop);
}

int get_token(char* token)
{
	mxml_node_t *tree, *token_node, *result_node;
	int result;
	char body[RESPONSE_BUFF_LEN] = { 0 };
	char url[256] = { 0 };
	GEN_HTTP_GET_TOKEN_URL(url);

	if (httpGet(url, body)) {
		CL_ERROR("get token:%s error.", url);
		return -1;
	}

	tree = mxmlLoadString(NULL, body, type_cb1);
	if (!tree)
		return -1;

	result_node = mxmlFindElement(tree, tree, "ErrorCode", NULL, NULL,
	MXML_DESCEND);
	if (result_node) {
		if (result_node->child)
			if ((result = atoi(result_node->child->value.text.string))) {
				CL_ERROR("get token error:%d", result);
				return -1;
			}
	}

	token_node = mxmlFindElement(tree, tree, "Token", NULL, NULL, MXML_DESCEND);
	if (token_node) {
		if (token_node->child)
			memcpy(token, token_node->child->value.text.string,
					strlen(token_node->child->value.text.string) + 1);
	} else {
		mxmlDelete(tree);
		return -1;
	}
	mxmlDelete(tree);
	return 0;
}

int get_channel_info_from_id(CHANNEL *channel, uint64_t Id)
{
	int ret;
	char token[RESPONSE_BUFF_LEN] = { 0 };
	if ((ret = get_token(token))) {
		return ret;
	}
	if ((ret = get_channel(Id, token, channel))) {
		return ret;
	}
	return 0;

}

int get_channel(uint64_t Id, char* token, CHANNEL *channel)
{
	mxml_node_t *tree, *result_node;
	mxml_node_t *ChannelNo, *DVRID, *dvrName, *dvrIp, *dvrPort, *ConfigPort,
			*LoginUser, *Password, *backSMTSIP, *backSMTSPort;
	mxml_node_t *StreamMediaIP, *StreamMediaPort;
	//*dvrType,*backSMTSName,*smtServerName,*channelId,*ConnectionType,*StreamMediaID,*backSMTSDwid,*backConnectionType
	int ret = 0;
	char body[RESPONSE_BUFF_LEN] = { 0 };
	{
		char url[256] = { 0 };
		GEN_HTTP_GET_CHANNEL_URL(url, token, Id);
		if (httpGet(url, body)) {
			CL_ERROR("get channel info:%s error:%d.\n", url, ret);
			return -1;
		}
	}
	tree = mxmlLoadString(NULL, body, type_cb1);
	if (!tree) {
		CL_ERROR("load channelinfo xml error.\n");
		return -1;
	}

	result_node = mxmlFindElement(tree, tree, "ErrorCode", NULL, NULL,
	MXML_DESCEND);
	if (result_node && result_node->child) {
		if ((ret = atoi(result_node->child->value.text.string))) {
			CL_ERROR("get channel info result:%d.\n", ret);
			return -1;
		}
	}

	//channelId = mxmlFindElement(tree,tree,"channelId",NULL,NULL,MXML_DESCEND);
	//if(channelId&&channelId->child)
	//	channel->Id = atoi(channelId->child->value.text.string);

	ChannelNo = mxmlFindElement(tree, tree, "ChannelNo", NULL, NULL,
	MXML_DESCEND);
	if (ChannelNo && ChannelNo->child)
		channel->ChannelNo = atoi(ChannelNo->child->value.text.string);

	DVRID = mxmlFindElement(tree, tree, "DVRID", NULL, NULL, MXML_DESCEND);
	if (DVRID && DVRID->child)
		channel->DVRID = atoi(DVRID->child->value.text.string);

	dvrName = mxmlFindElement(tree, tree, "dvrName", NULL, NULL, MXML_DESCEND);
	if (dvrName && dvrName->child)
		memcpy(channel->dvrName, dvrName->child->value.text.string,
				strlen(dvrName->child->value.text.string));

	dvrIp = mxmlFindElement(tree, tree, "dvrIp", NULL, NULL, MXML_DESCEND);
	if (dvrIp && dvrIp->child)
		memcpy(channel->dvrIp, dvrIp->child->value.text.string,
				strlen(dvrIp->child->value.text.string));

	dvrPort = mxmlFindElement(tree, tree, "dvrPort", NULL, NULL, MXML_DESCEND);
	if (dvrPort && dvrPort->child)
		channel->dvrPort = atoi(dvrPort->child->value.text.string);

	ConfigPort = mxmlFindElement(tree, tree, "ConfigPort", NULL, NULL,
	MXML_DESCEND);
	if (ConfigPort && ConfigPort->child)
		channel->ConfigPort = atoi(ConfigPort->child->value.text.string);

	LoginUser = mxmlFindElement(tree, tree, "LoginUser", NULL, NULL,
	MXML_DESCEND);
	if (LoginUser && LoginUser->child)
		memcpy(channel->LoginUser, LoginUser->child->value.text.string,
				strlen(LoginUser->child->value.text.string));

	Password = mxmlFindElement(tree, tree, "Password", NULL, NULL,
	MXML_DESCEND);
	if (Password && Password->child)
		memcpy(channel->Password, Password->child->value.text.string,
				strlen(Password->child->value.text.string));

	StreamMediaIP = mxmlFindElement(tree, tree, "StreamMediaIP", NULL, NULL,
	MXML_DESCEND);
	if (StreamMediaIP && StreamMediaIP->child)
		memcpy(channel->StreamMediaIP, StreamMediaIP->child->value.text.string,
				strlen(StreamMediaIP->child->value.text.string));

	StreamMediaPort = mxmlFindElement(tree, tree, "StreamMediaPort", NULL, NULL,
	MXML_DESCEND);
	if (StreamMediaPort && StreamMediaPort->child)
		channel->StreamMediaPort = atoi(
				StreamMediaPort->child->value.text.string);

	backSMTSIP = mxmlFindElement(tree, tree, "backSMTSIP", NULL, NULL,
	MXML_DESCEND);
	if (backSMTSIP && backSMTSIP->child)
		memcpy(channel->backSMTSIP, backSMTSIP->child->value.text.string,
				strlen(backSMTSIP->child->value.text.string));

	backSMTSPort = mxmlFindElement(tree, tree, "backSMTSPort", NULL, NULL,
	MXML_DESCEND);
	if (backSMTSPort && backSMTSPort->child)
		channel->backSMTSPort = atoi(backSMTSPort->child->value.text.string);

	mxmlDelete(tree);
	return 0;
}

int get_calendar_from_xmlptr(mxml_node_t *tree)
{
	mxml_node_t *calendar_node, *day_of_week_node, *day_of_month_node,
			*day_of_data_node;

	calendar_node = mxmlFindElement(tree, tree, "calendar", NULL, NULL,
	MXML_DESCEND);
	while (calendar_node) {
		CALENDAR *temp_calendar = (CALENDAR*) malloc(sizeof(CALENDAR));
		memset(temp_calendar, 0, sizeof(CALENDAR));

		temp_calendar->id = ATOLL(calendar_node->value.element.attrs[0].value);

		if (strcmp(calendar_node->value.element.attrs[2].value,
				"THolidayCalendar") == 0) {
			temp_calendar->flag = HOLIDAY;

			day_of_data_node = mxmlFindElement(calendar_node, tree, "date",
			NULL, NULL, MXML_DESCEND);
			while (day_of_data_node) {
				if (day_of_data_node->child)
					css_format_year_date(day_of_data_node->child->value.opaque,
							temp_calendar->month_day);

				day_of_data_node = mxmlFindElement(day_of_data_node, tree,
						"date", NULL, NULL, MXML_NO_DESCEND);
			}

			if ((day_of_week_node = mxmlFindElement(calendar_node, tree,
					"day_of_week", NULL, NULL, MXML_DESCEND))) {
				if (day_of_week_node->child)
					css_format_week_date(day_of_week_node->child->value.opaque,
							temp_calendar->day_in_week, 7);
			}

			if ((day_of_month_node = mxmlFindElement(calendar_node, tree,
					"day_of_month", NULL, NULL, MXML_DESCEND))) {
				if (day_of_month_node->child)
					css_format_month_date(
							day_of_month_node->child->value.opaque,
							temp_calendar->day_in_month, 31);
			}
		} else {
			temp_calendar->flag = SPECIALDAY;
			day_of_data_node = mxmlFindElement(calendar_node, tree, "date",
			NULL, NULL, MXML_DESCEND);
			while (day_of_data_node) {
				if (day_of_data_node->child)
					css_format_specialday(day_of_data_node->child->value.opaque,
							temp_calendar->month_day);

				day_of_data_node = mxmlFindElement(day_of_data_node, tree,
						"date", NULL, NULL, MXML_NO_DESCEND);
			}
		}	//JCaptureVideoTask

		if (today_need_work(temp_calendar))
			temp_calendar->needwork = 1;

		QUEUE_INSERT_TAIL(&CALENDAR_QUEUE, &temp_calendar->calendar_queue);

		calendar_node = mxmlFindElement(calendar_node, tree, "calendar", NULL,
		NULL, MXML_NO_DESCEND);
	}

	return 0;
}

int get_job_from_xmlPtr(mxml_node_t *tree)
{
	mxml_node_t *job_node, *entry_node, *trigger_node;

	job_node = mxmlFindElement(tree, tree, "job", NULL, NULL, MXML_DESCEND);
	while (job_node) {
		TRIGGER *trgger, *temp_trgger;
		CHANNEL *channel;
		temp_trgger = trgger = NULL;

		if ((trigger_node = mxmlFindElement(job_node, tree, "trigger", NULL,
		NULL, MXML_DESCEND))) {
			trgger = temp_trgger = (TRIGGER*) malloc(sizeof(TRIGGER));
			memset(temp_trgger, 0, sizeof(TRIGGER));
		}
		while (trigger_node) {
			mxml_node_t *schedule_interval_node, *start_time_node, *id_node;
			CALENDAR * temp;

			schedule_interval_node = mxmlFindElement(trigger_node, tree,
					"schedule_interval", NULL, NULL, MXML_DESCEND);
			start_time_node = mxmlFindElement(trigger_node, tree, "start_time",
			NULL, NULL, MXML_DESCEND);
			id_node = mxmlFindElement(trigger_node, tree, "id", NULL, NULL,
			MXML_DESCEND);

			if (id_node->child) {
				if ((temp = css_find_calendar_inlist(
						ATOLL(id_node->child->value.text.string))))
					temp_trgger->calendar = temp;
				else
					continue;
			}
			if (schedule_interval_node->child) {
				temp_trgger->schedule_interval = atoi(
						schedule_interval_node->child->value.text.string);
			}
			if (start_time_node->child) {
				css_format_start_time(&temp_trgger->startTime,
						start_time_node->child->value.text.string);
			}

			if (temp_trgger->calendar->needwork == 1)
				trigger_mingle_to_list(trgger, temp_trgger);

			if ((trigger_node = mxmlFindElement(trigger_node, tree, "trigger",
			NULL, NULL, MXML_NO_DESCEND))) {
				temp_trgger = (TRIGGER*) malloc(sizeof(TRIGGER));
				memset(temp_trgger, 0, sizeof(TRIGGER));
			}
		}

		entry_node = mxmlFindElement(job_node, tree, "entry", NULL, NULL,
		MXML_DESCEND);
		while (entry_node) {
			channel = (CHANNEL*) malloc(sizeof(CHANNEL));
			memset(channel,0,sizeof(CHANNEL));
			channel->trigger = copy_trigger(trgger);
			channel->Id = ATOLL(entry_node->value.element.attrs[0].value);
			insert_channellistByNoKeyClash(channel);
			entry_node = mxmlFindElement(entry_node, tree, "entry", NULL, NULL,
			MXML_NO_DESCEND);
		}
		delete_trigger(trgger);
		job_node = mxmlFindElement(job_node, tree, "job", NULL, NULL,
		MXML_NO_DESCEND);
	}
	return 0;
}

TRIGGER* copy_trigger(TRIGGER* srcTrg)
{
	TRIGGER* destTrg;
	TRIGGER* temp;
	temp = destTrg = (TRIGGER*) malloc(sizeof(TRIGGER));
	*destTrg = *srcTrg;
	while ((srcTrg = srcTrg->next)) {
		destTrg->next = (TRIGGER*) malloc(sizeof(TRIGGER));
		destTrg = destTrg->next;
		*destTrg = *srcTrg;
	}

	return temp;
}

void delete_trigger(TRIGGER* trg)
{
	TRIGGER* temp;
	while (trg) {
		temp = trg;
		trg = trg->next;
		free(temp);
	}
}

int deal_job_xml(char* xmlstr)
{
	mxml_node_t *tree;
	mxml_node_t *result_node;
	int result;

	tree = mxmlLoadString(NULL, xmlstr, type_cb1);
	if (!tree)
		return -1;

	result_node = mxmlFindElement(tree, tree, "resultCode", NULL, NULL,
	MXML_DESCEND);
	if (result_node) {
		if (result_node->child)
			if ((result = atoi(result_node->child->value.text.string))) {
				printf("error job response errorcode = %d", result);
				return result;
			}
	}

	get_calendar_from_xmlptr(tree);

	get_job_from_xmlPtr(tree);

	mxmlDelete(tree);

	return 0;
}

int trigger_mingle_two_list(TRIGGER* srcCh_listhead, TRIGGER* desCh_listhead)
{
	TRIGGER* trg = desCh_listhead;
	TRIGGER* flag = NULL;
	do {
		flag = trg->next;
		trigger_mingle_to_list(srcCh_listhead, trg);
		trg = flag;
	} while (flag);
	return 0;
}

//该函数释放合并的TRIGGER
int trigger_mingle_to_list(TRIGGER* srcTr_listhead, TRIGGER* Tr)
{
	TRIGGER* srcTrigger, *desTrigger, *temp_t;
	uint32_t temp;
	if (srcTr_listhead == Tr)
		return 0;

	srcTrigger = srcTr_listhead;
	desTrigger = Tr;
	do {
		//start by mini
		if (srcTrigger->startTime > desTrigger->startTime) {
			temp = desTrigger->startTime;
			desTrigger->startTime = srcTrigger->startTime;
			srcTrigger->startTime = temp;

			temp = desTrigger->schedule_interval;
			desTrigger->schedule_interval = srcTrigger->schedule_interval;
			srcTrigger->schedule_interval = temp;
		}
		//合并条件
		if (srcTrigger->startTime + srcTrigger->schedule_interval
				>= desTrigger->startTime) {
			if (srcTrigger->startTime + srcTrigger->schedule_interval
					< desTrigger->startTime + desTrigger->schedule_interval)
				srcTrigger->schedule_interval = desTrigger->startTime
						+ desTrigger->schedule_interval - srcTrigger->startTime;

			temp_t = desTrigger;
			if (srcTrigger->next) {	//重新排序该队列：提取后一个节点再进行比较
				desTrigger = (TRIGGER*) malloc(sizeof(TRIGGER));
				*desTrigger = *(srcTrigger->next);
				srcTrigger->next = desTrigger->next;
				desTrigger->next = NULL;
			} else
				desTrigger = NULL;
			free(temp_t);
		} else {
			if (srcTrigger->next == NULL) {
				srcTrigger->next = desTrigger;
				break;
			}
			srcTrigger = srcTrigger->next;
		}
	} while (desTrigger && srcTrigger);

	return 0;
}

int insert_channellistByNoKeyClash(CHANNEL *chan)
{
	//查找id是否在queue中
	QUEUE* q = NULL;
	CHANNEL * channel = NULL;
	QUEUE_FOREACH(q,&CHANNEL_QUEUE)
	{
		channel = QUEUE_DATA(q, CHANNEL, channel_queue);
		if (channel->Id == chan->Id) {
			//合并
			trigger_mingle_two_list(channel->trigger, chan->trigger);
			return 1;
		}
	}
	//get new channel infomation than insert to queue
	get_channel_info_from_id(chan, chan->Id);
	QUEUE_INSERT_TAIL(&CHANNEL_QUEUE, &chan->channel_queue);
	return 0;
}

CALENDAR* css_find_calendar_inlist(int64_t calendar_id)
{
	QUEUE* q = NULL;
	CALENDAR* cal = NULL;
	QUEUE_FOREACH(q,&CALENDAR_QUEUE)
	{
		cal = QUEUE_DATA(q, CALENDAR, calendar_queue);
		if (cal->id == calendar_id)
			return cal;
	}
	return NULL;
}

int begin_Scheduler(CHANNEL *chan, int64_t timestamp)
{
	css_record_info_t record_info;
	css_sms_info_t sms;
	int ret = 0;
	record_info.channelNo = chan->ChannelNo;
	record_info.dvrEquipId = chan->DVRID;
	record_info.flowType = FLOW_TYPE_PRIMARY;					//fyy
	sprintf(record_info.timestamp.eventId, "%lld", chan->Id);			//fyy
	record_info.timestamp.timestamp = timestamp*1000000;				//usc
	record_info.timestamp.type = 1;

	sms.smsAddr.sin_family = AF_INET;
	{
		sms.smsAddr.sin_addr.s_addr = inet_addr(chan->StreamMediaIP);
		sms.smsAddr.sin_port = htons(chan->StreamMediaPort);
		sms.smsEquipId = chan->StreamMediaID;
	}

	CL_INFO("sms info: ip:%s  port:%d  ChannelId:%lld\n ",chan->StreamMediaIP,chan->StreamMediaPort,chan->Id);

	ret = capture_start_record(&record_info, &sms);	//TODO: may change, not support back smtsinfo.
	if (ret) {		//TODO: handle error
		CL_ERROR("start record job,dvrid:%lld,channelNo:%d error:%d.\n",
				record_info.dvrEquipId, record_info.channelNo, ret);
	}
	return ret;
}

uint32_t get_today_sec_from_now()
{
	time_t nowtime;
	struct tm *begin_t;
	nowtime = time(NULL);
	begin_t = localtime(&nowtime);

	return (begin_t->tm_hour * 3600 + begin_t->tm_min * 60 + begin_t->tm_sec);
}

void timer_cb_for_start_job(uv_timer_t* handle)
{
	CHANNEL * channel = (CHANNEL*) handle->data;

	deal_this_trigger(channel);
}

int deal_this_trigger(CHANNEL* channel)
{
	uint32_t today_min = GET_TODAY_SEC_FROM_NOW
	while (channel->trigger) {
		channel->channelTimer.data = channel;
		if (today_min < channel->trigger->startTime) {
			uv_timer_start(&channel->channelTimer, timer_cb_for_start_job,
					(channel->trigger->startTime - today_min) * 1000, 0);
			return 1;
		} else if (today_min
				< (channel->trigger->startTime
						+ channel->trigger->schedule_interval)) {
			TRIGGER *temp;
			begin_Scheduler(channel,
					channel->trigger->schedule_interval
							- (today_min - channel->trigger->startTime));
			//delete this trigger
			temp = channel->trigger;
			channel->trigger = channel->trigger->next;
			free(temp);

			if (channel->trigger)
				uv_timer_start(&channel->channelTimer, timer_cb_for_start_job,
						(channel->trigger->startTime - today_min) * 1000, 0);
			return 1;
		} else {
			TRIGGER *temp;
			temp = channel->trigger;
			channel->trigger = channel->trigger->next;
			free(temp);
		}
	}

	uv_timer_stop(&channel->channelTimer);

	return 0;
}

int start_trigger()
{
	QUEUE* q = NULL;
	CHANNEL * channel = NULL;
	int ret = 0;

	//开始当前trigger 后面的交给定时回调处理

	QUEUE_FOREACH(q,&CHANNEL_QUEUE)
	{
		channel = QUEUE_DATA(q, CHANNEL, channel_queue);
		{
			uv_timer_init(loop, &channel->channelTimer);
			deal_this_trigger(channel);
		}
	}

	return 0;
}

#ifdef CSS_TEST

#include <assert.h>

static uint64_t assert_schedule_interval = 0;
uint32_t get_today_sec_from_now_for_test()
{
	static uint32_t time_sec = 0;
	static uint32_t time_hor = 0;
	time_t nowtime;
	struct tm *begin_t;
	nowtime = time(NULL);
	begin_t = localtime(&nowtime);

	if (time_sec == 0) {
		time_sec = begin_t->tm_sec;
		//printf("1: %d\n",time_sec);
		return time_hor;
	} else {
		time_hor += (begin_t->tm_sec + 60 - time_sec) % 60;
		//printf("1: %d %d %d\n",begin_t->tm_sec,time_sec,time_hor);
		time_sec = begin_t->tm_sec;
		return (time_hor);
	}
}

int print_begin_Scheduler(css_record_info_t* record_info, css_sms_info_t* sms)
{
	time_t nowtime = time(NULL);
	struct tm *begin_t;
	begin_t = localtime(&nowtime);
	printf("nowtime:%s", asctime(begin_t));
	printf("jobing's channel info: ChannelId=%hu, schedule_interval=%lld\n",
			record_info->channelNo, record_info->timestamp.timestamp);
	assert_schedule_interval += record_info->timestamp.timestamp;
	return 0;
}

TRIGGER* css_test_trigger_mingle_to_list()
{
	TRIGGER* srcCh_listhead;
	TRIGGER* temp;

	//0-2
	srcCh_listhead = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(srcCh_listhead, 0, sizeof(TRIGGER));
	srcCh_listhead->startTime = 0;
	srcCh_listhead->schedule_interval = 2 * 3600;

	//18-21
	temp = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(temp, 0, sizeof(TRIGGER));
	temp->startTime = 18 * 3600;
	temp->schedule_interval = 3 * 3600;
	trigger_mingle_to_list(srcCh_listhead, temp);

	//6-9
	temp = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(temp, 0, sizeof(TRIGGER));
	temp->startTime = 6 * 3600;
	temp->schedule_interval = 3 * 3600;
	trigger_mingle_to_list(srcCh_listhead, temp);

	//1-10
	temp = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(temp, 0, sizeof(TRIGGER));
	temp->startTime = 1 * 3600;
	temp->schedule_interval = 9 * 3600;
	trigger_mingle_to_list(srcCh_listhead, temp);

	assert(
			srcCh_listhead->startTime == 0
			&& srcCh_listhead->schedule_interval == 10 * 3600);
	assert(
			srcCh_listhead->next->startTime == 18 * 3600
			&& srcCh_listhead->next->schedule_interval == 3 * 3600);

	return srcCh_listhead;
}

TRIGGER* css_test_trigger_mingle_two_list()
{
	TRIGGER* srcCh_listhead = css_test_trigger_mingle_to_list();
	TRIGGER* desCh_listhead;

	desCh_listhead = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(desCh_listhead, 0, sizeof(TRIGGER));
	desCh_listhead->startTime = 0;
	desCh_listhead->schedule_interval = 5 * 3600;

	desCh_listhead->next = (TRIGGER*) malloc(sizeof(TRIGGER));
	memset(desCh_listhead->next, 0, sizeof(TRIGGER));
	desCh_listhead->next->startTime = 11 * 3600;
	desCh_listhead->next->schedule_interval = 5 * 3600;

	trigger_mingle_two_list(srcCh_listhead, desCh_listhead);

	assert(
			srcCh_listhead->startTime == 0
			&& srcCh_listhead->schedule_interval == 10 * 3600);
	assert(
			srcCh_listhead->next->startTime == 11 * 3600
			&& srcCh_listhead->next->schedule_interval == 5 * 3600);
	assert(
			srcCh_listhead->next->next->startTime == 18 * 3600
			&& srcCh_listhead->next->next->schedule_interval
			== 3 * 3600);

	return srcCh_listhead;
}

int css_test_deal_this_trigger()
{
	TRIGGER* srcCh_listhead = css_test_trigger_mingle_two_list();
	CHANNEL *channel;
	TRIGGER* temp;
	int i = 1;
	int total_schedule_interval = 0;
	channel = (CHANNEL*) malloc(sizeof(CHANNEL));
	memset(channel, 0, sizeof(CHANNEL));
	channel->trigger = srcCh_listhead;
	//将时间除以3600缩短执行时间
	temp = channel->trigger;
	while (temp) {
		temp->startTime /= 3600;
		temp->schedule_interval /= 3600;
//		printf("NO.%d startTime=%d schedule_interval=%d\n", i, temp->startTime,
//				temp->schedule_interval);
		total_schedule_interval += temp->schedule_interval;
		temp = temp->next;
	}
	uv_timer_init(loop, &channel->channelTimer);
	deal_this_trigger(channel);

	return total_schedule_interval;
}
char *job_xml =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?><config>"
"<calendars>"
"<calendar id=\"17753\" name=\"每天\" class_name=\"THolidayCalendar\">"
"<dates><date/></dates>"
"<base_calendar class_name=\"TWeeklyCalendar\">"
"<day_of_week />"
"<base_calendar class_name=\"TMonthlyCalendar\">"
"<day_of_month />"
"</base_calendar>"
"</base_calendar>"
"</calendar>"
"<calendar id=\"29943942\" name=\"工作日\" class_name=\"THolidayCalendar\">"
"<dates><date /></dates>"
"<base_calendar class_name=\"TWeeklyCalendar\">"
"<day_of_week>6,7</day_of_week>"
"<base_calendar class_name=\"TMonthlyCalendar\">"
"<day_of_month />"
"</base_calendar>"
"</base_calendar>"
"</calendar>"
"<calendar id=\"18463868\" name=\"周末\" class_name=\"THolidayCalendar\">"
"<dates><date /></dates>"
"<base_calendar class_name=\"TWeeklyCalendar\">"
"<day_of_week>1-5</day_of_week>"
"<base_calendar class_name=\"TMonthlyCalendar\"><day_of_month /></base_calendar>"
"</base_calendar>"
"</calendar>"
"<calendar id=\"1408516910242\" name=\"hh\" class_name=\"THolidayCalendar\">"
"<dates>"
"<date>5:1,3</date>"
"<date>6:3-8</date>"
"</dates>"
"<base_calendar class_name=\"TWeeklyCalendar\"><day_of_week>7,1-2</day_of_week>"
"<base_calendar class_name=\"TMonthlyCalendar\">  <day_of_month>6-8,1,2</day_of_month>"
"</base_calendar>"
"</base_calendar>"
"</calendar>"
"<calendar id=\"1408517651512\" name=\"ss\" class_name=\"TSpecialdayCalendar\">"
"<date>2005-5-1</date>"
"<date>2014-8-21</date>"
"<base_calendar class_name=\"\"><day_of_week />"
"<base_calendar class_name=\"\"><day_of_month /></base_calendar>"
"</base_calendar>"
"</calendar>"
"</calendars>"
"<jobs>"
"<job id=\"1408516923820\" name=\"aa\" class_name=\"JCaptureVideoTask\">"
"<context>"
"<entry channelId=\"196611\" channelInfo=\"196611,131082,/一级机构/43/123456789hik1/\" />"
"<entry channelId=\"196610\" channelInfo=\"196610,131080,/一级机构/112/CH1/\" />"
"</context><triggers>"
"<trigger>"
"<calendar_name>工作日</calendar_name>"
"<schedule_interval>7200</schedule_interval>"
"<start_time>20:00:00</start_time>"
"<id>29943942</id>"
"</trigger>"
"<trigger>"
"<calendar_name>工作日</calendar_name>"
"<schedule_interval>50400</schedule_interval>"
"<start_time>05:00:00</start_time>"
"<id>29943942</id>"
"</trigger>"
"<trigger>"
"<calendar_name>hh</calendar_name>"
"<schedule_interval>39600</schedule_interval>"
"<start_time>08:00:00</start_time>"
"<id>1408516910242</id>"
"</trigger>"
"<trigger>"
"<calendar_name>hh</calendar_name>"
"<schedule_interval>61200</schedule_interval>"
"<start_time>00:00:00</start_time>"
"<id>1408516910242</id>"
"</trigger>"
"<trigger>"
"<calendar_name>hh</calendar_name>"
"<schedule_interval>18000</schedule_interval>"
"<start_time>04:00:00</start_time>"
"<id>1408516910242</id>"
"</trigger>"
"</triggers>"
"</job>"
"<job id=\"1408517708637\" name=\"bb\" class_name=\"JCaptureVideoTask\">"
"<context>"
"<entry channelId=\"196612\" channelInfo=\"196612,131082,/一级机构/43/123456789hik2/\" />"
"<entry channelId=\"196610\" channelInfo=\"196610,131080,/一级机构/112/CH1/\" />"
"</context><triggers>"
"<trigger>"
"<calendar_name>工作日</calendar_name>"
"<schedule_interval>7200</schedule_interval>"
"<start_time>20:00:00</start_time>"
"<id>29943942</id></trigger><trigger>"
"<calendar_name>工作日</calendar_name>"
"<schedule_interval>36000</schedule_interval>"
"<start_time>05:00:00</start_time>"
"<id>29943942</id>"
"</trigger>"
"<trigger>"
"<calendar_name>ss</calendar_name>"
"<schedule_interval>36000</schedule_interval>"
"<start_time>05:00:00</start_time>"
"<id>1408517651512</id>"
"</trigger>"
"<trigger>"
"<calendar_name>ss</calendar_name>"
"<schedule_interval>25200</schedule_interval>"
"<start_time>02:00:00</start_time>"
"<id>1408517651512</id>"
"</trigger>"
"</triggers>"
"</job>"
"</jobs>"
"<resultCode>0</resultCode>"
"</config>";

static CALENDAR assert_jobs[] = { {17753, HOLIDAY, 1, 0, 0, 0, 0}, {29943942,
		HOLIDAY, 1, 0, 0, 0, 0}, {18463868, HOLIDAY, 0, 0, 0, 0, 0}, {
		1408516910242, HOLIDAY, 0, 0, 0, 0, 0}, {1408517651512, SPECIALDAY, 0,
		0, 0, 0, 0}};

int css_job_compare_calendars(CALENDAR *f, CALENDAR *t)
{
	assert(f->id == t->id);
	assert(f->flag == t->flag);
	assert(f->needwork == t->needwork);
	return 0;
}

static CHANNEL assert_channels[] = { {196611, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {196610, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {196612, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

static TRIGGER assert_triggers_0_next = {0, 72000, 7200, 0};
static TRIGGER assert_triggers_1_next = {0, 72000, 7200, 0};
static TRIGGER assert_triggers_2_next = {0, 72000, 7200, 0};
static TRIGGER assert_triggers[] = { {0, 18000, 50400, 0}, {0, 18000, 50400,
		0}, {0, 18000, 36000, 0}};

int css_job_compare_trigger(TRIGGER *f, TRIGGER *t)
{
	assert(f->startTime = t->startTime);
	assert(f->schedule_interval = t->schedule_interval);
	if (f->next) {
		assert(0 == css_job_compare_trigger(f->next, t->next));
	}
	return 0;
}

int assert_all_calendars()
{
	QUEUE* q;
	CALENDAR * cal;
	CHANNEL * chan;
	int i = 1;
	TRIGGER *trg;
	QUEUE_FOREACH(q,&CALENDAR_QUEUE)
	{
		cal = QUEUE_DATA(q, CALENDAR, calendar_queue);
		assert(0 == css_job_compare_calendars(&assert_jobs[i - 1], cal));
		i++;
	}
	i = 1;
	// init test triggers
	assert_triggers[0].next = &assert_triggers_0_next;
	assert_triggers[1].next = &assert_triggers_1_next;
	assert_triggers[2].next = &assert_triggers_2_next;

	QUEUE_FOREACH(q,&CHANNEL_QUEUE)
	{
		chan = QUEUE_DATA(q, CHANNEL, channel_queue);
		assert(assert_channels[i - 1].Id == chan->Id);
		trg = chan->trigger;
		assert(0 == css_job_compare_trigger(&assert_triggers[i - 1], trg));
		i++;
	}
	printf("\n\n");
	return 0;
}

int test_css_job_xml_to_calendars()
{
	QUEUE_INIT(&CHANNEL_QUEUE);
	QUEUE_INIT(&CALENDAR_QUEUE);
	uv_mutex_init(&gmutex);
	deal_job_xml(job_xml);
	assert(0 == assert_all_calendars());
	return 0;
}

int test_css_job_run_triggers()
{
	int total;
	loop = uv_loop_new();
	uv_loop_init(loop);

	total = css_test_deal_this_trigger();
	uv_run(loop, UV_RUN_DEFAULT);
	assert(assert_schedule_interval == total*1000000);
	return 0;
}

int test_css_job_suite()
{
	uv_loop_t* lopp = uv_default_loop();
	css_load_ini_file(DEFAULT_CONFIG_FILE);
	css_get_config();
	//css_start_job(lopp);
	//uv_run(loop, UV_RUN_DEFAULT);
	test_css_job_xml_to_calendars();
	test_css_job_run_triggers();
	return 0;
}

#endif
