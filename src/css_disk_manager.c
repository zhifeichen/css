#include "css_disk_manager.h"
#include "css_util.h"
#include "css.h"
#include "assert.h"
#include "css_linkedlist.h"
#include "css_clear_disk.h"
#include "uv/uv.h"
#include <string.h>
#include "css_logger.h"

#define REFRESH_DISK_TIME 5*60*1000

static css_disk_config* m_disk_config = NULL;
static css_linked_list m_disk_list;
static uv_timer_t timer;

int css_get_disk_status(disk_status** myDiskStatus)
{
	uint64_t free_size, total_size, other_size;
	int32_t error_code;
#ifdef _WIN32
	SetLastError(0);
	if(GetDiskFreeSpaceExA((*myDiskStatus)->disk_name,(PULARGE_INTEGER)&free_size,(PULARGE_INTEGER)&total_size,(PULARGE_INTEGER)&other_size) == FALSE); //返回bool（0 FALSE）
	{
		error_code = GetLastError();
		if (error_code != 0)
		{
			CL_ERROR("disk name: %s ,get disk space error! win_err_code: %d \n" ,(*myDiskStatus)->disk_name,error_code);
			(*myDiskStatus)->err_tag = 1;
			SetLastError(0);
			return -1;
		}
	}
	total_size = total_size / 1024 / 1024;
	free_size = free_size / 1024 / 1024;
#else
	struct statvfs m_statvfs;
	if (statvfs((*myDiskStatus)->disk_name, &m_statvfs) < 0) {
		CL_ERROR("disk name: %s ,get disk space error! \n",(*myDiskStatus)->disk_name);
		return -1;
	}
	total_size = m_statvfs.f_blocks * m_statvfs.f_frsize / 1000 / 1000;	//MAC计算方式除1000
	free_size = m_statvfs.f_bfree * m_statvfs.f_frsize / 1000 / 1000;//MAC计算方式除1000,OSX下将link文件夹也计算到已使用。
#endif
	if (total_size == 0) {
		(*myDiskStatus)->err_tag = 1;
		CL_ERROR("disk space is zero! \n");
		return -1;
	}
	(*myDiskStatus)->disk_total_size = total_size;
	(*myDiskStatus)->disk_unused_size = free_size;
	(*myDiskStatus)->weight = 0;
	(*myDiskStatus)->used_per = (int32_t) ((total_size - free_size) * 100
			/ total_size);
	CL_DEBUG("disk: %s ,total: %lld MB,free: %lld MB,usedPer: %d%% \n",(*myDiskStatus)->disk_name,total_size,free_size,(*myDiskStatus)->used_per);
	return 0;
}

static void refresh_disks_status_cb(void *data)
{
	disk_status* myDiskStatus = (disk_status*) data;
	css_get_disk_status(&myDiskStatus);
}

static void clear_disk_cb(void *data)
{
	disk_status* myDiskStatus = (disk_status*) data;
	if (myDiskStatus->used_per > myDiskStatus->max_per)
	{
		css_clear_disk(myDiskStatus);
	}
}

static void after_refresh_disk_work_cb(uv_work_t* req, int status)
{
	css_list_foreach(&m_disk_list, clear_disk_cb);
	FREE(req);
}
static void refresh_disk_work_cb(uv_work_t* req)
{
	CL_DEBUG("refresh disk status \n");
	css_list_foreach(&m_disk_list, refresh_disks_status_cb);
}
static void refresh_disk_timer_cb(uv_timer_t* handle)
{
	uv_work_t* w = (uv_work_t*) malloc(sizeof(uv_work_t));
	uv_queue_work(m_disk_config->myLoop, w, refresh_disk_work_cb,
			after_refresh_disk_work_cb);
}

