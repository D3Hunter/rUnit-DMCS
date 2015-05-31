////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    dm.h
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周一 十一月  4 10:59:53 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __DM_H__
#define __DM_H__
#include "common.h"
#include <mpi.h>
#include <vector>
#include <stdint.h>
#include <pthread.h>
#include "rdma.h"

#define FILENAME_LEN_MAX 128
#define PATH_LEN_MAX 512

#define MASTER2SLAVE_TAG 0

#define TONORMALPOINTER(ptr) ((CC_FILE *)((uintptr_t)ptr ^ 0x01))
#define ISCCPOINTER(ptr) (1 == ((uintptr_t)ptr & 0x01))
#define TOCCPOINTER(ptr) ptr = (void *)((uintptr_t)ptr | 0x01);

#define BITS_TO_SHIFT 20
#define TONORMALFD(fd) (fd >> BITS_TO_SHIFT)
#define TOCCFD(fd) fd <<= BITS_TO_SHIFT;

typedef struct tag_SharedFile
{
	char fileName[FILENAME_LEN_MAX];
	int size;					// file size
	int readedSize;				// readed size
	int offset;					// start address offset in data window.
	int target;					// target rank this file resides.
}SharedFile;

typedef struct tag_CC_FILE
{
	SharedFile *raw;			// meta-data
	int pos;					// current read pos
#ifdef NATIVE_RDMA
	struct conn_data *conn;
	struct memory_region *localMR;
	struct remote_addr *remote;
#endif
}CC_FILE;

typedef struct tag_DM_CFG
{
	//////////////////////////////////////////////////////////////////////
	// common variables 
	//////////////////////////////////////////////////////////////////////
	MPI_Win win;
#ifdef NATIVE_RDMA
	struct ib_common ibCommon;			// 
	struct remote_addr *remoteAddrs;	// one for each rank
	struct memory_region *localRegion;	// for RDMA READ
	struct conn_data *conns;			// connections
	int conPerRank;						// connection per rank
	int opened;
#endif
	char *base;
	int size;					// shared memory size
	int rankSize;				// rank size
	int rank;					// rank
	int repli;					// replication count of shared files
	int fileCount;				// total shared file count

	//////////////////////////////////////////////////////////////////////
	// for master
	//////////////////////////////////////////////////////////////////////
	#define TMP_FILENAME "___TMP___XX__"
	int fd;				   // for notifying master thread asynchronous
	pthread_t mpid;		   // master thread

	//////////////////////////////////////////////////////////////////////
	// for slave
	//////////////////////////////////////////////////////////////////////
	pthread_t spid;				// slave thread
	int distNum;				// local distributed file number
	SharedFile *metaData;		// meta data
	SharedFile *localMetaData;	// local meta data
	std::vector<CC_FILE *> sysOpenedFiles;
	pthread_mutex_t filesLock;	// sync for sysOpenedFiles
#ifdef MUTEX_EXCLUSIVE_READ
	pthread_mutex_t *readLocks;	// locks for read data through cc
#endif
#ifdef NATIVE_RDMA
	pthread_mutex_t connLock;	// for selecting conn and local mr
#endif
}DM_CFG;
extern DM_CFG dm_cfg;

// file distribution header
typedef struct tag_DistInfoHeader
{
	char path[PATH_LEN_MAX];	// path, currently single dir is supported
	int num;					// num of file distributed
	int len;					// body len
	int gatherNum;				// num of SharedFile will be gathered
	int fileCount;				// total shared file count
}DistInfoHeader;

// void *dm_master_func(void *arg);
void *dm_slave_func(void *arg);

CC_FILE *cc_file_open(const char *fileName);
int cc_file_read(void *ptr, int len, CC_FILE *stream);
int cc_file_seek(CC_FILE *stream, long offset, int origin);

#endif
