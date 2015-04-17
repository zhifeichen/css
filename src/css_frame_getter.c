#include "css_frame_getter.h"
#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "css_protocol_package.h"
#include "css_logger.h"
#include "css_util.h"

#define MAX_BUF_LEN 4*1024*1024
#define CSS_FRAME_GETTER_IIF_BUF_LEN (12*1024)
#define CSS_FRAME_GETTER_CIF_BUF_LEN (20*1024)

#define CSS_FRAME_GETTER_CHAIN_GET_HEADER_CHAIN(c) QUEUE_DATA(QUEUE_HEAD(&((c)->q)), css_frame_getter_chain_t, q)
#define CSS_FRAME_GETTER_CHAIN_GET_HEADER(c)		\
	QUEUE_HEAD(&(CSS_FRAME_GETTER_CHAIN_GET_HEADER_CHAIN(c))->header)
#define CSS_FRAME_GETTER_CHAIN_GET_CURRENT_DATA(c, type, field) QUEUE_DATA((c)->current, type, field)
#define CSS_FRAME_GETTER_QUEUE_IS_TAIL(h, q)		\
	((const QUEUE *) (h) != (const QUEUE *)(q) && (const QUEUE *) (h) == (const QUEUE *)QUEUE_NEXT(q))
									

static int css_frame_getter_chain_init(css_frame_getter_chain_t* c);
static int css_frame_getter_chain_release(css_frame_getter_chain_t* c);
static int css_frame_getter_chain_move2next(css_frame_getter_chain_t* c);
static int css_frame_getter_chain_release_header_bucket(css_frame_getter_chain_t* c);
static int css_frame_getter_chain_add_iif_bucket(css_frame_getter_chain_t* c, const uv_buf_t* buf);
static int css_frame_getter_chain_add_cif_bucket(css_frame_getter_chain_t* c, const uv_buf_t* buf);
static int css_frame_getter_chain_build_iif_queue(css_frame_getter_chain_t** c, const uv_buf_t* buf);
static int css_frame_getter_chain_build_cif_queue(css_frame_getter_chain_t** c, const uv_buf_t* buf);
static int css_frame_getter_decode_iif(css_iif_struct_t* iif, char* buf);
static int css_frame_getter_decode_cif(css_cif_struct_t* cif, char* buf);

static void css_file_readcif_cb(css_file_reader_t* reader);

static int css__replay_video_frame_queue_init(css_video_frame_queue_t* q);
static int css__replay_video_frame_queue_release(css_video_frame_queue_t* q);
static int css__replay_video_frame_queue_add(css_video_frame_queue_t* q, QUEUE* frame);
static int css__replay_video_frame_queue_move2next(css_video_frame_queue_t* q, QUEUE** h);

static int css__replay_frame_getter_init(css_replay_frame_getter_t* getter);
static int css__replay_frame_getter_open(css_replay_frame_getter_t* getter, const char* filename);
static int css__replay_frame_getter_close(css_replay_frame_getter_t* getter);
static int css__replay_frame_getter_set_position(css_replay_frame_getter_t* getter, int32_t position);
static int css__replay_frame_getter_get_next_frame(css_replay_frame_getter_t* getter,css_frame_getter_cb cb);

static int css__replay_frame_getter_read_cif(css_replay_frame_getter_t* getter);
static int css__replay_frame_getter_read_iif(css_replay_frame_getter_t* getter);
static int css__replay_frame_getter_read_rec(css_replay_frame_getter_t* getter, void (*cb)(css_file_reader_t* reader, const uv_buf_t buf));

static int css__replay_frame_getter_req_init(css_frame_getter_req_t* req, 
											 int type, 
											 css_replay_frame_getter_t* getter, 
											 const char* filename, 
											 int32_t position,
											 css_frame_getter_cb cb);
static void css__replay_frame_getter_req_clean(css_frame_getter_req_t* req);
static void css__replay_frame_getter_work(uv_work_t* w);
static void css__replay_frame_getter_done(uv_work_t* w, int status);

#define CSS__REPLAY_FG_QUEUE_JOB(loop, req)			\
	uv_queue_work(loop, &req->work_req, css__replay_frame_getter_work, css__replay_frame_getter_done);