int css_init_disk_manager(css_disk_config* myDiskConfig)
{
	char* p;
	//uv_work_t w;

	CHECK_INIT(m_disk_config, css_disk_config);
	CL_INFO("init css_disk_config ... \n");
	//printf("init css_disk_config ... \n");
	CHECK_CONFIG(myDiskConfig);
	m_disk_config = (css_disk_config*) malloc(sizeof(css_disk_config));
	COPY_STR(m_disk_config->disk_paths, myDiskConfig->disk_paths);
	m_disk_config->max_per = myDiskConfig->max_per;
	m_disk_config->init_flag = 0;

	css_list_init(&m_disk_list);
	p = strtok(m_disk_config->disk_paths, ";");
	while (p) {
		disk_status* myDiskStatus = (disk_status*) malloc(sizeof(disk_status));
		myDiskStatus->disk_name = (char*) malloc(strlen(p) + 1);
		strcpy(myDiskStatus->disk_name, p);
		myDiskStatus->disk_total_size = 0;
		myDiskStatus->disk_unused_size = 0;
		myDiskStatus->weight = 0;
		myDiskStatus->err_tag = 0;
		myDiskStatus->clear_file_day = myDiskConfig->clear_file_day;
		myDiskStatus->max_per = myDiskConfig->max_per;
		css_list_push(&m_disk_list, myDiskStatus);
		p = strtok(NULL, ";");
	}
	if (css_list_is_empty(&m_disk_list)) {
		CL_ERROR("disk list is empty! \n");
		//printf("disk list is empty! \n");
		return -1;
	}
	m_disk_config->init_flag = 1;

	if (myDiskConfig->myLoop == NULL) {
		m_disk_config->myLoop = uv_default_loop();
	} else {
		m_disk_config->myLoop = myDiskConfig->myLoop;
	}
	uv_timer_init(m_disk_config->myLoop, &timer);
	uv_timer_start(&timer, refresh_disk_timer_cb, 0, REFRESH_DISK_TIME);

	return 0;
}

static int css_get_disk(char* diskPath, int len, int count)
{
	//disk_status* myDiskStatus;
	disk_status* minDisk;
	if (count >= css_list_size(&m_disk_list)) {
		CL_ERROR("no disk can use , check disks !! \n");
		//printf("no disk can use , check disks !! \n");
		return -1;
	}
	css_list_pop(&m_disk_list, (void**) &minDisk);
	css_list_push(&m_disk_list, minDisk);
	if ((minDisk->err_tag == 1)
			|| ((minDisk->used_per) > (m_disk_config->max_per))) {
		return css_get_disk(diskPath, len, count + 1);
	}
	CHECK_COPY_LEN((int32_t )strlen(minDisk->disk_name), (len - 1));
	strcpy(diskPath, minDisk->disk_name);
	return 0;
}

static int get_disk_path(int type, uint64_t dvrEquipId, uint16_t channelNo, char* filePath,
	int len, int* volumePostion)
{
	char diskPath[20];
	char myFilePath[MAX_PATH];
	int ret = 0;
	if (css_get_disk(diskPath, 20, 0) < 0) {
		CL_ERROR("get disk error \n");
		//printf("get disk error \n");
		return -1;
	}
	(*volumePostion) = (int)strlen(diskPath);
	switch(type)
	{
		case CSS_VIDEO_FILE_PATH:
			CHECK_COPY_LEN(sprintf(myFilePath,FILE_PATH,diskPath,dvrEquipId,channelNo),(len - 1));
			break;
		case CSS_WARN_FILE_PATH:
			CHECK_COPY_LEN(sprintf(myFilePath,WARN_FILE_PATH,diskPath,dvrEquipId,channelNo),(len - 1));
	}
			
	strcpy(filePath, myFilePath);
	if ((ret = ensure_dir(filePath)) != 0) {
		CL_ERROR("create file path: %s error code: %d!!\n", filePath, ret);
		return -1;
	}
	return 0;
}

int css_get_disk_path(uint64_t dvrEquipId, uint16_t channelNo, char* filePath,
		int len, int* volumePostion)
{
	return get_disk_path(CSS_VIDEO_FILE_PATH,dvrEquipId,channelNo,filePath,len,volumePostion);
}
int css_get_disk_warn_path(uint64_t dvrEquipId, uint16_t channelNo, char* filePath,
	int len, int* volumePostion)
{
	return get_disk_path(CSS_WARN_FILE_PATH,dvrEquipId,channelNo,filePath,len,volumePostion);
}

