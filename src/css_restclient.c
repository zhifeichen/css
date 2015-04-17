#include <stdio.h>
#include <stdlib.h>
#include "css_restclient.h"
#include "uv/uv.h"
#include "css_util.h"

#ifdef WIN32
#define closesocket closesocket
#define REST_EWOULDBLOCK WSAEWOULDBLOCK
#define rest_getLastError() WSAGetLastError()
#else
#include <unistd.h>
#define closesocket close
#define REST_EWOULDBLOCK EINPROGRESS
#define rest_getLastError() errno
#endif


static int check_complete(const char* str, int length);
static int hexstr2int(const char* str, int* hexstr_len);

int parse_url(const char* url, char* host, unsigned short* port, char* path)
{
	char* tmp, *host_port, *pos;
	char tempurl[1024] ={0};

	if(*url == 0) return 0;

	memcpy(tempurl,url,strlen(url));

	if(strncmp(url, "http://",7) == 0)
		tmp = (char*)tempurl+7;
	else
		tmp = (char*)tempurl;

	if((pos = strstr(tmp,"/")))
	{
		//*pos = 0;
		memcpy(path,pos,strlen(pos));
	}
	else 
		*path = '/';

	if((host_port = strstr(tmp,":")))
	{
		*host_port = 0;
		*port = (unsigned short)atoi(host_port+1);
	}
	else
		*port = 80;

	memcpy(host,tmp,strlen(tmp));

	return 1;
}

int connect_to_server(const char* host, const unsigned short port, long connect_time_out)
{
	struct hostent* phe;
	struct sockaddr_in sin;
	int s;
	struct   timeval time_out;
	int max_s;
	unsigned long nonbloking_mode;

	fd_set r;
	int ret;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	if((phe = gethostbyname(host)))
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	else
		sin.sin_addr.s_addr = inet_addr(host);
	//std::cout << inet_ntoa(sin.sin_addr) << std::endl;
	sin.sin_port = htons(port);
	s = socket(PF_INET, SOCK_STREAM, 0);
	if(s < 0) return -1;
	
#if  defined(__WIN32__) || defined(_WIN32)
	nonbloking_mode =1;// 0: bloking mode; !0: nonbloking mode
	if(ioctlsocket(s, FIONBIO, &nonbloking_mode)){
		closesocket(s);
		return -1;
	}
	max_s = 0;
#else
	int flags = fcntl(s, F_GETFL, 0);
	if(fcntl(s, F_SETFL, flags | O_NONBLOCK)){
		closesocket(s);
		return -1;
	}
	max_s = s+1;
#endif
	if(connect(s , (struct sockaddr*) &sin, sizeof(sin)) == -1 && REST_EWOULDBLOCK != rest_getLastError()){
		closesocket(s);
		return -1;
	}
	time_out.tv_sec = connect_time_out;
	time_out.tv_usec = 0;
	
	while(1){
		FD_ZERO(&r);
		FD_SET(s, &r);
		ret = select(max_s, 0, &r, 0, &time_out);
		if( ret <= 0 ) 
		{
			closesocket(s);
			return -1;
		}
		if(FD_ISSET(s, &r)) break;
	}
#if  defined(__WIN32__) || defined(_WIN32)
	nonbloking_mode = 0; 
	ret   =   ioctlsocket(s, FIONBIO, &nonbloking_mode); 
	if(ret==SOCKET_ERROR){ 
		closesocket(s); 
		return -1;
	} 
#else
	flags = fcntl(s, F_GETFL, 0);
	if(fcntl(s, F_SETFL, flags & ~ O_NONBLOCK)){
		closesocket(s);
		return -1;
	}
#endif

	return s;
}

