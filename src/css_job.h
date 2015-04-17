#ifndef _CSS_JOB_H_
#define _CSS_JOB_H_

#include "uv/uv.h"
#include "css_trigger.h"

typedef struct
{
	uint64_t Id;
	int ChannelNo;
	uint32_t DVRID;
	char dvrName[20];
	char dvrIp[20];
	int dvrPort;
	int ConfigPort;
	char LoginUser[20];
	char DeviceType[20];
	char Password[20];
	char dvrType[20];
	int ConnectionType;
	char smtServerName[40];
	int StreamMediaID;
	char StreamMediaIP[20];
	int StreamMediaPort;
	int backConnectionType;
	char backSMTSName[40];
	int backSMTSDwid;
	char backSMTSIP[20];
	int backSMTSPort;
	TRIGGER *trigger;
	uv_timer_t channelTimer;
	QUEUE channel_queue;
}CHANNEL;

// api
int css_start_job(uv_loop_t *loop_in);	// sucess return 0;already run return 1 if read ini false return -1
void css_end_job();
int css_restart_job();					// return value the same to css_start_job

#endif