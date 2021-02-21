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
#include <omp.h>
#include <sys/mman.h>
#include <pthread.h>
#include "glex.h"
#include "glexcoll.h"
#include "glexallreduce.h"

static int allreduce_rank;
static int allreduce_procn;
static MPI_Comm allreduce_comm;
int allreduce_ongoin_msgN = 1;
int allreduce_slice_num = (1 << 10);

int SoftWare_Allreduce_Algorithm_Power_of_2 = Recursize_doubling_OMP;

#define ALLREDUCE_TAG 996
void recursive_doubling_double_sum(double *sendbuf, double *recvbuf, int num)
{
    //puts("check pjt");
    int size = num * sizeof(double);
    static MPI_Request req_send;
    static MPI_Request req_recv;
    static MPI_Status status;
    double *tmpbuf = (double *)allreduce_sendbuf;
    memcpy(recvbuf, sendbuf, size);

    int stepN = allreduce_recursive_doubling_stepn;
    int step = 1;
    // int step = allreduce_procn;
    // while (step > 1)
    // {
    //     stepN++;
    //     step = (step + 1) / 2;
    // }
    // if (allreduce_rank == 0)
    //     printf("step=%d\n", stepN);
    // exit(0);
    for (int i = 0; i < stepN; i++)
    {
        int target = (allreduce_rank ^ step);
        if (target < allreduce_procn)
        {
            MPI_Isend(recvbuf, size, MPI_CHAR, target, ALLREDUCE_TAG, allreduce_comm, &req_send);
            MPI_Irecv(tmpbuf, size, MPI_CHAR, target, ALLREDUCE_TAG, allreduce_comm, &req_recv);
            MPI_Wait(&req_recv, &status);
            MPI_Wait(&req_send, &status);
#pragma omp parallel for
            for (int j = 0; j < num; j++)
            {
                recvbuf[j] += tmpbuf[j];
            }
        }
        step *= 2;
    }
}
extern int mmin(int a, int b);
void recursive_doubling_double_slicing_sum(double *sendbuf, double *recvbuf, int num)
{
    //     //puts("check pjt");
    //     int size = num*sizeof(double);
    //     static MPI_Request req_send[32];
    //     static MPI_Request req_recv[32];
    //     static MPI_Status  status[32];
    //     double * tmpbuf = (double *)malloc(size);
    //     memcpy(recvbuf,sendbuf,size);

    //     int stepN=0;
    //     int step=allreduce_procn;
    //     while (step > 1){
    //         stepN++;
    //         step/=2;
    //     }
    //     for(int i = 0;i<stepN;i++)
    //     {
    //         int BigBlockNum = allreduce_slice_num*allreduce_ongoin_msgN;
    //         int target = (allreduce_rank ^ step);
    //         for(int startshift = 0;startshift < num;startshift+=BigBlockNum)
    //         {
    //             extern int mmin(int a,int b);
    //             int shift_end=mmin(num,startshift+BigBlockNum);
    //             int s=0,shift=startshift;
    //             while (shift < shift_end)
    //             {
    //                 int local_num = mmin(allreduce_slice_num,shift_end - shift);
    //                 MPI_Isend(recvbuf+shift,local_num,MPI_DOUBLE,target,s,allreduce_comm,&(req_send[s]));
    //                 MPI_Irecv(tmpbuf+shift,local_num,MPI_DOUBLE,target,s,allreduce_comm,&(req_recv[s]));
    //                 /* code */
    //                 shift+=allreduce_slice_num;
    //                 s++;
    //             }
    //             MPI_Waitall(s,req_send,status);
    //             MPI_Waitall(s,req_recv,status);
    //         }
    //         //int count_tmp = mmin(num,BigBlockNum);
    //         //接下来进行第一波消息发送
    // #pragma omp parallel for
    //         for(int j = 0;j<num;j++)
    //         {
    //             recvbuf[j]+=tmpbuf[j];
    //         }
    //         step*=2;
    //     }
    //     free(tmpbuf);

    //puts("check pjt");
    int size = num * sizeof(double);
    static MPI_Request req_send;
    static MPI_Request req_recv;
    static MPI_Status status;
    double *tmpbuf = (double *)malloc(size);
    memcpy(recvbuf, sendbuf, size);

    int stepN = 0;
    int step = allreduce_procn;
    while (step > 1)
    {
        stepN++;
        step /= 2;
    }
    //printf("%d\n",stepN);
    for (int i = 0; i < stepN; i++)
    {
        int target = (allreduce_rank ^ step);
        //printf("%d %d\n",allreduce_rank,target);
        int last_start = 0, start = 0;
        int local_size = mmin(num, allreduce_slice_num);
        int last_local_size = local_size;
        MPI_Isend(recvbuf, local_size, MPI_DOUBLE, target, start, allreduce_comm, &req_send);
        MPI_Irecv(tmpbuf, local_size, MPI_DOUBLE, target, start, allreduce_comm, &req_recv);
        //for(int start=0;start<num;start+=allreduce_slice_num)
        start += local_size;
        while (start < num)
        {
            //puts("x");
            MPI_Wait(&req_recv, &status);
            MPI_Wait(&req_send, &status);

            last_local_size = local_size;
            local_size = mmin(num - start, allreduce_slice_num);
            MPI_Isend(&(recvbuf[start]), local_size, MPI_DOUBLE, target, start, allreduce_comm, &req_send);
            MPI_Irecv(&(tmpbuf[start]), local_size, MPI_DOUBLE, target, start, allreduce_comm, &req_recv);
            //开始上一轮完成传输的数据的规约
            {
#pragma omp parallel for
                for (int j = 0; j < last_local_size; j++)
                {
                    recvbuf[last_start + j] += tmpbuf[last_start + j];
                }
            }
            last_start = start;
            start += local_size;
        }
        MPI_Wait(&req_recv, &status);
        MPI_Wait(&req_send, &status);
        //开始上一轮完成传输的数据的规约
        {
#pragma omp parallel for
            for (int j = 0; j < last_local_size; j++)
            {
                recvbuf[last_start + j] += tmpbuf[last_start + j];
            }
        }

        //         MPI_Isend(recvbuf,size,MPI_CHAR,target,ALLREDUCE_TAG,allreduce_comm,&req_send);
        //         MPI_Irecv(tmpbuf,size,MPI_CHAR,target,ALLREDUCE_TAG,allreduce_comm,&req_recv);
        //         MPI_Wait(&req_recv,&status);
        //         MPI_Wait(&req_send,&status);
        // #pragma omp parallel for
        //         for(int j = 0;j<num;j++)
        //         {
        //             recvbuf[j]+=tmpbuf[j];
        //         }

        step *= 2;
    }
    free(tmpbuf);
}