int send_request(int s, const char* path, const char* token, const char* host, const unsigned short port)
{
	char request[RESPONSE_BUFF_LEN]={0};
	char strport[10]={0};
	if(token==NULL||*token==0)
	{
		strcat(request,"GET ");
		strcat(request,path);
		strcat(request," HTTP/1.1\r\n");
		strcat(request,"Host: ");
		strcat(request,host);
		strcat(request,":");
		strcat(request,ITOA(port,strport,10));
		strcat(request,"\r\n");
		strcat(request,"Connection: keep-alive\r\n");
		strcat(request,"\r\n");
	}
	else
	{
		strcat(request,"GET ");
		strcat(request,path);
		strcat(request," HTTP/1.1\r\n");
		strcat(request,"Host: ");
		strcat(request,host);
		strcat(request,":");
		strcat(request,ITOA(port,strport,10));
		strcat(request,"\r\n");
		strcat(request,"Cookie: csst_token=");
		strcat(request,token);
		strcat(request,"\r\n");
		strcat(request,"Connection: keep-alive\r\n");
		strcat(request,"\r\n");
	}
	return send(s,request,strlen(request), 0);
	//shutdown(s, REST_SEND);
}

int send_post_request(int s, const char* path, const char* data, const char* token, const char* host, const unsigned short port)
{
	char request[RESPONSE_BUFF_LEN];
	char strport[10]={0};
	if(!strcmp(token,""))
	{
		strcat(request,"POST ");
		strcat(request,path);
		strcat(request," HTTP/1.1\r\n");
		strcat(request,"Host: ");
		strcat(request,host);
		strcat(request,":");
		strcat(request,ITOA(port,strport,10));
		strcat(request,"\r\n");
		strcat(request,"Content-Type: application/x-www-form-urlencoded\r\n");
		strcat(request,"\r\n");
	}
	else
	{
		strcat(request,"POST ");
		strcat(request,path);
		strcat(request," HTTP/1.1\r\n");
		strcat(request,"Host: ");
		strcat(request,host);
		strcat(request,":");
		strcat(request,ITOA(port,strport,10));
		strcat(request,"\r\n");
		strcat(request,"Cookie: csst_token=");
		strcat(request,token);
		strcat(request,"\r\n");
		strcat(request,"Content-Type: application/x-www-form-urlencoded\r\n");
		strcat(request,"Content-Length:");
		memset(strport,0,10);
		strcat(request,ITOA(strlen(data),strport,10));
		strcat(request,"\r\n");
		strcat(request,"Connection: keep-alive\r\n");
		strcat(request,"\r\n");
		strcat(request,data);
	}
	return send(s, request, strlen(request), 0);
	//Sleep(100);
	//shutdown(s, REST_SEND);
}

int get_response(int s, char* result)
{
	int received = 0, ret;


	while((ret = recv(s, result+received, MSG_BUGLEN-received, 0)) > 0){

		received += ret;
		if(check_complete(result, received) == 0) break;
	}
	return received;
}

int check_complete(const char* str, int length)
{
	const char* header_complete = NULL;
	const char* content_length = NULL;
	int rnrn = 0, cl = 0;
	header_complete = strstr(str, "\r\n\r\n");
	if(header_complete){
		rnrn = (int)(header_complete - str);
		content_length = strstr(str, "Content-Length:");
		if(content_length){
			sscanf(content_length, "Content-Length: %d", &cl);
			return( length - (rnrn+4) - cl);
		}
		else{
			content_length = strstr(str, "Transfer-Encoding: chunked");
			if(content_length){
				int ok = strncmp(str+length-5, "0\r\n\r\n", 5);
				return ok;
			}
		}
	}
	return -1;
}

