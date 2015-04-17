#include "css_file_reader.h"
#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "css_protocol_package.h"
#include "queue.h"


static void open_cb(uv_fs_t* req)
{
	css_file_reader_t* reader = (css_file_reader_t*)req->data;

	reader->fd = req->result;
	reader->result = req->result;
	uv_fs_req_cleanup(req);
	free(req);

	reader->on_open(reader);
}

static void read_cb(uv_fs_t* req)
{
	css_file_reader_req_t* read_req = (css_file_reader_req_t*)req->data;

	//reader->fd = req->result;
	read_req->reader->result = req->result;//req->result num of read 
	uv_fs_req_cleanup(req);
	free(req);

	read_req->cb(read_req->reader, read_req->buf);
	free(read_req);
}

static void close_cb(uv_fs_t* req)
{
	css_file_reader_t* reader = (css_file_reader_t*)req->data;
	reader->result = req->result;
	uv_fs_req_cleanup(req);
	free(req);
	reader->fd = -1;
	reader->on_close(reader);
}

int css_file_reader_open(css_file_reader_t* reader,css_file_openR_cb cb)
{
	uv_fs_t* open_req;
	int r;

	open_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!open_req){
		reader->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}
	reader->on_open = cb;
	open_req->data = reader;
	if(cb){
		r = uv_fs_open(reader->loop,open_req,reader->path, O_RDONLY | O_BINARY, 0, open_cb);
		if(r < 0){
			reader->result = r;
			free(open_req);
		}
		//reader->result = r;
		//reader->fd = open_req->result;
		return r;
	}
	else
	{
		r = uv_fs_open(reader->loop,open_req,reader->path, O_RDONLY | O_BINARY, 0, NULL);
		reader->result = r;
		reader->fd = open_req->result;
		uv_fs_req_cleanup(open_req);
		free(open_req);
		return r;
	}
	
}

int css_file_reader_close(css_file_reader_t* reader,css_file_closeR_cb cb)
{
	uv_fs_t* close_req;
	int r;

	close_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!close_req){
		reader->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}
	reader->on_close = cb;
	close_req->data = reader;

	if(cb){
		r = uv_fs_close(reader->loop,close_req,reader->fd,close_cb);
		if(r < 0){
			reader->result = r;
			free(close_req);
		}
		return r;
	}
	else{
		r = uv_fs_close(reader->loop,close_req,reader->fd,NULL);
	    reader->result = r;
		reader->fd = -1;
		uv_fs_req_cleanup(close_req);
		free(close_req);
		return r;
	}
}

int css_file_reader_read(css_file_reader_t* reader,uv_buf_t vbuf,int64_t offset,css_file_read_cb cb)
{
	uv_fs_t* read_req;
	css_file_reader_req_t* req;
	int r;

	reader->on_read = cb;

	read_req = (uv_fs_t*)malloc(sizeof(uv_fs_t));
	if(!read_req){
		reader->result = UV_EAI_MEMORY;
		return UV_EAI_MEMORY;
	}

	req = (css_file_reader_req_t*)malloc(sizeof(css_file_reader_req_t));
	if(!req){
		reader->result = UV_EAI_MEMORY;
		free(read_req);
		return UV_EAI_MEMORY;
	}
	
	read_req->data = req;
	req->cb = cb;
	req->buf = vbuf;
	req->reader = reader;
	
	if(cb){
		r = uv_fs_read(reader->loop, read_req, reader->fd, &req->buf, 1, offset, read_cb);
		if(r < 0){
			reader->result = r;
			free(read_req);
			free(req);
		}
		return r;
	}
	else{
		r = uv_fs_read(reader->loop, read_req, reader->fd, &vbuf, 1, offset, NULL);
	    reader->result = r;
		uv_fs_req_cleanup(read_req);
		free(read_req);
		free(req);
		return r;
	}
}

#ifdef CSS_TEST

#include "css_frame_getter.h"

#define MAX_BUF_LEN 5*1024*1024
static char  buf[MAX_BUF_LEN];
static uv_buf_t vbuf;
static QUEUE iif_Queue;//-------?