#define container_of(ptr, type, member)				\
  ((type *) ((char *) (ptr) - offsetof(type, member)))


/* chain */
static int css_frame_getter_chain_init(css_frame_getter_chain_t* c)
{
	int ret = 0;
	QUEUE_INIT(&c->header);
	QUEUE_INIT(&c->q);
	c->current = &c->header;
	c->nelts = 0;
	c->rest = 0;
	return ret;
}

static int css_frame_getter_chain_release(css_frame_getter_chain_t* c)
{
	int ret = 0;

	while(!QUEUE_EMPTY(&c->q)){
		ret = css_frame_getter_chain_release_header_bucket(c);
	}
	c->nelts = 0;
	c->rest = 0;
	return ret;
}

static int css_frame_getter_chain_move2next(css_frame_getter_chain_t* c)
{
	int ret = 0;
	if(c->nelts == 0){
		return UV_ENOBUFS;
	}
	c->current = QUEUE_NEXT(c->current);
	c->rest--;
	if(c->current == &(CSS_FRAME_GETTER_CHAIN_GET_HEADER_CHAIN(c)->header)){
		if(CSS_FRAME_GETTER_QUEUE_IS_TAIL(&c->q, QUEUE_HEAD(&c->q))){
			ret = UV_EOF;
		}
		else{
			css_frame_getter_chain_release_header_bucket(c);
			c->current = CSS_FRAME_GETTER_CHAIN_GET_HEADER(c);
		}
	}
	return ret;
}

static int css_frame_getter_chain_release_header_bucket(css_frame_getter_chain_t* c)
{
	int ret = 0;
	css_frame_getter_chain_t* h;
	QUEUE* header = QUEUE_HEAD(&c->q);

	h = QUEUE_DATA(header, css_frame_getter_chain_t, q);
	QUEUE_REMOVE(header);
	c->nelts -= h->nelts;
	FREE(h);
	return ret;
}

static int css_frame_getter_chain_add_iif_bucket(css_frame_getter_chain_t* c, const uv_buf_t* buf)
{
	int ret = 0;
	css_frame_getter_chain_t* tmp;
	ret = css_frame_getter_chain_build_iif_queue(&tmp, buf);
	if(ret < 0){
		return ret;
	}

	QUEUE_INSERT_TAIL(&c->q, &tmp->q);
	c->nelts += tmp->nelts;
	c->rest += tmp->nelts;
	return ret;
}

static int css_frame_getter_chain_add_cif_bucket(css_frame_getter_chain_t* c, const uv_buf_t* buf)
{
	int ret = 0;
	css_frame_getter_chain_t* tmp;
	ret = css_frame_getter_chain_build_cif_queue(&tmp, buf);
	if(ret < 0){
		return ret;
	}

	QUEUE_INSERT_TAIL(&c->q, &tmp->q);
	c->nelts += tmp->nelts;
	c->rest += tmp->nelts;
	return ret;
}

static int css_frame_getter_chain_build_iif_queue(css_frame_getter_chain_t** c, const uv_buf_t* buf)
{
	int ret;
	int count = buf->len / 12;
	int i;
	char* tmp = NULL;
	css_iif_struct_t* iif = NULL;
	*c = NULL;
	tmp = (char*)malloc(sizeof(css_frame_getter_chain_t) + count*sizeof(css_iif_struct_t));
	if(!tmp){
		return UV_ENOMEM;
	}
	*c = (css_frame_getter_chain_t*)tmp;
	iif = (css_iif_struct_t*)(tmp + sizeof(css_frame_getter_chain_t));
	css_frame_getter_chain_init(*c);

	for(i = 0; i < count; i++, iif++){
		ret = css_frame_getter_decode_iif(iif, buf->base + i * 12);
		QUEUE_INSERT_TAIL(&(*c)->header, &iif->queue);
	}
	(*c)->nelts = count;
	(*c)->rest = count;
	(*c)->current = QUEUE_HEAD(&(*c)->header);
	return count;
}

