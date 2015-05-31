////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    rUnit.h
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __RUNIT_H__
#define __RUNIT_H__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <mpi.h>
#include <pthread.h>
#include <vector>
#include "common.h"
#include "task.h"

#define MASTER_NODE_RANK 0

#define STATUS_OK           (1     )
#define STATUS_EXIT         (1 << 2)
#define DEFAULT_SHARED_SPACE 10

typedef struct tag_GL_CFG
{
    int commSize;				/* num of process started */
    int commRank;				/* rank id */
    int maxThreadNum;			// max number of thread in thread pool
    int flag;					/* status flags */
	char localIP[32];			// local IP address
    int numThreads;				// number of thread to render
    char serverAddr[32];
    MPI_Comm taskComm, dataComm;

	//////////////////////////////
	// for DM(data management)
	//////////////////////////////
	int sharedSpace;			/*  megabytes */
	int repli;

	//////////////////////////////
	// below two line is for local test
	//////////////////////////////
    char ribPath[MAX_PATH_LEN];	// rib file path
    int totalTaskNum, dispatchedTaskNum, finishedTaskNum;

	//////////////////////////////
	// about jobs
	//////////////////////////////
	std::vector<LenJob *> lenList;
	pthread_mutex_t lenMutex;
	int totalTasks, totalDispatched, totalFinished, totalReported, pendingReport;

	//////////////////////////////
	// about jobs
	//////////////////////////////
    float totalSerialTime;
}GL_CFG;

extern GL_CFG gl_cfg;

#endif
