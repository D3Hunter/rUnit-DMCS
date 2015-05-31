#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <infiniband/verbs.h>
#include <assert.h>
#include "mpi.h"
#include <errno.h>

#define DATA_WINDOW_SIZE 1024
#define READ_BUF_SIZE 128
#define FILLED_LEN 10
#define IB_PORT 1

#define DEBUG_PRINT_HEADER ">>>>>> Rank <<<<<< : "
#define DEBUG_PRINT(format, ...) printf(DEBUG_PRINT_HEADER format, ##__VA_ARGS__)

#define TEST_ZERO(x)	if((x)){printf("ERROR : " #x " failed (equal non-zero).\n"); exit(0);}
#define TEST_NONZERO(x) if(!(x)){printf("ERROR : " #x "failed (equal zero).\n"); exit(0);}

struct config_t
{
};

struct remote_addr
{
	uint64_t addr;				/* buf address */
	uint32_t rkey;				/* key for remote access */
	uint16_t lid;				/* LID of the IB port */
};

struct ib_common
{
	int rankSize;
	int rank;
	int port;

	struct ibv_port_attr port_attr;
	struct ibv_context *ib_ctx;
	struct ibv_pd *pd;
	struct ibv_mr *mr;
	const char *dev_name;
	char *base;					/* base for data window */
	int size;					/* data window size */
	struct remote_addr remote_addr;
};

struct resources
{
	struct ibv_cq *cq;
	struct ibv_qp *qp;
	struct ibv_mr *mr;			/* for rdma read */
	char *buf;					/* for rdma read */
};
struct ib_common ibcommon = {0};

