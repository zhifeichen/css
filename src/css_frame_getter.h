#ifndef __CSS_FRAME_GETTER_H__
#define __CSS_FRAME_GETTER_H__

#include "uv/uv.h"
#include "css.h"
#include "css_file_reader.h"
#include "queue.h"
#include "css_util.h"

#define CSS_FRAME_GETTER_CIF_LOW_LEVEL 25
#define CSS_FRAME_GETTER_REC_LOW_LEVEL 25

#define	CSS_FRAME_GETTER_REQ_OPEN 1
#define	CSS_FRAME_GETTER_REQ_CLOSE 2
#define	CSS_FRAME_GETTER_REQ_GET_NEXT 3
#define	CSS_FRAME_GETTER_REQ_SET_POSITION 4

typedef struct css_replay_frame_getter_s css_replay_frame_getter_t;
typedef struct css_iif_struct_s  css_iif_struct_t;
typedef struct css_cif_struct_s  css_cif_struct_t;
typedef struct css_frame_getter_chain_s css_frame_getter_chain_t;
typedef struct css_frame_getter_req_s css_frame_getter_req_t;
typedef struct css_video_frame_s css_video_frame_t;
typedef struct css_video_frame_queue_s css_video_frame_queue_t;
typedef void (*css_frame_getter_cb)(css_replay_frame_getter_t* getter);

struct css_iif_struct_s
{
	uint32_t offset;
	sv_time_t time;

	void* queue[2];
};

struct css_cif_struct_s
{
	uint32_t offset;
	uint32_t len;
	int32_t  type;
	sv_time_t time;

	void* queue[2];
};

struct css_frame_getter_chain_s
{
	QUEUE header;
	QUEUE* current;
	QUEUE q;
	int nelts;
	int rest;
};

struct css_video_frame_queue_s
{
	QUEUE h;
	int nelts;
	int state;
};

struct css_replay_frame_getter_s
{
	void* data;

	uv_loop_t* loop;
	int32_t position;//------------?
	

	uint32_t cif_offset;

	sv_time_t starttime;

	css_file_reader_t recordFile;
	css_file_reader_t cifFile;
	css_file_reader_t iifFile;

	css_frame_getter_chain_t iif_buckets;
	css_frame_getter_chain_t cif_buckets;

	css_video_frame_queue_t frame_queue;
	css_video_frame_t* frame;
	int result;
};

struct css_video_frame_s
{
	QUEUE q;
	int state;
	int32_t millisecond;
	int32_t frameType;
	int32_t sendType;
	uv_buf_t frameData;
};

struct css_frame_getter_req_s
{
	css_replay_frame_getter_t* getter;
	int type;
	uv_work_t work_req;
	char* path;
	int32_t position;
	css_frame_getter_cb cb;
};


int css_replay_frame_getter_open(css_replay_frame_getter_t* getter, const char* filename, css_frame_getter_cb cb);
int css_replay_frame_getter_close(css_replay_frame_getter_t* getter, css_frame_getter_cb cb);
int css_replay_frame_getter_set_position(css_replay_frame_getter_t* getter, int32_t position, css_frame_getter_cb cb);
int css_replay_frame_getter_get_next_frame(css_replay_frame_getter_t* getter, css_frame_getter_cb cb);
int css_replay_frame_getter_release_frame(css_replay_frame_getter_t* getter, css_video_frame_t* frame);

#endif