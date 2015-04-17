#include "css_trigger.h"

char* trim_space(char*strsrc)
{
	char *strtemp = strsrc;
	if (!strsrc)
		return NULL;

	while (*strtemp != 0) {
		if (*strtemp == 32){
			memcpy(strtemp, strtemp + 1, strlen(strtemp + 1) + 1);
		} else
			++strtemp;
	}

	return strsrc;
}

int calculate_timeDsc(TRIGGER *trigger)
{
	struct tm *nowtime;
	time_t long_time;
	uint32_t dscTime;

	time(&long_time);
	nowtime = localtime(&long_time);

	dscTime = trigger->startTime - nowtime->tm_hour * 3600
			- nowtime->tm_min * 60 - nowtime->tm_sec;

	return dscTime;
}

int today_need_work(CALENDAR* cal)
{
	time_t nowtime;
	struct tm *begin_t;   // Error. tm
	nowtime = time(NULL);
	begin_t = localtime(&nowtime);

	if (cal->flag == HOLIDAY) {
		if ((cal->month_day[begin_t->tm_mon][begin_t->tm_mday-1] == 1)
				|| (cal->day_in_month[begin_t->tm_mday-1] == 1)
				|| (cal->day_in_week[begin_t->tm_wday] == 1))
			return 0;
		else
			return 1;
	} else if (cal->flag == SPECIALDAY) {
		if (cal->month_day[begin_t->tm_mon][begin_t->tm_mday-1] == 1)
			return 1;
	} else {			//TODO:
	}
	return 0;
}

#define JUDE_SET(DATA,SET)		\
{								\
	if(DATA<10)					\
		SET = 1;				\
	else						\
		SET = 2;				\
}

int css_format_year_date(char* formatStr, uint32_t (*month_day)[31])
{			//5:1,3;7:1,2,10-20
	int i_month;
	char* strtemp = formatStr;
	char* strtemp1;
	int setcount = 0;

	if (!formatStr || !month_day)
		return -1;

	trim_space(strtemp);

	do {
		i_month = atoi(strtemp);
		if(i_month == 0)
			return -1;

		JUDE_SET(i_month, setcount)

		strtemp += (setcount + 1);		

		if((strtemp1 = strstr(strtemp, ";")))
			*strtemp1 = '\0';

		css_format_month_date(strtemp, month_day[i_month-1], 31);

		strtemp = strtemp1 + 1;

	} while (strtemp1);

	return 0;
}

int css_format_month_date(char* formatStr, uint32_t *day_in_month, int len)
{		//6,8,11-20
	int i_temp1, i_temp2;
	int i_loop;
	char* strtemp = formatStr;
	int setcount = 0;

	if (!formatStr || !day_in_month || len != 31)
		return -1;

	trim_space(strtemp);			//TODO:  not necessary

	while (*strtemp) {
		i_temp1 = atoi(strtemp);
		if (i_temp1)								
		{
			JUDE_SET(i_temp1, setcount)
			{
				if (*(strtemp + setcount) == ',') {
					day_in_month[i_temp1-1] = 1;
					strtemp += setcount;
					continue;
				} else if (*(strtemp + setcount) == '-') {
					strtemp += (setcount + 1);
					i_temp2 = atoi(strtemp);
					for (i_loop = i_temp1; i_loop < (i_temp2 + 1); ++i_loop) {
						day_in_month[i_loop-1] = 1;
					}
					JUDE_SET(i_temp2, setcount)

					strtemp += setcount;
					continue;
				} else {
					day_in_month[i_temp1-1] = 1;
					strtemp += setcount;
				}
			}
		} else
			++strtemp;
	}
	return 0;
}

int css_format_week_date(char* formatStr, uint32_t *day_in_week, int len)
{						//6,7,1-3
	int i_temp1, i_temp2;
	int i_loop;
	char* strtemp = formatStr;
	int setcount = 0;

	if (!formatStr || !day_in_week || len != 7)
		return -1;

	trim_space(strtemp);

	while (*strtemp) {
		i_temp1 = atoi(strtemp);
		if (i_temp1)								
		{
			JUDE_SET(i_temp1, setcount)
			{
				if (*(strtemp + setcount) == ',') {
					if (i_temp1 == 7)			
						i_temp1 = 0;

					day_in_week[i_temp1] = 1;
					strtemp += setcount;
					continue;
				} else if (*(strtemp + setcount) == '-') {
					strtemp += (setcount + 1);
					i_temp2 = atoi(strtemp);
					for (i_loop = i_temp1; i_loop < (i_temp2 + 1); ++i_loop) {
						if (i_loop == 7)
							day_in_week[0] = 1;

						day_in_week[i_loop] = 1;
					}
					JUDE_SET(i_temp2, setcount)

					strtemp += setcount;
					continue;
				} else {
					if (i_temp1 == 7)			
						i_temp1 = 0;
					day_in_week[i_temp1] = 1;
					++strtemp;
				}
			}
		} else
			++strtemp;
	}
	return 0;
}