static void ib_init_common(int sharedSize, int port)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *ib_dev = NULL;
	struct remote_addr local, *gather_buf;
	int mr_flags = 0;
	int num_devices;
	int i;
	int len;

	ibcommon.port = port;
	ibcommon.size = sharedSize;

	TEST_NONZERO(dev_list = ibv_get_device_list(&num_devices));
	TEST_NONZERO(num_devices);
	DEBUG_PRINT("found %d device(s)\n", num_devices);
	for(i = 0; i < num_devices; i++)
	{
		if(NULL == ibcommon.dev_name)
		{
			ibcommon.dev_name = strdup(ibv_get_device_name(dev_list[i]));
			DEBUG_PRINT("Device not specified, using first one found: %s\n", ibcommon.dev_name);
		}
		if(0 == strcmp(ibv_get_device_name(dev_list[i]), ibcommon.dev_name))
		{
			ib_dev = dev_list[i];
			break;
		}
	}
	TEST_NONZERO(ib_dev);
	TEST_NONZERO(ibcommon.ib_ctx = ibv_open_device(ib_dev));
	/* free device list */
	ibv_free_device_list(dev_list);

	TEST_ZERO(ibv_query_port(ibcommon.ib_ctx, ibcommon.port, &ibcommon.port_attr));
	TEST_NONZERO(ibcommon.pd = ibv_alloc_pd(ibcommon.ib_ctx));
	TEST_NONZERO(ibcommon.base = malloc(ibcommon.size));
	memset(ibcommon.base, 0, ibcommon.size);
	/* register the mem buf */
	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
	TEST_NONZERO(ibcommon.mr = ibv_reg_mr(ibcommon.pd, ibcommon.base, ibcommon.size, mr_flags));
	memset(&local, 0, sizeof(local));
	local.addr = (uintptr_t)ibcommon.base;
	local.rkey = ibcommon.mr->rkey;
	local.lid = ibcommon.port_attr.lid;

	/* exchange info required to connect QPs and MR to read/write to */
	assert(ibcommon.rankSize == 2);
	len = ibcommon.rankSize * sizeof(struct remote_addr);
	TEST_NONZERO(gather_buf = (struct remote_addr*)malloc(len));
	MPI_Allgather(&local, sizeof(local), MPI_BYTE, gather_buf, sizeof(local), MPI_BYTE, MPI_COMM_WORLD);
	ibcommon.remote_addr = gather_buf[1 - ibcommon.rank];

	DEBUG_PRINT("Remote addr|rkey|LID is 0x%"PRIx64"|0x%x|0x%x \n",
				ibcommon.remote_addr.addr,
				ibcommon.remote_addr.rkey,
				ibcommon.remote_addr.lid);
}
static void create_connect_qp(struct resources *res)
{
	struct ibv_qp_init_attr qp_init_attr;
	int mr_flags = 0;
	uint32_t *gather_buf;
	int len;
	struct ibv_qp_attr attr;
	int flags;

	/* create local mr */
	TEST_NONZERO(res->buf = malloc(READ_BUF_SIZE));
	memset(res->buf, 0, READ_BUF_SIZE);
	/* register the mem buf */
	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
	TEST_NONZERO(res->mr = ibv_reg_mr(ibcommon.pd, res->buf, READ_BUF_SIZE, mr_flags));

	/* create QP */
	TEST_NONZERO(res->cq = ibv_create_cq(ibcommon.ib_ctx, 1, NULL, NULL, 0));
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.sq_sig_all = 1;
	qp_init_attr.send_cq = res->cq;
	qp_init_attr.recv_cq = res->cq;
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	TEST_NONZERO(res->qp = ibv_create_qp(ibcommon.pd, &qp_init_attr));

	/* exchange qp_num */
	assert(ibcommon.rankSize == 2);
	len = ibcommon.rankSize * sizeof(uint32_t);
	TEST_NONZERO(gather_buf = (uint32_t *)malloc(len));
	MPI_Allgather(&res->qp->qp_num, sizeof(uint32_t), MPI_BYTE, gather_buf, sizeof(uint32_t), MPI_BYTE, MPI_COMM_WORLD);

	DEBUG_PRINT("Local/remote qpn is 0x%x|0x%x\n", res->qp->qp_num, gather_buf[1-ibcommon.rank]);

	memset(&attr, 0, sizeof(attr));
	/* modify QP to INIT */
	attr.qp_state = IBV_QPS_INIT;
	attr.port_num = ibcommon.port;
	attr.pkey_index = 0;
	attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
	flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags));
	DEBUG_PRINT("QP to INIT on rank %d... \n", ibcommon.rank);

	/* QP to RTR */
	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_256;
	attr.dest_qp_num = gather_buf[1-ibcommon.rank];
	attr.rq_psn = 0;
	attr.max_dest_rd_atomic = 1;
	attr.min_rnr_timer = 0x12;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.dlid = ibcommon.remote_addr.lid;
	attr.ah_attr.sl = 0;
	attr.ah_attr.src_path_bits = 0;
	attr.ah_attr.port_num = ibcommon.port; /* same as local */
	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
		IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags));
	DEBUG_PRINT("QP to RTR on rank %d... \n", ibcommon.rank);

	/* QP to RTS */
	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 0x12;
	attr.retry_cnt = 6;
	attr.rnr_retry = 0;
	attr.sq_psn = 0;
	attr.max_rd_atomic = 1;
	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags));
	DEBUG_PRINT("QP to RTS on rank %d... \n", ibcommon.rank);
}
/* static void resources_init_create(struct resources *res, int size, int port) */
/* { */
/* 	struct ibv_device **dev_list = NULL; */
/* 	struct ibv_qp_init_attr qp_init_attr; */
/* 	struct ibv_device *ib_dev = NULL; */
/* 	int mr_flags = 0; */
/* 	int cq_size = 1; */
/* 	int num_devices; */
/* 	int rc = 0; */
/* 	int i; */
/* 	struct ibv_context *tmp_ctx;	 */
/* 	TEST_NONZERO(dev_list = ibv_get_device_list(&num_devices)); */
/* 	TEST_NONZERO(num_devices); */
/* 	DEBUG_PRINT("found %d device(s)\n", num_devices); */
/* 	for(i = 0; i < num_devices; i++) */
/* 	{ */
/* 		if(NULL == config.dev_name) */
/* 		{ */
/* 			config.dev_name = strdup(ibv_get_device_name(dev_list[i])); */
/* 			DEBUG_PRINT("Device not specified, using first one found: %s\n", config.dev_name); */
/* 		} */
/* 		if(0 == strcmp(ibv_get_device_name(dev_list[i]), config.dev_name)) */
/* 		{ */
/* 			ib_dev = dev_list[i]; */
/* 			break; */
/* 		} */
/* 	} */
/* 	TEST_NONZERO(ib_dev); */
/* 	TEST_NONZERO(res->ib_ctx = ibv_open_device(ib_dev)); */
/* 	/\* free device list *\/ */
/* 	ibv_free_device_list(dev_list); */
/* 	res->port = port; */

