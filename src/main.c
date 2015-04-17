#include "css.h"
#include "css_protocol_package.h"
#include <stdlib.h>
#include "queue.h"
#include "css_db.h"
#include "css_ini_file.h"
#include "css_job.h"

#ifdef CSS_TEST
#include "css_test.h"
#endif

void signal_handler(uv_signal_t *handle, int signum)
{
	printf("Signal received: %d\n", signum);
	uv_signal_stop(handle);
	uv_stop(handle->loop);
}

#define CSS_LOG(s, ...) printf(s, __VA_ARGS__)

int main(int argc, char* argv[])
{

#ifdef CSS_TEST
	css_do_test_all();
//	css_do_test_by("css_linked_list_suite");		//do some test case alone.
#else
	uv_loop_t loop;
	uv_signal_t sig_int;
	int ret = 0;
	char* config_file = NULL;
	int index = 0;

	uv_loop_init(&loop);
	uv_signal_init(&loop, &sig_int);
	uv_signal_start(&sig_int, signal_handler, SIGINT);
	if(argc > 1){
		index = atoi(argv[1]);
	}
	if (argc > 2) {
		config_file = argv[2];
	}
	ret = css_server_init(config_file, index);
	if (ret < 0) {
		return ret;
	}
	ret = css_server_start();
	uv_run(&loop, UV_RUN_DEFAULT);
	css_server_stop();
	css_server_clean();
#endif
	return 0;
}
