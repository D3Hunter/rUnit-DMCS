////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    mystdio.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周四 十月 31 14:32:36 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#define CC_IMPLEMENTATION
#include "common.h"
#include "mystdio.h"
#include "dm.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////
// DM stdio interface 
// last bit of FILE is used to check whether it's a normal FILE or 
// opened using cc
//////////////////////////////////////////////////////////////////////
/* cc means: cooperative caching */
FILE * cc_fopen(const char *fileName, const char *mode)
{
	void *fp = NULL;
	if(strcmp(fileName + strlen(fileName) - 3, "rib") == 0)
		DEBUG_PRINT("Opening rib file %s on rank %d\n", fileName, dm_cfg.rank);
	if(strchr(mode, 'w') == NULL) // TODO: read only, not correct strictly but its fine
	{
		fp = cc_file_open(fileName);
		if(NULL != fp)
			TOCCPOINTER(fp);
	}else						// normal mode
	{
		fp = fopen(fileName, mode);
	}
	// DEBUG_PRINT("cc_fopen [%s][%s] [%x]\n", fileName, mode, fp);
	return (FILE *)fp;
}

size_t cc_fread(void *ptr, size_t size, size_t n, FILE *stream)
{
	// DEBUG_PRINT("fread size|n|stream|rank, %d|%d|%x|%d\n", size, n, stream, dm_cfg.rank);
	assert(ISCCPOINTER(stream));

	int bytes = 0;
	int blocks = 0;
	if(1 == size)
	{
		bytes = n;
	}else{
		CC_FILE *fp = (CC_FILE *)TONORMALPOINTER(stream);
		int left = fp->raw->size - fp->pos;
		blocks = left / size;
		if(blocks > n) blocks = n;
		bytes = size * blocks;
	}
	if(bytes > 0)
	{
		bytes = cc_file_read(ptr, bytes, TONORMALPOINTER(stream));
	}
	// DEBUG_PRINT("fread size|n|stream|ret, %d|%d|%x|%d\n", size, n, stream, bytes);

	if(1 != size) return blocks;
	return bytes;
}

int    cc_fclose(FILE *stream)
{
	int ret = 0;
	if(ISCCPOINTER(stream))		// cc
	{
		CC_FILE *fp = TONORMALPOINTER(stream);
#ifdef NATIVE_RDMA
		if(fp->raw->target != dm_cfg.rank)
		{
			pthread_mutex_lock(&dm_cfg.connLock);
			// DEBUG_PRINT("WE ARE CLOSING %d FILE(s)\n", dm_cfg.opened);
			dm_cfg.opened--;
			fp->conn->busy = fp->localMR->busy = 0;
			pthread_mutex_unlock(&dm_cfg.connLock);
		}
#endif
		free(fp);
	}else
		ret = fclose(stream);
	// DEBUG_PRINT("fclose %x|%d\n", stream, ret);
	return ret;
}

int    cc_ferror(FILE *stream)
{
	int ret = 0;
	if(ISCCPOINTER(stream))		// cc
	{
	}else
		ret = ferror(stream);

	// DEBUG_PRINT("ferror %x|%d\n", stream, ret);
	return ret;
}
// we don't support cc getc, we use normal one
int    cc_getc(FILE *stream)
{
	assert(!ISCCPOINTER(stream));
	int ret = 0;
	ret = getc(stream);

	DEBUG_PRINT("getc %x|%d\n", stream, ret);
	return ret;
}

int    cc_fseek(FILE *stream, long offset, int origin)
{
	int ret = 0;

	if(ISCCPOINTER(stream))
	{
		ret = cc_file_seek(TONORMALPOINTER(stream), offset, origin);
	}else
		ret = fseek(stream, offset, origin);

	// DEBUG_PRINT("fseek stream|offset|origin|ret %x|%d|%d|%d\n", stream, offset, origin, ret);
	return ret;
}

