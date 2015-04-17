#ifndef __CSS_FILE_WRITER_H__
#define __CSS_FILE_WRITER_H__

#include "uv/uv.h"
#include "css.h"

#define MAX_SYNC_SIZE (4*1024*1024)

typedef struct css_file_write_req_s css_file_write_req_t;
typedef struct css_file_writer_s css_file_writer_t;

typedef void (*css_file_write_cb)(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt);
typedef void (*css_file_open_cb)(css_file_writer_t* writer);
typedef void (*css_file_close_cb)(css_file_writer_t* writer);

/*
 * record file writer
 */
struct css_file_writer_s
{
	void* data;
	uv_loop_t* loop;
	char path[MAX_PATH]; /* file path; */
	int volume_length;
	ssize_t sync_size;
	uv_file fd; /* file; */
	ssize_t result;
	css_file_open_cb on_open;
	css_file_close_cb on_close;
};

struct css_file_write_req_s
{
	css_file_writer_t* writer;
	uv_buf_t* buf;
	int buf_cnt;
	css_file_write_cb cb;
};


/*
 * function:
 */
int css_file_writer_open(css_file_writer_t* writer, css_file_open_cb cb);
int css_file_writer_close(css_file_writer_t* writer, css_file_close_cb cb);

/* offset == -1: append model */
int css_file_writer_write(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt, int64_t offset, css_file_write_cb cb);

#endif /* __CSS_FILE_WRITER_H__ */