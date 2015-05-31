////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    dmslave.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :
// Date            :    周一 十一月  4 10:41:03 2013 (+0800)
// Version         :    v 1.0
// Description     :
////////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "dm.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static int file_compare(const void *p1, const void *p2)
{
	SharedFile *s1 = (SharedFile *)p1;
	SharedFile *s2 = (SharedFile *)p2;
	return -(strcmp(s1->fileName, s2->fileName));// decending order
}

void *dm_slave_func(void *arg)
{
	// DEBUG_PRINT("Slave thread func started on %d\n", dm_cfg.rank);
	DistInfoHeader header;
	SharedFile *files = NULL;
	MPI_Status mpiStat;

	// waiting for distribution info header
	MPI_Recv(&header, sizeof(DistInfoHeader), MPI_BYTE, 0, MASTER2SLAVE_TAG, MPI_COMM_WORLD, &mpiStat);
	DEBUG_PRINT("Get dist header path[%s] num[%d] gatherNum[%d] totalSharedCount[%d] on rank %d\n",
				header.path, header.num, header.gatherNum, header.fileCount, dm_cfg.rank);
	dm_cfg.fileCount = header.fileCount;
#ifdef LOCAL_READ
	assert(header.num == header.fileCount);
#endif
	dm_cfg.distNum = header.num;
	int size = header.gatherNum * sizeof(SharedFile);
	dm_cfg.localMetaData = files = (SharedFile *)malloc(size);
	memset(files, 0, size);

	// read dist info body based on header
	MPI_Recv(files, header.len, MPI_BYTE, 0, MASTER2SLAVE_TAG, MPI_COMM_WORLD, &mpiStat);
	// read all files distributed into data window
	char fileName[PATH_LEN_MAX];
	char *ptr = dm_cfg.base;
	int fd;

	for(int i = 0; i < header.num; i++)
	{
		// update meta-data
		files[i].readedSize = files[i].size;
		files[i].offset = ptr - dm_cfg.base;
		files[i].target = dm_cfg.rank;
		sprintf(fileName, "%s/%s", header.path, files[i].fileName);
		DEBUG_PRINT("Reading file %s on rank %d size|offset|target %d|%d|%d\n", fileName, dm_cfg.rank, files[i].size, files[i].offset, files[i].target);

		if((fd = open(fileName, O_RDONLY)) >= 0)
		{
			read(fd, ptr, files[i].size);
			ptr += files[i].size;
		}else
		{
			// TODO: skip files cannot read
			perror("Failed to open file");
		}
	}
	// do all-gather to get all meta-data
	// DEBUG_PRINT("Gathering meta-data on slave %d\n", dm_cfg.rank);
	dm_cfg.metaData = (SharedFile *)malloc(dm_cfg.rankSize * size);
	memset(dm_cfg.metaData, 0, dm_cfg.rankSize * size);
	MPI_Allgather(files, size, MPI_BYTE, dm_cfg.metaData, size, MPI_BYTE, MPI_COMM_WORLD);

	//////////////////////////////////////////////////
	// SharedFile with fileName='\0' is dummy files
	// TODO: save all meta-data into hash table(map)
	// each pair with filename and a array of size repli
	// this part of space can be allocated together
	//////////////////////////////////////////////////
	// sort gathered meta data
	// DEBUG_PRINT("Sorting meta-data on rank %d\n", dm_cfg.rank);
	qsort(dm_cfg.metaData, dm_cfg.rankSize * header.gatherNum, sizeof(SharedFile), file_compare);

	// add first slot
	if(dm_cfg.sysOpenedFiles.empty())
		dm_cfg.sysOpenedFiles.push_back((CC_FILE *)NULL); // slot 0 is not allowed

	// init read locks
#ifdef OVERALL_EPOCH_FLUSH
	for(int i = 1; i < dm_cfg.rankSize; i++)
		MPI_Win_lock(MPI_LOCK_SHARED, i, 0, dm_cfg.win);
#endif
#ifdef MUTEX_EXCLUSIVE_READ
	dm_cfg.readLocks = (pthread_mutex_t *)malloc(dm_cfg.rankSize * sizeof(pthread_mutex_t));
	for(int i = 1; i < dm_cfg.rankSize; i++)
		pthread_mutex_init(&dm_cfg.readLocks[i], NULL);
#endif
	// MPI_Barrier
	MPI_Barrier(MPI_COMM_WORLD);
	// DEBUG_PRINT("Slave thread func ending on %d\n", dm_cfg.rank);

	return NULL;
}
