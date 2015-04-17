#include <stdlib.h>
#include <time.h>
#include "css_alarm.h"
#include "uv/uv.h"
#include "mxml/mxml.h"
#include "css_capture.h"
#include "css_db.h"
#include "css_util.h"
#include "css_restclient.h"
#include "css_logger.h"
#include "css_ini_file.h"

int warningTimeout = 40;						//持续报警超时时间
int warningContinueCaptureInterval = 50;		//报警结束后持续时间

const int64_t MICROSECONDS_IN_SECOND = 1000000LL;
int HTTP_TIMEOUT = 5;
char* manage_ctr = "manager center";
char* alarm_config = "alarmCfg";
uv_loop_t *loop = NULL;

char *loginName = NULL;
char *password = NULL;
char *login_addr = NULL;
char *channel_addr = NULL;
char *type = NULL;
char *css_URL = NULL;
//static char Token[256] = { 0 };

static int format_alarm_xml(char* alarmXml, AlarmProtocol_t *tempAlaPro);
static uint64_t* format_relation_xml(char* relationXml,int relationLen,int* len);
int get_smsinfo(AlarmProtocol_t *Tempalarminfo);
static css_record_info_t alarm_info_to_recode_info(struct AlarmProtocol_t *alarminfo,int len);
static void begin_task(struct AlarmProtocol_t * alarminfo);
static void excute_beginfile_listener(uv_work_t* req);
static void update_file_endtime(uv_work_t* req);
static void afterInsert(uv_work_t* req, int status);
static int css_set_alarm_config();
static int split_str_to_int(char* srcstr,char*delim,uint64_t* buf,int len);
static int init_alarmprotocol(AlarmProtocol_t* alpro);
static int uninit_alarmprotocol(AlarmProtocol_t* alpro);

//warning list
//

typedef struct WarningLinkList
{
	AlarmProtocol_t node;
	struct WarningLinkList* next;
} WarningLinkList;

static WarningLinkList *H_warnList; //= new WarningLinkList;
static uv_mutex_t gmutex;

int css_alarm_init(uv_loop_t *loop_in)
{
	CL_INFO("css alarm init.\n");
	H_warnList = (WarningLinkList*) malloc(sizeof(WarningLinkList));
	H_warnList->next = NULL;
	if((loop = loop_in) == NULL)
		return -2;
	uv_mutex_init(&gmutex);
	return css_set_alarm_config();
}

void css_alarm_uninit()
{
	CL_INFO("css alarm uninit.\n");
	while (H_warnList->next) {
		WarningLinkList* p = H_warnList->next;
		H_warnList->next = H_warnList->next->next;
		uv_close((uv_handle_t*) &p->node.lastTimer, NULL);
		free(p);
	}
	uv_mutex_destroy(&gmutex);
	if(css_URL)
	{
		free(css_URL);
		css_URL = NULL;
		free(loginName);
		loginName = NULL;
		free(password);
		password = NULL;
		free(login_addr);
		login_addr = NULL;
		free(channel_addr);
		channel_addr = NULL;
		free(login_addr);
		login_addr = NULL;
	}

}

AlarmProtocol_t* insert2warnList(WarningLinkList* plist,
		AlarmProtocol_t warningNode)
{
	struct WarningLinkList* p;
	uv_mutex_lock(&gmutex);
	p = plist;
	while (p->next)
		p = p->next;
	p->next = (WarningLinkList*) malloc(sizeof(WarningLinkList));
	p->next->node = warningNode;
	p->next->next = NULL;
	uv_mutex_unlock(&gmutex);
	return &p->next->node;

}

void deleteWarnByEventID(WarningLinkList* plist, char *alarmeventid)
{
	struct WarningLinkList* p;
	uv_mutex_lock(&gmutex);
	p = plist;
	while (p->next) {
		if (strcmp(p->next->node.uuid_, alarmeventid))
			p = p->next;
		else {
			WarningLinkList *tempN = p->next;
			p->next = p->next->next;
			free(tempN);
			break;
		}
	}
	uv_mutex_unlock(&gmutex);
}

