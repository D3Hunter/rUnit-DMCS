#include "../mystdio.h"
#include <mpi.h>
#include <stdlib.h>
#include <string.h>
// make with mpicc -o run test1.cpp -L.. -lsdudmcs
#define REPLICATION 1

#define BLOCKSIZE 8192
int rank;

void read_whole_file_in_stdio(char *fileName)
{
	char buf[BLOCKSIZE];
	char outName[128];
	static int num = 1;

	memset(buf, 0, BLOCKSIZE);
	FILE *fp = fopen(fileName, "r");
	int len = 0;
	while((len = fread(buf, 1, BLOCKSIZE, fp)) > 0)
	{
		buf[len] = 0;
		printf(">>>>>>> %s on rank %d\n", buf, rank);
	}
	fclose(fp);
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

	if(2 == rank || 1 == rank)
	{
		read_whole_file_in_stdio("data/1.txt");
		read_whole_file_in_stdio("data/2.txt");
		// read_whole_file_in_stdio("data/1.dat");
	}
	MPI_Finalize();
	// sleep(20);
	dm_finish();

	return 0;
}
