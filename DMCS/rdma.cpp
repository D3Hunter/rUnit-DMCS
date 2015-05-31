////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    rdma.cpp
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周一 十二月  2 22:17:20 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#include "rdma.h"
#include "dm.h"
void ib_init_common(int size, int port)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *ib_dev = NULL;
	struct remote_addr local;
	int mr_flags = 0;
	int num_devices;
	int len;

	dm_cfg.ibCommon.port = port;
	dm_cfg.ibCommon.size = size;

	TEST_NONZERO(dev_list = ibv_get_device_list(&num_devices));
	TEST_NONZERO(num_devices);
	DEBUG_PRINT("found %d device(s)\n", num_devices);
	for(int i = 0; i < num_devices; i++)
	{
		DEBUG_PRINT("Device not specified, using first one found: %s\n", ibv_get_device_name(dev_list[i]));
		ib_dev = dev_list[i];
	}
	TEST_NONZERO(ib_dev);
	TEST_NONZERO(dm_cfg.ibCommon.ib_ctx = ibv_open_device(ib_dev));
	/* free device list */
	ibv_free_device_list(dev_list);

	TEST_ZERO(ibv_query_port(dm_cfg.ibCommon.ib_ctx, dm_cfg.ibCommon.port, &dm_cfg.ibCommon.port_attr));
	TEST_NONZERO(dm_cfg.ibCommon.pd = ibv_alloc_pd(dm_cfg.ibCommon.ib_ctx));
	TEST_NONZERO(dm_cfg.ibCommon.base = (char *)malloc(dm_cfg.ibCommon.size));
	memset(dm_cfg.ibCommon.base, 0, dm_cfg.ibCommon.size);
	/* register the mem buf */
	mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
	TEST_NONZERO(dm_cfg.ibCommon.mr = ibv_reg_mr(dm_cfg.ibCommon.pd, dm_cfg.ibCommon.base, dm_cfg.ibCommon.size, mr_flags));

	/* exchange info required to connect QPs and MR to read/write to */
	memset(&local, 0, sizeof(local));
	local.addr = (uintptr_t)dm_cfg.ibCommon.base;
	local.rkey = dm_cfg.ibCommon.mr->rkey;
	local.lid = dm_cfg.ibCommon.port_attr.lid;
	len = dm_cfg.rankSize * sizeof(struct remote_addr);
	TEST_NONZERO(dm_cfg.remoteAddrs = (struct remote_addr*)malloc(len));
	memset(dm_cfg.remoteAddrs, 0, len);
	MPI_Allgather(&local, sizeof(local), MPI_BYTE, dm_cfg.remoteAddrs, sizeof(local), MPI_BYTE, MPI_COMM_WORLD);

	// create mr for RDMA READ
	len = dm_cfg.conPerRank * sizeof(struct memory_region);
	TEST_NONZERO(dm_cfg.localRegion = (struct memory_region *)malloc(len));
	memset(dm_cfg.localRegion, 0, len);
	for(int i = 0; i < dm_cfg.conPerRank; i++)
	{
		TEST_NONZERO(dm_cfg.localRegion[i].buf = (char *)malloc(READ_BUF_SIZE));
		memset(dm_cfg.localRegion[i].buf, 0, READ_BUF_SIZE);
		mr_flags = IBV_ACCESS_LOCAL_WRITE;
		TEST_NONZERO(dm_cfg.localRegion[i].mr = ibv_reg_mr(dm_cfg.ibCommon.pd, dm_cfg.localRegion[i].buf, READ_BUF_SIZE, mr_flags));
	}
}