static int css_frame_getter_chain_build_cif_queue(css_frame_getter_chain_t** c, const uv_buf_t* buf)
{
	int ret;
	int count = buf->len / 20;
	int i;
	char* tmp = NULL;
	css_cif_struct_t* cif = NULL;
	*c = NULL;
	tmp = (char*)malloc(sizeof(css_frame_getter_chain_t) + count*sizeof(css_cif_struct_t));
	if(!tmp){
		return UV_ENOMEM;
	}
	*c = (css_frame_getter_chain_t*)tmp;
	cif = (css_cif_struct_t*)(tmp + sizeof(css_frame_getter_chain_t));
	css_frame_getter_chain_init(*c);

	for(i = 0; i < count; i++, cif++){
		ret = css_frame_getter_decode_cif(cif, buf->base + i * 20);
		QUEUE_INSERT_TAIL(&(*c)->header, &cif->queue);
	}
	(*c)->nelts = count;
	(*c)->rest = count;
	(*c)->current = QUEUE_HEAD(&(*c)->header);
	return count;
}

static int css_frame_getter_decode_iif(css_iif_struct_t* iif, char* buf)
{
	int r;
	QUEUE_INIT(&iif->queue);

	r = css_uint32_decode(buf,&iif->offset);
	r += css_uint16_decode(buf+r,&iif->time.year);
	r += css_uint8_decode(buf+r,&iif->time.month);
	r += css_uint8_decode(buf+r,&iif->time.day);
	r += css_uint32_decode(buf+r,&iif->time.milliSeconds);
	return r;
}

static int css_frame_getter_decode_cif(css_cif_struct_t* cif, char* buf)
{
	int r;
	QUEUE_INIT(&cif->queue);

	r = css_uint32_decode(buf,&cif->offset);
	r += css_uint32_decode(buf+r,&cif->len);
	r += css_int32_decode(buf+r,&cif->type);
	r += css_uint16_decode(buf+r,&cif->time.year);
	r += css_uint8_decode(buf+r,&cif->time.month);
	r += css_uint8_decode(buf+r,&cif->time.day);
	r += css_uint32_decode(buf+r,&cif->time.milliSeconds);
	return r;
}

/* frame queue */
static int css__replay_video_frame_queue_init(css_video_frame_queue_t* q)
{
	q->nelts = 0;
	q->state = 0;
	QUEUE_INIT(&q->h);
	return 0;
}

static int css__replay_video_frame_queue_release(css_video_frame_queue_t* q)
{
	QUEUE* h;
	css__replay_video_frame_queue_move2next(q, &h);
	while(h){
		css_video_frame_t* f = container_of(h, css_video_frame_t, q);
		free(f);
		css__replay_video_frame_queue_move2next(q, &h);
	}
	return 0;
}

static int css__replay_video_frame_queue_add(css_video_frame_queue_t* q, QUEUE* frame)
{
	QUEUE_INSERT_TAIL(&q->h, frame);
	q->nelts++;
	return 0;
}

static int css__replay_video_frame_queue_move2next(css_video_frame_queue_t* q, QUEUE** h)
{
	if(QUEUE_EMPTY(&q->h)){
		*h = NULL;
	}
	else{
		*h = QUEUE_HEAD(&q->h);
		QUEUE_REMOVE(*h);
		q->nelts--;
	}
	return 0;
}

/* assist function */
static int css__replay_frame_getter_init(css_replay_frame_getter_t* getter)
{
	int ret;

	do{
		ret = css_frame_getter_chain_init(&getter->iif_buckets);
		if(ret < 0){
			break;
		}
		ret = css_frame_getter_chain_init(&getter->cif_buckets);
		if(ret < 0){
			break;
		}
		getter->cif_offset = 0;
		css__replay_video_frame_queue_init(&getter->frame_queue);

		memset(&getter->starttime, 0, sizeof(getter->starttime));
		getter->frame = NULL;
	}while(0);

	getter->result = ret;
	return ret;
}

