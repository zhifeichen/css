#ifndef __CSS_SEARCH_RECORD_H__
#define __CSS_SEARCH_RECORD_H__

#include "css_db.h"
#include "css_stream.h"

typedef struct{
	css_search_conditions_t conds;
	css_stream_t* stream;
	uv_buf_t buf;
}css_search_record_t;

void search_record(uv_work_t* req);
void after_search_record(uv_work_t* req, int status);

#endif /* __CSS_SEARCH_RECORD_H__ */
