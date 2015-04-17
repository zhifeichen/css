#ifndef _CSS_FILE_READ_H_
#define _CSS_FILE_READ_H_

#include "uv/uv.h"
#include "css.h"

typedef struct css_file_reader_s css_file_reader_t;
typedef struct css_file_reader_req_s css_file_reader_req_t;

typedef void (*css_file_openR_cb)(css_file_reader_t* reader);
typedef void (*css_file_read_cb)(css_file_reader_t* reader, const uv_buf_t buf);
typedef void (*css_file_closeR_cb)(css_file_reader_t* reader);

struct css_file_reader_s
{
	void* data;
	uv_loop_t* loop;
	char path[MAX_PATH];
	uv_file fd;
	ssize_t result;

	css_file_openR_cb on_open;
	css_file_read_cb  on_read;
	css_file_closeR_cb on_close;
};

struct css_file_reader_req_s
{
	void* data;
	css_file_reader_t* reader;
	uv_buf_t buf;
	css_file_read_cb cb;
};

int css_file_reader_open(css_file_reader_t* reader,css_file_openR_cb cb);
int css_file_reader_close(css_file_reader_t* reader,css_file_closeR_cb cb);
int css_file_reader_read(css_file_reader_t* reader,uv_buf_t vbuf,int64_t offset,css_file_read_cb cb);
#endif