/* 	TEST_ZERO(ibv_query_port(res->ib_ctx, res->port, &res->port_attr)); */
/* 	TEST_NONZERO(res->pd = ibv_alloc_pd(res->ib_ctx)); */
/* 	TEST_NONZERO(res->cq = ibv_create_cq(res->ib_ctx, cq_size, NULL, NULL, 0)); */
/* 	res->size = size;			/\* data window size *\/ */
/* 	TEST_NONZERO(res->base = malloc(res->size)); */
/* 	memset(res->base, 0, res->size); */
/* 	/\* register the mem buf *\/ */
/* 	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ; */
/* 	TEST_NONZERO(res->mr = ibv_reg_mr(res->pd, res->base, res->size, mr_flags)); */
/* 	/\* create QP *\/ */
/* 	memset(&qp_init_attr, 0, sizeof(qp_init_attr)); */
/* 	qp_init_attr.qp_type = IBV_QPT_RC; */
/* 	qp_init_attr.sq_sig_all = 1; */
/* 	qp_init_attr.send_cq = res->cq; */
/* 	qp_init_attr.recv_cq = res->cq; */
/* 	qp_init_attr.cap.max_send_wr = 1; */
/* 	qp_init_attr.cap.max_recv_wr = 1; */
/* 	qp_init_attr.cap.max_send_sge = 1; */
/* 	qp_init_attr.cap.max_recv_sge = 1; */
/* 	TEST_NONZERO(res->qp = ibv_create_qp(res->pd, &qp_init_attr)); */
/* 	DEBUG_PRINT("QP was created, QP number = 0x%x \n", res->qp->qp_num); */
/* } */

/* static void connect_to_another_qp(struct resources *src, struct resources *dest) */
/* { */
/* 	struct ibv_device **dev_list = NULL; */
/* 	struct ibv_qp_init_attr qp_init_attr; */
/* 	struct ibv_device *ib_dev = NULL; */
/* 	int mr_flags = 0; */
/* 	int cq_size = 1; */
/* 	int num_devices; */
/* 	int rc = 0; */
/* 	int i; */
/* 	int len; */
/* 	struct cm_con_data local; */
/* 	struct cm_con_data *accu_con = NULL; */
/* 	union ibv_gid my_gid; */
/* 	struct ibv_qp_attr attr; */
/* 	int flags; */
/* 	dest->ib_ctx = src->ib_ctx; */
/* #define NEW_PORT config.ib_port */
/* 	errno = 0; */
/* 	if(0 != (ibv_query_port(dest->ib_ctx, NEW_PORT, &dest->port_attr))) */
/* 	{ */
/* 		perror("ERROR query_port"); */
/* 	} */
/* 	TEST_NONZERO(dest->pd = ibv_alloc_pd(dest->ib_ctx)); */
/* 	TEST_NONZERO(dest->cq = ibv_create_cq(dest->ib_ctx, cq_size, NULL, NULL, 0)); */
/* 	dest->size = src->size;			/\* data window size *\/ */
/* 	TEST_NONZERO(dest->base = malloc(dest->size)); */
/* 	memset(dest->base, 0, dest->size); */
/* 	/\* register the mem buf *\/ */
/* 	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ; */
/* 	TEST_NONZERO(dest->mr = ibv_reg_mr(dest->pd, dest->base, dest->size, mr_flags)); */

