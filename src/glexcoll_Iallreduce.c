#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <endian.h>
#include <sched.h>
#include <errno.h>
#include <mpi.h>
#include <stdint.h>
#include <sys/mman.h>
#include <pthread.h>
#include "glex.h"
#include "glexcoll.h"

int inter_comm_mode = OFFLOAD; //0 offload ,1 non-offload

int GLEXCOLL_AllreduceFinalize()
{
	if(intra_procn > 1)
	{
		munmap((void *)shared_release_flags, Get_shared_buffer_size());
		munmap((void *)_GLEXCOLL.SHM_buf, 64 * intra_procn);
	}
}

int _TreeID = 1;
void GLEXCOLL_Iallreduce_LoadingData(void *sendbuf, int count, MPI_Datatype mpitype, MPI_Op op, enum glex_coll_reduce_op *GLEXOP)
{

	for (int i = 0; i < count; i++)
	{

		data_loc[i].double_s.data = ((double *)sendbuf)[i];
		data_loc[i].double_s.location = _ReduceTreeS[_TreeID].DownReduceRank;
	}
	*GLEXOP = GLEX_COLL_REDUCE_SUM_FLOAT;
	// switch ((uint64_t) mpitype)
	// {
	// case (MPI_DOUBLE):
	// 	for(int i = 0;i<count;i++)
	// 	{
	// 			//将数据装载
	// 			data_loc[i].double_s.data = ((double *)sendbuf)[i];
	// 			data_loc[i].double_s.location = _ReduceTreeS[_TreeID].DownReduceRank;
	// 	}
	// 	switch ((uint64_t)op)
	// 	{
	// 		case MPI_SUM:
	// 			*GLEXOP = GLEX_COLL_REDUCE_SUM_FLOAT;
	// 			break;
	// 		default:
	// 			break;
	// 	}
	// 	/* code */
	// 	break;
	// case MPI_INT:
	// 	for(int i = 0;i<count;i++)
	// 	{
	// 			//将数据装载
	// 			data_loc[i].int32_s.data = ((int *)sendbuf)[i];
	// 			data_loc[i].int32_s.location = _ReduceTreeS[_TreeID].DownReduceRank;
	// 	}
	// 	switch ((uint64_t)op)
	// 	{
	// 		case MPI_SUM:
	// 			*GLEXOP = GLEX_COLL_REDUCE_SUM_SIGNED;
	// 			break;
	// 		default:
	// 			break;
	// 	}
	// default:
	// 	break;
	// }
}
int GLEXCOLL_Iallreduce_Tree(void *sendbuf, void *recvbuf, int send_count, enum glex_coll_reduce_op GLEX_OP)
{
	static int CallCount = 0;
	glex_ret_t ret;
	//printf("inter rank %d\n",inter_rank);
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	int i = 0;

	if (_ReduceTreeS[_TreeID].type == LEAF)
	{
		glex_coll_compose_mp_reduce_data(data_loc,
										 send_count,
										 _GLEXCOLL._allreduce.bit_size,
										 GLEX_OP,
										 _ReduceTreeS[_TreeID].brothersN,
										 GLEX_Coll_req.data,
										 &(GLEX_Coll_req.len));

		GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
		GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START |
							 GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_COUNTER;
		GLEX_Coll_req.coll_counter = 0;

		while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		{
		}
		if (ret != GLEX_SUCCESS)
		{
			printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}
	}
	if (_ReduceTreeS[_TreeID].type == MID)
	{
		//puts("check");
		//MID PROC
		//if(inter_rank != 1)
		{

			glex_coll_compose_mp_reduce_data(data_loc,
											 send_count,
											 _GLEXCOLL._allreduce.bit_size,
											 GLEX_OP,
											 _ReduceTreeS[_TreeID].brothersN,
											 GLEX_Coll_req.data,
											 &(GLEX_Coll_req.len));
			//_GLEXCOLL.ep_addrs[inter_rank].v;//
			GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
			GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL |
								 GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP |
								 GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_COUNTER;
			GLEX_Coll_req.coll_counter = _ReduceTreeS[_TreeID].childsN;
		}
		// else
		// {
		// 	/* code */
		// 	glex_coll_compose_mp_reduce_data(data_loc,
		// 									 send_count,
		// 									 _GLEXCOLL._allreduce.bit_size,
		// 									 GLEX_OP,
		// 									 1,
		// 									 GLEX_Coll_req.data,
		// 									 &(GLEX_Coll_req.len));
		// 	//_GLEXCOLL.ep_addrs[inter_rank].v;//
		// 	GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[inter_rank].v;

		// 	GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_CP_DATA |
		// 						 GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_COUNTER;
		// 	GLEX_Coll_req.coll_counter = _ReduceTreeS[_TreeID].childsN;
		// }
		//sleep(0.2);

		while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		{
		}
		if (ret != GLEX_SUCCESS)
		{
			printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}

		// {
		// 	glex_coll_compose_mp_set_data(_ReduceTreeS[_TreeID].childAddrs1, _ReduceTreeS[_TreeID].childsN,
		// 								  GLEX_Coll_req.data, &GLEX_Coll_req.len);
		// 	GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v;
		// 	GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START |
		// 	GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_SWAP_DATA |
		// 	GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SWAP_SET | GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_DATA;
		// 	GLEX_Coll_req.coll_counter = 1;
		// 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// 	{
		// 	}
		// 	if (ret != GLEX_SUCCESS)
		// 	{
		// 		printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 		exit(1);
		// 	}
		// }

		// {
		// 		glex_coll_compose_mp_set_data(&(_GLEXCOLL.ep_addrs1[inter_rank]), 0,
		// 						GLEX_Coll_req.data, &GLEX_Coll_req.len);
		// 		GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v; /* one for itself */
		// 		GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL
		// 				| GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA
		// 				| GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SWAP_SET;
		// 		GLEX_Coll_req.coll_counter = 1;
		// 		 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY) {
		// 		}
		// 		if (ret != GLEX_SUCCESS) {
		// 			printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 			exit(1);
		// 		}
		// }

		//MID开始广播通信请求

		// int i = 0;
		// for(i = 0;i<_ReduceTreeS[_TreeID].childsN;i++)
		// 		{
		// 			GLEX_Coll_req.rmt_ep_addr.v =_ReduceTreeS[_TreeID].childAddrs1[i].v;
		// 			if(_ReduceTreeS[_TreeID].childTypes[i] == MID)
		// 			{
		// 				GLEX_Coll_req.flag =  GLEX_FLAG_COLL| GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_CP_DATA |
		// 				GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_SWAP_DATA; //GLEX_FLAG_COLL_SEQ_TAIL
		// 			}
		// 			else
		// 			{
		// 				//_ReduceTreeS[_TreeID].childTypes[i] == LEAF
		// 				GLEX_Coll_req.flag =  GLEX_FLAG_COLL| GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA |GLEX_FLAG_COLL_SWAP ; //GLEX_FLAG_COLL_SEQ_TAIL
		// 			}

		// 			if (i == 0)
		// 				GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_START;
		// 			if (i == _ReduceTreeS[_TreeID].childsN - 1)
		// 				GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_TAIL;

		// 			GLEX_Coll_req.coll_counter = 1; //
		// 			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// 			{
		// 			}
		// 			if (ret != GLEX_SUCCESS)
		// 			{
		// 				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 				exit(1);
		// 			}
		// 		}

		int i = 0;
		for (i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
		{
			GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].childAddrs1[i].v;
			if (_ReduceTreeS[_TreeID].childTypes[i] == MID)
			{
				//puts("iph_check");
				GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_CP_DATA |
									 GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_SWAP_DATA; //GLEX_FLAG_COLL_SEQ_TAIL
			}
			else
			{
				//_ReduceTreeS[_TreeID].childTypes[i] == LEAF
				GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA |
									 GLEX_FLAG_COLL_SWAP; //GLEX_FLAG_COLL_SEQ_TAIL
			}
			if (i == 0)
				GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_START;
			if (i == _ReduceTreeS[_TreeID].childsN - 1)
				GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_TAIL;

			GLEX_Coll_req.coll_counter = 1; //
											//sleep(1);
			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			{
			}
			if (ret != GLEX_SUCCESS)
			{
				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
				exit(1);
			}
		}
		// GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v;
		// GLEX_Coll_req.flag = GLEX_FLAG_FENCE | GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT |
		// 					 GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SEQ_TAIL;
		// GLEX_Coll_req.coll_counter = 1; //
		// while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// {
		// }
		// if (ret != GLEX_SUCCESS)
		// {
		// 	printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 	exit(1);
		// }
	}
	if (_ReduceTreeS[_TreeID].type == ROOT)
	{
		//root leader
		// glex_coll_compose_mp_reduce_data(data_loc,
		// 								send_count,
		// 								_GLEXCOLL._allreduce.bit_size,
		// 								GLEX_OP,
		// 								1, /* for root node, must be 1 */
		// 								GLEX_Coll_req.data,
		// 								&(GLEX_Coll_req.len));

		// GLEX_Coll_req.rmt_ep_addr.v =_ReduceTreeS[_TreeID].parentAddr.v;

		// GLEX_Coll_req.flag =   GLEX_FLAG_COLL   |  GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL |
		// GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP |
		// GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_DATA;
		// GLEX_Coll_req.coll_counter = _ReduceTreeS[_TreeID].childsN;
		// while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// {
		// }
		// if (ret != GLEX_SUCCESS)
		// {
		// 	printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 	exit(1);
		// }
		//sleep(1);

		// {
		// 	glex_coll_compose_mp_set_data(&(_ReduceTreeS[_TreeID].childAddrs), _ReduceTreeS[_TreeID].childsN,
		// 								  GLEX_Coll_req.data, &GLEX_Coll_req.len);
		// 	GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v; /* one for itself */
		// 	GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE_RESULT
		// 		| GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SWAP_SET ;//| GLEX_FLAG_COLL_COUNTER;
		// 	GLEX_Coll_req.coll_counter = 1;
		// 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// 	{
		// 	}
		// 	if (ret != GLEX_SUCCESS)
		// 	{
		// 		printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 		exit(1);
		// 	}
		// }
		//root leader 开始广播通信请求
		// int i = 0;
		// for(i = 0;i<_ReduceTreeS[_TreeID].childsN;i++)
		// {
		// 	GLEX_Coll_req.rmt_ep_addr.v =_ReduceTreeS[_TreeID].childAddrs[i].v;
		// 	if(_ReduceTreeS[_TreeID].childTypes[i] == MID)
		// 	{
		// 		//puts("iph_check");
		// 		GLEX_Coll_req.flag =  GLEX_FLAG_COLL| GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_CP_DATA |
		// 		GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_SWAP_DATA; //GLEX_FLAG_COLL_SEQ_TAIL
		// 	}
		// 	else
		// 	{
		// 		//_ReduceTreeS[_TreeID].childTypes[i] == LEAF
		// 		GLEX_Coll_req.flag =  GLEX_FLAG_COLL| GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA |  GLEX_FLAG_COLL_SWAP; //GLEX_FLAG_COLL_SEQ_TAIL
		// 	}
		// 	if(i == 0)
		// 		GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_START;
		// 	if(i == _ReduceTreeS[_TreeID].childsN - 1)
		// 		GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_TAIL;

		// 	GLEX_Coll_req.coll_counter =  1; //
		// //sleep(1);
		// 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		// 	{
		// 	}
		// 	if (ret != GLEX_SUCCESS)
		// 	{
		// 		printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
		// 		exit(1);
		// 	}
		// }
		{
			glex_coll_compose_mp_reduce_data(data_loc,
											 send_count,
											 _GLEXCOLL._allreduce.bit_size,
											 GLEX_OP,
											 1, /* for root node, must be 1 */
											 GLEX_Coll_req.data,
											 &(GLEX_Coll_req.len));

			GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v;

			GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL |
								 GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP |
								 GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_DATA;
			GLEX_Coll_req.coll_counter = _ReduceTreeS[_TreeID].childsN;
			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			{
			}
			if (ret != GLEX_SUCCESS)
			{
				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
				exit(1);
			}

			// {
			// 	glex_coll_compose_mp_set_data(_ReduceTreeS[_TreeID].childAddrs1, _ReduceTreeS[_TreeID].childsN,
			// 								GLEX_Coll_req.data, &GLEX_Coll_req.len);
			// 	GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[intra_rank].v; /* one for itself */
			// 	GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE_RESULT
			// 		| GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP |
			// 		GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP_SET | GLEX_FLAG_COLL_COUNTER;
			// 	GLEX_Coll_req.coll_counter = 1;
			// 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			// 	{
			// 	}
			// 	if (ret != GLEX_SUCCESS)
			// 	{
			// 		printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			// 		exit(1);
			// 	}
			// }

			// {
			// 	glex_coll_compose_mp_set_data(&(_GLEXCOLL.ep_addrs1[inter_rank]), 0,
			// 								  GLEX_Coll_req.data, &GLEX_Coll_req.len);
			// 	GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v; /* one for itself */
			// 	GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START |
			// 	GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE_RESULT |
			// 	GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SWAP_SET;
			// 	GLEX_Coll_req.coll_counter = 1;
			// 	while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			// 	{
			// 	}
			// 	if (ret != GLEX_SUCCESS)
			// 	{
			// 		printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			// 		exit(1);
			// 	}
			// }

			int i = 0;
			for (i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
			{
				GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].childAddrs1[i].v;
				if (_ReduceTreeS[_TreeID].childTypes[i] == MID)
				{
					//puts("iph_check");
					GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_CP_DATA |
										 GLEX_FLAG_COLL_COUNTER | GLEX_FLAG_COLL_CP_SWAP_DATA; //GLEX_FLAG_COLL_SEQ_TAIL
				}
				else
				{
					//_ReduceTreeS[_TreeID].childTypes[i] == LEAF
					GLEX_Coll_req.flag = GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA |
										 GLEX_FLAG_COLL_SWAP; //GLEX_FLAG_COLL_SEQ_TAIL
				}
				if (i == 0)
					GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_START;
				if (i == _ReduceTreeS[_TreeID].childsN - 1)
					GLEX_Coll_req.flag |= GLEX_FLAG_COLL_SEQ_TAIL;

				GLEX_Coll_req.coll_counter = 1; //
												//sleep(1);
				while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
				{
				}
				if (ret != GLEX_SUCCESS)
				{
					printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
					exit(1);
				}
			}
			// GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs1[inter_rank].v;
			// GLEX_Coll_req.flag = GLEX_FLAG_FENCE | GLEX_FLAG_COLL | GLEX_FLAG_COLL_REDUCE_RESULT |
			// 					 GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SEQ_TAIL;
			// GLEX_Coll_req.coll_counter = 1; //
			// while ((ret = glex_send_imm_mp(_GLEXCOLL.ep1, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			// {
			// }
			// if (ret != GLEX_SUCCESS)
			// {
			// 	printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			// 	exit(1);
			// }
		}
	}

	CallCount++;
}