AlarmProtocol_t* get_warm_by_eventid(WarningLinkList* plist, char *alarmeventid)
{
	struct WarningLinkList* p;
	uv_mutex_lock(&gmutex);
	p = plist->next;
	while (p) {
		if (strcmp(p->node.uuid_, alarmeventid))
			p = p->next;
		else {
			uv_mutex_unlock(&gmutex);
			return &(p->node);
		}
	}
	uv_mutex_unlock(&gmutex);
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

static mxml_type_t type_cb1(mxml_node_t* node)
{
	if (!strcmp(node->value.opaque, "time") || !strcmp(node->value.opaque, "param"))
		return (MXML_OPAQUE);
	return (MXML_TEXT);
}

void mxml_error_cb(const char * err)
{
//	printf(err);
	CL_ERROR("fomat xml error:%s\n", err);
}

int format_alarm_xml(char* alarmXml, AlarmProtocol_t *tempAlaPro)
{
	//AlarmProtocol_t tempAlaPro;
	mxml_node_t *tree, *alarmSourceId, *channelNo, *time, *sendType, *status,
			*uuid;

	if (!alarmXml || !tempAlaPro) {
		CL_ERROR("format alarm xml error.\n");
		return -1;
	}

	mxmlSetErrorCallback(mxml_error_cb);

	tree = mxmlLoadString(NULL, alarmXml, type_cb1);
	if (!tree) {
		CL_ERROR("load alarm xml error.\n");
		return -2;
	}
	alarmSourceId = mxmlFindElement(tree, tree, "alarmSourceId", NULL, NULL,
	MXML_DESCEND);
	if (alarmSourceId->child)
		tempAlaPro->alarmSourceId_ = ATOLL(
				alarmSourceId->child->value.text.string);

	channelNo = mxmlFindElement(tree, tree, "channelNo", NULL, NULL,
	MXML_DESCEND);
	if (channelNo->child)
		tempAlaPro->channelNo = atoi(channelNo->child->value.text.string);

	time = mxmlFindElement(tree, tree, "time", NULL, NULL, MXML_DESCEND);
	if (time->child)
		strcpy(tempAlaPro->time_, time->child->value.opaque);

	sendType = mxmlFindElement(tree, tree, "sendType", NULL, NULL,
	MXML_DESCEND);
	if (sendType->child)
		tempAlaPro->sendType_ = atoi(sendType->child->value.text.string);

	status = mxmlFindElement(tree, tree, "status", NULL, NULL, MXML_DESCEND);
	if (status->child)
		tempAlaPro->status_ = atoi(status->child->value.text.string);

	uuid = mxmlFindElement(tree, tree, "uuid", NULL, NULL, MXML_DESCEND);
	if (uuid->child)
		strcpy(tempAlaPro->uuid_, uuid->child->value.text.string);

	mxmlDelete(tree);

	//CL_INFO("alarminfo: uuid:%s\nchannelNO:%d\ntime:%s\nsendtype:%d\nstatus:%d\n",tempAlaPro->uuid_,tempAlaPro->channelNo,tempAlaPro->time_,tempAlaPro->sendType_,tempAlaPro->status_);

	return 0;
}

uint64_t* format_relation_xml(char* relationXml,int relationLen, int* len)
{
	uint64_t* channelNoId = NULL;
	char* tempbuf;
	int i = 0;

	mxml_node_t *tree, *alarmEvent, *param;
	* len = 0;
	tempbuf = (char*)malloc(relationLen);
	memset(tempbuf,0,relationLen);
	if (!relationXml) {
		CL_ERROR("format relationXml error.\n");
		return NULL;
	}
	mxmlSetErrorCallback(mxml_error_cb);

	tree = mxmlLoadString(NULL, relationXml, type_cb1);
	if (!tree) {
		CL_ERROR("load alarm xml error.\n");
		return NULL;
	}

	alarmEvent = mxmlFindElement(tree, tree, "alarmEvent", NULL, NULL,MXML_DESCEND);
	while(alarmEvent)
	{
		if(!strcmp(alarmEvent->value.element.attrs[0].value,"07"))
		{
			param = mxmlFindElement(alarmEvent, tree, "param", NULL, NULL,MXML_DESCEND);
			if(param)
			{
				strcat(tempbuf,param->child->value.opaque);
			}
		}
		alarmEvent = mxmlFindElement(alarmEvent, tree, "alarmEvent", NULL, NULL,MXML_NO_DESCEND);
	}
	while(tempbuf[i])
	{
		if(tempbuf[i] == ',')
			++(*len);
		++i;
	}
	++(*len);
	channelNoId = (uint64_t*)malloc(*len*sizeof(uint64_t));
	split_str_to_int(tempbuf,",",channelNoId,*len);
	free(tempbuf);
	return channelNoId;
}

static int get_token(char* token)
{
	mxml_node_t *tree, *token_node, *result_node;
	int result;
	char body[RESPONSE_BUFF_LEN] = { 0 };
	char url[256] = { 0 };
	//memset(Token,0,sizeof(Token));
	//http://10.3.0.224:8080/sc/login?_type=ws&loginName=sysadmin&password=12345
	char http_format[] = "%s%s?_type=%s&loginName=%s&password=%s";
	sprintf(url, http_format, css_URL, login_addr, type, loginName, password);

	if ((result = get_rest(url, body, NULL, HTTP_TIMEOUT)) != 200) {
		CL_ERROR("load token:%s error:%d\n", url, result);
		return -2;

	}

	tree = mxmlLoadString(NULL, body, type_cb1);
	if (!tree) {
		CL_ERROR("load token xml error.\n");
		return -1;
	}
	result_node = mxmlFindElement(tree, tree, "ErrorCode", NULL, NULL,
	MXML_DESCEND);
	if (result_node) {
		if (result_node->child)
			if ((result = atoi(result_node->child->value.text.string))) {
				CL_ERROR("load token result:%d\n", result);
				return result;
			}
	}

	token_node = mxmlFindElement(tree, tree, "Token", NULL, NULL, MXML_DESCEND);
	if (token_node) {
		if (token_node->child)
			strcpy(token, token_node->child->value.text.string);
	} else {
		CL_ERROR("token xml error:no token node.\n");
		mxmlDelete(tree);
		return -1;
	}
	mxmlDelete(tree);
	return 0;
}

//http://{$host}:{$port}/sc/channel?_type=ws&token={$token}&channelId={$channelId}&equipId={$equipId}&channelNo={$channelNo}&channelSerialId={$channelSerialId}
int get_smsinfo(AlarmProtocol_t *Tempalarminfo)
{
	int ret = 0;
	char url[256] = { 0 };
	char body[RESPONSE_BUFF_LEN] = { 0 };
	char token[256] = { 0 };
	char http_format[] = "%s%s?_type=%s&channelId=%lld&token=%s";
	int temp_i = Tempalarminfo->channelNum;

	mxml_node_t *tree, *result_node;
	mxml_node_t *backSMTSIP, *backSMTSPort;
	mxml_node_t *StreamMediaIP, *StreamMediaPort, *ChannelNo,*DVRID;

	if (0 != (ret = get_token(token))) {
		CL_ERROR("load token error:%d.\n", ret);
		return -1;
	}
	while(-1 < --temp_i)
	{
		//http://10.3.0.224:8080/sc/channel?_type=ws&channelId=589834&token=sc47@5@10.3.0.162:1405057485937-10.3.0.224--15326665284029583095674386736650392646-33935239872011307088728051780234754770-685754
		sprintf(url,http_format,css_URL,channel_addr,type,Tempalarminfo->channelNoId[temp_i],token);

		if (200 != (ret = get_rest(url, body, NULL, HTTP_TIMEOUT))) {
			CL_ERROR("load smtsinfo:%s error:%d\n", url, ret);
			Tempalarminfo->flag[temp_i] = 0;
			continue;
		}

		tree = mxmlLoadString(NULL, body, type_cb1);
		if (!tree) {
			CL_ERROR("load smtsinfo xml error.\n");
			Tempalarminfo->flag[temp_i] = 0;
			continue;
		}
		result_node = mxmlFindElement(tree, tree, "ErrorCode", NULL, NULL,
			MXML_DESCEND);
		if (result_node && result_node->child) {
			if ((ret = atoi(result_node->child->value.text.string))) {
				CL_ERROR("load smtsinfo error channelId:%lld errorid:%d.\n",Tempalarminfo->channelNoId[temp_i], ret);
				Tempalarminfo->flag[temp_i] = 0;
				continue;
			}
		}

		Tempalarminfo->smsAddr[temp_i].sin_family = Tempalarminfo->backsmsAddr[temp_i].sin_family = AF_INET;
		//smsAddr->sin_family = backsmsAddr->sin_family = AF_INET;

		ChannelNo = mxmlFindElement(tree, tree, "ChannelNo", NULL, NULL,MXML_DESCEND);
		if (ChannelNo && ChannelNo->child)
			Tempalarminfo->channelNo_[temp_i] = atoi(ChannelNo->child->value.text.string);

		DVRID = mxmlFindElement(tree, tree, "DVRID", NULL, NULL, MXML_DESCEND);
		if (DVRID && DVRID->child)
			Tempalarminfo->DVRID[temp_i] = atoi(DVRID->child->value.text.string);

		StreamMediaIP = mxmlFindElement(tree, tree, "StreamMediaIP", NULL, NULL,MXML_DESCEND);
		if (StreamMediaIP && StreamMediaIP->child)
			Tempalarminfo->smsAddr[temp_i].sin_addr.s_addr = inet_addr(
			StreamMediaIP->child->value.text.string);

		StreamMediaPort = mxmlFindElement(tree, tree, "StreamMediaPort", NULL, NULL,MXML_DESCEND);
		if (StreamMediaPort && StreamMediaPort->child)
			Tempalarminfo->smsAddr[temp_i].sin_port = htons(atoi(StreamMediaPort->child->value.text.string));

		backSMTSIP = mxmlFindElement(tree, tree, "backSMTSIP", NULL, NULL,
			MXML_DESCEND);
		if (backSMTSIP && backSMTSIP->child)
			Tempalarminfo->backsmsAddr[temp_i].sin_addr.s_addr = inet_addr(
			backSMTSIP->child->value.text.string);

		backSMTSPort = mxmlFindElement(tree, tree, "backSMTSPort", NULL, NULL,
			MXML_DESCEND);
		if (backSMTSPort && backSMTSPort->child)
			Tempalarminfo->backsmsAddr[temp_i].sin_port = htons(atoi(backSMTSPort->child->value.text.string));

		Tempalarminfo->flag[temp_i] = 1;
		mxmlDelete(tree);
	}

	return 0;
}
css_record_info_t alarm_info_to_recode_info(struct AlarmProtocol_t *alarminfo,int num)
{
	css_record_info_t record_info;
	record_info.channelNo = alarminfo->channelNo_[num];
	record_info.dvrEquipId = alarminfo->DVRID[num];
	record_info.flowType = FLOW_TYPE_PRIMARY;							//fyy
	//strcpy(record_info.timestamp.eventId, alarminfo->uuid_);			//fyy
	sprintf(record_info.timestamp.eventId, "%s", alarminfo->uuid_);
	record_info.timestamp.timestamp = alarminfo->timestamp;
	record_info.timestamp.type = 0;
	return record_info;
}

void begin_task(struct AlarmProtocol_t * alarminfo)
{
	//css_record_info_t temp;
	css_record_info_t record_info;
	css_sms_info_t sms;

	int len = alarminfo->channelNum;
	CL_INFO("alarm: uuid:%s channelno:%d status:%d\n",alarminfo->uuid_,alarminfo->channelNo,alarminfo->status_);
	while(-1 < --len)
	{
		if(!alarminfo->flag[len])
			continue;

		record_info = alarm_info_to_recode_info(alarminfo,len);

		if (alarminfo->status_ == 0 || alarminfo->status_ == 1)	//in warning			//
		{
			record_info.timestamp.timestamp = warningTimeout * MICROSECONDS_IN_SECOND;
		} else {

			record_info.timestamp.timestamp = warningContinueCaptureInterval * MICROSECONDS_IN_SECOND;
		}

		sms.smsAddr = alarminfo->smsAddr[len];
		//sms.smsEquipId = alarminfo->alarmSourceId_;

		capture_start_record(&record_info, &sms);			//fyy
	}
}

//database operate proc
void excute_beginfile_listener(uv_work_t* req)
{
	int ret = 0;
	struct timeval tp;
	AlarmProtocol_t *Tempalarminfo = (AlarmProtocol_t *) req->data;
	tbl_warning myWarning = { 0 };
	strcpy(myWarning.alarmeventid, Tempalarminfo->uuid_);
	myWarning.dvrEquipId = Tempalarminfo->alarmSourceId_;
	myWarning.channelNo = Tempalarminfo->channelNo;
	gettimeofday(&tp, NULL);
	timeval_to_svtime_string(&tp, myWarning.beginTime, 24);
	tp.tv_sec += (long)((AlarmProtocol_t *) req->data)->timestamp;//TODO: timestamp sec?
	timeval_to_svtime_string(&tp, myWarning.endTime, 24);

	if((ret = css_db_insert_warning(&myWarning)))
	{
		CL_ERROR("insert db error%d\n",ret);
		req->data = NULL;
	}

	//get channel info
	if(get_smsinfo(Tempalarminfo) != 0)
	{
		req->data = NULL;
	}

}

void update_file_endtime(uv_work_t* req)
{
	struct timeval tp;
	AlarmProtocol_t *Tempalarminfo = (AlarmProtocol_t *) req->data;
	tbl_warning myWarning = { 0 };
	strcpy(myWarning.alarmeventid, Tempalarminfo->uuid_);

	gettimeofday(&tp, NULL);
	tp.tv_sec += (long)((AlarmProtocol_t *) req->data)->timestamp;//TODO: timestamp sec?
	timeval_to_svtime_string(&tp, myWarning.endTime, 24);
	css_db_update_warning(&myWarning);
}

//
void timerout_cb(uv_timer_t* handle)
{
	uninit_alarmprotocol((AlarmProtocol_t *) handle->data);
	//uv_close((uv_handle_t*)handle,NULL);
	deleteWarnByEventID(H_warnList, ((AlarmProtocol_t *) handle->data)->uuid_);
}

void afterInsert(uv_work_t* req, int status)
{
	AlarmProtocol_t* temp_alarm_pro;

	if (NULL == req->data) {
		free(req);
		return;
	}

	if (status == 0) {
		temp_alarm_pro = (AlarmProtocol_t *)req->data;
		begin_task(temp_alarm_pro);

		temp_alarm_pro->lastTimer.data = temp_alarm_pro;

		if (temp_alarm_pro->status_ == 2) {
			uv_timer_start(&temp_alarm_pro->lastTimer,
				timerout_cb, warningContinueCaptureInterval * 1000, 0);
		} else {
			uv_timer_start(&temp_alarm_pro->lastTimer,
				timerout_cb, warningTimeout * 1000, 0);
		}
	} else {
		CL_ERROR("start alarm job error:%s\n", uv_strerror(status));
	}

	free(req);
}

int css_start_alarmtask_by_xml(char* alarmXml,int alarmlen,char* relationXml,int relationlen)
{
	uv_work_t *work;
	AlarmProtocol_t alarminfo;
	AlarmProtocol_t * temp_alarminfo;

	if (!alarmXml)
		return -1;

	if (format_alarm_xml(alarmXml, &alarminfo))
		return -2;

	temp_alarminfo = get_warm_by_eventid(H_warnList, alarminfo.uuid_);
	if (temp_alarminfo == NULL) {			//haven't this id
		if(alarminfo.status_ != 0)			//not begin status
			return -3;

		temp_alarminfo = insert2warnList(H_warnList, alarminfo);
		uv_timer_init(loop, &temp_alarminfo->lastTimer);
		///////
		temp_alarminfo->channelNoId = format_relation_xml(relationXml,relationlen,(int*)&temp_alarminfo->channelNum);
		init_alarmprotocol(temp_alarminfo);
	} else if (alarminfo.status_ == 0) {			//
		return 1;
	} else
		temp_alarminfo->status_ = alarminfo.status_;
	
	work = (uv_work_t*) malloc(sizeof(uv_work_t));
	work->data = temp_alarminfo;

	switch (temp_alarminfo->status_) {
	case 0:						//开始
		CL_DEBUG("start alarm uuid:%s.\n", temp_alarminfo->uuid_);
		temp_alarminfo->timestamp = warningTimeout;
		uv_queue_work(loop, work, excute_beginfile_listener,
				afterInsert);
		break;
	case 1:						//持续
		temp_alarminfo->timestamp = warningTimeout;
		uv_queue_work(loop, work, update_file_endtime,
				afterInsert);
		break;
	case 2:						//结束
		CL_DEBUG("end of alarm uuid:%s.\n", temp_alarminfo->uuid_);
		temp_alarminfo->timestamp = warningContinueCaptureInterval;
		uv_queue_work(loop, work, update_file_endtime,
				afterInsert);
		break;
	default:
		break;
	}
	return 0;
}

int css_set_alarm_config()
{

	char *httpTimeOut,*wTimeout,*wContinueCaptureInterval;
	int ret = 0;
	if(css_URL == NULL)
	{
		ret = css_get_env(manage_ctr,"wsUriPref","",&css_URL);
		if(ret != 0)
			return -1;
		ret = css_get_env(manage_ctr,"login_addr","login",&login_addr);
		css_get_env(manage_ctr,"channel_addr","channel",&channel_addr);
		css_get_env(manage_ctr,"type","ws",&type);
		css_get_env(manage_ctr,"loginName","sysadmin",&loginName);
		css_get_env(manage_ctr,"password","12345",&password);
		css_get_env(manage_ctr,"httpTimeOut","5",&httpTimeOut);
		HTTP_TIMEOUT = atoi(httpTimeOut);
		free(httpTimeOut);

		css_get_env(alarm_config,"warningTimeout","40",&wTimeout);
		css_get_env(alarm_config,"warningContinueCaptureInterval","50",&wContinueCaptureInterval);
		warningTimeout = atoi(wTimeout);
		free(wTimeout);
		warningContinueCaptureInterval = atoi(wContinueCaptureInterval);
		free(wContinueCaptureInterval);
	}

	return ret;
}

int split_str_to_int(char* srcstr,char*delim,uint64_t* buf,int len)
{
	char * result = NULL;
	result = strtok(srcstr,delim);
	while(result != NULL)
	{
		if(--len < 0)
			return -1;

		*buf = atoi(result);
		++buf;
		result = strtok(NULL,delim);
	}
	return 0;
}

int init_alarmprotocol(AlarmProtocol_t* alpro)
{
	alpro->flag = (uint32_t*)malloc(sizeof(uint32_t)*alpro->channelNum);
	memset(alpro->flag,0,sizeof(int)*alpro->channelNum);
	alpro->smsAddr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in)*alpro->channelNum);
	memset(alpro->smsAddr,0,sizeof(struct sockaddr_in)*alpro->channelNum);
	alpro->backsmsAddr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in)*alpro->channelNum);
	memset(alpro->backsmsAddr,0,sizeof(struct sockaddr_in)*alpro->channelNum);
	alpro->channelNo_ = (uint32_t*)malloc(sizeof(uint32_t)*alpro->channelNum);
	memset(alpro->channelNo_,0,sizeof(int)*alpro->channelNum);
	alpro->DVRID = (uint32_t*)malloc(sizeof(uint32_t)*alpro->channelNum);
	memset(alpro->DVRID,0,sizeof(uint32_t)*alpro->channelNum);
	return 0;
}