void ib_create_connect_qps()
{
	struct ibv_qp_init_attr qp_init_attr;
	int mr_flags = 0;
	uint32_t *local;
	uint32_t *gather_buf;
	int num_of_qps, len;
	struct ibv_qp_attr attr;
	int flags;

	// allocate QPs
	num_of_qps = dm_cfg.rankSize * dm_cfg.conPerRank;
	len = num_of_qps * sizeof(struct conn_data);
	TEST_NONZERO(dm_cfg.conns = (struct conn_data *)malloc(len));
	memset(dm_cfg.conns, 0, len);
	len = num_of_qps * sizeof(uint32_t);
	TEST_NONZERO(local = (uint32_t *)malloc(len));
	memset(local, 0, len);

	for(int i = 0; i < num_of_qps; i++)
	{
		/* create QP */
		TEST_NONZERO(dm_cfg.conns[i].cq = ibv_create_cq(dm_cfg.ibCommon.ib_ctx, 1, NULL, NULL, 0));
		memset(&qp_init_attr, 0, sizeof(qp_init_attr));
		qp_init_attr.qp_type = IBV_QPT_RC;
		qp_init_attr.sq_sig_all = 1;
		qp_init_attr.send_cq = dm_cfg.conns[i].cq;
		qp_init_attr.recv_cq = dm_cfg.conns[i].cq;
		qp_init_attr.cap.max_send_wr = 1;
		qp_init_attr.cap.max_recv_wr = 1;
		qp_init_attr.cap.max_send_sge = 1;
		qp_init_attr.cap.max_recv_sge = 1;
		TEST_NONZERO(dm_cfg.conns[i].qp = ibv_create_qp(dm_cfg.ibCommon.pd, &qp_init_attr));
		local[i] = dm_cfg.conns[i].qp->qp_num;
	}
	/* exchange qp_num */
	len = num_of_qps * sizeof(uint32_t);
	TEST_NONZERO(gather_buf = (uint32_t *)malloc(len));
	memset(gather_buf, 0, len);
	MPI_Alltoall(local, dm_cfg.conPerRank, MPI_UINT32_T, 
				 gather_buf, dm_cfg.conPerRank, MPI_UINT32_T,
				 MPI_COMM_WORLD);

	int dest;
	for(int i = 0; i < num_of_qps; i++)
	{
		dest = i / dm_cfg.conPerRank;
		if(dest == dm_cfg.rank) // skip local-local conn
			continue;
		memset(&attr, 0, sizeof(attr));
		/* modify QP to INIT */
		attr.qp_state = IBV_QPS_INIT;
		attr.port_num = dm_cfg.ibCommon.port;
		attr.pkey_index = 0;
		attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;
		flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
		TEST_ZERO(ibv_modify_qp(dm_cfg.conns[i].qp, &attr, flags));

		/* QP to RTR */
		attr.qp_state = IBV_QPS_RTR;
		attr.path_mtu = IBV_MTU_256;
		attr.dest_qp_num = gather_buf[i];
		attr.rq_psn = 0;
		attr.max_dest_rd_atomic = 1;
		attr.min_rnr_timer = 0x12;
		attr.ah_attr.is_global = 0;
		attr.ah_attr.dlid = dm_cfg.remoteAddrs[dest].lid;
		attr.ah_attr.sl = 0;
		attr.ah_attr.src_path_bits = 0;
		attr.ah_attr.port_num = dm_cfg.ibCommon.port; /* same as local */
		flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
			IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
		TEST_ZERO(ibv_modify_qp(dm_cfg.conns[i].qp, &attr, flags));

		/* QP to RTS */
		attr.qp_state = IBV_QPS_RTS;
		attr.timeout = 0x12;
		attr.retry_cnt = 6;
		attr.rnr_retry = 0;
		attr.sq_psn = 0;
		attr.max_rd_atomic = 1;
		flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
			IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
		TEST_ZERO(ibv_modify_qp(dm_cfg.conns[i].qp, &attr, flags));
	}
}
void ib_post_rdma_read(struct conn_data *conn, struct remote_addr *remote,
					   struct memory_region *local, int offset, int len)
{
	struct ibv_send_wr sr;
	struct ibv_sge sge;
	struct ibv_send_wr *bad_wr = NULL;
	/* prepare the scatter/gather entry */
	memset(&sge, 0, sizeof(sge));
	sge.addr = (uintptr_t)local->buf;
	sge.length = len;
	sge.lkey = local->mr->lkey;
	/* prepare the send work request */
	memset(&sr, 0, sizeof(sr));
	sr.next = NULL;
	sr.wr_id = 0;
	sr.sg_list = &sge;
	sr.num_sge = 1;
	sr.opcode = IBV_WR_RDMA_READ;
	sr.send_flags = IBV_SEND_SIGNALED;
	sr.wr.rdma.remote_addr = remote->addr + offset;
	sr.wr.rdma.rkey = remote->rkey;
	TEST_ZERO(ibv_post_send(conn->qp, &sr, &bad_wr));
}
void ib_poll_completion(struct conn_data *conn)
{
	struct ibv_wc wc;
	unsigned long start_time_msec;
	unsigned long cur_time_msec;
	struct timeval cur_time;
	int poll_result;
	/* polling, busy waiting */
	do {
		poll_result = ibv_poll_cq(conn->cq, 1, &wc);
	}while(poll_result == 0);
	if(poll_result < 0)
	{
		DEBUG_PRINT("FAILED to poll CQ\n");
		exit(0);
	}else
	{
		assert(wc.opcode == IBV_WC_RDMA_READ);
		// DEBUG_PRINT("Found CQE in CQ with status 0x%x code %d\n", wc.status, wc.opcode);
		if(wc.status != IBV_WC_SUCCESS)
		{
			DEBUG_PRINT("God bad CQE with vencor syndrome: 0x%x\n", wc.vendor_err);
		}
	}
}
void ib_destroy()
{
	// free local RDMA buf
	for(int i = 0; i < dm_cfg.conPerRank; i++)
	{
		ibv_dereg_mr(dm_cfg.localRegion[i].mr);
		free(dm_cfg.localRegion[i].buf);
	}
	for(int i = 0; i < dm_cfg.conPerRank * dm_cfg.rankSize; i++)
	{
		ibv_destroy_cq(dm_cfg.conns[i].cq);
		ibv_destroy_qp(dm_cfg.conns[i].qp);
	}
	free(dm_cfg.remoteAddrs);
	ibv_dereg_mr(dm_cfg.ibCommon.mr);
	free(dm_cfg.ibCommon.base);
	ibv_dealloc_pd(dm_cfg.ibCommon.pd);
	ibv_close_device(dm_cfg.ibCommon.ib_ctx);
}
