////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    common.h
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-08
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef DEBUG
    #define DEBUG
#endif

#ifdef DEBUG
#define RESULT_FILE_NAME "renderTimeSummarize.txt"
#define RUNIT_PRINT_HEADER "------ rUnit ------ : "
#define DEBUG_PRINT(format, ...) printf(RUNIT_PRINT_HEADER format, ##__VA_ARGS__)
#define DEBUG_TO_FILE(fName, format, ...)		\
	{											\
	FILE *__outf = fopen(fName, "a");			\
	fprintf(__outf, format, ##__VA_ARGS__);		\
	fflush(__outf);								\
	fclose(__outf);								\
	}
#else
#define DEBUG_PRINT(format, ...)
#define DEBUG_TO_FILE(fName, format, ...)
#endif

#define MAX_CONN_CLIENT 10
#define BUF_SIZE 10240
#define FALSE 0
#define TRUE 1

#define MAX_PATH_LEN 256


int getNextTaskID(void);
void mySleep(int sec);
int connectToServer(char *IP, int port);
int listenPort(int port);// one client per time
int sendMessage(int fd, char *msg, int len);
char *recvMessageWithTail(int fd, char *tail);
int getIP(char *host, char *buf, int size);

#endif
