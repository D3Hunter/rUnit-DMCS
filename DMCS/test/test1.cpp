#include "../mystdio.h"
#include <mpi.h>
#include <stdlib.h>
#include <string.h>
// make with mpicc -o run test1.cpp -L.. -lsdudmcs
#define REPLICATION 1

#define BLOCKSIZE 8192
void read_whole_file_in_stdio(char *fileName)
{
	char buf[BLOCKSIZE];
	char outName[128];
	static int num = 1;

	memset(buf, 0, BLOCKSIZE);
	sprintf(outName, "a-%d.jpg", num++);
	FILE *fp = fopen(fileName, "r");
	FILE *out = fopen(outName, "w");
	int len = 0;
	while((len = fread(buf, 1, BLOCKSIZE, fp)) > 0)
		fwrite(buf, 1, len, out);
	// ftell(fp);
	// fseek(fp, 0, SEEK_SET);
	// while(fread(buf, 1, BLOCKSIZE, fp) > 0);
	fclose(fp);
	fclose(out);
}
void read_whole_file_in_syscall(char *fileName)
{
	char buf[BLOCKSIZE];
	int fd = open(fileName, O_RDONLY, 0);
	while(read(fd, buf, BLOCKSIZE) > 0);
	lseek(fd, 0, SEEK_SET);
	while(read(fd, buf, BLOCKSIZE) > 0);
	lseek(fd, 0, SEEK_CUR);
	close(fd);
}

int main(int argc, char **argv)
{
	int rank;
	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if(0 == rank)
	{
		dm_init_master(REPLICATION);
		dm_loadfiles("./data");
	}else
	{
		dm_init_slave(20000000, REPLICATION);
	}
	if(1 == rank)
	{
		read_whole_file_in_stdio("data/B_nanpu.jpg");
		read_whole_file_in_stdio("data/hezi.tga");
		// read_whole_file_in_stdio("data/1.dat");
	}else if(2 == rank)
	{
		// read_whole_file_in_syscall("data/hezi.tga");
	}
	// sleep(20);
	dm_finish();

	MPI_Finalize();
	return 0;
}
