#ifndef CSS_ALARM_H_
#define CSS_ALARM_H_

#include "uv/uv.h"
#include "css_trigger.h"


//s

typedef struct AlarmProtocol_t
{
	uint64_t				alarmSourceId_;			//报警源id  to equipi
	uint32_t				channelNo;				//报警源通道号
	char					time_[128];				//报警时间
	uint32_t				sendType_;				//报警发送模式 （0连续 1非连续）
	uint32_t				status_;				//状态（0开始 1持续 2结束）
	char					uuid_[128];				//报警事件id  to eventid

	char					alarmEventCode_[128];	//处理事件代号
	char					param_[128];			//处理事件详细参数
	uint64_t*				channelNoId;			//通道集
	uint32_t				channelNum;				//通道数
	uint32_t*				flag;					//各通道状态
	uint32_t*				channelNo_;				//通道号
	uint32_t*				DVRID;					//

	//////////////////////////////////////////////////////////////////////////
	char beginTime[128] ;
	char endTime[128] ;
	uint64_t				timestamp;						//录像时长

	uint64_t						*smsEquipId;
	struct sockaddr_in				*smsAddr;				//流媒体地址
	struct sockaddr_in				*backsmsAddr;
	uv_timer_t						lastTimer;				//持续时间定时

}AlarmProtocol_t;

int css_alarm_init(uv_loop_t *loop_in);		//sucess 0 parama err -2 config err -1 config warnint 1

void css_alarm_uninit();

int css_start_alarmtask_by_xml(char* alarmXml,int alarmlen,char* relationXml,int relation);


#endif
