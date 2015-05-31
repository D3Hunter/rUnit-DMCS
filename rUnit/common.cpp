////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    common.cpp
// Projectname     :    RenderingUnit
// Author          :    Jujj
// Email           :    
// Date            :    2013-07-10
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"

// not thread safe
int getNextTaskID(void)
{
    static int curID = 0;
    return curID++;
}

void mySleep(int sec)
{
    struct timespec tim;
    tim.tv_sec = sec;
    tim.tv_nsec = 0;
    nanosleep(&tim , NULL);
}

int connectToServer(char *IP, int port)
{
    int sockfd;
    struct sockaddr_in servAddr;
    
    memset(&servAddr, 0, sizeof(servAddr));
    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Failed to create socket");
        return -1;
    }
    
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = inet_addr(IP);

    // connect
    if(connect(sockfd, (const struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
    {
		close(sockfd);
        perror("Failed to connect to server");
        return -1;
    }
    return sockfd;
}

int listenPort(int port)
{
    int sockfd;
    struct sockaddr_in servAddr;

    memset(&servAddr, 0, sizeof(servAddr));
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("Failed to create socket");
        return -1;
    }
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    // bind
    bind(sockfd, (const sockaddr*)&servAddr, sizeof(servAddr));
    
    // listen
    listen(sockfd, MAX_CONN_CLIENT);

    return sockfd;
}

// return len on success
// -1 on error
int sendMessage(int fd, char *msg, int len)
{
	int leftLen = len;
	
	assert(msg != NULL && len > 0);
	while(leftLen > 0)
	{
		int sentLen = write(fd, msg, leftLen);
		if(sentLen < 0)
		{
			perror("Failed to send messages");
			return -1;
		}
		leftLen -= sentLen;
	}
	return len;
}

// we assume there is no stick packet
// invoker is responsible for freeing buf
// return NULL on error
char *recvMessageWithTail(int fd, char *tail)
{
	assert(tail != NULL);

	char *buf = (char *)malloc(BUF_SIZE);
	int tailLen = strlen(tail);
	int leftSize = BUF_SIZE, bufSize = BUF_SIZE, currSize = 0;
	while(1)
	{
		int size = read(fd, buf+currSize, leftSize);
		if(size < 0)
		{
			perror("Failed to recv message");
			free(buf);
			return NULL;
		}else if(0 == size)
			continue;
		leftSize -= size;
		currSize += size;
		// check tail
		if(currSize >= tailLen && 0 == strncmp(&buf[currSize-tailLen], tail, tailLen))
			return buf;
		// resize buf if needed
		if(leftSize <= 0)
		{
			bufSize += BUF_SIZE;
			leftSize += BUF_SIZE;
			buf = (char *)realloc(buf, bufSize);
		}
	}
}

int getIP(char *host, char *buf, int size)
{
	assert(host != NULL && buf != NULL);
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;// make sure this IP can do bind
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	
	s = getaddrinfo(host, NULL, &hints, &result);
	if(s != 0)
	{
		fprintf(stderr, "Failed to get IP of %s: %s\n", host, gai_strerror(s));
		return -1;
	}
	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sfd < 0) continue;
		s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		//inet_ntop(rp->ai_family, &((struct sockaddr_in *)rp->ai_addr)->sin_addr, buf, size);
		//DEBUG_PRINT("    ----ip IS %s\n", buf);
		close(sfd);
		if(s == 0) break;// we find one IP that can bind
	}
	if(rp != NULL)
	{
		// convert to ip str
		inet_ntop(rp->ai_family, &((struct sockaddr_in *)rp->ai_addr)->sin_addr, buf, size);
		DEBUG_PRINT("Got IP of %s is %s\n", host, buf);
	}
	// release buf
	freeaddrinfo(result);

	return (rp == NULL) ? -1 : 0;
}
