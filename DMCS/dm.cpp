////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    dm.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周一 十一月  4 10:43:55 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#define CC_IMPLEMENTATION
#include "common.h"
#include "mystdio.h"
#include "dm.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <vector>
#include <assert.h>
#include <pthread.h>
#include "rdma.h"

DM_CFG dm_cfg;

//////////////////////////////////////////////////////////////////////
// communication on MPI_COMM_WORLD
//////////////////////////////////////////////////////////////////////
void dm_init_common(int size, int repli)
{
	memset(&dm_cfg, 0, sizeof(DM_CFG));
	MPI_Comm_size(MPI_COMM_WORLD, &dm_cfg.rankSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &dm_cfg.rank);

	// repli should smaller than rankSize
	dm_cfg.repli = (dm_cfg.rankSize > repli) ? repli : dm_cfg.rankSize - 1;
	DEBUG_PRINT("Normalized repli is %d [old is %d]\n", dm_cfg.repli, repli);
#ifdef LOCAL_READ
	assert(dm_cfg.repli == dm_cfg.rankSize - 1);
#endif
#if defined(LOCAL_READ) || defined(MUTEX_EXCLUSIVE_READ) || defined(OVERALL_EPOCH_FLUSH)
	// allocate mem space for the window
	dm_cfg.size = size;
	dm_cfg.base = (char *)malloc(dm_cfg.size);
	// lock this part of memory
	// avoid been swapped into disk
	if(0 != mlock(dm_cfg.base, dm_cfg.size))
	{
		// TODO: continue executing on lock error
		perror("Failed to lock memory");
	}
	// open window
	DEBUG_PRINT("Creating window on rank %d with size %d\n", dm_cfg.rank, dm_cfg.size);
	MPI_Win_create(dm_cfg.base, dm_cfg.size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &dm_cfg.win);
#endif
#ifdef NATIVE_RDMA
	// init rdma stuffs
	dm_cfg.conPerRank = 10;		// tmp usage
	ib_init_common(size, IB_PORT);
	ib_create_connect_qps();
	dm_cfg.size = size;
	dm_cfg.base = dm_cfg.ibCommon.base;
#endif
}
void dm_init_master(int repli)
{
	DEBUG_PRINT("Master initilizing\n");
	dm_init_common(0, repli);

	DEBUG_PRINT("Master initilized\n");
}
//////////////////////////////////////////////////////////////////////
// blocking init
// waken when master calls dm_loadfiles
//////////////////////////////////////////////////////////////////////
void dm_init_slave(int size, int repli)
{
	// DEBUG_PRINT("Slave initilizing\n");
	dm_init_common(size, repli);

	pthread_mutex_init(&dm_cfg.filesLock, NULL);
#ifdef NATIVE_RDMA
	pthread_mutex_init(&dm_cfg.connLock, NULL);
#endif
	// awaiting for file distribute
	dm_slave_func(NULL);

	// DEBUG_PRINT("Slave initilized\n");
}
//////////////////////////////////////////////////////////////////////
// Only be called in rank 0, i.e. master node
//////////////////////////////////////////////////////////////////////
void dm_loadfiles(char *paths)
{
	DEBUG_PRINT("File loading\n");
	assert(dm_cfg.rank == 0);

	// load data from paths
	std::vector<SharedFile *> fList(0);
	struct dirent *dirItem = NULL;

	DIR *dir = opendir(paths);
	if(NULL == dir)
	{
		perror("Failed to open dir");
		return;
	}
	int strLen = 0;
	struct stat statBuf;
	char fullFileName[PATH_LEN_MAX];
	while((dirItem = readdir(dir)) != NULL)
	{
		sprintf(fullFileName, "%s/%s", paths, dirItem->d_name);
		if(0 != stat(fullFileName, &statBuf))
		{
			perror("Failed to stat file");
			continue;			// we skip this file
		}
		// regular file are shared
		if(S_ISREG(statBuf.st_mode))
		{
			// append to file list
			SharedFile *sf = (SharedFile *)malloc(sizeof(SharedFile));
			memset(sf, 0, sizeof(SharedFile));
			strcpy(sf->fileName, dirItem->d_name);
			sf->size = statBuf.st_size;
			fList.push_back(sf);
			// DEBUG_PRINT("File %s added\n", sf->fileName);
		}
	}

	// TODO: algorithms for distributing files to slaves
	// accounting for file size and replication
	// We'll distribute them in the order they are read
	// and assume fileCount * repli greater than ranksize
	// First 'left' slave nodes will get aver+1 files, others will get aver files
	dm_cfg.fileCount = fList.size();
	assert(dm_cfg.fileCount * dm_cfg.repli >= dm_cfg.rankSize-1);
	int aver = dm_cfg.fileCount * dm_cfg.repli / (dm_cfg.rankSize-1); // average
	int left = (dm_cfg.fileCount * dm_cfg.repli) % (dm_cfg.rankSize-1);
	int startIdx = 0;			// index into fList
	int count = 0;
	DistInfoHeader header;
	strcpy(header.path, paths);
	header.gatherNum = (left > 0) ? aver+1 : aver;
	header.fileCount = dm_cfg.fileCount;
	SharedFile *files = (SharedFile *)malloc(header.gatherNum * sizeof(SharedFile));

	DEBUG_PRINT("Distributing files to slave\n");
	for(int i = 1; i < dm_cfg.rankSize; i++)
	{
		count = left > 0 ? aver+1 : aver;
		left--;
		for(int j = 0; j < count; j++)
		{
			memcpy(&files[j], fList[startIdx], sizeof(SharedFile));
			files[j].target = i;
			startIdx = (startIdx + 1) % dm_cfg.fileCount; // next file
		}
		header.num = count;
		header.len = count * sizeof(SharedFile);
		// blocking send
		MPI_Send(&header, sizeof(DistInfoHeader), MPI_BYTE, i, MASTER2SLAVE_TAG, MPI_COMM_WORLD);
		MPI_Send(files, header.len, MPI_BYTE, i, MASTER2SLAVE_TAG, MPI_COMM_WORLD);
	}
	// DEBUG_PRINT("Gathering meta-data\n");
	int size = header.gatherNum * sizeof(SharedFile);
	SharedFile *metaData = (SharedFile *)malloc(dm_cfg.rankSize * size);
	// empty files
	memset(files, 0, size);
	memset(metaData, 0, dm_cfg.rankSize * size);
	MPI_Allgather(files, size, MPI_BYTE, metaData, size, MPI_BYTE, MPI_COMM_WORLD);

	// free
	free(files);
	free(metaData);
	for(int i = 0; i < dm_cfg.fileCount; i++)
	{
		free(fList[i]);
	}
	//////////////////////////////////////////////////////////////////////
	// barrier, wait all slave complete their work
	//////////////////////////////////////////////////////////////////////
	MPI_Barrier(MPI_COMM_WORLD);
	DEBUG_PRINT("File loaded\n");
}
void dm_finish()
{
	DEBUG_PRINT("Finishing on rank %d\n", dm_cfg.rank);

	// destroy mutexes
	if(0 != dm_cfg.rank)
	{
#ifdef OVERALL_EPOCH_FLUSH
		for(int i = 1; i < dm_cfg.rankSize; i++)
			MPI_Win_unlock(i, dm_cfg.win);
#endif
#ifdef MUTEX_EXCLUSIVE_READ
		for(int i = 1; i < dm_cfg.rankSize; i++)
			pthread_mutex_destroy(&dm_cfg.readLocks[i]);
		free(dm_cfg.readLocks);
#endif
#ifdef NATIVE_RDMA
		pthread_mutex_destroy(&dm_cfg.connLock);
#endif
		pthread_mutex_destroy(&dm_cfg.filesLock);
	}
#if defined(LOCAL_READ) || defined(MUTEX_EXCLUSIVE_READ) || defined(OVERALL_EPOCH_FLUSH)
	MPI_Win_free(&dm_cfg.win);
	// unlock and free window space
	munlock(dm_cfg.base, dm_cfg.size);
	free(dm_cfg.base);
#endif
#ifdef NATIVE_RDMA
	ib_destroy();
#endif
	if(NULL != dm_cfg.metaData)
	{
		free(dm_cfg.localMetaData);
		free(dm_cfg.metaData);
	}
	DEBUG_PRINT("Finished on rank %d\n", dm_cfg.rank);
}
