////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    taskServer.cpp
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "rUnit.h"
#include "task.h"
#include "threadPool.h"
#include "dmwrapper.h"
// parse job message
// thread-unsafe, should be call by job recerver
static int parseJobMessage(char *buf)
{
	const char *jobMsgHeader = "#taskdiscription#";
	LenJob *job = (LenJob *)malloc(sizeof(LenJob));
	memset(job, 0, sizeof(LenJob));
	job->taskList.clear();
	job->dispatchedList.clear();
	job->finishedList.clear();
	pthread_mutex_init(&job->jobMutex, NULL);

	assert(buf != NULL && 0 == strncmp(buf, jobMsgHeader, strlen(jobMsgHeader)));
	char *ptr = buf + strlen(jobMsgHeader);
	ptr = strtok(ptr, "#");
	strcpy(job->jobID, ptr);
	ptr = strtok(NULL, "#");
	strcpy(job->lenPath, ptr);
	ptr = strtok(NULL, "#");
	job->taskNum = atoi(ptr);
	
	// insert tasks to list
	for(int i = 0; i < job->taskNum; i++)
	{
		Task *task = (Task *)malloc(sizeof(Task));
		memset(task, 0, sizeof(Task));
		task->id = getNextTaskID();
		task->status = TASK_STATUS_READY;
		task->progress = 0;
		task->errorNo = TASK_SUCCESS;
		task->redoFlag = TASK_REDO_NO;
        strcpy(task->errStr, TASK_SUCCESS_S);
        
		strcpy(task->fPath, job->lenPath);
		ptr = strtok(NULL, "#");
		strcpy(task->fName, ptr);
		job->taskList.push_back(task);
	}

	// other info
	ptr = strtok(NULL, "#");	job->memNeeded = atoi(ptr);
	ptr = strtok(NULL, "#");	job->preRenderFlag = atoi(ptr);
	ptr = strtok(NULL, "#");	job->resWidth = atoi(ptr);
	ptr = strtok(NULL, "#");	job->resHeight = atoi(ptr);
	ptr = strtok(NULL, "|");	job->sampleRate = atoi(ptr);

	//////////////////////////////
	// load shared data
	//////////////////////////////
	if(FALSE == job->isSharedFilesLoaded)
	{
		dmLoadSharedFiles(job);
		job->isSharedFilesLoaded = TRUE;
	}

	// insert to len list
	pthread_mutex_lock(&gl_cfg.lenMutex);
	gl_cfg.lenList.push_back(job);
	gl_cfg.totalTasks += job->taskNum;
	pthread_mutex_unlock(&gl_cfg.lenMutex);

	DEBUG_PRINT("\n%s\n%s\n%d\n%d\n%d\n%d\n%d\n%d\n",
				job->lenPath,
				job->jobID,
				job->taskNum,
				job->memNeeded,
				job->preRenderFlag,
				job->resWidth,
				job->resHeight,
				job->sampleRate);

	return 1;
}
// one job message per time
// close on recv
static void *jobReceiver_func(void *args)
{
	DEBUG_PRINT("Job Receiver running\n");
	// listen
	int servfd = listenPort(RENDERING_UNIT_PORT);
	char msgBuf[128];

	while(gl_cfg.flag != STATUS_EXIT)
	{
		// wait for connection
		// TODO: blocking wait, seems rendering unit only receive one task message
		//       so we can just exit this thread after receive it
		int clientfd = accept(servfd, NULL, NULL);
		if(clientfd < 0)
		{
			// do nothing currently
			perror("Failed to accept connection");
			continue;
		}
		//////////////////////////////
		// send header message
		// TODO: don't check result
		//////////////////////////////
		sprintf(msgBuf, "%s%s", PRO_HEADER_MESSAGE, PRO_MESSAGE_TAIL);
		sendMessage(clientfd, msgBuf, strlen(msgBuf));

		// recv message
		char *jobMsg = recvMessageWithTail(clientfd, PRO_MESSAGE_TAIL);

		if(NULL != jobMsg)
		{
			printf("Recv Message : %s\n", jobMsg);
			// parse message
			parseJobMessage(jobMsg);

			// free message
			free(jobMsg);
		}
		//////////////////////////////
		// send tail message
		// TODO: don't check result
		//////////////////////////////
		sprintf(msgBuf, "%s%s", PRO_TAIL_MESSAGE, PRO_MESSAGE_TAIL);
		sendMessage(clientfd, msgBuf, strlen(msgBuf));

		close(clientfd);
	}
	// this line will never be executed
	close(servfd);
	
	DEBUG_PRINT("Job Receiver ended\n");
}

char *genProgressMessage(std::vector<Task *> list)
{
	int len = list.size();
	char *buf = (char *)malloc(sizeof(char)*(MAX_FNAME_LEN+MAX_ERROR_LEN+64)*len);
	char *ptr = buf;

	sprintf(ptr, "#taskstatus#%s#%d", gl_cfg.lenList[0]->jobID, len);
	for(int i = 0; i < len; i++)
	{
		ptr += strlen(ptr);
		sprintf(ptr, "#rendering_frameprogress#%s#%d#%d#%d#%s#%d|progress",
				list[i]->fName, list[i]->status, list[i]->progress,
				list[i]->errorNo, list[i]->errStr, list[i]->redoFlag);
	}
	ptr += strlen(ptr);
    int allJobFinished = (gl_cfg.totalTasks == gl_cfg.totalFinished);
	sprintf(ptr, "#%d|end", allJobFinished);

	return buf;
}

