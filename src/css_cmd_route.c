#include "css_cmd_route.h"
#include "css_protocol_package.h"
#include "css.h"
#include "css_capture.h"
#include "css_alarm.h"
#include "css_replay.h"


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

DCL_CMD_HANDLER(sv_cmd_alarmupmsg_server_ex)
DCL_CMD_HANDLER(sv_cmd_perform_test)
DCL_CMD_HANDLER(sv_cmd_add_centralizestorage_server)
DCL_CMD_HANDLER(sv_cmd_restartcomputer)

static void css_cmd_delete_stream(css_stream_t* stream);

static void css_cmd_delete_stream(css_stream_t* stream)
{
	if(stream)
		free(stream);
}

IMP_CMD_HANDLER(sv_cmd_alarmupmsg_server_ex)
{
	JNetCmd_AlarmUpMsgServer_Ex cmd;
	int ret;
	do{
		ret = CSS_RT_GET_CMD(cmd);
		if (ret <= 0) {
			break;
		}

		css_start_alarmtask_by_xml(cmd.AlarmInfoXml.base,cmd.AlarmInfoXml.len,cmd.RelationInfoXml.base,cmd.RelationInfoXml.len);
		CSS_RT_FREE_PACKAGE();
		return;/* do not close stream, wait next cmd */
	}while(0);

	CSS_RT_FREE_PACKAGE();
	CSS_RT_CLOSE_STREAM();
}

IMP_CMD_HANDLER(sv_cmd_perform_test)
{
	JNetCmd_Perform_Test cmd;
	int ret;
	int64_t dvr_id;
	int16_t channel_no;
	css_record_info_t record_info = { 0 };
	struct timeval tv;
	css_sms_info_t sms, *psms = NULL;

	do{
		ret = CSS_RT_GET_CMD(cmd);
		if (ret <= 0) {
			break;
		}

		dvr_id = cmd.ChannelId / 100;
		channel_no = cmd.ChannelId % 100;

		record_info.dvrEquipId = dvr_id;
		record_info.channelNo = channel_no;
		record_info.timestamp.timestamp = cmd.Timestamp * 1000 * 1000;
		record_info.timestamp.type = 1;

		gettimeofday(&tv, NULL);
		sprintf(record_info.timestamp.eventId, "%ld%d%lld", tv.tv_sec, tv.tv_usec,
			cmd.ChannelId);

		if ((cmd.ChannelId % 3) == 1) {
			uv_ip4_addr("192.168.8.209", 65005, &sms.smsAddr);
			psms = &sms;
		} else if ((cmd.ChannelId % 3) == 2) {
			uv_ip4_addr("192.168.8.207", 65005, &sms.smsAddr);
			psms = &sms;
		} else if ((cmd.ChannelId % 3) == 0) {
			uv_ip4_addr("192.168.8.215", 65005, &sms.smsAddr);
			psms = &sms;
		} else {
			psms = NULL;
		}

		capture_start_record(&record_info, psms);
		printf("start record %s\n", record_info.timestamp.eventId);
	}while(0);

	CSS_RT_FREE_PACKAGE();
	CSS_RT_CLOSE_STREAM();
}

IMP_CMD_HANDLER(sv_cmd_add_centralizestorage_server)
{
	JNetCmd_AddCentralizeStorageServer cmd;
	int ret;

	do{
		ret = CSS_RT_GET_CMD(cmd);
		if (ret <= 0) {
			break;
		}
	}while(0);

	CSS_RT_FREE_PACKAGE();
	CSS_RT_CLOSE_STREAM();
}

IMP_CMD_HANDLER(sv_cmd_restartcomputer)
{
	JNetCmd_RestartComputer cmd;
	int ret;

	do{
		ret = CSS_RT_GET_CMD(cmd);
		if (ret <= 0) {
			break;
		}
	}while(0);

	CSS_RT_FREE_PACKAGE();
	CSS_RT_CLOSE_STREAM();
}


int css_cmd_dispatch(css_stream_t* stream, char* package, ssize_t length)
{
	int ret = -1;

	JNetCmd_Header header;

	/* stop read first, start read later if necessary */
	css_stream_read_stop(stream);

	if (length <= 0) {
		if (package)
			free(package);
		css_stream_close(stream, css_cmd_delete_stream);
		return (int)length;
	}

	ret = css_proto_package_header_decode(package, length, &header);
	if(ret <= 0){
		free(package);
		css_stream_close(stream, css_cmd_delete_stream);
		return ret;
	}

	switch (header.I_CmdId) {
	DSP_CMD_HANDLER(sv_cmd_alarmupmsg_server_ex)
	DSP_CMD_HANDLER(sv_cmd_perform_test)
	DSP_CMD_HANDLER(sv_cmd_add_centralizestorage_server)
	DSP_CMD_HANDLER(sv_cmd_restartcomputer)

	case sv_cmd_replay_search_record:
	case sv_cmd_replay_play_file:
	case sv_cmd_replay_set_speed:
	case sv_cmd_replay_set_postion:
	case sv_cmd_replay_pause:
	case sv_cmd_replay_continue:
	case sv_cmd_replay_shutdown:
		css_replay_cmd_dispatch(header.I_CmdId, stream, package, length, 0);
		break;

	default:
		free(package);
		css_stream_close(stream, css_cmd_delete_stream);
		break;
	}

	return ret;
}