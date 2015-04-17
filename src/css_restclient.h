#ifndef GET_REST_H
#define GET_REST_H


#define  RESPONSE_BUFF_LEN  20*1024
#define MSG_BUGLEN 20*1024*1024

// get_rest: use GET method; post_rest: use POST method
// url: in
// data: in,  post data
// body: out, response body
// connect_time_out: in, connect time out, second
// token: in, user token
// return: 0 success, else failure
int get_rest(const char* url, char* body,  const char* token,long connect_time_out);
int post_rest(const char* url, const char* data, char* body,  const char* token ,long connect_time_out);


#endif //#ifndef GET_REST_H