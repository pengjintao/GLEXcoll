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

volatile char *SHM_bufs_Recv[64];
volatile char *SHM_bufs_Send[64];
volatile char *SHM_bufs_bcast;
volatile char *SHM_bufs_bcast1;
void Intel_release(int bcastflag, void *sendbuf, void *recvbuf, int count);
void Intel_gather(int reduceflag, void *sendbuf, void *recvbuf, int count);

void one_double_allreduce_intra_l2_cache_aware_intel(void *sendbuf, void *recvbuf, int count)
{
	//第一步为gather
	static int pvt_gather_state = 0;
	static int pvt_release_state = 0;
	int block_leader = (intra_rank >> 2) << 2;
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; i++)
		re[i] = ((double *)sendbuf)[i];
	if (block_leader == intra_rank)
	{
		pvt_gather_state++;
		//第一步规约到block_leader
		//sleep(1);
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile int *p = shared_gather_flags[intra_rank + c];
			while (*p != pvt_gather_state)
				;
			//printf("*p =%d\n",*p);
			for (int i = 0; i < count; i++)
				re[i] += ((double *)(Reduce_buffers[intra_rank + c]))[i];
		}
		*(shared_gather_flags[intra_rank]) = pvt_gather_state;
		//printf("%d %f\n",intra_rank,re[0]);
		/////////////////////////////////////////////////////////////////
		// pvt_gather_state++;
		// pvt_release_state++;
		// if(intra_rank == 0)
		// {
		// 	//root
		// 	for(int c = 1;c<(intra_procn>>2);++c)
		// 	{
		// 		volatile int * p = shared_gather_flags[(c<<2)];
		// 		while (*p != pvt_gather_state);
		// 		//puts("check 296");
		// 		for(int i = 0;i<count;i++)
		// 			re[i] += ((double *)(Reduce_buffers[(c<<2)]))[i];
		// 	}
		// 	for (int i = 0; i < count; i++)
		// 		((double *)(broad_cast_buffer))[i] = re[i];
		// 	*(shared_release_flags[intra_rank]) = pvt_release_state;
		// }else{
		// 	//block leader
		// 	//向上规约
		// 	for (int i = 0; i < count; i++)
		// 	{
		// 		((double *)Reduce_buffers[intra_rank])[i] = re[i];
		// 	}
		// 	*(shared_gather_flags[intra_rank]) = pvt_gather_state;
		// 	//等待广播结果
		// 	while(*(shared_release_flags[0]) != pvt_release_state);
		// 		for(int c = 0;c<count;c++)
		// 		{
		// 			re[c] = ((double *)broad_cast_buffer)[c];
		// 		}

		// }
		// pvt_gather_state--;
		// pvt_release_state--;
		////////////////////////////////////////////////////////////////////////////////
		//将结果广播回其它进程
		pvt_release_state++;
		for (int i = 0; i < count; i++)
			((double *)(broad_cast_buffer))[i] = re[i];
		*(shared_release_flags[intra_rank]) = pvt_release_state;
	}
	else
	{
		pvt_gather_state++;
		for (int i = 0; i < count; i++)
		{
			((double *)Reduce_buffers[intra_rank])[i] = ((double *)sendbuf)[i];
		}
		*(shared_gather_flags[intra_rank]) = pvt_gather_state;

		//等待广播结果
		pvt_release_state++;
		while (*(shared_release_flags[block_leader]) != pvt_release_state)
			;
		for (int c = 0; c < count; c++)
		{
			((double *)recvbuf)[c] = ((double *)broad_cast_buffer)[c];
		}
		//printf("%d %f\n",intra_rank,((double *)recvbuf)[0]);
	}
	//exit(0);
}