static int css__replay_frame_getter_open(css_replay_frame_getter_t* getter, const char* filename)
{
	int ret = 0;
	char path[MAX_PATH] = {0};
	size_t path_len;

	ret = css__replay_frame_getter_init(getter);
	strcpy(path, filename);
	path_len = strlen(path);
	if(strcmp(path+path_len-4, ".mp4") == 0){
		path[path_len-4] = '\0';
	}

	sprintf(getter->recordFile.path,"%s.mp4",path);
	sprintf(getter->cifFile.path,"%s.cif",path);
	sprintf(getter->iifFile.path,"%s.iif",path);

	getter->recordFile.data = getter;
	getter->cifFile.data = getter;
	getter->iifFile.data = getter;

	getter->recordFile.loop = getter->loop;
	getter->cifFile.loop = getter->loop;
	getter->iifFile.loop = getter->loop;

	do{
		ret = css_file_reader_open(&getter->recordFile,NULL);
		if(ret < 0)
			break;
		ret = css_file_reader_open(&getter->cifFile,NULL);
		if(ret < 0)
			break;
		ret = css_file_reader_open(&getter->iifFile,NULL);
		if(ret < 0)
			break;
	}while(0);

	getter->result = ret;
	if(ret < 0){
		css__replay_frame_getter_close(getter);
	}
	return ret;
}

static int css__replay_frame_getter_close(css_replay_frame_getter_t* getter)
{
	css_file_reader_close(&getter->recordFile, NULL);
	css_file_reader_close(&getter->cifFile, NULL);
	css_file_reader_close(&getter->iifFile, NULL);

	css_frame_getter_chain_release(&getter->iif_buckets);
	css_frame_getter_chain_release(&getter->cif_buckets);
	css__replay_video_frame_queue_release(&getter->frame_queue);

	return 0;
}

static int css__replay_frame_getter_read_cif(css_replay_frame_getter_t* getter)
{
	int ret;
	uv_buf_t buf;
	buf = uv_buf_init((char*)malloc(CSS_FRAME_GETTER_CIF_BUF_LEN), CSS_FRAME_GETTER_CIF_BUF_LEN);
	if(!buf.base){
		//getter->result = UV_ENOMEM;
		getter->cif_offset = UV_ENOMEM;
		return UV_ENOMEM;
	}
	ret = css_file_reader_read(&getter->cifFile, buf, getter->cif_offset, NULL);
	if(ret <= 0){
		//getter->result = ret;
		free(buf.base);
		if(ret == 0){
			ret = UV_EOF;
		}
		getter->cif_offset = ret;
		return ret;
	}
	if(getter->cifFile.result < buf.len){
		buf.len = getter->cifFile.result;
		getter->cif_offset = UV_EOF;
	}
	else{
		getter->cif_offset += buf.len;
	}
	ret = css_frame_getter_chain_add_cif_bucket(&getter->cif_buckets, &buf);
	free(buf.base);
	return ret;
}

static int css__replay_frame_getter_read_iif(css_replay_frame_getter_t* getter)
{
	int ret;
	uv_buf_t buf;

	buf = uv_buf_init((char*)malloc(CSS_FRAME_GETTER_IIF_BUF_LEN), CSS_FRAME_GETTER_IIF_BUF_LEN);
	if(!buf.base){
		//getter->result = UV_ENOMEM;
		return UV_ENOMEM;
	}
	ret = css_file_reader_read(&getter->iifFile, buf, -1, NULL);
	if(ret <= 0){
		//getter->result = ret;
		free(buf.base);
		return ret;
	}
	if(getter->iifFile.result < buf.len){
		buf.len = getter->iifFile.result;
	}
	ret = css_frame_getter_chain_add_iif_bucket(&getter->iif_buckets, &buf);
	free(buf.base);
	return ret;
}

void css__replay_frame_getter_read_rec_cb(css_file_reader_t* reader, const uv_buf_t buf)
{
	css_replay_frame_getter_t* getter = (css_replay_frame_getter_t*)reader->data;
	css_video_frame_t* frame = (css_video_frame_t*)(buf.base - sizeof(css_video_frame_t));
	if(reader->result <= 0){
		if(reader->result == 0){
			frame->state = UV_EOF;
		}
		else{
			frame->state = reader->result;
		}
	}
	css__replay_video_frame_queue_add(&getter->frame_queue, &frame->q);
	if(reader->result > 0){
		css_frame_getter_chain_move2next(&getter->cif_buckets);
		if(getter->frame_queue.nelts < 2*CSS_FRAME_GETTER_REC_LOW_LEVEL){
			int ret;
			ret = css__replay_frame_getter_read_rec(getter, css__replay_frame_getter_read_rec_cb);
			if(ret < 0){
				getter->frame_queue.state = ret;
			}
		}
	}
}

