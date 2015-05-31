////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    rdma.h
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周三 十二月  4 15:10:30 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __RDMA_H__
#define __RDMA_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <infiniband/verbs.h>
#include <assert.h>
#include <errno.h>
#include "mpi.h"
#include "common.h"

#define IB_PORT 1
#define READ_BUF_SIZE 1048576	// 1M

#define TEST_ZERO(x)	if((x)){DEBUG_PRINT("ERROR : " #x " failed (equal non-zero).\n"); exit(0);}
#define TEST_NONZERO(x) if(!(x)){DEBUG_PRINT("ERROR : " #x "failed (equal zero).\n"); exit(0);}

struct remote_addr
{
	uint64_t addr;				/* buf address */
	uint32_t rkey;				/* key for remote access */
	uint16_t lid;				/* LID of the IB port */
};

struct ib_common
{
	int port;
	struct ibv_port_attr port_attr;
	struct ibv_context *ib_ctx;
	struct ibv_pd *pd;
	struct ibv_mr *mr;
	char *base;					/* base for data window */
	int size;					/* data window size */
};

struct conn_data
{
	struct ibv_cq *cq;
	struct ibv_qp *qp;
	int busy;					// been used
};
struct memory_region
{
	struct ibv_mr *mr;			/* for rdma read */
	char *buf;					/* for rdma read */
	int busy;					// been used
};

void ib_init_common(int size, int port);
void ib_create_connect_qps();
void ib_post_rdma_read(struct conn_data *conn, struct remote_addr *remote,
					   struct memory_region *local, int offset, int len);
void ib_poll_completion(struct conn_data *conn);
void ib_destroy();
#endif