void one_double_allreduce_intra_REDUCE_BCAST_Intel(void *sendbuf, void *recvbuf, int count)
{
	Intel_release(0, sendbuf, recvbuf, count);
	Intel_gather(1, sendbuf, recvbuf, count);
	Intel_release(1, recvbuf, recvbuf, count);
	Intel_gather(1, recvbuf, recvbuf, count);
	//第一步为gather
	// static int pvt_gather_state = 111;
	// static int pvt_release_state = 111;
	// double * re = (double *) recvbuf;
	// for(int i = 0;i<count;i++) re[i] = ((double *)sendbuf)[i];
	// if(intra_rank == 0)
	// {
	// 	//root
	// 	pvt_gather_state++;
	//     for(int i = 1;i<intra_procn;++i)
	//     {
	//         while(*(shared_gather_flags[i]) != pvt_gather_state);
	//             for(int c = 0;c<count;c++)
	//                 re[c] += ((double *)Reduce_buffers[i])[c];
	//     }
	// 	pvt_release_state++;
	//     for(int c = 0;c<count;c++)
	//     {
	//         ((double *)broad_cast_buffer)[c] = re[c];
	//     }
	// 	*shared_release_flags[0] = pvt_release_state;

	// }else{
	// 	//childs
	// 	pvt_gather_state++;
	//     for(int i = 0;i<count;i++)
	//         {
	//             ((double *)Reduce_buffers[intra_rank])[i] = ((double *)sendbuf)[i];
	//         }
	// 	 *(shared_gather_flags[intra_rank]) = pvt_gather_state;
	// 	pvt_release_state++;
	// 	while (*(shared_release_flags[0]) != pvt_release_state) ;
	//     for(int c = 0;c<count;c++)
	//     {
	//         re[c] = ((double *)broad_cast_buffer)[c];
	//     }
	// }
	//printf("%f \n",re[0]);
	//
}
void one_double_allreduce_intra_REDUCE_to_0(void *sendbuf, void *recvbuf, int count)
{
	// double * re = recvbuf;
	// if(_GLEXCOLL.intra_rank == 0)
	// {

	// 	volatile char *p1 = (_GLEXCOLL.SHM_0+((ppn-1)<<6));
	// 	*p1 = 'P';
	// 	for(int i = 0;i<count;++i) re[i] = ((double *)sendbuf)[i];
	// 	//sleep(1);
	// 	for(int i = 0;i<ppn -1 ;++i)
	// 	{
	// 		volatile char *p = _GLEXCOLL.SHM_buf+(i<<6);
	//     	while (*p != 'R');
	// 		// printf("check %d\n",_GLEXCOLL.intra_rank);
	// 		// fflush(stdout);
	// 			for(int i = 0;i<count;i++)
	// 				re[i] += ((volatile SHM_FLIT *)p)->payload.T.data_double[i];
	// 	}
	// }else{
	// 	volatile char *p = _GLEXCOLL.SHM_buf;
	// 	*p ='S';
	//     while (*p != 'S');
	//         //((SHM_FLIT *)p)->payload.size=1;
	// 		for(int i = 0;i<count;i++)
	//         	((volatile SHM_FLIT *)p)->payload.T.data_double[i]=((double *)sendbuf)[i];
	// 		*p = 'R';
	// }
	//puts("check 187");
	int block_leader = (intra_rank >> 2) << 2;
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; i++)
		re[i] = ((double *)sendbuf)[i];

	if (block_leader == intra_rank)
	{
		//第一步规约到block_leader
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			//printf("p = %p\n",p);
			while (*p != 'R')
				;
			//puts("check 66");
			for (int i = 0; i < count; i++)
				re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
		}
		/////////////////////////////////////////////////////////////////
		if (intra_rank == 0)
		{
			//root
			for (int c = 1; c < (intra_procn >> 2); ++c)
			{
				volatile char *p = SHM_bufs_Recv[(c << 2)];
				while (*p != 'R')
					;
				//puts("check 79");
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
			}

			//printf("%d %f \n",inter_rank, re[0]);
			// for(int c = 1;c<(intra_procn>>2);++c)
			// {
			// 	volatile char * p = SHM_bufs_Recv[(c<<2)];
			// 	for(int i = 0;i<count;i++)
			// 		((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			// 	*p = 'B';
			// }
		}
		else
		{
			//block leader
			//向上规约
			volatile char *p = SHM_bufs_Send[0];
			while (*p != 'S')
				;
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'R';
			// //等待root leader结果
			// while(*p != 'B');
			// for (int i = 0; i < count; i++)
			// 	re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
			// *p = 'S';
		}
		////////////////////////////////////////////////////////////////////////////////
		//将结果广播回其它进程
		// for(int c = 1;c<=3;++c)
		// {
		// 	//对每个孩子
		// 	volatile char *p = SHM_bufs_Recv[intra_rank + c];
		// 	for(int i = 0;i<count;i++)
		// 		((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
		// 	*p = 'B';
		// }
	}
	else
	{
		volatile char *p = SHM_bufs_Send[block_leader];
		while (*p != 'S')
			;
		//puts("check 119");
		for (int i = 0; i < count; i++)
			((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
		*p = 'R';
		//等待广播结果
		// while(*p != 'B');
		// for (int i = 0; i < count; i++)
		// 	re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
		// *p = 'S';
	}
}
void one_double_allreduce_intra_BCAST_from_0(void *sendbuf, void *recvbuf, int count)
{
	// if(inter_rank == 0 && intra_rank == 0)
	// 	puts("check bcast");
	// double * re = sendbuf;
	// //puts("check");
	// if (_GLEXCOLL.intra_rank == 0)
	// {
	// 	for (int target = 0; target < ppn - 1; ++target)
	// 	{
	// 		volatile char *p = (_GLEXCOLL.SHM_buf + (target << 6));
	// 		for (int i = 0; i < count; i++)
	// 			((volatile SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
	// 		*p = 'B';
	// 	}
	// }
	// else
	// {
	// 	volatile char *p = _GLEXCOLL.SHM_buf;
	// 	while (*p != 'B')
	// 		;
	// 	for (int i = 0; i < count; i++)
	// 		((double *)recvbuf)[i] = ((volatile SHM_FLIT *)(p))->payload.T.data_double[i];
	// 	*p = 'S';
	// }
	int block_leader = (intra_rank >> 2) << 2;
	double *re = (double *)recvbuf;
	if (block_leader == intra_rank)
	{
		//第一步规约到block_leader
		/////////////////////////////////////////////////////////////////
		if (intra_rank == 0)
		{
			//root
			// //printf("%f \n",re[0]);
			//printf("%d \n",_GLEXCOLL.global_rank);
			for (int c = 1; c < (intra_procn >> 2); ++c)
			{
				volatile char *p = SHM_bufs_Recv[(c << 2)];
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p)->payload.T.data_double[i] = ((double *)sendbuf)[i];
				*p = 'B';
			}
		}
		else
		{
			//block leader
			volatile char *p = SHM_bufs_Send[0];
			//等待root leader结果
			while (*p != 'B')
				;
			for (int i = 0; i < count; i++)
				re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
			*p = 'S';
		}
		////////////////////////////////////////////////////////////////////////////////
		//将结果广播回其它进程
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'B';
		}
	}
	else
	{
		volatile char *p = SHM_bufs_Send[block_leader];
		//等待广播结果
		while (*p != 'B')
			;
		for (int i = 0; i < count; i++)
			re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
		*p = 'S';
	}
}