int uninit_alarmprotocol(AlarmProtocol_t* alpro)
{
	free(alpro->channelNoId);
	free(alpro->flag);
	free(alpro->smsAddr);
	free(alpro->backsmsAddr);
	free(alpro->channelNo_);
	free(alpro->DVRID);

	return 0;
}


#ifdef CSS_TEST
#include <assert.h>
static uv_loop_t* temp_loop;
char test_xml[] =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>	\
<alarm>										\
	<alarmSourceId>123456<alarmSourceId>			\
	<channelNo>123<channelNo>					\
	<time>12:00:00<time>							\
	<sendType><sendType>					\
	<status><status>						\
	<uuid><uuid>							\
</alarm> ";

char* alarm_info = "<?xml version='1.0' encoding='utf-8' standalone='yes'?>"
"<AlarmInfo ParserVer='1.1'>"
"<alarmSourceId>589825</alarmSourceId>"
"<alarmSourceIp>192.168.1.39</alarmSourceIp>"
"<alarmSourceName>c1</alarmSourceName>"
"<alarmTypeCode>1</alarmTypeCode>"
"<alarmTypeName>ydzc</alarmTypeName>"
"<channelNo>2</channelNo>"
"<alarmNo>-1</alarmNo>"
"<time>2009-05-15 17:33:02.817</time>"
"<sendType>3</sendType>"
"<status>0</status>"
"<uuid>21d440d3-c608-4702-80c0-c172ebb5df20</uuid>"
"</AlarmInfo>";

