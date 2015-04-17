#ifndef __CSS_CAPTURE_H__
#define __CSS_CAPTURE_H__

#include "css.h"
#include "css_stream.h"
#include "css_file_writer.h"
#include "css_util.h"

/*
 * session最大缓存数据包数
 */
#define MAX_CAPTURE_FRAME_LIST_LEN 500

#define DEFAULT_INCISIONTIME (20*60*1000)

#define APPEND_TIME (10*1000) /* 10 second */

#define CIF_BUF_LEN (5*1024)
#define IIF_BUF_LEN (3*1024)

typedef struct css_session_manager_s css_session_manager_t;
/*
 * 码流类型 flow type
 */
typedef enum{
	FLOW_TYPE_PRIMARY = 0,
	FLOW_TYPE_SUB = 1
}css_flow_type;

/*
 * session status
 */
#define	SESSION_STATUS_TYPE_UNINIT		0x0
#define	SESSION_STATUS_TYPE_INIT		0x1
#define	SESSION_STATUS_TYPE_CONNECTED	0x2
#define	SESSION_STATUS_TYPE_READING		0x4
#define	SESSION_STATUS_TYPE_FILE_OPENED	0x8
#define	SESSION_STATUS_TYPE_CLOSING		0x10
typedef int css_session_status_type;

/*
 * package type
 */
typedef enum{
	PACKAGE_TYPE_OPENFILE = 0,
	PACKAGE_TYPE_FRAME = 1,
	PACKAGE_TYPE_REOPENFILE = 2,
	PACKAGE_TYPE_CLOSEFILE = 3,
	PACKAGE_TYPE_CLOSESESSION = 4,
}package_type;

/*
 * frame type
 */
typedef enum{
	FRAME_TYPE_SysHeader = 0x0080,	
	FRAME_TYPE_I_Frame = 0x0001,	
	FRAME_TYPE_P_Frame = 0x0002 ,
	FRAME_TYPE_B_Frame = 0x0003 ,
	FRAME_TYPE_BP_Frame = 0x0100 ,
	FRAME_TYPE_BBP_Frame = 0x0004 ,
	FRAME_TYPE_Audio_Frame = 0x0008 ,
	FRAME_TYPE_Unknown_Frame = 0x0000 ,
	FRAME_TYPE_MotionDetection  = 0x0010 ,		//对IDVR的支持
	FRAME_TYPE_Axis_Data_Frame	= 0x0081		//对Axis的第2系统头
}frame_type;

/*
 * record type
 */
typedef enum{
	RECORD_TYPE_WARNING = 0,
	RECORD_TYPE_SCHEDULE = 1
}css_record_type;

/*
 * record timestamp info && record type
 */
typedef struct css_record_timestamp_s
{
	char eventId[128];
	uint64_t timestamp; // 设置的时间戳,以微秒为单位;
	uint8_t  type; // 类型 0:报警,1:计划
}css_record_timestamp_t;

/*
 * record info DTO
 */
typedef struct css_record_info_s
{
	int64_t					dvrEquipId; // dvr设备ID
	uint16_t				channelNo; // dvr通道号
	css_record_timestamp_t	timestamp; // record timestamp && type
	css_flow_type			flowType; // 码流类型
}css_record_info_t;


typedef struct css_capture_package_s
{
	package_type	type;
	int status;						//0:not use  1:already used(will be ignore).
	uv_buf_t	buf;
	struct timeval timestamp;
	struct{
		frame_type frameType;
		uv_buf_t frame;
	} frame;
	struct css_capture_package_s*		next;
}css_capture_package_t;

typedef struct css_capture_write_buf_s
{
	uv_buf_t buf;
	int max_size;
}css_capture_write_buf_t;

/*
 * captrue session
 */
typedef struct css_capture_session_s
{
	int					id;
	int					db_result;
	css_session_status_type status;
	css_record_info_t	recordInfo;
	css_sms_info_t		smsInfo;

	css_session_manager_t*	mng;

	css_file_writer_t	recordFile;
	css_file_writer_t	iifFile;
	css_file_writer_t	cifFile;

	int32_t				recordFile_offset;
	int32_t				cifFile_offset;

	struct timeval		fileStartTime;
	struct timeval		fileEndTime;
	long				fileIncisionTime; /* Incision time */

	css_capture_write_buf_t iifBuf;
	css_capture_write_buf_t cifBuf;

	css_stream_t		client;
	struct timeval		sessionStartTime;

	void*				queue[2];

	int package_cnt;
	css_capture_package_t *package_header, *package_tail;

	css_capture_package_t package_sysheader;
}css_capture_session_t;

/*
 * capture session manager
 */
struct css_session_manager_s
{
	int32_t status;				//1: init.   2: started.
	uv_loop_t* loop;
	uv_thread_t tid;
	uv_timer_t timer;
	void* session_queue[2];
};

/*
 * function:
 */

/* capture service function */
int capture_service_init(void);
int capture_service_start(void);
int capture_service_stop(void);
int capture_service_clean(void);

/* record function */
int capture_start_record(css_record_info_t* record_info, css_sms_info_t* sms); /* sms == NULL ,use default sms*/
void capture_stop_all(void);

#endif /* __CSS_CAPTURE_H__ */