/* 	/\* create QP *\/ */
/* 	memset(&qp_init_attr, 0, sizeof(qp_init_attr)); */
/* 	qp_init_attr.qp_type = IBV_QPT_RC; */
/* 	qp_init_attr.sq_sig_all = 1; */
/* 	qp_init_attr.send_cq = dest->cq; */
/* 	qp_init_attr.recv_cq = dest->cq; */
/* 	qp_init_attr.cap.max_send_wr = 1; */
/* 	qp_init_attr.cap.max_recv_wr = 1; */
/* 	qp_init_attr.cap.max_send_sge = 1; */
/* 	qp_init_attr.cap.max_recv_sge = 1; */
/* 	TEST_NONZERO(dest->qp = ibv_create_qp(dest->pd, &qp_init_attr)); */
/* 	printf("NEW QP was created, QP number = 0x%x \n", dest->qp->qp_num); */

/* 	memset(&my_gid, 0, sizeof(my_gid)); */
/* 	local.addr = (uintptr_t)dest->base; */
/* 	local.rkey = dest->mr->rkey; */
/* 	local.qp_num = dest->qp->qp_num; */
/* 	local.lid = dest->port_attr.lid; */
/* 	memcpy(local.gid, &my_gid, 16); */

/* 	/\* exchange info required to connect QPs *\/ */
/* 	dest->rankSize = src->rankSize; */
/* 	dest->rank = src->rank; */
/* 	assert(dest->rankSize == 2); */
/* 	len = dest->rankSize * sizeof(struct cm_con_data); */
/* 	TEST_NONZERO(accu_con = (struct cm_con_data*)malloc(len)); */
/* 	dest->accu = accu_con; */
/* 	MPI_Allgather(&local, sizeof(local), MPI_BYTE, accu_con, sizeof(local), MPI_BYTE, MPI_COMM_WORLD); */
/* 	dest->remote_props = accu_con[1-dest->rank]; */
/* 	printf("Remote addr|rkey|QP-num|LID is 0x%"PRIx64"|0x%x|0x%x|0x%x \n", */
/* 				dest->remote_props.addr, */
/* 				dest->remote_props.rkey, */
/* 				dest->remote_props.qp_num, */
/* 				dest->remote_props.lid); */
	
/* 	/\* modify QP to INIT *\/ */
/* 	memset(&attr, 0, sizeof(attr)); */
/* 	attr.qp_state = IBV_QPS_INIT; */
/* 	attr.port_num = NEW_PORT; */
/* 	attr.pkey_index = 0; */
/* 	attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ; */
/* 	flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS; */
/* 	TEST_ZERO(ibv_modify_qp(dest->qp, &attr, flags)); */
/* 	printf("QP to INIT... \n"); */

/* 	/\* QP to RTR *\/ */
/* 	attr.qp_state = IBV_QPS_RTR; */
/* 	attr.path_mtu = IBV_MTU_256; */
/* 	attr.dest_qp_num = dest->remote_props.qp_num; */
/* 	attr.rq_psn = 0; */
/* 	attr.max_dest_rd_atomic = 1; */
/* 	attr.min_rnr_timer = 0x12; */
/* 	attr.ah_attr.is_global = 0; */
/* 	attr.ah_attr.dlid = dest->remote_props.lid; */
/* 	attr.ah_attr.sl = 0; */
/* 	attr.ah_attr.src_path_bits = 0; */
/* 	attr.ah_attr.port_num = config.ib_port; */
/* 	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | */
/* 		IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER; */
/* 	TEST_ZERO(ibv_modify_qp(dest->qp, &attr, flags)); */
/* 	printf("QP to RTR... \n"); */