MPI_Request MPI_Iallreduce_request;
MPI_Status MPI_Iallreduce_status;
int GLEXCOLL_Wait_Iallreduce()
{
	if (inter_procn > 1)
	{
		sem_wait(&(_GLEXCOLL.FinishREQ));
		//此处只有62个进程等待到了信号。
		int i;
		if (intra_rank == 0)
		{
			if (inter_comm_mode == OFFLOAD)
			{
				glex_ret_t ret;
				glex_ep_addr_t rmt_ep_addr;
				//if(inter_rank == 0)// || inter_rank == 1|| inter_rank == 2
				{
					// if(inter_rank != 0)
					// 	ret = glex_receive_mp(_GLEXCOLL.ep, -1, &rmt_ep_addr, GLEXCOLL_databuf, &GLEXCOLL_databuf_len);
					// else{
					// }
					ret = glex_receive_mp(_GLEXCOLL.ep1, -1, &rmt_ep_addr, GLEXCOLL_databuf, &GLEXCOLL_databuf_len);

					if (ret == GLEX_NO_MP)
					{
						printf("_receive_mp() (%d), return NO_MP?\n", __LINE__);
						exit(1);
					}
					else if (ret != GLEX_SUCCESS)
					{
						printf("_receive_mp() (%d), return: %d\n", __LINE__, ret);
						exit(1);
					}
					glex_coll_decompose_mp_reduce_data(data_loc,
													   _GLEXCOLL._allreduce.count,
													   _GLEXCOLL._allreduce.bit_size,
													   GLEXCOLL_databuf);
					//数据拷贝
					for (i = 0; i < _GLEXCOLL._allreduce.count; ++i)
					{
						if (_GLEXCOLL._allreduce.bit_size == GLEX_COLL_REDUCE_DATA_BITS_32)
						{
							((int32_t *)(_GLEXCOLL._allreduce.recvbuf))[i] = data_loc[i].int32_s.data;
						}
						else
						{
							((int64_t *)(_GLEXCOLL._allreduce.recvbuf))[i] = data_loc[i].int64_s.data;
						}
					}
					//puts("4 \t 消息传输完毕");
					double t = ((double *)(_GLEXCOLL._allreduce.recvbuf))[0];
					//if (t - 288.0 > 0.01 || t - 288.0 < -0.01)
					//printf("node rank = %d check recv result = %f\n", inter_rank, ((double *)(_GLEXCOLL._allreduce.recvbuf))[0]);
					//usleep(rand()%1000);
				}
			}

			//puts("等待完成");
		}
		//puts("check 465");
		// 	MPI_Wait(&MPI_Iallreduce_request,&MPI_Iallreduce_status);
		//sleep(1);

		if (INTRA_ALLREDUCE_TYPE == 3 && _GLEXCOLL._allreduce.datatype == MPI_DOUBLE)
		{
			one_double_allreduce_intra_BCAST_from_0(_GLEXCOLL._allreduce.recvbuf, _GLEXCOLL._allreduce.recvbuf, _GLEXCOLL._allreduce.count);
		}
		else
		{
			MPI_Bcast(_GLEXCOLL._allreduce.recvbuf,
					  _GLEXCOLL._allreduce.count,
					  _GLEXCOLL._allreduce.datatype,
					  0, Comm_intra);
		}
		sem_post(&(_GLEXCOLL.FinishREQ));
		//printf("rank = %d wait finished\n",_GLEXCOLL.global_rank);
	}
	return 0;
}
int GLEXCOLL_Iallreduce_int(int *sendbuf, int *recvbuf, int count, MPI_Op op)
{
	// 	glex_ret_t ret;
	// 	switch (op)
	// 	{
	// 	case MPI_SUM:
	// /* code */
	// #define GLEX_OP GLEX_COLL_REDUCE_SUM_SIGNED
	// 		break;
	// 	default:
	// 		break;
	// 	}
	// 	int i = 0;
	// 	{
	// 		int send_count = 0;
	// 		while (send_count < GLEX_COLL_REDUCE_MAX_DATA_UNITS && i < count)
	// 		{
	// 			data_loc[send_count].int32_s.data = sendbuf[i++];
	// 			data_loc[send_count++].int32_s.location = inter_rank;
	// 		}
	// 		if (inter_rank == 0)
	// 		{
	// 			//root leader
	// 			glex_coll_compose_mp_reduce_data(data_loc,
	// 											 send_count,
	// 											 _GLEXCOLL._allreduce.bit_size,
	// 											 GLEX_OP,
	// 											 1, /* for root node, must be 1 */
	// 											 GLEX_Coll_req.data,
	// 											 &(GLEX_Coll_req.len));

	// 			GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[_GLEXCOLL._allreduce.root].v;
	// 			GLEX_Coll_req.flag =  GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL |
	// 			GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_COUNTER;
	// 			GLEX_Coll_req.coll_counter = inter_procn - 1;

	// 			GLEX_Coll_req_backup = GLEX_Coll_req;
	// 			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
	// 			{
	// 			}
	// 			if (ret != GLEX_SUCCESS)
	// 			{
	// 				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
	// 				exit(1);
	// 			}

	// 			//root leader 开始广播通信请求
	// 			glex_coll_compose_mp_set_data(&(_GLEXCOLL.ep_addrs[1]), inter_procn - 1,
	// 										  GLEX_Coll_req.data, &(GLEX_Coll_req.len));
	// 			GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[_GLEXCOLL._allreduce.root].v; /* one for itself */
	// 			GLEX_Coll_req.flag =  GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE_RESULT | GLEX_FLAG_COLL_CP_DATA | GLEX_FLAG_COLL_SWAP | GLEX_FLAG_COLL_SWAP_SET;
	// 			GLEX_Coll_req.coll_counter = 1; //
	// 			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
	// 			{
	// 			}
	// 			if (ret != GLEX_SUCCESS)
	// 			{
	// 				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
	// 				exit(1);
	// 			}
	// 		}
	// 		else
	// 		{
	// 			//leaf
	// 			glex_coll_compose_mp_reduce_data(data_loc,
	// 											 send_count,
	// 											 _GLEXCOLL._allreduce.bit_size,
	// 											 GLEX_OP,
	// 											 inter_procn - 1,
	// 											 GLEX_Coll_req.data,
	// 											 &(GLEX_Coll_req.len));

	// 			GLEX_Coll_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[_GLEXCOLL._allreduce.root].v;
	// 			GLEX_Coll_req.flag =  GLEX_FLAG_FENCE | GLEX_FLAG_COLL | GLEX_FLAG_COLL_SEQ_START | GLEX_FLAG_COLL_SEQ_TAIL | GLEX_FLAG_COLL_REDUCE | GLEX_FLAG_COLL_CP_SWAP_DATA | GLEX_FLAG_COLL_COUNTER;
	// 			GLEX_Coll_req.coll_counter = 0;

	// 			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
	// 			{
	// 			}
	// 			if (ret != GLEX_SUCCESS)
	// 			{
	// 				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
	// 				exit(1);
	// 			}

	// 		}
	// 	}
}

