////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    cc_file.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周六 十一月  9 12:42:50 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "dm.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

static SharedFile *getFileMetaData(const char *fullPath)
{
	// get file name from fullPath(not null)
	assert(fullPath != NULL);
	const char *fileName = strrchr(fullPath, '/');
	if(fileName == NULL) fileName = fullPath; // no path
	else fileName += 1;

	SharedFile *head = dm_cfg.localMetaData, *ret = NULL;
	// search in local metadata
	int i = 0;
	for(i = 0; i < dm_cfg.distNum; i++, head++)
	{
		if(0 == strcmp(fileName, head->fileName))
			break;
	}
	if(i < dm_cfg.distNum)		// found in local metadata
	{
		return head;
	}

	head = dm_cfg.metaData;
	// search in sorted meta-data
	for(i = 0; i < dm_cfg.fileCount; i++, head += dm_cfg.repli)
	{
		if(0 == strcmp(fileName, head->fileName))
			break;
	}

	// choose a target at random
	if(i < dm_cfg.fileCount)
	{
		ret = head + rand() % dm_cfg.repli;
	}
	return ret;// NULL on not found
}
//////////////////////////////////////////////////
// fileName may contain path
// file opened by thread A cannot be used by 
// thread B
//////////////////////////////////////////////////
CC_FILE *cc_file_open(const char *fileName)
{
	CC_FILE *file = NULL;
	SharedFile *meta = getFileMetaData(fileName);
	if(NULL != meta)		// found
	{
		file = (CC_FILE *)malloc(sizeof(CC_FILE));
		file->raw = meta;
		file->pos = 0;
#ifdef NATIVE_RDMA
		// remote data
		file->conn = NULL;
		file->localMR = NULL;
		file->remote = &dm_cfg.remoteAddrs[file->raw->target];
		if(file->raw->target != dm_cfg.rank)
		{
			pthread_mutex_lock(&dm_cfg.connLock);
			dm_cfg.opened++;
			// DEBUG_PRINT("WE ARE OPENING %d FILE(s)\n", dm_cfg.opened);
			// find one conn 
			for(int idx = file->raw->target * dm_cfg.conPerRank, i = 0; i < dm_cfg.conPerRank; i++, idx++)
			{
				if(!dm_cfg.conns[idx].busy)
				{
					dm_cfg.conns[idx].busy = 1;
					file->conn = &dm_cfg.conns[idx];
					break;
				}
			}
			// fine one local region
			for(int i = 0; i < dm_cfg.conPerRank; i++)
				if(!dm_cfg.localRegion[i].busy)
				{
					dm_cfg.localRegion[i].busy = 1;
					file->localMR = &dm_cfg.localRegion[i];
					break;
				}
			pthread_mutex_unlock(&dm_cfg.connLock);
			assert(file->conn != NULL);
			assert(file->localMR != NULL);
		}
#endif
		// DEBUG_PRINT("Opened file with target[%d] on rank [%d] size %d\n", meta->target, dm_cfg.rank, file->raw->size);
	}
	return file;
}

int cc_file_read(void *ptr, int len, CC_FILE *stream)
{
	CC_FILE *fp = (CC_FILE *)(stream);
	int bytes = len;
	// DEBUG_PRINT("cc_file_read bytes|pos|size %d|%d|%d\n", bytes, fp->pos, fp->raw->size);
	if(bytes + fp->pos > fp->raw->size) bytes = fp->raw->size - fp->pos;

	if(bytes > 0)
	{
		int target = fp->raw->target;
#ifdef LOCAL_READ
		assert(target == dm_cfg.rank); // read from local
		memcpy(ptr, dm_cfg.base + fp->raw->offset + fp->pos, bytes);
#endif
#ifdef OVERALL_EPOCH_FLUSH
		MPI_Get(ptr, bytes, MPI_BYTE, target, fp->raw->offset + fp->pos, bytes, MPI_BYTE, dm_cfg.win);
		MPI_Win_flush(target, dm_cfg.win);
#endif
#ifdef MUTEX_EXCLUSIVE_READ
		pthread_mutex_lock(&dm_cfg.readLocks[target]);
		MPI_Win_lock(MPI_LOCK_SHARED, target, 0, dm_cfg.win);
		MPI_Get(ptr, bytes, MPI_BYTE, target, fp->raw->offset + fp->pos, bytes, MPI_BYTE, dm_cfg.win);
		MPI_Win_unlock(target, dm_cfg.win);
		pthread_mutex_unlock(&dm_cfg.readLocks[target]);
#endif
#ifdef NATIVE_RDMA
		if(target == dm_cfg.rank) // local
		{
			memcpy(ptr, dm_cfg.base + fp->raw->offset + fp->pos, bytes);
		}else
		{
			ib_post_rdma_read(fp->conn, fp->remote, fp->localMR, fp->raw->offset + fp->pos, bytes);
			ib_poll_completion(fp->conn);
			memcpy(ptr, fp->localMR->buf, bytes);
		}
#endif
	}
	// update pos
	fp->pos += bytes;

	return bytes;
}

// do not support SEEK_END
int cc_file_seek(CC_FILE *stream, long offset, int origin)
{
	CC_FILE *fp = (CC_FILE *)(stream);
	switch(origin)
	{
	case SEEK_SET:
		fp->pos = offset;
		break;
	case SEEK_END:
		fp->pos = fp->raw->size + offset;
		break;
	case SEEK_CUR:
	default:
		fp->pos += offset;
		break;
	}
	return fp->pos;
}