/* 	/\* QP to RTS *\/ */
/* 	attr.qp_state = IBV_QPS_RTS; */
/* 	attr.timeout = 0x12; */
/* 	attr.retry_cnt = 6; */
/* 	attr.rnr_retry = 0; */
/* 	attr.sq_psn = 0; */
/* 	attr.max_rd_atomic = 1; */
/* 	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | */
/* 		IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC; */
/* 	TEST_ZERO(ibv_modify_qp(dest->qp, &attr, flags)); */
/* 	printf("QP to RTS... \n"); */
/* } */

/* /\* only work for 2 process *\/ */
/* static void connect_qp(struct resources *res) */
/* { */
/* 	struct cm_con_data local; */
/* 	struct cm_con_data *accu_con = NULL; */
/* 	union ibv_gid my_gid; */
/* 	int len; */
/* 	struct ibv_qp_attr attr; */
/* 	int flags; */

/* 	memset(&my_gid, 0, sizeof(my_gid)); */
/* 	local.addr = (uintptr_t)res->base; */
/* 	local.rkey = res->mr->rkey; */
/* 	local.qp_num = res->qp->qp_num; */
/* 	local.lid = res->port_attr.lid; */
/* 	memcpy(local.gid, &my_gid, 16); */

/* 	/\* exchange info required to connect QPs *\/ */
/* 	assert(res->rankSize == 2); */
/* 	len = res->rankSize * sizeof(struct cm_con_data); */
/* 	TEST_NONZERO(accu_con = (struct cm_con_data*)malloc(len)); */
/* 	res->accu = accu_con; */
/* 	MPI_Allgather(&local, sizeof(local), MPI_BYTE, accu_con, sizeof(local), MPI_BYTE, MPI_COMM_WORLD); */
/* 	res->remote_props = accu_con[1-res->rank]; */
/* 	DEBUG_PRINT("Remote addr|rkey|QP-num|LID is 0x%"PRIx64"|0x%x|0x%x|0x%x \n", */
/* 				res->remote_props.addr, */
/* 				res->remote_props.rkey, */
/* 				res->remote_props.qp_num, */
/* 				res->remote_props.lid); */
/* 	/\* modify QP to INIT *\/ */
/* 	memset(&attr, 0, sizeof(attr)); */
/* 	attr.qp_state = IBV_QPS_INIT; */
/* 	attr.port_num = res->port; */
/* 	attr.pkey_index = 0; */
/* 	attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ; */
/* 	flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS; */
/* 	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags)); */
/* 	DEBUG_PRINT("QP to INIT... \n"); */

/* 	/\* QP to RTR *\/ */
/* 	attr.qp_state = IBV_QPS_RTR; */
/* 	attr.path_mtu = IBV_MTU_256; */
/* 	attr.dest_qp_num = res->remote_props.qp_num; */
/* 	attr.rq_psn = 0; */
/* 	attr.max_dest_rd_atomic = 1; */
/* 	attr.min_rnr_timer = 0x12; */
/* 	attr.ah_attr.is_global = 0; */
/* 	attr.ah_attr.dlid = res->remote_props.lid; */
/* 	attr.ah_attr.sl = 0; */
/* 	attr.ah_attr.src_path_bits = 0; */
/* 	attr.ah_attr.port_num = res->port; */
/* 	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | */
/* 		IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER; */
/* 	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags)); */
/* 	DEBUG_PRINT("QP to RTR... \n"); */

/* 	/\* QP to RTS *\/ */
/* 	attr.qp_state = IBV_QPS_RTS; */
/* 	attr.timeout = 0x12; */
/* 	attr.retry_cnt = 6; */
/* 	attr.rnr_retry = 0; */
/* 	attr.sq_psn = 0; */
/* 	attr.max_rd_atomic = 1; */
/* 	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | */
/* 		IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC; */
/* 	TEST_ZERO(ibv_modify_qp(res->qp, &attr, flags)); */
/* 	DEBUG_PRINT("QP to RTS... \n"); */
/* } */