void GLEXCOLL_Allreduce_NonOffload(void *sendbuf, int count, void *recvbuf)
{
	glex_ret_t ret;
	double *tmpbuf = (double *)malloc(sizeof(double) * count);
	double *buf = (double *)malloc(sizeof(double) * count);
	for (int i = 0; i < count; i++)
		tmpbuf[i] = ((double *)sendbuf)[i];
	if (_ReduceTreeS[_TreeID].type == LEAF)
	{
		//printf("check LEAF  %d\n",_GLEXCOLL.global_rank);
		GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
		GLEX_Coll_req.data = tmpbuf;
		GLEX_Coll_req.len = sizeof(double) * count;
		GLEX_Coll_req.flag = 0;
		GLEX_Coll_req.next = NULL;
		while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		{
		}
		if (ret != GLEX_SUCCESS)
		{
			printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}

		glex_ret_t ret;
		glex_ep_addr_t rmt_ep_addr;
		ret = glex_receive_mp(_GLEXCOLL.ep, -1, &rmt_ep_addr, buf, &GLEXCOLL_databuf_len);
		if (ret == GLEX_NO_MP)
		{
			printf("_receive_mp() (%d), return NO_MP?\n", __LINE__);
			exit(1);
		}
		else if (ret != GLEX_SUCCESS)
		{
			printf("_receive_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}
		for (int i = 0; i < count; i++)
			((double *)recvbuf)[i] = (buf)[i];
		//printf("%f \n",buf[0]);//
	}
	else if (_ReduceTreeS[_TreeID].type == MID || _ReduceTreeS[_TreeID].type == ROOT)
	{
		//printf("check MID ROOT %d\n",_GLEXCOLL.global_rank);
		//MID节点第一步是RECV
		glex_ret_t ret;
		glex_ep_addr_t rmt_ep_addr;
		for (int i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
		{
			ret = glex_receive_mp(_GLEXCOLL.ep, -1, &rmt_ep_addr, buf, &GLEXCOLL_databuf_len);
			if (ret == GLEX_NO_MP)
			{
				printf("_receive_mp() (%d), return NO_MP?\n", __LINE__);
				exit(1);
			}
			else if (ret != GLEX_SUCCESS)
			{
				printf("_receive_mp() (%d), return: %d\n", __LINE__, ret);
				exit(1);
			}
			// for (int i = 0; i < count; i++)
			// {
			// 	tmpbuf[i] += buf[i];
			// }
			//printf("tmpbuf[0]=%lf buf[0]=%lf\n",tmpbuf[0],buf[0]);
		}
		// if(_ReduceTreeS[_TreeID].type == ROOT)
		// 	printf("buf[0] = %f\n",tmpbuf[0]);
		//MID节点第二步是向上传输消息
		GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
		GLEX_Coll_req.data = tmpbuf;
		GLEX_Coll_req.len = sizeof(double) * count;
		GLEX_Coll_req.flag = 0;
		GLEX_Coll_req.next = NULL;
		while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
		{
		}
		if (ret != GLEX_SUCCESS)
		{
			printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}

		//第三步是等待来自parent传输的规约结果
		ret = glex_receive_mp(_GLEXCOLL.ep, -1, &rmt_ep_addr, buf, &GLEXCOLL_databuf_len);
		if (ret == GLEX_NO_MP)
		{
			printf("_receive_mp() (%d), return NO_MP?\n", __LINE__);
			exit(1);
		}
		else if (ret != GLEX_SUCCESS)
		{
			printf("_receive_mp() (%d), return: %d\n", __LINE__, ret);
			exit(1);
		}
		//printf("buf[0] = %f\n",buf[0]);
		//第四步是将规约结果分发给每个孩子
		for (int i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
		{
			GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].childAddrs[i].v;
			GLEX_Coll_req.data = buf;
			GLEX_Coll_req.len = sizeof(double) * count;
			GLEX_Coll_req.flag = 0;
			GLEX_Coll_req.next = NULL;
			while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
			{
			}
			if (ret != GLEX_SUCCESS)
			{
				printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
				exit(1);
			}
		}
		//第五步是将消息拷贝到recvbuf
		for (int i = 0; i < count; i++)
		{
			((double *)recvbuf)[i] = buf[i];
		}
	}
	else
	{
		puts("error node type");
		exit(0);
	}
	// MPI_Barrier(Comm_inter);
	free(tmpbuf);
	free(buf);
}
int GLEXCOLL_Iallreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op)
{
	if (_GLEXCOLL.inter_procn > 1)
	{
		//节点间规约
		//节点间小消息规约
		sem_wait(&(_GLEXCOLL.FinishREQ));
		pthread_mutex_lock(&(_GLEXCOLL.mtx));
		//puts("1 \t 将消息提交到通信线程");
		//通信请求提交临界区，_GLEXCOLL.结构体同时只能有一个进程写入，主线程是生产者，通信线程是消费者
		//printf("MPI_Type_size = %d\n",size);
		_GLEXCOLL._allreduce.sendbuf = sendbuf;
		_GLEXCOLL._allreduce.recvbuf = recvbuf;
		_GLEXCOLL._allreduce.count = count;
		_GLEXCOLL._allreduce.datatype = datatype;
		_GLEXCOLL._allreduce.op = op;
		_GLEXCOLL.th_flag = ALLREDUCE;
		pthread_mutex_unlock(&(_GLEXCOLL.mtx));
		sem_post(&(_GLEXCOLL.PostREQ));
		//puts("2 \t 计算");
	}
	else
	{
		//节点内小消息规约
		//puts("check 716");
		one_double_allreduce_intra(sendbuf, recvbuf, count);
	}
}
void *comm_main_loop(void *pvoid)
{
	//puts("thread set_affinity");
	GLEX_set_affinity(_GLEXCOLL.intra_rank);

	while (1)
	{
		//等待CPU信号。

		sem_wait(&(_GLEXCOLL.PostREQ));
		// pthread_mutex_lock(&(_GLEXCOLL.mtx));
		// //puts("2 \t 网卡收到通信请求");
		// pthread_mutex_unlock(&(_GLEXCOLL.mtx));
		// puts("2 \t 网卡开始通信");
		// fflush(stdout);
		if (_GLEXCOLL.th_flag == EXIT)
		{
			break;
		}
		else if (_GLEXCOLL.th_flag == ALLREDUCE)
		{
			int size;
			MPI_Type_size(_GLEXCOLL._allreduce.datatype, &size);
			void *tmpbuf = (void *)malloc(size * _GLEXCOLL._allreduce.count);
			if (size == 4)
			{
				_GLEXCOLL._allreduce.bit_size = GLEX_COLL_REDUCE_DATA_BITS_32;
			}
			else
			{
				_GLEXCOLL._allreduce.bit_size = GLEX_COLL_REDUCE_DATA_BITS_64;
			}
			//printf("check 754,INTRA_ALLREDUCE_TYPE = %d\n",INTRA_ALLREDUCE_TYPE);
			if (INTRA_ALLREDUCE_TYPE == 3 && _GLEXCOLL._allreduce.datatype == MPI_DOUBLE && _GLEXCOLL._allreduce.op == MPI_SUM)
			{
				one_double_allreduce_intra_REDUCE_to_0(_GLEXCOLL._allreduce.sendbuf, tmpbuf, _GLEXCOLL._allreduce.count);

				// if(intra_rank == 0)
				// {
				// 	printf("tmpbuf[0] = %f\n",((double *)tmpbuf)[0]);
				// }
			}
			else
			{
				//puts("check");
				if (MPI_Reduce(_GLEXCOLL._allreduce.sendbuf, tmpbuf, _GLEXCOLL._allreduce.count, _GLEXCOLL._allreduce.datatype, _GLEXCOLL._allreduce.op, 0, Comm_intra) != MPI_SUCCESS)
				{
					puts("MPI_Reduce error ");
					MPI_Finalize();
					exit(0);
				}
			}
			if (intra_rank == 0)
			{
				//MPI_Iallreduce(tmpbuf,recvbuf,count,datatype,op,Comm_inter,&MPI_Iallreduce_request);

				//MPI_Allreduce(tmpbuf,_GLEXCOLL._allreduce.recvbuf,_GLEXCOLL._allreduce.count,MPI_DOUBLE,MPI_SUM,Comm_inter);
				if (inter_comm_mode == 0)
				{ //使用通信卸载
					enum glex_coll_reduce_op GLEXOP;
					GLEXCOLL_Iallreduce_LoadingData(tmpbuf, _GLEXCOLL._allreduce.count,
													_GLEXCOLL._allreduce.datatype, _GLEXCOLL._allreduce.op, &GLEXOP);
					GLEXCOLL_Iallreduce_Tree(tmpbuf, _GLEXCOLL._allreduce.recvbuf,
											 _GLEXCOLL._allreduce.count, GLEXOP);
				}
				else
				{
					//不使用通信卸载
					GLEXCOLL_Allreduce_NonOffload(tmpbuf, _GLEXCOLL._allreduce.count, _GLEXCOLL._allreduce.recvbuf);
				}
			}
			//通信线程等待消息传输完成
			free(tmpbuf);
		}
		sem_post(&(_GLEXCOLL.FinishREQ));
	}
}