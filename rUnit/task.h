////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    task.h
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __TASK_H__
#define __TASK_H__

#define MAX_FNAME_LEN   256
#define MAX_ERROR_LEN   256
#define TO_MASTER_MSG_TAG 0
#define TO_SLAVE_MSG_TAG 1
#define MASTER_WAIT_TIME 1// SECONDS
#define SERVER_WAIT_TIME 1// SECONDS
#define SLAVE_WAIT_TIME 3// SECONDS

#define TASK_FILE_EXTENSION ".rib"

// protocol message type
#define PRO_INIT_MESSAGE   "#initMessage"
#define PRO_STATUS_MESSAGE "#taskstatus"
#define PRO_TASK_MESSAGE   "#taskdiscription"
#define PRO_HEADER_MESSAGE "#helloClient"
#define PRO_TAIL_MESSAGE   "#serverProcessing"
#define PRO_MESSAGE_TAIL   "|end"

#define TASK_STATUS_READY 1
#define TASK_STATUS_RUNNING 2
#define TASK_STATUS_FINISH 3

#define TASK_SUCCESS 0
#define TASK_ERROR 1

#define TASK_SUCCESS_S "Success"

#define TASK_REDO_YES 1
#define TASK_REDO_NO  0

#define SCHED_SERVER_PORT 5168
#define RENDERING_UNIT_PORT 5169

////////////////////////////////////////////////////////////////////////////////
//  task
////////////////////////////////////////////////////////////////////////////////
typedef struct tag_Task
{
    int id;// task id(unique)
	int status;
    int progress;// task progress, in range [0~100]
	int errorNo;// errno number
	int redoFlag;// 1 need redo
	char errStr[MAX_ERROR_LEN];// coresponding error string
    char fPath[MAX_FNAME_LEN];// absolute path
    char fName[MAX_FNAME_LEN];
}Task;

typedef struct tag_LenJob
{
	std::vector<Task *> taskList, dispatchedList, finishedList;
	char lenPath[MAX_PATH_LEN];
	char jobID[32];
	pthread_mutex_t jobMutex;
	int taskNum, dispatchedTaskNum, finishedTaskNum;
	int memNeeded;
	int preRenderFlag;
	int resWidth;
	int resHeight;
	int sampleRate;
	int isSharedFilesLoaded;
}LenJob;

////////////////////////////////////////////////////////////////////////////////
// task management, inter master-slave comm protocol message header
////////////////////////////////////////////////////////////////////////////////
typedef enum tag_TM_msgType{TYPE_REQUEST = 0, TYPE_NOTASK, TYPE_DISPATCH, TYPE_TERMINATE, TYPE_PROGRESS}TM_msgType;
typedef struct tag_TM_msgHeader
{
    TM_msgType type;
    int from;
}TM_msgHeader;

typedef struct tag_TM_dispatchMsg
{
    TM_msgHeader header;
    Task task;
}TM_dispatchMsg;

typedef struct tag_TM_progressMsg
{
    TM_msgHeader header;
    Task task;
	double renderTime;
	int target;					// target rank for rendering
}TM_progressMsg;

void *taskServer_func(void *args);
void *taskMaster_func(void *args);
void *taskSlave_func(void *args);
void terminateAllSlave(void);
void startTaskServer();
void endTaskServer();
extern int bleman_render(char *workDir, char *fileName, int numThreads);

#endif