void refresh_disk_status()
{
	css_list_foreach(&m_disk_list, refresh_disks_status_cb);
}

static void destroy_disk_manager_cb(void *data)
{
	disk_status* myDiskStatus = (disk_status*) data;
	css_list_remove(&m_disk_list, myDiskStatus);
	FREE(myDiskStatus->disk_name);
	FREE(myDiskStatus);
}

void css_destroy_disk_manager()
{
	CL_INFO("destroy css_disk_config!!! \n");
	//printf("destroy css_disk_config!!! \n");
	uv_timer_stop(&timer);
	uv_close((uv_handle_t*)&timer, NULL);
	FREE(m_disk_config);
	css_list_foreach(&m_disk_list, destroy_disk_manager_cb);
}

#ifdef CSS_TEST

#include "assert.h"
#include "css_ini_file.h"

void test_disk_manager(void)
{
	disk_status* myDiskStatus;
	char filePath[1024] = "";
	char filePath2[1024] = "";
	char warnFilePath[1024] = "";
	int volumePostion = 0;
	css_disk_config myDiskConfig;

	css_load_ini_file(DEFAULT_CONFIG_FILE);
	css_get_disk_config(&myDiskConfig);
	css_destory_ini_file();
	myDiskConfig.myLoop = NULL;

#ifdef _WIN32
//css_disk_config myDiskConfig = {"C:;D:;ZZ:",85};
#else
//css_disk_config myDiskConfig = {"/;.;/testtest",85};
	strcpy(myDiskConfig.disk_paths,".");
#endif // _WIN32

	printf("disk manager test start ... \n");
//test init
	assert(css_list_size(&m_disk_list) == 0);
	assert(css_init_disk_manager(&myDiskConfig) == 0);
	assert(css_list_size(&m_disk_list) > 0);
	assert(css_init_disk_manager(&myDiskConfig) == -1);

	refresh_disk_status();

	assert(css_list_get(&m_disk_list,0,(void**)&myDiskStatus) == 0);
	assert((strcmp(myDiskStatus->disk_name,"E:") && strcmp(myDiskStatus->disk_name,".")) == 0);
	assert(myDiskStatus->used_per >= 0);
	assert(myDiskStatus->used_per <= 100);

//test get disk path
	assert(strcmp(filePath,"") == 0);
	css_get_disk_path(2,23,filePath,1024,&volumePostion);
#ifdef _WIN32
	assert(volumePostion == 2);
	assert(strstr(filePath,"E:/dvr_data/2/23") != 0);
#else
	assert(volumePostion == 1);
	assert(strstr(filePath,"./dvr_data/2/23") != 0);
#endif
	assert(ACCESS(filePath,0) == 0);

	assert(strcmp(filePath2,"") == 0);
	css_get_disk_path(1,1,filePath2,1024,&volumePostion);
#ifdef _WIN32
	assert(volumePostion == 2);
	assert(strstr(filePath2,"E:/dvr_data/1/1") != 0);
#else
	assert(volumePostion == 1);
	assert(strstr(filePath2,"./dvr_data/1/1") != 0);
#endif
	assert(ACCESS(filePath2,0) == 0);

	assert(strcmp(warnFilePath,"") == 0);
	css_get_disk_warn_path(1,1,warnFilePath,1024,&volumePostion);
#ifdef _WIN32
	assert(volumePostion == 2);
	assert(strstr(warnFilePath,"E:/warn_data/1/1") != 0);
#else
	assert(volumePostion == 1);
	assert(strstr(warnFilePath,"./warn_data/1/1") != 0);
#endif
	assert(ACCESS(warnFilePath,0) == 0);

	unlink(filePath);
	unlink(filePath2);
	unlink(warnFilePath);

//test destroy 
	css_destroy_disk_manager();
	assert(css_list_is_empty(&m_disk_list) == 1);
	printf("disk manager test end! \n");
}

void test_disk_manager_suite()
{
	test_disk_manager();
}

#endif