static int css__replay_frame_getter_read_rec(css_replay_frame_getter_t* getter, void (*cb)(css_file_reader_t* reader, const uv_buf_t buf))
{
	int ret = 0;

	if(getter->cif_buckets.nelts == 0 && getter->cif_offset < 0){
		getter->result = getter->cif_offset;
		return getter->cif_offset;
	}

	if(getter->cif_buckets.rest <= CSS_FRAME_GETTER_CIF_LOW_LEVEL){
		css_cif_struct_t* cif;
		ret = css__replay_frame_getter_read_cif(getter);
		if(ret < 0 && getter->cif_buckets.nelts == 0){
			getter->result = ret;
			return ret;
		}
		if(getter->starttime.year == 0){
			getter->cif_buckets.current = CSS_FRAME_GETTER_CHAIN_GET_HEADER(&getter->cif_buckets);
			cif = CSS_FRAME_GETTER_CHAIN_GET_CURRENT_DATA(&getter->cif_buckets, css_cif_struct_t, queue);
			getter->starttime.year = cif->time.year;
			getter->starttime.month = cif->time.month;
			getter->starttime.day = cif->time.day;
			getter->starttime.milliSeconds = cif->time.milliSeconds;
		}
	}

	if(getter->cif_buckets.rest <= 0){
		getter->result = UV_EOF;
		return UV_EOF;
	}

	do{
		css_cif_struct_t* cif;
		int len;
		css_video_frame_t* frame;

		cif = CSS_FRAME_GETTER_CHAIN_GET_CURRENT_DATA(&getter->cif_buckets, css_cif_struct_t, queue);
		len = cif->len + sizeof(css_video_frame_t);
		frame = (css_video_frame_t*)malloc(len);
		if(!frame){
			ret = UV_ENOMEM;
			getter->result = ret;
			break;
		}
		memset(frame, 0, sizeof(css_video_frame_t));
		frame->frameType = cif->type;
		frame->millisecond = diffsvtime(&cif->time, &getter->starttime);
		frame->sendType = 0;
		frame->frameData = uv_buf_init(((char*)frame) + sizeof(css_video_frame_t), cif->len);
		QUEUE_INIT(&frame->q);

		if(cb){
			ret = css_file_reader_read(&getter->recordFile, frame->frameData, cif->offset, cb);
			if(ret < 0){
				free(frame);
				break;
			}
		}
		else{
			ret = css_file_reader_read(&getter->recordFile, frame->frameData, cif->offset, NULL);
			if(ret <= 0){
				if(ret == 0){
					ret = UV_EOF;
				}
				free(frame);
				getter->result = ret;
				getter->frame = NULL;
				break;
			}

			getter->frame = frame;
			getter->result = ret;

			css_frame_getter_chain_move2next(&getter->cif_buckets);
		}
	}while(0);

	return ret;
}

static int css__replay_frame_getter_set_position(css_replay_frame_getter_t* getter, int32_t position)
{
	int ret = 0;
	return ret;
}

static int css__replay_frame_getter_get_next_frame(css_replay_frame_getter_t* getter,css_frame_getter_cb cb)
{
	int ret = 0;

	if(getter->cif_buckets.nelts == 0 || getter->frame_queue.nelts == 0){
		ret = css__replay_frame_getter_read_rec(getter, NULL);
		getter->result = ret;
	}
	else{
		QUEUE* h;
		ret = css__replay_video_frame_queue_move2next(&getter->frame_queue, &h);
		getter->frame = QUEUE_DATA(h, css_video_frame_t, q);
		getter->result = ret;
	}

	//if(getter->frame_queue.nelts < 2*CSS_FRAME_GETTER_REC_LOW_LEVEL){
	//	int ret;
	//	ret = css__replay_frame_getter_read_rec(getter, css__replay_frame_getter_read_rec_cb);
	//	if(ret < 0){
	//		getter->frame_queue.state = ret;
	//	}
	//}

	return ret;
}