static int find_I(int time)
{
	int offset;
	css_iif_struct_t* p;
	css_iif_struct_t* pn;

	QUEUE *q,*qn;
	QUEUE_FOREACH(q,&iif_Queue)
	{
		p = QUEUE_DATA(q,css_iif_struct_t,queue);
		if(time - p->time.milliSeconds < 0){
			return -1;
		}
		if(time - p->time.milliSeconds == 0){
			offset = p->offset;
			return offset;
		}
		else if(time - p->time.milliSeconds > 0){
			qn = QUEUE_NEXT(q);
			pn = QUEUE_DATA(qn,css_iif_struct_t,queue);
			if(time - pn->time.milliSeconds < 0){
				offset = p->offset;
				return offset;
			}
		}	
	}
	offset = p->offset;
	return offset;
}
static void test_file_readcif_cb(css_file_reader_t* reader, const uv_buf_t buf);
static void test_file_close_cb(css_file_reader_t* reader)
{
	if(reader->result < 0){
		printf("close error! error no.: %d\n", reader->result);
	}
}

static void free_iif_list()
{
	QUEUE *q;
	q = NULL;
	QUEUE_FOREACH(q,&iif_Queue){
		QUEUE_REMOVE(q);
	}

}

static void test_file_readiif_cb(css_file_reader_t* reader, const uv_buf_t rbuf)
{
	//read
	int r;
	css_iif_struct_t* iif_struct;

	if(reader->result <= 0){
		r = css_file_reader_close(reader,test_file_close_cb);
		return;
	}
	
	iif_struct = (css_iif_struct_t*)malloc(sizeof(css_iif_struct_t));

	vbuf = uv_buf_init(buf,12);

	r = css_uint32_decode(buf,&iif_struct->offset);
	r += css_uint16_decode(buf+r,&iif_struct->time.year);
	r += css_uint8_decode(buf+r,&iif_struct->time.month);
	r += css_uint8_decode(buf+r,&iif_struct->time.day);
	r += css_uint32_decode(buf+r,&iif_struct->time.milliSeconds);

	//--------------------------?
	QUEUE_INIT(&iif_struct->queue);

	QUEUE_INSERT_TAIL(&iif_Queue,&iif_struct->queue);

	//--------------------------?
	r = css_file_reader_read(reader,vbuf,-1,test_file_readiif_cb);
}

static void test_file_readrecordfile_cb(css_file_reader_t* reader, const uv_buf_t rbuf)
{
	//read file--->mp4
	int r;
	FILE* f = fopen("1.dat","ab+");
	fwrite(vbuf.base,vbuf.len,1,f);
	vbuf = uv_buf_init(buf,20);
	r = css_file_reader_read(reader,vbuf,-1,test_file_readcif_cb);
}

static void test_file_readcif_cb(css_file_reader_t* reader, const uv_buf_t rbuf)
{
    //read
    int r;
	int offset;
	int len;
	int type;
    int16_t year;
	int8_t month;
	int8_t day;
	int millisecond;

	if(reader->result <= 0){
		r = css_file_reader_close(reader,test_file_close_cb);
		return;
	}

	//vbuf = uv_buf_init(buf,20);
	r = css_int32_decode(buf,&offset);
	r += css_int32_decode(buf+r,&len);
	r += css_int32_decode(buf+r,&type);
	r += css_int16_decode(buf+r,&year);
	r += css_int8_decode(buf+r,&month);
	r += css_int8_decode(buf+r,&day);
	r += css_int32_decode(buf+r,&millisecond);
	//-------读取文件-----------------//
	vbuf = uv_buf_init(buf,len);
	r = css_file_reader_read(reader,vbuf,offset,test_file_readrecordfile_cb);
	//r = css_file_reader_read(reader,vbuf,-1,test_file_readcif_cb);
}

static void test_file_open_cb(css_file_reader_t* reader)
{
	//open后读取close；
	int r;
	//r = css_file_reader_close(reader);
	vbuf = uv_buf_init(buf,12);
	r = css_file_reader_read(reader,vbuf,-1,test_file_readiif_cb);
}

void test_file_reader(void)
{
	int time = 62224440;
	int offset;
	css_file_reader_t* reader;
	char* filename="11";
	strcat(filename,".iif");
	
	//----------------------?
	QUEUE_INIT(&iif_Queue);
	//-----------------------?

	reader = (css_file_reader_t*)malloc(sizeof(css_file_reader_t));
	reader->loop = uv_default_loop();
	strcpy(reader->path,filename);
	//reader->result = 0;

	css_file_reader_open(reader,test_file_open_cb);

	uv_run(reader->loop,UV_RUN_DEFAULT);

	offset = find_I(time);

	free_iif_list();

	free(reader);
}
#endif