extern int K_nominal_tree_stepN(int procn, int k);
extern int K_nominal_tree_my_stepN(int rank, int procn, int k);
extern int K_nominal_tree_parent(int rank, int k, int step);
extern void K_nominal_tree_child_vec(int rank, int procn, int step, int *Childvec, int *childn, int k);

void GLEXCOLL_Allreduce_K_ary(void *sendbuf, void *recvbuf,int count)
{
    //puts("x");
    static struct glex_rdma_req rdma_req;
    static struct glex_rdma_req *bad_rdma_req;
    static glex_event_t *event;
    glex_ret_t ret;
    //第一步奖消息拷贝到allreduce缓冲区
    memcpy(allreduce_sendbuf,sendbuf,count*sizeof(double));
    //allreduce_send_recv_pair
    
    if (_ReduceTreeS[_TreeID].type == LEAF)
    {
        //printf("check LEAF  %d\n",_GLEXCOLL.global_rank);
            int target = _ReduceTreeS[_TreeID].parentID;
            rdma_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
            rdma_req.local_mh.v = send_mhs[allreduce_send_recv_pair][allreduce_rank].v;
            rdma_req.local_offset = 0;
            rdma_req.len = count*sizeof(double);
            rdma_req.rmt_mh.v = recv_mhs[allreduce_send_recv_pair][target].v;
            rdma_req.rmt_offset = (count*sizeof(double))*(_ReduceTreeS[_TreeID].DownReduceRank - 1);
            //printf("_ReduceTreeS[_TreeID].DownReduceRank - 1 =%d\n",_ReduceTreeS[_TreeID].DownReduceRank - 1);
            rdma_req.type = GLEX_RDMA_TYPE_PUT;
            rdma_req.rmt_evt.cookie_0 = 0x9696969696969696ULL;
            rdma_req.rmt_evt.cookie_1 = 0x9696969696969696ULL;
            // rdma_req.local_evt.cookie_0 = 998;
            // rdma_req.local_evt.cookie_1 = 997;
            rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
            rdma_req.flag = GLEX_FLAG_REMOTE_EVT ;//| GLEX_FLAG_LOCAL_EVT;
            rdma_req.next = NULL;
            int ret;
            while ((ret = glex_rdma(_GLEXCOLL.ep, &rdma_req, &bad_rdma_req)) == GLEX_BUSY)
            {
            }
            if (ret != GLEX_SUCCESS)
            {
                if (ret == GLEX_INVALID_PARAM)
                    printf("%d, _rdma() 非法参数", global_rank);
                printf("_rdma(), return: %d\n", ret);
                exit(1);
            }
        //等待从父节点发送过来的消息

            // while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
            //     ;
            // if (event->cookie_1 != 997)
            // {
            //     printf("probed a new event, but cookie_1 is invalid: %#llx\n",
            //            (long long)event->cookie_1);
            // }
            // _GLEXCOLL.event_credit -= 1; //= Event_CREDIT_MAX;
            // if (_GLEXCOLL.event_credit <= 0)
            // {
            //     glex_discard_probed_event(_GLEXCOLL.ep);
            //     _GLEXCOLL.event_credit = Event_CREDIT_MAX;
            // }
        //将消息拷贝回用户缓冲区
        memcpy(recvbuf,allreduce_sendbuf,count*sizeof(double));
        // for(int i = 0;i<count;i++)
        // {
        //     printf("%lf ",((double *)allreduce_sendbuf)[i]);
        // }
        // puts("");
        // glex_ret_t ret;
        // glex_ep_addr_t rmt_ep_addr;
        // //puts("start wait");
        // while ((ret = glex_probe_next_mp(_GLEXCOLL.ep, &rmt_ep_addr, (void **)&(r_data), &tmp_len)) == GLEX_NO_MP)
        // {
        // }
        // for (int i = 0; i < count; i++)
        //     ((double *)recvbuf)[i] = (r_data)[i];
        // _GLEXCOLL.ep_credit -= 1;
        // if (_GLEXCOLL.ep_credit <= 0)
        // {
        //     glex_discard_probed_mp(_GLEXCOLL.ep);
        //     _GLEXCOLL.ep_credit = EP_CREDIT_MAX;
        // }

        //printf("%f \n",buf[0]);//
    }
    else if(_ReduceTreeS[_TreeID].type == MID)
    {
        {
            //接收来自children发送的消息并奖它们求和
            glex_ret_t ret;
            glex_ep_addr_t rmt_ep_addr;
            for (int i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
            {

                while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                    ;
                if (event->cookie_1 != 0x9696969696969696ULL)
                {
                    printf("probed a new event, but cookie_1 is invalid: %#llx\n",
                           (long long)event->cookie_1);
                }
                _GLEXCOLL.event_credit -= 1; //= Event_CREDIT_MAX;
                if (_GLEXCOLL.event_credit <= 0)
                {
                    glex_discard_probed_event(_GLEXCOLL.ep);
                    _GLEXCOLL.event_credit = Event_CREDIT_MAX;
                }
                // puts("check recv");
                //printf("tmpbuf[0]=%lf buf[0]=%lf\n",tmpbuf[0],buf[0]);
            }
            for (int j = 0; j < _ReduceTreeS[_TreeID].childsN; ++j)
            {
                double *bufadd = (double *)(allreduce_recvbuf + count * j * sizeof(double));
                // printf("*bufadd=%lf\n",*bufadd);
                for (int i = 0; i < count; i++)
                {
                    ((double *)allreduce_sendbuf)[i] += bufadd[i];
                }
            }
            //printf("my rank = %d, re = %lf\n",allreduce_rank, ((double *)allreduce_sendbuf)[0]);
        }
        {
        //将收来的消息发送给父节点
            int target = _ReduceTreeS[_TreeID].parentID;
            rdma_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
            rdma_req.local_mh.v = send_mhs[allreduce_send_recv_pair][allreduce_rank].v;
            rdma_req.local_offset = 0;
            rdma_req.len = count * sizeof(double);
            rdma_req.rmt_mh.v = recv_mhs[allreduce_send_recv_pair][target].v;
            rdma_req.rmt_offset = (count * sizeof(double)) * (_ReduceTreeS[_TreeID].DownReduceRank - 1);
            //printf("_ReduceTreeS[_TreeID].DownReduceRank - 1 =%d\n",_ReduceTreeS[_TreeID].DownReduceRank - 1);
            rdma_req.type = GLEX_RDMA_TYPE_PUT;
            rdma_req.rmt_evt.cookie_0 = 0x9696969696969696ULL;
            rdma_req.rmt_evt.cookie_1 = 0x9696969696969696ULL;
            // rdma_req.local_evt.cookie_0 = 998;
            // rdma_req.local_evt.cookie_1 = 997;
            rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
            rdma_req.flag = GLEX_FLAG_REMOTE_EVT; //| GLEX_FLAG_LOCAL_EVT;
            rdma_req.next = NULL;
            int ret;
            while ((ret = glex_rdma(_GLEXCOLL.ep, &rdma_req, &bad_rdma_req)) == GLEX_BUSY)
            {
            }
            if (ret != GLEX_SUCCESS)
            {
                if (ret == GLEX_INVALID_PARAM)
                    printf("%d, _rdma() 非法参数", global_rank);
                printf("_rdma(), return: %d\n", ret);
                exit(1);
            }
        }
    }else 
    {
        //if( _ReduceTreeS[_TreeID].type == ROOT)
        //printf("check MID ROOT %d\n",_GLEXCOLL.global_rank);
        //MID节点第一步是RECV
        glex_ret_t ret;
        glex_ep_addr_t rmt_ep_addr;
        for (int i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
        {
            while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                ;
            if (event->cookie_1 != 0x9696969696969696ULL)
            {
                printf("probed a new event, but cookie_1 is invalid: %#llx\n",
                       (long long)event->cookie_1);
            }
            _GLEXCOLL.event_credit -= 1; //= Event_CREDIT_MAX;
            if (_GLEXCOLL.event_credit <= 0)
            {
                glex_discard_probed_event(_GLEXCOLL.ep);
                _GLEXCOLL.event_credit = Event_CREDIT_MAX;
            }
            // puts("check recv");
            //printf("tmpbuf[0]=%lf buf[0]=%lf\n",tmpbuf[0],buf[0]);
        }
        for (int j = 0; j < _ReduceTreeS[_TreeID].childsN; ++j)
        {
            double *bufadd = (double *)(allreduce_recvbuf + count*j*sizeof(double));
            //printf("*bufadd=%lf\n",*bufadd);
            for (int i = 0; i < count; i++)
            {
                ((double *)allreduce_sendbuf)[i] += bufadd[i];
            }
        }
        // for(int i = 0;i<count;i++)
        // {
        //     printf("%lf ",((double *)allreduce_sendbuf)[i]);
        // }
        // puts("");
        // exit(0);
        //root要将sendbuf广播给它的孩子

        // // if(_ReduceTreeS[_TreeID].type == ROOT)
        // // 	printf("buf[0] = %f\n",tmpbuf[0]);
        // //MID节点第二步是向上传输消息
        // GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].parentAddr.v;
        // GLEX_Coll_req.data = recvbuf;
        // GLEX_Coll_req.len = sizeof(double) * count;
        // GLEX_Coll_req.flag = 0;
        // GLEX_Coll_req.next = NULL;
        // while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
        // {
        // }
        // if (ret != GLEX_SUCCESS)
        // {
        //     printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
        //     exit(1);
        // }

        // //第三步是等待来自parent传输的规约结果

        // while ((ret = glex_probe_next_mp(_GLEXCOLL.ep, &rmt_ep_addr, (void **)&(r_data), &tmp_len)) == GLEX_NO_MP)
        // {
        // }
        // _GLEXCOLL.ep_credit -= 1;
        // //printf("buf[0] = %f\n",buf[0]);
        //将规约结果分发给每个孩子,
        //事实说明广播的度不应该这么高。
            // for (int i = 0; i < _ReduceTreeS[_TreeID].childsN; i++)
            // {
            //     int target = _ReduceTreeS[_TreeID].childIds[i];
            //     rdma_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].childAddrs[i].v;
            //     rdma_req.local_mh.v = send_mhs[allreduce_send_recv_pair][allreduce_rank].v;
            //     rdma_req.local_offset = 0;
            //     rdma_req.len = count*sizeof(double);
            //     rdma_req.rmt_mh.v = send_mhs[allreduce_send_recv_pair][target].v;
            //     rdma_req.rmt_offset = 0;
            //     rdma_req.type = GLEX_RDMA_TYPE_PUT;
            //     rdma_req.rmt_evt.cookie_0 = 0x9696969696969696ULL;
            //     rdma_req.rmt_evt.cookie_1 = 997;//广播flag 997
            //     // rdma_req.local_evt.cookie_0 = 998;
            //     // rdma_req.local_evt.cookie_1 = 997;
            //     rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
            //     rdma_req.flag = GLEX_FLAG_REMOTE_EVT;//| GLEX_FLAG_LOCAL_EVT;
            //     rdma_req.next = NULL;
            //     int ret;
            //     while ((ret = glex_rdma(_GLEXCOLL.ep, &rdma_req, &bad_rdma_req)) == GLEX_BUSY)
            //     {
            //     }
            //     if (ret != GLEX_SUCCESS)
            //     {
            //         if (ret == GLEX_INVALID_PARAM)
            //             printf("%d, _rdma() 非法参数", global_rank);
            //         printf("_rdma(), return: %d\n", ret);
            //         exit(1);
            //     }
            // }
        memcpy(recvbuf,allreduce_sendbuf,count*sizeof(double));
        // //第五步是将消息拷贝到recvbuf
        // for (int i = 0; i < count; i++)
        // {
        //     ((double *)recvbuf)[i] = r_data[i];
        // }
        // if (_GLEXCOLL.ep_credit <= 0)
        // {
        //     glex_discard_probed_mp(_GLEXCOLL.ep);
        //     _GLEXCOLL.ep_credit = EP_CREDIT_MAX;
        // }
    }
    //MPI_Bcast(recvbuf,count,MPI_DOUBLE,0,allreduce_comm);
        // MPI_Barrier(Comm_inter);
}

void Print_K_nomial_tree_information(int rank, int procn, int k, MPI_Comm comm)
{
    int my_depth = K_nominal_tree_my_stepN(allreduce_rank, allreduce_procn, 3);
    for (int j = 0; j < procn; j++)
    {
        for (int step = 0; step < my_depth; step++)
        {
            // if (rank == 0)
            //     printf("-------------my_depth$=%d-----------------------\n", my_depth);
            if (rank == j)
            {
                int parent = K_nominal_tree_parent(rank, k, step);
                printf("step=%d myrank=%d| parent=%d\n", step, rank, parent);
                int childvec[k];
                int childn;
                K_nominal_tree_child_vec(allreduce_rank, allreduce_procn, step, childvec, &childn, k);
                if (childn > 0)
                {
                    for (int x = 0; x < childn; x++)
                    {
                        printf("------childvec[%d]=%d\n", x, childvec[x]);
                    }
                }
            }
        }
        MPI_Barrier(comm);
        MPI_Barrier(comm);
        MPI_Barrier(comm);
    }
}
//K-nominal 树
int allreduce_k = 4;
extern double *allreduce_k_ary_bufvec[64];
void K_nomial_double_sum(double *sendbuf, double *recvbuf, int num)
{
    // memcpy(recvbuf, sendbuf, num * sizeof(double));
    // static MPI_Request reqVec[64];
    // static MPI_Status statusvec[64];
    // static int childvec[64];
    // // int tree_depth = K_nominal_tree_stepN(allreduce_procn, 3);
    // int k = allreduce_k;
    // int my_depth = K_nominal_tree_my_stepN(allreduce_rank, allreduce_procn, k);
    // // {
    // //     printf("%d\tmy_depth = %d\n", allreduce_rank, my_depth);
    // // }
    // //Print_K_nomial_tree_information(allreduce_rank, allreduce_procn, 3, allreduce_comm);
    // //第一步进行reduce操作
    // for (int step = 0; step < my_depth; step++)
    // {
    //     {
    //         int parent = K_nominal_tree_parent(allreduce_rank, k, step);
    //         int childvec[k];
    //         int childn;
    //         K_nominal_tree_child_vec(allreduce_rank, allreduce_procn, step, childvec, &childn, k);
    //         if (childn > 0)
    //         {
    //             //parent recv
    //             for (int c = 0; c < childn; c++)
    //             {
    //                 int child = childvec[c];
    //                 MPI_Irecv(allreduce_k_ary_bufvec[c], num, MPI_DOUBLE, child, 0, allreduce_comm, &(reqVec[c]));
    //             }
    //             for (int c = 0; c < childn; c++)
    //             {
    //                 MPI_Wait(&(reqVec[c]),&(statusvec[c]));
    //                 // for (int i = 0; i < num; i++)
    //                 //     recvbuf[i] += allreduce_k_ary_bufvec[c][i];
    //             }
    //         }
    //         if (parent != allreduce_rank)
    //         {
    //             MPI_Send(recvbuf, num, MPI_DOUBLE, parent, 0, allreduce_comm);
    //         }
    //     }
    // }
    MPI_Reduce(sendbuf, recvbuf, num, MPI_DOUBLE, MPI_SUM, 0, allreduce_comm);
    MPI_Bcast(recvbuf, num, MPI_DOUBLE, 0, allreduce_comm);
}
int GLEXCOLL_Allreduce(void *sendbuf, void *recvbuf, int count,
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    if (comm == MPI_COMM_WORLD)
    {

        allreduce_rank = global_rank;
        allreduce_procn = global_procn;
        allreduce_comm = comm;
        if (count == 1)
        {
            if (datatype == MPI_DOUBLE && op == MPI_SUM)
            {
                GLEXCOLL_Iallreduce(sendbuf, recvbuf, count, MPI_DOUBLE, MPI_SUM);
                GLEXCOLL_Wait_Iallreduce();
            }
        }
        else
        {
            //printf("intra_procn=%d\n",intra_procn);
            switch (SoftWare_Allreduce_Algorithm_Power_of_2)
            {
            case K_nomial_tree_OMP:
                if (datatype == MPI_DOUBLE)
                {
                    GLEXCOLL_Allreduce_K_ary(sendbuf, recvbuf, count);
                    return 1;
                }
                break;
            case Recursize_doubling_OMP:
                if (datatype == MPI_DOUBLE)
                {
                    recursive_doubling_double_sum(sendbuf, recvbuf, count);
                    return 1;
                }
                break;
            default:
                break;
            }
            //先判断是否为没节点单进程
            if (intra_procn == 1)
            {
                //判断进程数量是否为2^n个。
                if ((allreduce_procn & (allreduce_procn - 1)) == 0)
                {
                    //进程数量为2^n个
                    //puts("进程数量为2^n个");
                    //MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
                    switch (SoftWare_Allreduce_Algorithm_Power_of_2)
                    {
                    case Recursize_doubling_OMP:
                        /* code */
                        if (datatype == MPI_DOUBLE)
                            recursive_doubling_double_sum(sendbuf, recvbuf, count);
                        else
                            MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
                        break;
                    case Recursize_doubling_Slicing:
                        //puts("a");
                        if (datatype == MPI_DOUBLE)
                        {
                            //puts("x");
                            recursive_doubling_double_slicing_sum(sendbuf, recvbuf, count);
                        }
                        else
                            MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
                        break;
                    default:
                        MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
                        break;
                    }
                }
                else
                {
                    //进程数量不是2^n个
                    //puts("进程数量不是2^n个");
                    MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
                }
            }
            else
            {
                //每节点多进程
                MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
            }
        }
    }
    else
    {
        MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    }
}


            // ret = glex_receive_mp(_GLEXCOLL.ep, -1, &rmt_ep_addr, buf, &GLEXCOLL_databuf_len);
            // if (ret == GLEX_NO_MP)
            // {
            // 	printf("_receive_mp() (%d), return NO_MP?\n", __LINE__);
            // 	exit(1);
            // }
            // else if (ret != GLEX_SUCCESS)
            // {
            // 	printf("_receive_mp() (%d), return: %d\n", __LINE__, ret);
            // 	exit(1);
            // }
            // while ((ret = glex_probe_next_mp(_GLEXCOLL.ep, &rmt_ep_addr,(void **) &(r_data), &tmp_len)) == GLEX_NO_MP)
            // {
            // }
            // //printf("recv %d byte\n",tmp_len);
            // _GLEXCOLL.ep_credit -= 1;
            // if (_GLEXCOLL.ep_credit <= 0)
            // {
            //     glex_discard_probed_mp(_GLEXCOLL.ep);
            //     _GLEXCOLL.ep_credit = EP_CREDIT_MAX;
            // }
            // GLEX_Coll_req.rmt_ep_addr.v = _ReduceTreeS[_TreeID].childAddrs[i].v;
            // GLEX_Coll_req.data = r_data;
            // GLEX_Coll_req.len = sizeof(double) * count;
            // GLEX_Coll_req.flag = 0;
            // GLEX_Coll_req.next = NULL;
            // while ((ret = glex_send_imm_mp(_GLEXCOLL.ep, &GLEX_Coll_req, NULL)) == GLEX_BUSY)
            // {
            // }
            // if (ret != GLEX_SUCCESS)
            // {
            //     printf("_send_imm_mp() (%d), return: %d\n", __LINE__, ret);
            //     exit(1);
            // }