/**
 * format day
 * format_str like "yyyy-MM-dd;yyyy-MM-dd"
 */
int css_format_specialday(char* formatStr, uint32_t (*month_day)[31])
{
	int i_year, i_month, i_day;
	char* strtemp = formatStr;
	char* strtemp1 = NULL;

	if (!formatStr || !month_day)
		return -1;

	trim_space(strtemp);

	do {
		if((strtemp1 = strstr(strtemp, ",")))
		{
			*strtemp1 = '\0';
			strtemp1 += 1;
		}

		sscanf(strtemp, "%d-%d-%d", &i_year, &i_month, &i_day);
		{
			time_t nowtime;
			struct tm *begin_t;
			nowtime = time(NULL);
			begin_t = localtime(&nowtime);
			if ((begin_t->tm_year+1900) == i_year) {
				month_day[i_month-1][i_day-1] = 1;
			}
		}
		strtemp = strtemp1;
	} while (strtemp);

	return 0;
}

/**
 * format hour min and second
 * format_str  like 'hh:mm:dd'
 */
int css_format_start_time(uint32_t *startime, char* formatStr)
{
	uint32_t i_hour, i_min, i_sec;
	sscanf(formatStr, "%d:%d:%d", &i_hour, &i_min, &i_sec);
	*startime = i_hour * 3600 + i_min * 60 + i_sec;
	return 0;
}

#ifdef CSS_TEST

#include <assert.h>

void test_trim_space()
{
	char str[100];
	sprintf(str, " a bc ");
	trim_space(str);
	assert(strcmp(str, "abc") == 0);

}

void test_css_format_year_date()
{
	//5:1,3;7:1,2,10-20
	uint32_t month_day[12][31] = { 0 };
	char day_of_year[50];
	int i;
	sprintf(day_of_year, "5:1,3;7:1,2,10-20,25;6:2");
	assert(0 == css_format_year_date(day_of_year, month_day));
	assert(month_day[4][0] == 1);
	assert(month_day[4][2] == 1);
	assert(month_day[6][0] == 1);
	assert(month_day[6][1] == 1);
	for (i = 9; i < 20; i++) {
		assert(month_day[6][i] == 1);
	}
	assert(month_day[6][24] == 1);
	assert(month_day[5][1] == 1);
}

void test_css_format_month_date()
{
	//6,8,11-20
	char day_of_month[50];
	uint32_t month_day[31] = { 0 };
	int i;
	sprintf(day_of_month, "6,8,11-20");
	assert(0 == css_format_month_date(day_of_month, month_day, 31));
	assert(month_day[5] == 1);
	assert(month_day[7] == 1);
	for (i = 10; i < 20; i++) {
		assert(month_day[i] == 1);
	}
}

void test_css_format_week_date()
{
	//6,7,1-3
	uint32_t weeks[7] = { 0 };
	char week[30];
	sprintf(week, "%d,%s,%d", 6, "1-3", 7);
	assert(
			0
					== css_format_week_date(week, weeks,
							sizeof(weeks) / sizeof(weeks[0])));
	assert(weeks[0] == 1);
	assert(weeks[1] == 1);
	assert(weeks[2] == 1);
	assert(weeks[3] == 1);
	assert(weeks[4] == 0);
	assert(weeks[5] == 0);
	assert(weeks[6] == 1);
	memset(weeks, 0, sizeof(uint32_t) * 7);

	sprintf(week, "%d,%d", 6, 1);
	assert(
			0
					== css_format_week_date(week, weeks,
							sizeof(weeks) / sizeof(weeks[0])));
	assert(weeks[6] == 1);
	assert(weeks[1] == 1);

}

void test_css_format_specialday()
{
	uint32_t month_day[12][31] = { 0 };
	time_t nowtime = time(NULL);
	struct tm *now = localtime(&nowtime);
	char day[100];
	sprintf(day, "%s,%d-%d-%d,%d-%d-%d", "2012-12-1,2013-1-2,2014-2-3",
			now->tm_year+1900, 7, 8, now->tm_year+1900, 8, 9);
	printf("format day:%s.\n", day);
	assert(0 == css_format_specialday(day, month_day));
	assert(month_day[6][7] == 1);
	assert(month_day[7][8] == 1);
	//  test when specialday is " "
	sprintf(day, " ");
	assert(0 == css_format_specialday(day, month_day));

}

void test_css_format_start_time()
{
	uint32_t st;
	assert(0 == css_format_start_time(&st, "1:2:3"));
//	printf("st:%d.\n",st);
	assert((1 * 3600 + 2 * 60 + 3) == st);

}

void test_css_job_trigger_suite()
{
	test_trim_space();
	test_css_format_year_date();
	test_css_format_month_date();
	test_css_format_week_date();
	test_css_format_specialday();
	test_css_format_start_time();
}
#endif