static void css__replay_frame_getter_work(uv_work_t* w)
{
	css_frame_getter_req_t* req;
	css_replay_frame_getter_t* getter;

	req = container_of(w, css_frame_getter_req_t, work_req);
	getter = req->getter;
	switch(req->type){
	case CSS_FRAME_GETTER_REQ_OPEN:
		css__replay_frame_getter_open(getter, req->path);
		break;
	case CSS_FRAME_GETTER_REQ_CLOSE:
		css__replay_frame_getter_close(getter);
		break;
	case CSS_FRAME_GETTER_REQ_GET_NEXT:
		css__replay_frame_getter_get_next_frame(getter, req->cb);
		break;
	case CSS_FRAME_GETTER_REQ_SET_POSITION:
		css__replay_frame_getter_set_position(getter, req->position);
		break;
	default:
		CL_ERROR("bad replay frame getter type!\n");
	}
}

static void css__replay_frame_getter_done(uv_work_t* w, int status)
{
	css_frame_getter_req_t* req;
	css_replay_frame_getter_t* getter;

	req = container_of(w, css_frame_getter_req_t, work_req);
	getter = req->getter;
	if(req->cb)
		req->cb(getter);
	css__replay_frame_getter_req_clean(req);
	free(req);
}


static int css__replay_frame_getter_req_init(css_frame_getter_req_t* req, 
											 int type, 
											 css_replay_frame_getter_t* getter, 
											 const char* filename, 
											 int32_t position, 
											 css_frame_getter_cb cb)
{
	int len;

	if(filename){
		len = strlen(filename) + 1;
		if(len > MAX_PATH){
			return UV_E2BIG;
		}

		req->path = (char*)malloc(len);
		if(req->path == NULL){
			return UV_ENOMEM;
		}
		strcpy(req->path, filename);
	}
	else{
		req->path = NULL;
	}

	req->type = type;
	req->getter = getter;
	req->cb = cb;
	req->position = position;

	return 0;
}

static void css__replay_frame_getter_req_clean(css_frame_getter_req_t* req)
{
	if(req->path){
		free(req->path);
		req->path = NULL;
	}
}

int css_replay_frame_getter_open(css_replay_frame_getter_t* getter, const char* filename, css_frame_getter_cb cb)
{
	if(cb){
		css_frame_getter_req_t* req;
		int ret;

		req = (css_frame_getter_req_t*)malloc(sizeof(css_frame_getter_req_t));
		if(!req){
			return UV_ENOMEM;
		}
		ret = css__replay_frame_getter_req_init(req, CSS_FRAME_GETTER_REQ_OPEN, getter, filename, 0, cb);
		if(ret < 0){
			free(req);
			return ret;
		}
		ret = CSS__REPLAY_FG_QUEUE_JOB(getter->loop, req);
		return ret;
	}
	else{
		css__replay_frame_getter_open(getter, filename);
		return getter->result;
	}
}

int css_replay_frame_getter_close(css_replay_frame_getter_t* getter, css_frame_getter_cb cb)
{
	if(cb){
		css_frame_getter_req_t* req;
		int ret;

		req = (css_frame_getter_req_t*)malloc(sizeof(css_frame_getter_req_t));
		if(!req){
			return UV_ENOMEM;
		}
		ret = css__replay_frame_getter_req_init(req, CSS_FRAME_GETTER_REQ_CLOSE, getter, NULL, 0, cb);
		if(ret < 0){
			free(req);
			return ret;
		}
		ret = CSS__REPLAY_FG_QUEUE_JOB(getter->loop, req);
		return ret;
	}
	else{
		css__replay_frame_getter_close(getter);
		return getter->result;
	}
}

int css_replay_frame_getter_set_position(css_replay_frame_getter_t* getter, int32_t position, css_frame_getter_cb cb)
{
	if(cb){
		css_frame_getter_req_t* req;
		int ret;

		req = (css_frame_getter_req_t*)malloc(sizeof(css_frame_getter_req_t));
		if(!req){
			return UV_ENOMEM;
		}
		ret = css__replay_frame_getter_req_init(req, CSS_FRAME_GETTER_REQ_SET_POSITION, getter, NULL, position, cb);
		if(ret < 0){
			free(req);
			return ret;
		}
		ret = CSS__REPLAY_FG_QUEUE_JOB(getter->loop, req);
		return ret;
	}
	else{
		css__replay_frame_getter_set_position(getter, position);
		return getter->result;
	}
}

