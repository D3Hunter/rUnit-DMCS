////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    taskSlave.cpp
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "rUnit.h"
#include "task.h"


void *taskSlave_func(void *args)
{
	double start = 0, end = 0;
    DEBUG_PRINT("Task Slave[%d]  started\n", gl_cfg.commRank);
    MPI_Status status;
    while(STATUS_EXIT != gl_cfg.flag)
    {
        // request task from task Master 
        // block send
        TM_msgHeader msgBuf;
        msgBuf.type = TYPE_REQUEST;
        msgBuf.from = gl_cfg.commRank;
        MPI_Send(&msgBuf, sizeof(msgBuf), MPI_CHAR, MASTER_NODE_RANK, TO_MASTER_MSG_TAG, gl_cfg.taskComm);

        // DEBUG_PRINT("Request sended from Slave[%d]\n", gl_cfg.commRank);
        int msgSize = 0;
        // blockint probe and receive message
        // int flag = 0;
        // while(!flag && STATUS_EXIT != gl_cfg.flag)
        // {
        //     MPI_Iprobe(MASTER_NODE_RANK, TO_SLAVE_MSG_TAG, gl_cfg.taskComm, &flag, &status);
        //     mySleep(1);
        // }
        // if(STATUS_EXIT == gl_cfg.flag) continue;
        MPI_Probe(MASTER_NODE_RANK, TO_SLAVE_MSG_TAG, gl_cfg.taskComm, &status);
        MPI_Get_count(&status, MPI_CHAR, &msgSize);
        assert(msgSize > 0);
        void *msg = malloc(msgSize);
        MPI_Recv(msg, msgSize, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, gl_cfg.taskComm, MPI_STATUS_IGNORE);
        TM_msgHeader *header = (TM_msgHeader *)msg;
        
        switch(header->type)
        {
        case TYPE_NOTASK:
            // DEBUG_PRINT("Slave [%d] recv NOTASK\n", gl_cfg.commRank);
			// mySleep for some time
			mySleep(SLAVE_WAIT_TIME);
            break;
        case TYPE_TERMINATE:// Will not be sued, i think
            // DEBUG_PRINT("Slave [%d] recv TERMINATE\n", gl_cfg.commRank);
            gl_cfg.flag = STATUS_EXIT;
            break;
        case TYPE_DISPATCH:
            // do the job
            TM_dispatchMsg *taskMsg = (TM_dispatchMsg *)msg;
            DEBUG_PRINT("Slave [%d] recv DISPATCH[%s|%s]\n", gl_cfg.commRank, taskMsg->task.fPath, taskMsg->task.fName);

			//////////////////////////////
			// timing
			start = MPI_Wtime();

            // do the job
            bleman_render(taskMsg->task.fPath, taskMsg->task.fName, gl_cfg.numThreads);

			// timing
			//////////////////////////////
			end = MPI_Wtime();
			printf("====== Frame[%s] rendered with %f (s)======\n", taskMsg->task.fName, (end-start));

            // report progress
            TM_progressMsg progMsg;
            progMsg.header.type = TYPE_PROGRESS;
            progMsg.header.from = gl_cfg.commRank;
            memcpy(&progMsg.task, &taskMsg->task, sizeof(Task));
            progMsg.task.progress = 100;
			progMsg.renderTime = end - start;
			progMsg.target = gl_cfg.commRank;
            MPI_Send(&progMsg, sizeof(TM_progressMsg), MPI_CHAR, MASTER_NODE_RANK, TO_MASTER_MSG_TAG, gl_cfg.taskComm);
            break;
        }
        free(msg);
    }
    DEBUG_PRINT("Task Slave[%d]  ended\n", gl_cfg.commRank);
    return NULL;
}
