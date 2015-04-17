#include "css_search_record.h"
#include "css_util.h"
#include "css_protocol_package.h"

static int encode_ok_response(uv_buf_t *buf, QUEUE *filelist);
static int encode_error_response(uv_buf_t *buf, int err);
static int encode_record_fileinfo(char *p, css_file_record_info *vf);
static void encode_response_header(char *p, int packageLen);
static void on_css_stream_write(css_write_req_t* req, int status);
static void css_search_close_stream(css_stream_t* stream);

static void css_search_close_stream(css_stream_t* stream) {
	if(stream)
		free(stream);
}

void search_record(uv_work_t* req) {
	int ret;
	QUEUE filelist;
	css_search_record_t *search_req = NULL;
	uv_buf_t *buf = NULL;

	do {
		search_req = (css_search_record_t*)req->data;
		buf = &(search_req->buf);

		QUEUE_INIT(&filelist);
		ret = css_db_select_video_file_by_conditions(&(search_req->conds), &filelist);

		if (ret < 0)
			break;

		ret = encode_ok_response(buf, &filelist);
	} while(0);

	if (ret < 0) {
		if (encode_error_response(buf, ret) < 0) {
			buf->base = NULL;
			buf->len = 0;
		}
	}

	css_free_file_list(&filelist);
}

void after_search_record(uv_work_t* req, int status) {
	css_search_record_t *search_req = NULL;
	uv_buf_t buf;
	css_stream_t *stream = NULL;
	int ret = 0;
	css_write_req_t *write_req;

	search_req = (css_search_record_t*)req->data;
	buf = search_req->buf;
	stream = search_req->stream;

	FREE(search_req);
	FREE(req);

	write_req = (css_write_req_t *)malloc(sizeof(css_write_req_t));

	if (buf.base && write_req) {
		ret = css_stream_write(stream, write_req, buf, on_css_stream_write);

		if(ret == 0) /* success */
			return;
	}

	FREE(buf.base);
	FREE(write_req);
	css_stream_close(stream, css_search_close_stream);
}


static int encode_ok_response(uv_buf_t *buf, QUEUE *filelist) {
	int recNum = 0, bufLen = 0;
	css_file_record_info *vf = NULL;
	QUEUE* q = NULL;
	char *pt = NULL, *ipt = NULL;

	QUEUE_FOREACH(q, filelist) {
		vf = QUEUE_DATA(q, css_file_record_info, queue);
		recNum++;
		bufLen += strlen(vf->fields.fileName) + 82;
	}
	bufLen += 28;

	pt = (char *)malloc(bufLen);
	if (pt == NULL)
		return -1;

	memset(pt, 0, bufLen);
	ipt = pt + 20;

	ipt += css_int32_encode(ipt, 0);
	ipt += css_int32_encode(ipt, recNum);

	QUEUE_FOREACH(q, filelist) {
		vf = QUEUE_DATA(q, css_file_record_info, queue);
		ipt += encode_record_fileinfo(ipt, vf);
	}

	encode_response_header(pt, bufLen);

	buf->base = pt;
	buf->len = bufLen;
	return 0;
}

static int encode_error_response(uv_buf_t *buf, int err) {
	char *p = NULL;

	p = (char *)malloc(24);
	if (!p)
		return -1;

	memset(p, 0, 24);
	encode_response_header(p, 24);
	css_int32_encode(p+20, err);

	buf->base = p;
	buf->len = 24;
	return 0;
}

static void encode_response_header(char *p, int packageLen) {
	css_uint32_encode(p, packageLen);
	css_uint32_encode(p+4, 0x80060006);
}

static int encode_record_fileinfo(char *p, css_file_record_info *vf) {
	int pos = 4, fileSize = 0, fileNameLen;
	pos += css_uint64_encode(p+pos, vf->fileID);
	pos += css_uint64_encode(p+pos, vf->fields.dvrEquipId);
	pos += css_uint16_encode(p+pos, vf->fields.channelNo);
	memcpy(p+pos, vf->fields.beginTime, 23);
	pos += 23;
	memcpy(p+pos, vf->fields.endTime, 23);
	pos += 23;
	pos += css_uint64_encode(p+pos, vf->fields.playLength);
	fileNameLen = strlen(vf->fields.fileName);
	pos += css_uint16_encode(p+pos, fileNameLen);
	memcpy(p+pos, vf->fields.fileName, fileNameLen);
	pos += fileNameLen;
	pos += css_uint32_encode(p+pos, fileSize);
	css_uint32_encode(p, pos-4);
	return pos;
}

static void on_css_stream_write(css_write_req_t* req, int status) {
	css_stream_close(req->stream, css_search_close_stream);
	FREE(req->buf.base);
	FREE(req);
}
