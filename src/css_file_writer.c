#include "css_file_writer.h"
#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void open_cb(uv_fs_t* req);
static void close_cb(uv_fs_t* req);
static void write_cb(uv_fs_t* req);

static void open_cb(uv_fs_t* req)
{
	css_file_writer_t* writer = (css_file_writer_t*)req->data;

	writer->fd = req->result;
	writer->result = req->result;
	uv_fs_req_cleanup(req);
	free(req);
	writer->on_open(writer);
}

static void close_cb(uv_fs_t* req)
{
	css_file_writer_t* writer = (css_file_writer_t*)req->data;
	writer->result = req->result;
	uv_fs_req_cleanup(req);
	free(req);
	writer->fd = -1;
	writer->on_close(writer);
}

static void sync_cb(uv_fs_t* req)
{
	css_file_write_req_t* write_req = (css_file_write_req_t*)req->data;
	if(req->result == -1){
		write_req->writer->result = req->result;
	}
	uv_fs_req_cleanup(req);
	free(req);
	write_req->writer->sync_size = 0;
	if(write_req->cb)
		write_req->cb(write_req->writer, write_req->buf, write_req->buf_cnt);
	free(write_req->buf);
	free(write_req);
}

static void write_cb(uv_fs_t* req)
{
	css_file_write_req_t* write_req = (css_file_write_req_t*)req->data;
	write_req->writer->result = req->result;
	write_req->writer->sync_size += req->result;
	uv_fs_req_cleanup(req);
	free(req);
	if(write_req->writer->sync_size < MAX_SYNC_SIZE){
		if(write_req->cb)
			write_req->cb(write_req->writer, write_req->buf, write_req->buf_cnt);
		free(write_req->buf);
		free(write_req);
	}
	else{
		int r;
		uv_fs_t* sync_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
		if(!sync_req){
			write_req->writer->result = UV_EAI_MEMORY;
			if(write_req->cb)
				write_req->cb(write_req->writer, write_req->buf, write_req->buf_cnt);
			free(write_req->buf);
			free(write_req);
		}
		sync_req->data = write_req;
		r = uv_fs_fdatasync(write_req->writer->loop, sync_req, write_req->writer->fd, sync_cb);
		if(r < 0){
			write_req->writer->result = r;
			if(write_req->cb)
				write_req->cb(write_req->writer, write_req->buf, write_req->buf_cnt);
			free(sync_req);
			free(write_req->buf);
			free(write_req);
		}
	}
}

int css_file_writer_open(css_file_writer_t* writer, css_file_open_cb cb)
{
	uv_fs_t* open_req;
	int r;

	open_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!open_req){
		writer->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}
	writer->sync_size = 0;
	writer->on_open = cb;
	open_req->data = writer;
	r = uv_fs_open(writer->loop, open_req, writer->path, O_RDWR | O_CREAT,S_IWUSR | S_IRUSR, open_cb);
	if(r < 0){
		writer->result = r;
		free(open_req);
	}
	return r;
}

int css_file_writer_close(css_file_writer_t* writer, css_file_close_cb cb)
{
	int r;
	uv_fs_t* close_req;

	close_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!close_req){
		writer->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}
	writer->on_close = cb;
	close_req->data = writer;
	if(cb){
		r = uv_fs_close(writer->loop, close_req, writer->fd, close_cb);
		if(r < 0){
			writer->result = r;
			free(close_req);
		}
		return r;
	}
	else{
		r = uv_fs_close(writer->loop, close_req, writer->fd, NULL);
		writer->result = r;
		writer->fd = -1;
		uv_fs_req_cleanup(close_req);
		free(close_req);
		return r;
	}
}

int css_file_writer_write(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt,  int64_t offset, css_file_write_cb cb)
{
	int r;
	uv_fs_t* write_req;
	css_file_write_req_t* req;

	req = (css_file_write_req_t*)malloc(sizeof(css_file_write_req_t));
	if(!req){
		writer->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}

	req->writer = writer;
	req->buf = (uv_buf_t*)malloc(sizeof(uv_buf_t)*buf_cnt);

	if(!req->buf){
		free(req);
		writer->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}
	req->buf_cnt = buf_cnt;
	memcpy(req->buf, buf, sizeof(uv_buf_t)*buf_cnt);
	req->cb = cb;

	write_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!write_req){
		free(req->buf);
		free(req);
		writer->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}

	write_req->data = req;
	if(cb){
		r = uv_fs_write(req->writer->loop, write_req, req->writer->fd, req->buf, buf_cnt, offset, write_cb);
		if(r < 0){
			writer->result = r;
			free(write_req);
			free(req->buf);
			free(req);
		}
		return r;
	}
	else{
		r = uv_fs_write(req->writer->loop, write_req, req->writer->fd, req->buf, buf_cnt, offset, NULL);
		uv_fs_req_cleanup(write_req);
		writer->result = r;
		free(write_req);
		free(req->buf);
		free(req);

		return r;
	}
}


/*
 * TEST:
 */
#ifdef CSS_TEST


static void test_file_close_cb(css_file_writer_t* writer)
{
	if(writer->result < 0){
		printf("close error! error no.: %d\n", writer->result);
	}
}

static void test_file_write_cb(css_file_writer_t* writer, uv_buf_t buf[], int buf_cnt)
{
	if(writer->result < 0){
		printf("write error! error no.: %d\n", writer->result);
	}
	free(buf[1].base);
}

static void test_file_open_cb(css_file_writer_t* writer)
{
	css_file_write_req_t* req;
	char* p = "test data\ntest data\nend";
	uv_buf_t *buf;
	int ret;

	if(writer->result < 0) return;

	req = (css_file_write_req_t*)malloc(sizeof(css_file_write_req_t));
	buf = (uv_buf_t*)malloc(2*sizeof(uv_buf_t));

	buf[0].base = p;
	buf[0].len = strlen(p);
	buf[1].base = (char*)malloc(24);
	buf[1].len = 24;
	strcpy(buf[1].base, "12345678901234567890123");
	ret = css_file_writer_write(writer, buf, 2, -1, test_file_write_cb);
	if(ret < 0){
		free(buf[1].base);
		css_file_writer_close(writer, test_file_close_cb);
	}
	css_file_writer_close(writer, test_file_close_cb);
}

void test_timer_cb(uv_timer_t* timer)
{
	css_file_writer_t* writer = timer->data;
	css_file_writer_close(writer, test_file_close_cb);
}

void test_file_writer(void)
{
	css_file_writer_t* writer;
	char* filename = "testfile";
	uv_timer_t timer;

	writer = (css_file_writer_t*)malloc(sizeof(css_file_writer_t));
	writer->loop = uv_default_loop();
	strcpy(writer->path, filename);
	css_file_writer_open(writer, test_file_open_cb);

	uv_timer_init(writer->loop, &timer);
	timer.data = writer;
	//uv_timer_start(&timer, test_timer_cb, 10000, 0);

	uv_run(writer->loop, UV_RUN_DEFAULT);
	free(writer);
}

#endif