int get_response_body(const char* response, char* body)
{
	int state = -1;
	char* pos_header;
	char* chunked;
	char * s;

	char* pos;
	char* buff;
	buff = (char*)malloc(MSG_BUGLEN);
	memset(buff,0,MSG_BUGLEN);
	memcpy(buff,response,strlen(response));


	if((pos = strstr(buff,"\r\n")))
	{
		*pos = 0;
		s = buff+8;
		while(*s == ' ' || *s == '\t')
			s++;
		if((pos = strchr(s,' ')))
		{
			*pos = 0;
			state = atoi(s);
		}
		else
		{
			pos = strchr(s,'\t');
		}
	}

	memcpy(buff,response,strlen(response));

	if((chunked = strstr(buff,"Transfer-Encoding: chunked\r\n")))
	{
		int chunk_len, hexstr_len;
		char *body_pos_;
		int res_body = 0;
		
		char* response_body_ = (char*)malloc(RESPONSE_BUFF_LEN);
		memset(response_body_,0,RESPONSE_BUFF_LEN);
		
		pos_header = strstr(buff,"\r\n\r\n");
		body_pos_ = pos_header+4;
		while((chunk_len = hexstr2int(body_pos_,&hexstr_len)))
		{
			//memset(response_body_, 0, RESPONSE_BUFF_LEN);
			body_pos_ += hexstr_len;
			memcpy(response_body_+res_body, body_pos_, chunk_len);
			res_body += chunk_len;
			body_pos_ += chunk_len;
		}
		memcpy(body,response_body_,strlen(response_body_));
		free(response_body_);
	}
	else
	{
		if((pos_header = strstr(buff,"\r\n\r\n")))
			memcpy(body,pos_header+4,strlen(pos_header+4));
	}

	free(buff);
	return state;
}

int hexstr2int(const char* str, int* hexstr_len)
{
	int chunk_len = 0;
	int base = 0;
	*hexstr_len = 0;
	while(*str == '\r' || *str == '\n'){
		(*hexstr_len)++;
		str++;
	}
	while(*str != '\r' || *str != '\n'){
		if(*str >= '0' && *str <= '9'){
			base = *str - '0';
		}
		else if(*str >= 'a' && *str <= 'f'){
			base = *str - 'a' + 10;
		}
		else if(*str >= 'A' && *str <= 'F'){
			base = *str - 'A' + 10;
		}
		else{
			break;
		}
		chunk_len = chunk_len * 16 + base;
		str++;
		(*hexstr_len)++;
	}
	/*while(*str == '\r' || *str == '\n'){
		hexstr_len++;
		str++;
	}*/
	*hexstr_len += 2;
	str++;str++;
	return chunk_len;
}

int get_rest(const char* url, char* body, const char* token,long connect_time_out)
{
	int result = -1;
	int state = result;
	char* host, *path, *response;
	unsigned short port;
	int s;
	host = (char*)malloc(100);
	memset(host,0,100);
	path = (char*)malloc(RESPONSE_BUFF_LEN);
	memset(path,0,RESPONSE_BUFF_LEN);
	response = (char*)malloc(MSG_BUGLEN);
	memset(response,0,MSG_BUGLEN);

	if(parse_url(url, host, &port, path)){
		s = connect_to_server(host, port, connect_time_out);
		if(s > 0){
			if(send_request(s, path, token, host, port) > 0){
				if(get_response(s, response) > 0){
					state = get_response_body(response, body);
					//if(state >= 200 && state < 300)
						result = state;
				}
			}
			closesocket(s);
		}
	}
	free(host);
	free(path);
	free(response);
	return result;
}

int post_rest(const char* url, const char* data, char* body,const char* token, long connect_time_out)
{
	int result = -1;
	int state = result;
	char* host, *path, *response;
	unsigned short port;
	int s;

	host = (char*)malloc(100);
	memset(host,0,100);
	path = (char*)malloc(RESPONSE_BUFF_LEN);
	memset(path,0,100);
	response = (char*)malloc(MSG_BUGLEN);
	memset(response,0,MSG_BUGLEN);

	if(parse_url(url, host, &port, path)){
		s = connect_to_server(host, port, connect_time_out);
		if(s > 0){
			if(send_post_request(s, path, data, token, host, port) > 0){
				if(get_response(s, response) > 0){
					state = get_response_body(response, body);
					if(state >= 200 && state < 300)
						result = 0;
				}
			}
			closesocket(s);
		}
	}
	free(host);
	free(path);
	free(response);
	return result;
}