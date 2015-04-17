#ifndef __CSS_REPALY_H__
#define __CSS_REPALY_H__

#include "css_stream.h"
#include "queue.h"
#include "css_frame_getter.h"

/*************************************************
 * repaly type define:
 *************************************************/
typedef struct css_replay_session_s css_replay_session_t;
typedef struct css_replay_sessions_manager_s css_replay_sessions_manager_t;


enum css_replay_stat_e
{
	CSS_REPLAY_UNINIT = 0,		/* uninitialize */
	CSS_REPLAY_INITED = 1,		/* initialized */
	CSS_REPLAY_PLAYNG = 2,		/* playing */
	CSS_REPLAY_PAUSE  = 3,		/* paused */
	CSS_REPLAY_SHUTDW = 4,		/* should shutdown */
};
/*************************************************
 * repaly struct:
 *************************************************/

struct css_replay_session_s
{
	QUEUE q;
	int state;
	char file_name[MAX_PATH];
	uv_timer_t* timer;
	css_stream_t* stream;
	css_replay_frame_getter_t getter;
	uint64_t last_timestemp;
	uint64_t last_send_time;
	uint64_t now;
	uint64_t next_send_time;
	uint16_t rate;
};

struct css_replay_sessions_manager_s
{
	QUEUE active_sessions;
};


/*************************************************
 * repaly functions:
 *************************************************/
void css_replay_cmd_dispatch(uint32_t cmdid, css_stream_t* stream, char* package, ssize_t length, int internal);

#endif