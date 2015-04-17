/**
auth:			fyy
date:			2014-8-5 14:50:00
describe:		trigger by calendar
detail:			just for css. it's unstd.
*/

#ifndef _CSS_TRIGGER_H__
#define _CSS_TRIGGER_H__

#include "uv/uv.h"
#include "css_util.h"
#include "queue.h"

typedef enum
{
	UNKNOW = 0,
	HOLIDAY,
	SPECIALDAY
}CALENAR_TYPE;

typedef struct
{
	uint64_t id;
	CALENAR_TYPE flag;
	int			needwork;
	uint32_t	month_day[12][31];						//将年日历初始化到该数组
	uint32_t	day_in_week[7];
	uint32_t	day_in_month[31];
	QUEUE		calendar_queue;
}CALENDAR;

typedef struct TRIGGER
{
	CALENDAR *calendar;
	uint32_t startTime;						//开始时间以秒为单位
	uint32_t schedule_interval;
	struct TRIGGER  *next;
}TRIGGER;

/**步骤
1.format_data
2.today_need_work
3.calculate_timeDsc
*/
//在此日历中今天是否需要工作 返回1需要，0不需要
int  today_need_work(CALENDAR* cal);

int css_format_year_date(char* formatStr,uint32_t (*month_day)[31]);

int css_format_month_date(char* formatStr,uint32_t *day_in_month,int len);

int css_format_week_date(char* formatStr,uint32_t *day_in_week,int len);

int css_format_specialday(char* formatStr,uint32_t (*month_day)[31]);

int calculate_timeDsc(TRIGGER *trigger);			//计算开始时间和当前时间的差值

int css_format_start_time(uint32_t *startime, char* formatStr);
 
#endif