static int pushMessage(char *buf)
{
	// connect to server and send message
	int fd = connectToServer(gl_cfg.serverAddr, SCHED_SERVER_PORT);
	//assert(fd >= 0);// we assume we can connect to the server
	if(fd < 0) return -1;
	
	//////////////////////////////
	// recv header message
	//////////////////////////////
	char *headMsg = recvMessageWithTail(fd, PRO_MESSAGE_TAIL);
	DEBUG_PRINT("---Received head message %s\n", headMsg);
	free(headMsg);

	//////////////////////////////
	// send message
	//////////////////////////////
	int ret = sendMessage(fd, buf, strlen(buf));
	DEBUG_PRINT("---Message sended with status[%d] [%s]\n", ret, buf);
	//////////////////////////////
	// recv tail message
	//////////////////////////////
	char *tailMsg = recvMessageWithTail(fd, PRO_MESSAGE_TAIL);
	DEBUG_PRINT("---Received tail message %s\n", tailMsg);
	free(tailMsg);

	close(fd);

	return ret;
}

static void *messagePush_func(void *args)
{
	DEBUG_PRINT("Job progress pusher started\n");
	char *buf = (char *)malloc(64);
	int pendingMsg;
	LenJob *job = NULL;

	////////////////////////////////
	// hands shaking with sched server
	// send init message to sched server
	// we use pending message
	// for it has to be sent first
	// sprintf(buf, "#start#%s|end", gl_cfg.localIP);
	sprintf(buf, "%s%s", PRO_INIT_MESSAGE, PRO_MESSAGE_TAIL);
	pendingMsg = TRUE;

	while(gl_cfg.flag != STATUS_EXIT)
	{
		// when there are pending messages, send first
		if(pendingMsg)
		{
			if(pushMessage(buf) > 0)
			{
				pendingMsg = FALSE;
				free(buf);
			}
		}else if(gl_cfg.pendingReport != 0)
		{
			// if there are finished task, then connect to server
			// else wait for a while
			job = gl_cfg.lenList[0];

			//////////////////////
			// currently only 1 len is supported
			//////////////////////
			pthread_mutex_lock(&job->jobMutex);
			// copy finished list to tmp list and clear it
			std::vector<Task *> tmpList(job->finishedList);
			job->finishedList.clear();
			pthread_mutex_unlock(&job->jobMutex);

			pthread_mutex_lock(&gl_cfg.lenMutex);
			gl_cfg.totalReported += tmpList.size();
			gl_cfg.pendingReport -= tmpList.size();
			pthread_mutex_unlock(&gl_cfg.lenMutex);

			// generate progress message
			buf = genProgressMessage(tmpList);
			tmpList.clear();
			if(pushMessage(buf) < 0) pendingMsg = TRUE;
			else free(buf);
		}
		// sleep for several second
		mySleep(SERVER_WAIT_TIME);
	}
	// save pending message

	DEBUG_PRINT("Job progress pusher ended\n");
}

static void initTaskList()
{
	gl_cfg.lenList.clear();
	return;

    // // for local test
    // DIR * dir = opendir(gl_cfg.ribPath);
    // if(NULL == dir)
    // {
    //     printf("Failed to open [%s]:%s\n", gl_cfg.ribPath, strerror(errno));
    //     return;
    // }
    // struct dirent * dirItem;
    // while((dirItem = readdir(dir)) != NULL)
    // {
    //     int len = strlen(dirItem->d_name);
    //     if(len < 4 || 0 != strcmp(&dirItem->d_name[len-4], TASK_FILE_EXTENSION)) continue;// filter all file not *.rib
        
    //     // insert task
    //     Task *task = (Task *)malloc(sizeof(Task));
    //     task->id = getNextTaskID();
    //     task->progress = 0;
    //     strcpy(task->fPath, gl_cfg.ribPath);
    //     strcpy(task->fName, dirItem->d_name);
        
    //     // sync operation
    //     pthread_mutex_lock(&gl_cfg.listMutex);
    //     gl_cfg.taskList.push_back(task);
    //     gl_cfg.totalTaskNum++;
    //     pthread_mutex_unlock(&gl_cfg.listMutex);
    // }
    // closedir(dir);
}
static pthread_t jobServer, progressClient;
void startTaskServer()
{
	pthread_create(&jobServer, NULL, jobReceiver_func, NULL);
	pthread_create(&progressClient, NULL, messagePush_func, NULL); 
}
void endTaskServer()
{
	pthread_join(jobServer, NULL);
	pthread_join(progressClient, NULL);
}

// void *taskServer_func(void *args)
// {
//     DEBUG_PRINT("Task Server[%d] started\n", gl_cfg.commRank);
    
//     // accept rendering request from clients
//     initTaskList();//
//     mySleep(1);// mySleep for one second
    
//     while(gl_cfg.totalFinished != gl_cfg.totalTasks)
//     {
//         // if it's not exit request, forward to thread pool
//         // else set exit flag and exit
//         DEBUG_PRINT("------->Server Sleeping\n");
//         mySleep(SERVER_WAIT_TIME);
//         //break;
//     }
//     gl_cfg.flag = STATUS_EXIT;
    
//     DEBUG_PRINT("Task Server[%d] ended\n", gl_cfg.commRank);
//     return NULL;
// }