void css_test_format_alarm_xml()
{
	AlarmProtocol_t p;
	assert(0 == format_alarm_xml(alarm_info, &p));
	assert(p.alarmSourceId_ == 589825);
	assert(p.channelNo = 2);
	assert(p.sendType_ = 3);
	assert(strcmp(p.time_, "2009-05-15 17:33:02.817") == 0);
	assert(p.status_ == 0);
	assert(strcmp(p.uuid_, "21d440d3-c608-4702-80c0-c172ebb5df20") == 0);

}

void css_test_alarm_normal()
{
	css_alarm_init(temp_loop);

}

void css_test_delete_warn_by_eventid()
{
	AlarmProtocol_t p = {1, 2, "time", 4, 5, "uuid", "eventcode", "param",0,0,0,0,0,
		"st", "et", 6, 0, 0, 0, 0};
	css_alarm_init(temp_loop);
	assert(0 == uv_timer_init(uv_default_loop(), &p.lastTimer));
	insert2warnList(H_warnList, p);
	deleteWarnByEventID(H_warnList, "euuid");
	assert(H_warnList->next != NULL);
	deleteWarnByEventID(H_warnList, "uuid");
	assert(H_warnList->next == NULL);
	css_alarm_uninit();
}

void css_test_get_warm_by_eventid()
{
	AlarmProtocol_t p = {1, 2, "time", 4, 5, "uuid", "eventcode", "param",0,0,0,0,0,
		"st", "et", 6, 0, 0, 0, 0};
	css_alarm_init(temp_loop);
	assert(0 == uv_timer_init(uv_default_loop(), &p.lastTimer));
	insert2warnList(H_warnList, p);
	assert(get_warm_by_eventid(H_warnList,"uuid") != NULL);
	assert(get_warm_by_eventid(H_warnList,"euuid") == NULL);
	css_alarm_uninit();
}
void css_test_insert_wlist()
{
	AlarmProtocol_t node;
	AlarmProtocol_t p = {1, 2, "time", 4, 5, "uuid", "eventcode", "param",0,0,0,0,0,
		"st", "et", 6, 0, 0, 0, 0};
	css_alarm_init(temp_loop);
	assert(0 == uv_timer_init(uv_default_loop(), &p.lastTimer));
	insert2warnList(H_warnList, p);
	assert(H_warnList->next != NULL);
	node = H_warnList->next->node;
	assert(node.alarmSourceId_ == 1);
	assert(node.channelNo == 2);
	assert(strcmp(node.time_, "time") == 0);
	assert(node.sendType_ == 4);
	assert(node.status_ == 5);
	assert(strcmp(node.uuid_, "uuid") == 0);
	assert(strcmp(node.beginTime, "st") == 0);
	assert(strcmp(node.endTime, "et") == 0);
	css_alarm_uninit();
}
void css_test_warnings_list()
{
	css_test_insert_wlist();
	css_test_get_warm_by_eventid();
	css_test_delete_warn_by_eventid();
}

void css_test_warning_get_token()
{
	char token[256];
	css_set_alarm_config();
	assert(0 == get_token(token));
	assert(strlen(token) == 128);
}

void test_css_alarm_suite()
{
	int ret = 0;
	uv_loop_t *loop = uv_loop_new();
	temp_loop = loop;
	uv_loop_init(loop);
	css_load_ini_file(DEFAULT_CONFIG_FILE);
	ret = css_alarm_init(temp_loop);
	css_test_format_alarm_xml();
	css_test_warnings_list();
	css_test_warning_get_token();
	css_test_alarm_normal();

	uv_run(loop,UV_RUN_DEFAULT);
}

#endif