int css_replay_frame_getter_get_next_frame(css_replay_frame_getter_t* getter, css_frame_getter_cb cb)
{
	getter->frame = NULL;
	if(cb){
		css_frame_getter_req_t* req;
		int ret;

		req = (css_frame_getter_req_t*)malloc(sizeof(css_frame_getter_req_t));
		if(!req){
			return UV_ENOMEM;
		}
		ret = css__replay_frame_getter_req_init(req, CSS_FRAME_GETTER_REQ_GET_NEXT, getter, NULL, 0, cb);
		if(ret < 0){
			free(req);
			return ret;
		}
		ret = CSS__REPLAY_FG_QUEUE_JOB(getter->loop, req);
		return ret;
	}
	else{
		css__replay_frame_getter_get_next_frame(getter, NULL);
		return getter->result;
	}
}

int css_replay_frame_getter_release_frame(css_replay_frame_getter_t* getter, css_video_frame_t* frame)
{
	free(frame);
	return 0;
}

//right I frame -->offset in cif
static int find_right_I(QUEUE* iif_queue,int32_t time)
{
	int offset;
	css_iif_struct_t* p;
	css_iif_struct_t* pn;

	QUEUE *q;
	QUEUE *qn;

	QUEUE_FOREACH(q,iif_queue)
	{
		p = QUEUE_DATA(q,css_iif_struct_t,queue);
		if(time - p->time.milliSeconds < 0)
		{
			return -1;
		}
		if(time - p->time.milliSeconds == 0)
		{
			offset = p->offset;
			return offset;
		}
		else if(time - p->time.milliSeconds > 0)
		{
			qn = QUEUE_NEXT(q);
			pn = QUEUE_DATA(qn,css_iif_struct_t,queue);
			if(time - pn->time.milliSeconds < 0)
			{
				offset = p->offset;
				return offset;
			}
		}	
	}
	//offset = p->offset;
	return -1;
}


#ifdef CSS_TEST

static void test_open_cb(css_replay_frame_getter_t* getter);
static void test_close_cb(css_replay_frame_getter_t* getter);
static void test_get_cb(css_replay_frame_getter_t* getter);

static void getter_start(void);

static int cnt = 0;
static int len = 0;
static int frame_cnt = 0;
//static css_replay_frame_getter_t getter;

static void test_open_cb(css_replay_frame_getter_t* g)
{
	css_replay_frame_getter_get_next_frame(g, test_get_cb);
}

static void test_close_cb(css_replay_frame_getter_t* g)
{
	cnt++;
	free(g);
	if(cnt < 5){
		getter_start();
	}
}

static void test_get_cb(css_replay_frame_getter_t* g)
{
	
	JNetCmd_Replay_Send_Frame cmd;
	ssize_t len = 0;
	char *buf = NULL;

	if(!g->frame || g->frame->state < 0){
		if(g->frame)
			css_replay_frame_getter_release_frame(g, g->frame);
		css_replay_frame_getter_close(g, test_close_cb);
		printf("frame_cnt: %d\n", frame_cnt);
		return;
	}


	JNetCmd_Replay_Send_Frame_init(&cmd);
	cmd.frameType = g->frame->frameType;
	cmd.sendType = g->frame->sendType;
	cmd.frameData = g->frame->frameData;
	cmd.replayDataLen = g->frame->frameData.len;

	len = css_proto_package_calculate_buf_len((JNetCmd_Header*)&cmd);
	buf = (char *)malloc(len);
	if (buf != NULL){
		css_proto_package_encode(buf, len, (JNetCmd_Header*)&cmd);
		free(buf);
	}

	len += g->frame->frameData.len;
	css_replay_frame_getter_release_frame(g, g->frame);
	frame_cnt++;
	css_replay_frame_getter_get_next_frame(g, test_get_cb);
	//css_replay_frame_getter_close(g, test_close_cb);
}

static void getter_start(void)
{
	char* filename = "20150121121721074.mp4";
	int ret = 0;
	css_replay_frame_getter_t* getter = (css_replay_frame_getter_t*)malloc(sizeof(css_replay_frame_getter_t));
	getter->loop = uv_default_loop();
	len = 0;
	do{
		ret = css_replay_frame_getter_open(getter, filename, test_open_cb);
	}while(0);
}

void test_reader(void)
{
	getter_start();
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

#endif