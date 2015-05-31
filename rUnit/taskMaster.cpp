////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    taskMaster.cpp
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

void terminateAllSlave(void)
{
    TM_msgHeader buf;
    MPI_Request *request = new MPI_Request[gl_cfg.commSize - 1];
    MPI_Status *status   = new MPI_Status[gl_cfg.commSize - 1];
    
    buf.type = TYPE_TERMINATE;
    buf.from = gl_cfg.commRank;
    
    // send a terminatation message to all slave and wait
    for(int i = 0; i < gl_cfg.commSize - 1; i++)
    {
        MPI_Isend(&buf, sizeof(buf), MPI_CHAR, i+1, TO_SLAVE_MSG_TAG, gl_cfg.taskComm, &request[i]);
    }
    MPI_Waitall(gl_cfg.commSize - 1, request, status);
    
    delete [] request;
    delete [] status;
}

// responsible to release msg
void *processMessageFromSlave(void *msg)
{
    assert(msg != NULL);
    Task *pTask = NULL;
	LenJob *job = NULL;
    TM_msgHeader *pHeader = (TM_msgHeader *)msg;

    switch(pHeader->type)
    {
    case TYPE_REQUEST:
        // DEBUG_PRINT("Recv request from %d\n", pHeader->from);
        // get task from len 0
		if(!gl_cfg.lenList.empty())
		{
			job = gl_cfg.lenList[0];
			pthread_mutex_lock(&job->jobMutex);
			if(!job->taskList.empty()) 
			{
				// FIFO
				pTask = job->taskList.front();
				job->taskList.erase(job->taskList.begin());
			}
			pthread_mutex_unlock(&job->jobMutex);
		}
        
        // send task to client
        if(NULL == pTask)
        {
            TM_msgHeader buf;
            buf.type = TYPE_NOTASK;
            buf.from = gl_cfg.commRank;
            MPI_Send(&buf, sizeof(buf), MPI_CHAR, pHeader->from, TO_SLAVE_MSG_TAG, gl_cfg.taskComm);
        }else
        {
            TM_dispatchMsg buf;
            buf.header.type = TYPE_DISPATCH;
            buf.header.from = gl_cfg.commRank;
            memcpy(&buf.task, pTask, sizeof(Task));
            MPI_Send(&buf, sizeof(buf), MPI_CHAR, pHeader->from, TO_SLAVE_MSG_TAG, gl_cfg.taskComm);
            
            // add to dispatched list
            pthread_mutex_lock(&job->jobMutex);
            job->dispatchedList.push_back(pTask);
            job->dispatchedTaskNum++;
            pthread_mutex_unlock(&job->jobMutex);
			
			// modi total variale
			pthread_mutex_lock(&gl_cfg.lenMutex);
			gl_cfg.totalDispatched++;
			pthread_mutex_unlock(&gl_cfg.lenMutex);
        }
        break;
    case TYPE_PROGRESS:
        // record progress
        TM_progressMsg *pMsg = (TM_progressMsg *)msg;

		// there must have one len if we can receive progress message
        job = gl_cfg.lenList[0];
        for(std::vector<Task *>::iterator iter = job->dispatchedList.begin(); iter != job->dispatchedList.end(); iter++)
        {
            if((*iter)->id != pMsg->task.id) continue;
            
            // delete element in dispatched List
            // push it into finished list
            pthread_mutex_lock(&job->jobMutex);
            Task *tsk = *iter;
            job->dispatchedList.erase(iter);
            memcpy(tsk, &pMsg->task, sizeof(Task));// updata task info
            job->finishedList.push_back(tsk);
            job->finishedTaskNum++;
            pthread_mutex_unlock(&job->jobMutex);

			pthread_mutex_lock(&gl_cfg.lenMutex);
			gl_cfg.totalFinished++;
			gl_cfg.pendingReport++;
            gl_cfg.totalSerialTime += pMsg->renderTime;
			// DEBUG_TO_FILE(RESULT_FILE_NAME, "%s on rank %02d %f\n", pMsg->task.fName, pMsg->target, pMsg->renderTime);
			pthread_mutex_unlock(&gl_cfg.lenMutex);
            break;
        }
        DEBUG_PRINT("Recv progress from %d, current progress [%3d/%-3d]\n", pMsg->header.from, gl_cfg.totalFinished, gl_cfg.totalTasks);
        if(gl_cfg.totalTasks == gl_cfg.totalFinished) // last task
        {
            DEBUG_TO_FILE(RESULT_FILE_NAME, "Node[%d] Frame[%d] Thread[%d] Total Serial Time %f, average rendering time %f\n", 
                          gl_cfg.commSize - 1, gl_cfg.totalTasks, gl_cfg.numThreads,
                          gl_cfg.totalSerialTime, gl_cfg.totalSerialTime / gl_cfg.totalTasks);
        }
        break;
    }
    free(msg);
    return NULL;
}
void *taskMaster_func(void *args)
{
    DEBUG_PRINT("Task Master[%d] started\n", gl_cfg.commRank);
    MPI_Status status;
    // Listening request from task slave, then forward it to thread pool
    // Need to check whether EXIT flag be set
    while(gl_cfg.flag != STATUS_EXIT)
    {
        int flag = 0;
        // blocking probe whether message arrived
        while(!flag)
        {
            MPI_Iprobe(MPI_ANY_SOURCE, TO_MASTER_MSG_TAG, gl_cfg.taskComm, &flag, &status);
            // mySleep(MASTER_WAIT_TIME);
            // DEBUG_PRINT("------->Detecting message\n");
        }
        // DEBUG_PRINT("------->Message Detected on Master\n");
        //if(flag)
        //{
            // get the message
            int msgSize = 0;
            MPI_Get_count(&status, MPI_CHAR, &msgSize);
            void *msg = malloc(msgSize);
            MPI_Recv(msg, msgSize, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, gl_cfg.taskComm, MPI_STATUS_IGNORE);
            
            // forward to thread pool
            pool_add_worker(processMessageFromSlave, msg);
        //}
    }
    // Send termination message to slave 0
    // TM_msgHeader buf;
    // buf.type = TYPE_TERMINATE;
    // buf.from = gl_cfg.commRank;
    // MPI_Isend(&buf, sizeof(buf), MPI_CHAR, gl_cfg.commRank, TO_SLAVE_MSG_TAG, gl_cfg.taskComm, NULL);
        
    DEBUG_PRINT("Task Master[%d] ended\n", gl_cfg.commRank);
    return NULL;
}
