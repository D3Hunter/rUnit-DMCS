////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    rUnit.cpp
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

GL_CFG gl_cfg;


static void printHelp()
{
    printf("Usage: rUnit -d <dir> -t <num>\n");
    printf("  -d <path>     directory of ribs(required)  \n");
    printf("  -t <num>      num of threads used to render\n");
    printf("  -s <IP>       IP address of schedual server\n");
    printf("  -b <num>      space used for DM on slave node, in megabytes, 10 default\n");
    printf("  -r            DM shared file replication              \n");
    printf("  -h            print this info              \n");
}

static void finalizeExit(int code)
{
    MPI_Finalize();
    exit(code);
}

static void clearTaskList()
{
	// list should be cleared
	if(!gl_cfg.lenList.empty())
	{
#define CLEAR_LIST(list) for(std::vector<Task *>::iterator iter = list.begin(); iter != list.end(); iter++) delete *iter;\
    list.clear();
    CLEAR_LIST(gl_cfg.lenList[0]->taskList);
    CLEAR_LIST(gl_cfg.lenList[0]->dispatchedList);
    CLEAR_LIST(gl_cfg.lenList[0]->finishedList);
#undef CLEAR_LIST
	}
}

static void processCmdLines(int argc, char** argv)
{
    int opt, len;
	
	gl_cfg.repli = 1;			// default
    ////////////////////////////////////////////////////////////
    // Recv task from command line
    ////////////////////////////////////////////////////////////
    while((opt = getopt(argc, argv, "d:t:s:b:r:h")) != -1)
    {
        switch(opt)
        {
        case 'd':// rendering directory
            memcpy(&gl_cfg.ribPath, optarg, strlen(optarg));
            break;
        case 't':// num of thread
            gl_cfg.numThreads = atoi(optarg);
            if(gl_cfg.numThreads <= 0)
            {
                printf("Num of threads should be positive\n");
                finalizeExit(EXIT_FAILURE);
            }
            break;
        case 's':// server address
            len = strlen(optarg);// max(strlen(optarg), 32-1);
            memcpy(&gl_cfg.serverAddr, optarg, len);
            break;
		case 'b':// shared space in slave node
			gl_cfg.sharedSpace = atoi(optarg);
			break;
		case 'r':				// DM repli
			gl_cfg.repli = atoi(optarg);
			break;
        case 'h':// num of thread
            printHelp();
            finalizeExit(EXIT_SUCCESS);
            break;
        default: /* '?' */
            fprintf(stderr, "Invalid command line argument\n", argv[0]);
            printHelp();
            finalizeExit(EXIT_FAILURE);
            break;
        }
    }
    if(gl_cfg.serverAddr[0] == 0 || 0 == gl_cfg.numThreads)
    {
        fprintf(stderr, "Server IP and num of thread is required\n");
        finalizeExit(EXIT_FAILURE);
    }
	// DEBUG_PRINT("sharedSpace is %dM\n", gl_cfg.sharedSpace);
	if(gl_cfg.sharedSpace <= 0)
	{
		gl_cfg.sharedSpace = DEFAULT_SHARED_SPACE;
	}
	// DEBUG_PRINT("sharedSpace is %dM\n", gl_cfg.sharedSpace);
}

static void initCommAndParameter(int argc, char** argv)
{
    int worldCommSize, worldCommRank, taskCommSize, taskCommRank, dataCommSize, dataCommRank;
    memset(&gl_cfg, 0, sizeof(gl_cfg));
    
    // create task && data communicator
    MPI_Comm_dup(MPI_COMM_WORLD, &gl_cfg.taskComm);
    MPI_Comm_dup(MPI_COMM_WORLD, &gl_cfg.dataComm);
    MPI_Comm_size(MPI_COMM_WORLD,&worldCommSize);
    MPI_Comm_rank(MPI_COMM_WORLD,&worldCommRank);
    MPI_Comm_size(gl_cfg.taskComm,&taskCommSize);
    MPI_Comm_rank(gl_cfg.taskComm,&taskCommRank);
    MPI_Comm_size(gl_cfg.dataComm,&dataCommSize);
    MPI_Comm_rank(gl_cfg.dataComm,&dataCommRank);
    DEBUG_PRINT("World[%d, %d], Task[%d, %d], Data[%d, %d]\n", worldCommSize, worldCommRank, taskCommSize, taskCommRank, dataCommSize, dataCommRank);
    assert(worldCommRank == taskCommRank && taskCommRank == dataCommRank);

    gl_cfg.commSize = worldCommSize;
    gl_cfg.commRank = worldCommRank;
    gl_cfg.maxThreadNum = gl_cfg.commSize / 2 + 1;// tmp use
    gl_cfg.flag = STATUS_OK;
    gl_cfg.totalTaskNum = gl_cfg.dispatchedTaskNum = gl_cfg.finishedTaskNum = 0;
    gl_cfg.totalSerialTime = 0;
    pthread_mutex_init(&gl_cfg.lenMutex, NULL);
	char hostname[128];
	gethostname(hostname, 128);
	getIP(hostname, gl_cfg.localIP, sizeof(gl_cfg.localIP));
    
    // process command line args
    processCmdLines(argc, argv);
}

int main(int argc, char** argv)
{
    int providedLevel;
    pthread_t taskServer, taskMaster, taskSlave;
    
    // Initilazation
    MPI_Init_thread(0,0,MPI_THREAD_MULTIPLE,&providedLevel);
    initCommAndParameter(argc, argv);
    
    // start you workers
    if(MASTER_NODE_RANK == gl_cfg.commRank)
    {
        // init thread pool
        pool_init(gl_cfg.maxThreadNum);
        // task server thread
		startTaskServer();
        // task master
        pthread_create(&taskMaster, NULL, taskMaster_func, NULL);

		// start DM master 
		startDMMaster();
    }else
    {
		// start DM slave first
		startDMSlave();

        // task slave, run in main thread
        pthread_create(&taskSlave, NULL, taskSlave_func, NULL);
    }

    // wait for threads
    if(MASTER_NODE_RANK == gl_cfg.commRank)
    {
		endTaskServer();
        pthread_join(taskMaster, NULL);
        
        // wait for thread pool to complete
        wait_pool_complete();
        // destroy thread pool
        pool_destroy();
        
        // notified all slaves to terminate youself
        terminateAllSlave();
        
        // clear task list
        clearTaskList();
    }else
    {
        pthread_join(taskSlave, NULL);
    }
	// finish DM
	dmFinish();

    // slave cleanup
    MPI_Finalize();
    
    return 0;
}