void one_double_allreduce_intra_REDUCE_BCAST_QUEUE_sim(void *sendbuf, void *recvbuf, int count)
{
	double *re = recvbuf;
	if (intra_rank == 0)
	{ //root
		for (int i = 0; i < count; ++i)
			re[i] = ((double *)sendbuf)[i];
		//sleep(1);
		for (int i = 1; i < intra_procn; ++i)
		{
			volatile char *p = SHM_bufs_Recv[i];
			volatile char *p1 = SHM_bufs_Send[i];
			while (*p != 'R')
				;
			__sync_synchronize();
			// printf("check *p =%c\n",*p);
			// fflush(stdout);
			for (int i = 0; i < count; i++)
				re[i] += ((SHM_FLIT *)p1)->payload.T.data_double[i];
			__sync_synchronize();
			*p = 'S';
		}
		//puts("check 128");
		for (int target = 1; target < intra_procn; ++target)
		{
			volatile char *p = SHM_bufs_Recv[target];
			volatile char *p1 = SHM_bufs_Send[target];
			while (*p != 'S')
				;
			__sync_synchronize();
			for (int i = 0; i < count; i++)
				((volatile SHM_FLIT *)p1)->payload.T.data_double[i] = re[i];
			__sync_synchronize();
			*p = 'B';
		}
	}
	else
	{
		volatile char *p = SHM_bufs_Send[0];
		volatile char *p1 = SHM_bufs_Recv[0];
		while (*p != 'S')
			;
		__sync_synchronize();
		((volatile SHM_FLIT *)p1)->payload.size = count;
		for (int i = 0; i < count; i++)
			((volatile SHM_FLIT *)p1)->payload.T.data_double[i] = ((double *)sendbuf)[i];
		__sync_synchronize();
		*p = 'R';

		while (*p != 'B')
			;
		__sync_synchronize();
		//puts("check 145");
		for (int i = 0; i < count; i++)
			((double *)recvbuf)[i] = ((volatile SHM_FLIT *)(p1))->payload.T.data_double[i];
		__sync_synchronize();
		*p = 'S';
	}
}
void one_double_allreduce_intra_REDUCE_BCAST(void *sendbuf, void *recvbuf, int count)
{

	double *re = recvbuf;
	if (intra_rank == 0)
	{ //root
		for (int i = 0; i < count; ++i)
			re[i] = ((double *)sendbuf)[i];
		//sleep(1);
		for (int i = 0; i < intra_procn - 1; ++i)
		{
			volatile char *p = SHM_bufs_Recv[i + 1];
			while (*p != 'R')
				;
			// printf("check *p =%c\n",*p);
			// fflush(stdout);
			for (int i = 0; i < count; i++)
				re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
		}
		//puts("check 128");
		for (int target = 0; target < intra_procn - 1; ++target)
		{
			volatile char *p = SHM_bufs_Recv[target + 1];
			//while(*p != 'S');
			for (int i = 0; i < count; i++)
				((volatile SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'B';
		}
	}
	else
	{
		volatile char *p = SHM_bufs_Send[0];
		while (*p != 'S')
			;
		((volatile SHM_FLIT *)p)->payload.size = count;
		for (int i = 0; i < count; i++)
			((volatile SHM_FLIT *)p)->payload.T.data_double[i] = ((double *)sendbuf)[i];
		*p = 'R';
		while (*p != 'B')
			;
		//puts("check 145");
		for (int i = 0; i < count; i++)
			((double *)recvbuf)[i] = ((volatile SHM_FLIT *)(p))->payload.T.data_double[i];
		*p = 'S';
	}

	// if(intra_rank == 0)
	// 	puts("check");
}
void SHM_Barrier(volatile int *p)
{
	//
	// while(*p !=intra_procn);
	// __sync_sub_and_fetch(p,1);
	// while(*p != 0);
	if (intra_rank != 0)
	{
		__sync_add_and_fetch(p, 1);
		//printf("*p = %d intra_procn - 1 =%d\n",*p,intra_procn - 1);
		while (*p != 0)
			;
	}
	else
	{
		while (__sync_bool_compare_and_swap(p, intra_procn - 1, 0))
		{
		}
		//printf("*p = %d\n",*p);
		//puts("release");
	}
}

void one_double_allreduce_intra_recursive_doubling(void *sendbuf, void *recvbuf, int count)
{
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; ++i)
		re[i] = ((double *)sendbuf)[i];
	int step = 1;
	while (step < intra_procn)
	{
		int t = intra_rank / step;
		int target;
		if (t % 2 == 0)
		{
			target = intra_rank + step;
			//left进程
			//printf("step = %d left %d -> right = %d\n",step,intra_rank,target);
		}
		else
		{
			//right进程
			target = intra_rank - step;
			//printf("step = %d right %d->left = %d\n",step,intra_rank,target);
		}
		//先把数据写到target
		volatile char *p = SHM_bufs_Send[target]; //_GLEXCOLL.SHM_bufs[target];
		//p = p+(intra_rank<<6);
		while (*p != 'S')
			;
		for (int i = 0; i < count; i++)
			((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
		*p = 'R';
		//再等待target把数据传输来

		// p = _GLEXCOLL.SHM_bufs[intra_rank];
		// p = p+(target<<6);
		p = SHM_bufs_Recv[target];
		while (*p != 'R')
			;
		for (int i = 0; i < count; i++)
			re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
		*p = 'S';

		step = step << 1;
	}
	// 	printf("%f\n",((double *)recvbuf)[0]);
}
void one_double_allreduce_intra_l2_cache_performance_aware(void *sendbuf, void *recvbuf, int count)
{ //第一步：进行同L2cache的进程规约
	static int latency_vec[] = {20, 16, 24, 28, 8, 12, 4};

	int block_leader = (intra_rank >> 2) << 2;
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; i++)
		re[i] = ((double *)sendbuf)[i];
	if (block_leader == intra_rank)
	{
		//第一步规约到block_leader
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			while (*p != 'R')
				;
			//puts("check 283");
			for (int i = 0; i < count; i++)
				re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
		}
		//printf("%d %f\n",intra_rank,re[0]);
		/////////////////////////////////////////////////////////////////
		if (intra_rank == 16)
		{
			//root
			//for (int c = 1; c < (intra_procn >> 2); ++c)
			//for (int c = 6; c >=0; --c)
			{
				volatile char *p1 = SHM_bufs_Recv[0];
				volatile char *p2 = SHM_bufs_Recv[4];
				volatile char *p3 = SHM_bufs_Recv[8];
				volatile char *p4 = SHM_bufs_Recv[12];
				volatile char *p5 = SHM_bufs_Recv[20];
				volatile char *p6 = SHM_bufs_Recv[24];
				volatile char *p7 = SHM_bufs_Recv[28];
				while (*p1 != 'R')
					;
				while (*p2 != 'R')
					;
				while (*p3 != 'R')
					;
				while (*p4 != 'R')
					;
				while (*p5 != 'R')
					;
				while (*p6 != 'R')
					;
				while (*p7 != 'R')
					;
				// while (*p7 != 'R');
				// while (*p6 != 'R');
				// while (*p5 != 'R');
				// while (*p4 != 'R');
				// while (*p3 != 'R');
				// while (*p2 != 'R');
				// while (*p1 != 'R');
				//puts("check 296");
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p1)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p2)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p3)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p4)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p5)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p6)->payload.T.data_double[i];
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p7)->payload.T.data_double[i];
				//开始广播
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p7)->payload.T.data_double[i] = re[i];
				*p7 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p6)->payload.T.data_double[i] = re[i];
				*p6 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p5)->payload.T.data_double[i] = re[i];
				*p5 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p4)->payload.T.data_double[i] = re[i];
				*p4 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p3)->payload.T.data_double[i] = re[i];
				*p3 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p2)->payload.T.data_double[i] = re[i];
				*p2 = 'B';
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p1)->payload.T.data_double[i] = re[i];
				*p1 = 'B';
			}
		}
		else
		{
			//block leader
			//向上规约
			volatile char *p = SHM_bufs_Send[16];
			while (*p != 'S')
				;
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'R';
			//等待root leader结果
			while (*p != 'B')
				;
			for (int i = 0; i < count; i++)
				re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
			*p = 'S';
		}
		////////////////////////////////////////////////////////////////////////////////
		//将结果广播回其它进程
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'B';
		}
	}
	else
	{
		volatile char *p = SHM_bufs_Send[block_leader];
		while (*p != 'S')
			;
		for (int i = 0; i < count; i++)
			((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
		*p = 'R';
		//等待广播结果
		while (*p != 'B')
			;
		for (int i = 0; i < count; i++)
			re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
		*p = 'S';
	}
}
void one_double_allreduce_intra_l2_cache_aware(void *sendbuf, void *recvbuf, int count)
{
	//第一步：进行同L2cache的进程规约
	int block_leader = (intra_rank >> 2) << 2;
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; i++)
		re[i] = ((double *)sendbuf)[i];
	if (block_leader == intra_rank)
	{
		//第一步规约到block_leader
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			while (*p != 'R')
				;
			//puts("check 283");
			for (int i = 0; i < count; i++)
				re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
		}
		//printf("%d %f\n",intra_rank,re[0]);
		/////////////////////////////////////////////////////////////////
		if (intra_rank == 0)
		{
			//root
			for (int c = 1; c < (intra_procn >> 2); ++c)
			{
				volatile char *p = SHM_bufs_Recv[(c << 2)];
				while (*p != 'R')
					;
				//puts("check 296");
				for (int i = 0; i < count; i++)
					re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
			}
			//printf("%f \n",re[0]);
			for (int c = 1; c < (intra_procn >> 2); ++c)
			{
				volatile char *p = SHM_bufs_Recv[(c << 2)];
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
				*p = 'B';
			}
		}
		else
		{
			//block leader
			//向上规约
			volatile char *p = SHM_bufs_Send[0];
			while (*p != 'S')
				;
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'R';
			//等待root leader结果
			while (*p != 'B')
				;
			for (int i = 0; i < count; i++)
				re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
			*p = 'S';
		}
		////////////////////////////////////////////////////////////////////////////////
		//将结果广播回其它进程
		for (int c = 1; c <= 3; ++c)
		{
			//对每个孩子
			volatile char *p = SHM_bufs_Recv[intra_rank + c];
			for (int i = 0; i < count; i++)
				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
			*p = 'B';
		}
	}
	else
	{
		volatile char *p = SHM_bufs_Send[block_leader];
		while (*p != 'S')
			;
		for (int i = 0; i < count; i++)
			((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
		*p = 'R';
		//等待广播结果
		while (*p != 'B')
			;
		for (int i = 0; i < count; i++)
			re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
		*p = 'S';
	}

	// if(intra_rank == 31)
	// 	printf("%f\n",re[0]/32.0);

	// if(intra_rank == 31)
	// 	printf("%f\n",re[0]/32.0);
	// if(block_leader == intra_rank)
	// {
	// 	//block leader
	// 	//printf("block leader %d \n",intra_rank);
	// 	for(int i = 0;i<count;i++) re[i] = ((double *)sendbuf)[i];

	// 	volatile char *p = (char *)_GLEXCOLL.SHM_bufs[intra_rank];
	// 	p+= (intra_rank+1)<<6;
	// 	//对每一个孩子
	// 	for(int c = 0;c<3;c++)
	// 	{
	// 		while(*p != 'R');
	// 		for(int i = 0;i<count;i++)
	// 			re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
	// 		*p = 'S';
	// 		p+=64;
	// 	}

	// 	if(intra_rank == 0)
	// 	{
	// 		//root
	// 		p =  (char *)_GLEXCOLL.SHM_bufs[0];
	// 		for(int i = 4;i<intra_procn - 1;i+=4)
	// 		{
	// 			volatile char *p1 = p + (i<<6);
	// 			while (*p1 != 'R');
	// 			for(int i = 0;i<count;i++)
	// 				re[i] += ((SHM_FLIT *)p1)->payload.T.data_double[i];
	// 			*p1 = 'S';
	// 		}
	// 		//printf("%f \n",re[0]);

	// 		//接下来root开始广播
	// 		for(int target = 1;target<intra_procn ;++target)
	// 		{
	// 			volatile char *p = _GLEXCOLL.SHM_bufs[target];
	// 			for(int i = 0;i<count;i++)
	// 				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
	// 			*p = 'B';
	// 		}
	// 	}else{
	// 		//block leader
	// 		//将块写到root上去
	// 		p = (char *)_GLEXCOLL.SHM_bufs[0];
	// 		p+= (intra_rank<<6);
	// 		while (*p!='S');
	// 		for(int i = 0;i<count;i++)
	// 			((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
	// 		*p = 'R';
	// 		//等待root广播结果
	// 		p = (char *)_GLEXCOLL.SHM_bufs[intra_rank];
	// 		while(*p != 'B');
	// 			for(int i = 0;i<count;i++)
	// 				re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
	// 		*p = 'S';
	// 	}
	// }else{
	// 	//block leaf
	// 	//将块写到block leader上去。
	// 	volatile char * p =(volatile char *) _GLEXCOLL.SHM_bufs[block_leader];
	// 	p += (intra_rank<<6);
	// 	while(*p != 'S');
	// 	for(int i = 0;i<count;i++)
	// 		((SHM_FLIT *)p)->payload.T.data_double[i] = ((double *)sendbuf)[i];
	// 	*p = 'R';

	// 	//等待root广播结果
	// 	p = (char *)_GLEXCOLL.SHM_bufs[intra_rank];
	// 	while (*p != 'B');
	// 	for (int i = 0; i < count; i++)
	// 		re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
	// 	*p = 'S';
	// }
}
void one_double_allreduce_intra_2_NOMINAL(void *sendbuf, void *recvbuf, int count)
{
	int n = intra_procn;
	double *re = (double *)recvbuf;
	for (int i = 0; i < count; ++i)
		re[i] = ((double *)sendbuf)[i];
	int step = 1;
	//规约过程

	while (step <= n - 1)
	{
		if (intra_rank % step == 0)
		{
			int p = intra_rank / step;
			if (p % 2 == 0)
			{
				int source = intra_rank + step;
				if (source < intra_procn)
				{
					//接收进程
					//printf("step = %d intra_rank  = %d recv\n",step,intra_rank);
					// volatile char *p=(volatile char *)_GLEXCOLL.SHM_bufs[intra_rank];
					// p+=(source<<6);
					volatile char *p = SHM_bufs_Recv[source];
					while (*p != 'R')
						;
					for (int i = 0; i < count; i++)
						re[i] += ((SHM_FLIT *)p)->payload.T.data_double[i];
				}
			}
			else
			{
				//发送进程
				int target = intra_rank - step;
				//printf("step = %d intra_rank  = %d Send\n",step,intra_rank);
				// volatile char *p=(volatile char *)_GLEXCOLL.SHM_bufs[target];
				// p += (intra_rank<<6);
				volatile char *p = SHM_bufs_Send[target];
				while (*p != 'S')
					;
				for (int i = 0; i < count; i++)
					((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
				*p = 'R';
			}
		}
		step = (step << 1);
	}
	//printf("%d %f\n",intra_rank,re[0]);
	//接下来是广播过程。
	step = (step >> 1);
	while (step > 0)
	{
		if (intra_rank % step == 0)
		{
			int p = intra_rank / step;
			if (p % 2 == 0)
			{
				int target = intra_rank + step;
				if (target < intra_procn)
				{
					//发送进程
					//printf("step = %d intra_rank  = %d recv\n",step,intra_rank);
					// volatile char *p=(volatile char *)_GLEXCOLL.SHM_bufs[intra_rank];
					// p+=(source<<6);
					volatile char *p = SHM_bufs_Recv[target];
					for (int i = 0; i < count; i++)
						((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
					*p = 'B';
				}
			}
			else
			{
				//接收进程
				int source = intra_rank - step;
				//printf("step = %d intra_rank  = %d Send\n",step,intra_rank);
				// volatile char *p=(volatile char *)_GLEXCOLL.SHM_bufs[target];
				// p += (intra_rank<<6);
				volatile char *p = SHM_bufs_Send[source];
				while (*p != 'B')
					;
				for (int i = 0; i < count; i++)
					re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
				*p = 'S';
			}
		}
		step = (step >> 1);
	}

	// MPI_Barrier(Comm_intra);
	// if(intra_rank == 0){
	// 		for(int target = 1;target<intra_procn ;++target)
	// 		{
	// 			volatile char *p = _GLEXCOLL.SHM_bufs[target];
	// 			for(int i = 0;i<count;i++)
	// 				((SHM_FLIT *)p)->payload.T.data_double[i] = re[i];
	// 			*p = 'B';
	// 		}
	//  }else{
	// 	volatile char *p = (char *)_GLEXCOLL.SHM_bufs[intra_rank];
	// 	while (*p != 'B');
	// 	for (int i = 0; i < count; i++)
	// 		re[i] = ((SHM_FLIT *)p)->payload.T.data_double[i];
	// 	*p = 'S';
	//  }
	// if(intra_rank == 23)
	// 	printf("%f\n",re[0]);
}
void one_double_allreduce_intra(void *sendbuf, void *recvbuf, int count)
{
	switch (INTRA_ALLREDUCE_TYPE)
	{
	case 0:
		/* REDUCE_BCAST */
		one_double_allreduce_intra_REDUCE_BCAST(sendbuf, recvbuf, count);
		break;
	case 1:
		/*2_NOMINAL*/
		one_double_allreduce_intra_2_NOMINAL(sendbuf, recvbuf, count);
		break;
	case 2:
		/*Recurseive doubling*/
		one_double_allreduce_intra_recursive_doubling(sendbuf, recvbuf, count);
		break;
	case 3:
		/*Recurseive doubling*/
		one_double_allreduce_intra_l2_cache_aware(sendbuf, recvbuf, count);
		break;
	case 4:
		/*Intel REDUCE_BCAST reorder*/
		one_double_allreduce_intra_REDUCE_BCAST_Intel(sendbuf, recvbuf, count);
		break;
	case 5:
		/**/
		one_double_allreduce_intra_REDUCE_BCAST_QUEUE_sim(sendbuf, recvbuf, count);
		break;
	case 6:
		/**/
		one_double_allreduce_intra_l2_cache_performance_aware(sendbuf, recvbuf, count);
		break;

	default:
		break;
	}
}