long   cc_ftell(FILE *stream)
{
	long ret = 0;
	if(ISCCPOINTER(stream))		// cc
	{
		CC_FILE *fp = TONORMALPOINTER(stream);
		ret = fp->pos;
	}else
		ret = ftell(stream);

	// DEBUG_PRINT("ftell %x|%d\n", stream, ret);
	return ret;
}


//////////////////////////////////////////////////////////////////////
// DM system call of file operation
// fcntl to check whether a fd is valid
// file opened by cc will be left-shifted by 10
// it's not correct strictly, but it's cheep and fit well for this app
//////////////////////////////////////////////////////////////////////
/* 
 * only three parameter open is 
 * two argument version is hard to implement properly
 */
int    cc_open(const char *fileName, int flags, mode_t mode)
{
	int ret;
	if(flags == O_RDONLY)		// read only
	{
		CC_FILE *file = cc_file_open(fileName);
		if(NULL != file)
		{
			// lock, search and save
			// vector is not thread-safe for our scenario
			pthread_mutex_lock(&dm_cfg.filesLock);
			int len = dm_cfg.sysOpenedFiles.size();
			int i = 1;
			for(i = 1; i < len; i++)
				if(NULL == dm_cfg.sysOpenedFiles[i]) // find a slot
					break;

			if(i < len)
			{
				dm_cfg.sysOpenedFiles[i] = file; // save in old slot
			}else
			{
				dm_cfg.sysOpenedFiles.push_back(file); // in new slot
			}
			pthread_mutex_unlock(&dm_cfg.filesLock);

			ret = i;
			TOCCFD(ret);
		}else
			ret = -1;
	}else
		ret = open(fileName, flags, mode);

	// DEBUG_PRINT("open %s|%d|%d|%d\n", fileName, flags, mode, ret);
	return ret;
}

size_t cc_read(int fd, void *buf, size_t count)
{
	int ret;
	// DEBUG_PRINT("read %d|%x|%d\n", fd, buf, count);
	if(!isFDValid(fd))			// cc read
	{
		CC_FILE *file = dm_cfg.sysOpenedFiles[TONORMALFD(fd)];
		assert(file != NULL);
		ret =  cc_file_read(buf, count, file);
	}else
		ret = read(fd, buf, count);

	// DEBUG_PRINT("read %d|%x|%d|%d\n", fd, buf, count, ret);
	return ret;
}

int    cc_close(int fd)
{
	int ret = 0;
	if(!isFDValid(fd))
	{
		CC_FILE *file = dm_cfg.sysOpenedFiles[TONORMALFD(fd)];
#ifdef NATIVE_RDMA
		if(file->raw->target != dm_cfg.rank)
		{
			pthread_mutex_lock(&dm_cfg.connLock);
			// DEBUG_PRINT("WE ARE CLOSING %d FILE(s)\n", dm_cfg.opened);
			dm_cfg.opened--;
			file->conn->busy = file->localMR->busy = 0;
			pthread_mutex_unlock(&dm_cfg.connLock);
		}
#endif
		free(file);

		// lock and update
		pthread_mutex_lock(&dm_cfg.filesLock);
		dm_cfg.sysOpenedFiles[TONORMALFD(fd)] = NULL;
		pthread_mutex_unlock(&dm_cfg.filesLock);
	}else
		ret = close(fd);

	// DEBUG_PRINT("close %d|%d\n", fd, ret);
	return ret;
}

off_t  cc_lseek(int fd, off_t offset, int whence)
{
	int ret = 0;
	if(!isFDValid(fd))
	{
		CC_FILE *file = dm_cfg.sysOpenedFiles[TONORMALFD(fd)];
		ret = cc_file_seek(file, offset, whence);
	}else
		ret = lseek(fd, offset, whence);

	// DEBUG_PRINT("lseek %d|%d|%d|%d\n", fd, offset, whence, ret);
	return ret;
}

