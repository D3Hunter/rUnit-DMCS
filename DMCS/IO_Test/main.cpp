// 
// 
// Filename: main.cpp
// Description: 
// Author: Jujj
// Created: 周五 十月 25 11:41:11 2013 (+0800)
// Version: 1.0
// 
// Make with mpicxx -o run -O3 main.cpp
// 

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <assert.h>
#include "mpi.h"

#define DATAPATH "./data"
#define KB_SIZE 1024
#define REPLICATION 10

int main(int argc, char **argv)
{
	if(argc < 2) 
	{
		printf("Usage <run> <block size in KB>\n");
	}

	MPI_Init(&argc, &argv);

	struct dirent *dirItem = NULL;
	struct stat fileStat;
	char fileName[128];
	float sumFileSize = 0;
	int blockSize = atoi(argv[1]) * KB_SIZE;
	char *readBuf = (char *)malloc(blockSize);
	int rankSize = 0, rank;
	double time = 0;

	double *collectedTime = NULL;
	double *collectedBW = NULL;
	MPI_Comm_size(MPI_COMM_WORLD, &rankSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if(0 == rank)
	{
		// alloc collect buffer
		collectedTime = new double[rankSize];
		collectedBW = new double[rankSize];
		// printf("Testing with %d nodes\n", rankSize);
		// printf("Testing bandwidth for block size %dKB\n", blockSize / KB_SIZE);
	}

	double start;
	start = MPI_Wtime();

	// all rank except rank 0 will read all files
	if(0 == rank)
	{
	}
	else
	{
		DIR *dp = NULL;
		if((dp = opendir(DATAPATH)) == NULL)
			perror("opendir\n");
		while((dirItem = readdir(dp)) != NULL)
		{
			// skip . and ..
			if(!strcmp(dirItem->d_name,".") || !strcmp(dirItem->d_name,".."))
				continue;
			sprintf(fileName, "%s/%s", DATAPATH, dirItem->d_name);
			if(stat(fileName, &fileStat) == -1)
				perror("file");
			if(S_ISDIR(fileStat.st_mode)) continue; // skip dir

			// sum file size
			sumFileSize += fileStat.st_size/(float)KB_SIZE/KB_SIZE;

			// open and read file
			for(int i = 0; i < REPLICATION; i++)
			{
				FILE *fp = fopen(fileName, "r");
				int readLen = 0;
				do{
					readLen = fread(readBuf, 1, blockSize, fp);
				}while(readLen == blockSize);
				fclose(fp);
			}
		}
		closedir(dp);
	}
	time = MPI_Wtime() - start;

	free(readBuf);

	// collect time
	MPI_Gather(&time, 1, MPI_DOUBLE, collectedTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	if(0 == rank)
	{
		printf("Time(s) of reading [%fMB] Data with bufSize[%04dKB] for [%d] times on [%02d] nodes is :\n", 
			   sumFileSize, blockSize / KB_SIZE, REPLICATION, rankSize-1);
		for(int i = 1; i < rankSize; i++)
			printf("RANK %2d TIME %f\n", i, collectedTime[i]);
		delete collectedTime;
	}else
	{
		fprintf(stderr, "FILE size %f on rank %d\n", sumFileSize, rank);
	}

	MPI_Finalize();

	return 0;
}