static void post_rdma_read(struct resources *res, int offset, int len)
{
	struct ibv_send_wr sr;
	struct ibv_sge sge;
	struct ibv_send_wr *bad_wr = NULL;
	/* prepare the scatter/gather entry */
	memset(&sge, 0, sizeof(sge));
	sge.addr = (uintptr_t)res->buf;
	sge.length = len;
	sge.lkey = res->mr->lkey;
	/* prepare the send work request */
	memset(&sr, 0, sizeof(sr));
	sr.next = NULL;
	sr.wr_id = 0;
	sr.sg_list = &sge;
	sr.num_sge = 1;
	sr.opcode = IBV_WR_RDMA_READ;
	sr.send_flags = IBV_SEND_SIGNALED;
	sr.wr.rdma.remote_addr = ibcommon.remote_addr.addr + offset;
	sr.wr.rdma.rkey = ibcommon.remote_addr.rkey;
	TEST_ZERO(ibv_post_send(res->qp, &sr, &bad_wr));
}

/* static void resources_destroy(struct resources *res) */
/* { */
/* 	if(res->qp){ TEST_ZERO(ibv_destroy_qp(res->qp));} */
/* 	if(res->mr){ TEST_ZERO(ibv_dereg_mr(res->mr));} */
/* 	if(res->base){free(res->base);} */
/* 	if(res->cq){ TEST_ZERO(ibv_destroy_cq(res->cq));} */
/* 	if(res->pd){ TEST_ZERO(ibv_dealloc_pd(res->pd));} */
/* 	if(res->ib_ctx){ TEST_ZERO(ibv_close_device(res->ib_ctx));} */
/* 	if(res->accu){ free(res->accu);} */
/* } */

static void poll_completion(struct resources *res)
{
	struct ibv_wc wc;
	unsigned long start_time_msec;
	unsigned long cur_time_msec;
	struct timeval cur_time;
	int poll_result;
	/* polling, busy waiting */
	do {
		poll_result = ibv_poll_cq(res->cq, 1, &wc);
	}while(poll_result == 0);
	if(poll_result < 0)
	{
		DEBUG_PRINT("FAILED to poll CQ\n");
		exit(0);
	}else
	{
		assert(wc.opcode == IBV_WC_RDMA_READ);
		DEBUG_PRINT("Found CQE in CQ with status 0x%x code %d\n", wc.status, wc.opcode);
		if(wc.status != IBV_WC_SUCCESS)
		{
			DEBUG_PRINT("God bad CQE with vencor syndrome: 0x%x\n", wc.vendor_err);
		}
	}
}

int main(int argc, char **argv)
{
	struct resources res, res2;

	memset(&ibcommon, 0, sizeof(ibcommon));

	/* init MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &ibcommon.rankSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &ibcommon.rank);

	assert(ibcommon.rankSize == 2); 	   /* only for 2 process */

	/* parse parameters */
	ib_init_common(DATA_WINDOW_SIZE, IB_PORT);
	/* connect QPs */
	create_connect_qp(&res);

	/* rank 0 fill mem with 0 */

	if(0 == ibcommon.rank)
	{
		memset(ibcommon.base, 'A', FILLED_LEN);
		memset(ibcommon.base + FILLED_LEN, 'B', FILLED_LEN);
	}
	/* MPI_Barrier */
	MPI_Barrier(MPI_COMM_WORLD);
	/* rank 1 post read */
	if(0 == ibcommon.rank)
	{
		/* connect QPs */
		create_connect_qp(&res2);
	}else if(1 == ibcommon.rank)
	{
		/* post read */
		post_rdma_read(&res, 0, FILLED_LEN);
		/* poll completion */
		poll_completion(&res);
		printf("RDMA read info : %s\n", res.buf);

		/* connect QPs */
		create_connect_qp(&res2);
		post_rdma_read(&res2, FILLED_LEN, FILLED_LEN);
		poll_completion(&res2);
		printf("NEW RDMA read info : %s\n", res2.buf);
	}
	/* MPI_Barrier */
	MPI_Barrier(MPI_COMM_WORLD);
	/* destroy resources */
	/* TODO: */

	MPI_Finalize();
}
