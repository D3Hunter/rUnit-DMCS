////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    mystdio.h
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周四 十月 31 14:32:55 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __MYSTDIO_H__
#define __MYSTDIO_H__
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if __cplusplus
#define EXPORTED_SYMBOL_PREFIX extern "C"
#else
#define EXPORTED_SYMBOL_PREFIX
#endif

//////////////////////////////////////////////////////////////////////
// DM stdio interface 
//////////////////////////////////////////////////////////////////////
/* cc means: cooperative caching */
/* 
 * only three parameter open is 
 * two argument version is hard to implement properly
 */
EXPORTED_SYMBOL_PREFIX FILE * cc_fopen(const char *fileName, const char *mode);
EXPORTED_SYMBOL_PREFIX size_t cc_fread(void *ptr, size_t size, size_t n, FILE *stream);
EXPORTED_SYMBOL_PREFIX int    cc_fclose(FILE *stream);
EXPORTED_SYMBOL_PREFIX int    cc_ferror(FILE *stream);
EXPORTED_SYMBOL_PREFIX int    cc_getc(FILE *stream);
EXPORTED_SYMBOL_PREFIX int    cc_fseek(FILE *stream, long offset, int origin);
EXPORTED_SYMBOL_PREFIX long   cc_ftell(FILE *stream);

//////////////////////////////////////////////////////////////////////
// DM system call of file operation
//////////////////////////////////////////////////////////////////////
EXPORTED_SYMBOL_PREFIX int    cc_open(const char *fileName, int flags, mode_t mode);
EXPORTED_SYMBOL_PREFIX size_t cc_read(int fd, void *buf, size_t count);
EXPORTED_SYMBOL_PREFIX int    cc_close(int fd);
EXPORTED_SYMBOL_PREFIX off_t  cc_lseek(int fd, off_t offset, int whence);

/* dm insterface */
EXPORTED_SYMBOL_PREFIX void dm_init_master(int repli);
EXPORTED_SYMBOL_PREFIX void dm_init_slave(int size, int repli);
EXPORTED_SYMBOL_PREFIX void dm_loadfiles(char *paths);
EXPORTED_SYMBOL_PREFIX void dm_finish();

//////////////////////////////////////////////////////////////////////
// Outer application access interface
//////////////////////////////////////////////////////////////////////
#ifndef CC_IMPLEMENTATION
/* stdio */
#define fopen  cc_fopen
#define fread  cc_fread
#define fclose cc_fclose
#define ferror cc_ferror
#define getc   cc_getc
#define fseek  cc_fseek
#define ftell  cc_ftell
/* system call */
#define read   cc_read
#define open   cc_open
#define close  cc_close
#define lseek  cc_lseek
#endif

#endif
