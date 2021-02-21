#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <unistd.h>
#include "glexcoll.h"
#include "glexalltoall.h"

//enum ALGORITHM {BRUCK,DIRECT};
int Alltoall_algorithm = 0;

MPI_Win ALLTOALL_win;
char *Bruck_buffer;
int NBUF_SEND = 4;
char SendStatus[64];
char RecvStatus[64];
char *block_tmp; //intra_procn * intra_procn * 8*1024
char *bufS, *bufR;
uint64_t BUFSR_size;
//RDMA需要使用的相关数据
static struct glex_rdma_req rdma_req;
static struct glex_rdma_req *bad_rdma_req;
static glex_event_t *event;
static  glex_mem_handle_t send_mh;

extern int leaderN;
int BruckStepN = 0; //指示着Bruck算法的运行步数

extern int mmin(int a, int b);

int num_of_ongoing_msg;
int Num_of_ongoing_and_SelfAdapting_ON = 0;
int Memcpy_On = 1;

struct Runtime_Data
{
    int target;
    double time;
};
struct Runtime_Data *RT_Collections;
int Runtime_Data_cmp(const void *a, const void *b)
{
    return ((struct Runtime_Data *)a)->time > ((struct Runtime_Data *)b)->time;
}

static void glexcoll_Init_RDMA()
{

    int ret = glex_register_mem(_GLEXCOLL.ep, bufR, BUFSR_size,
                                GLEX_MEM_READ | GLEX_MEM_WRITE,
                                &(_GLEXCOLL.local_mh));
    ret = glex_register_mem(_GLEXCOLL.ep, bufS, BUFSR_size,
                            GLEX_MEM_READ | GLEX_MEM_WRITE,
                            &send_mh);
    if (ret != GLEX_SUCCESS)
    {
        printf("_register_mem(), return: %d\n", ret);
        exit(1);
    }
    _GLEXCOLL.rmt_mhs = (glex_mem_handle_t *)malloc(sizeof(_GLEXCOLL.local_mh) * inter_procn);
    MPI_Allgather((void *)&(_GLEXCOLL.local_mh), sizeof(_GLEXCOLL.local_mh), MPI_CHAR,
                  _GLEXCOLL.rmt_mhs, sizeof(_GLEXCOLL.local_mh), MPI_CHAR, Comm_inter);
    if (intra_procn == 1)
    {
        RT_Collections = (struct Runtime_Data *)malloc(sizeof(*RT_Collections) * inter_procn);
        for (int i = 1; i < global_procn; i++)
        {

            RT_Collections[i - 1].target = (global_rank + i) % global_procn;
            RT_Collections[i - 1].time = 0.0;
        }
    }
}

void glexcoll_InitAlltoall()
{
    char *pathvar = getenv("ALLTOALL_TYPE");
    if (strcmp(pathvar, "BRUCK") == 0)
    {
        Alltoall_algorithm = BRUCK;
    }
    else if (strcmp(pathvar, "BRUCK_RDMA") == 0)
    {
        Alltoall_algorithm = BRUCK_RDMA;
    }
    else if (strcmp(pathvar, "DIRECT") == 0)
    {
        Alltoall_algorithm = DIRECT;
    }
    else if (strcmp(pathvar, "DIRECT_NODE_AWARE_alltoall") == 0)
    {
        Alltoall_algorithm = DIRECT_NODE_AWARE;
    }
    else if (strcmp(pathvar, "DIRECT_Kleader_NODE_AWARE") == 0)
    {
        Alltoall_algorithm = DIRECT_Kleader_NODE_AWARE;
    }
    else if (strcmp(pathvar, "DIRECT_Kleader_NODE_AWARE_RDMA") == 0)
    {
        Alltoall_algorithm = DIRECT_Kleader_NODE_AWARE_RDMA;
    }
    else if (strcmp(pathvar, "DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE") == 0)
    {
        Alltoall_algorithm = DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE;
        //节点内初始化单边通信区域。
    }
    else if (strcmp(pathvar, "XOR_EXCHANGE_RDMA_ONGOING") == 0)
    {
        Alltoall_algorithm = XOR_EXCHANGE_RDMA_ONGOING;
        //节点内初始化单边通信区域。
    }

    //分配缓冲区
    if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
    {
        //BUFSR_size = (inter_procn * (1<<20) * 8 * intra_procn * intra_procn);
        BUFSR_size = 2147483648ULL;
        bufS = (char *)malloc(BUFSR_size);
        bufR = (char *)malloc(BUFSR_size);
        block_tmp = (char *)malloc(intra_procn * intra_procn * 8 * 1024);
    }
    Bruck_buffer = (char *)malloc(1 << 25);
    int tmp = global_procn - 1;
    BruckStepN = 1;
    while (tmp != 1)
    {
        tmp = (tmp >> 1);
        BruckStepN++;
    }
    //RDMA的初始化准备
    if (inter_procn > 1 && intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
        glexcoll_Init_RDMA();
}

void MATRIX_transform(char *buf, int n, int block_size)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < i; j++)
        {
            char *p = buf + (i * n + j) * block_size;
            char *q = buf + (j * n + i) * block_size;
            memcpy(block_tmp, p, block_size);
            memcpy(p, q, block_size);
            memcpy(q, block_tmp, block_size);
        }
    }
}
int COMMUNICATION_ON = 1;
int Intra_Gather_Scatter_ON = 1;
int MATRIX_Tanfform_ON = 1;
void DIRECT_NODE_AWARE_alltoall(void *sendbuf,
                                int sendsize,
                                void *recvbuf,
                                int recvsize,
                                MPI_Comm comm)
{
    //puts("check DIRECT_NODE_AWARE_alltoall");
    //第一步将消息聚合到leader上
    MPI_Request reqs[inter_procn];
    MPI_Request reqSend[inter_procn];
    MPI_Request reqRecv[inter_procn];
    MPI_Status status[inter_procn];
    char *source, *target;
    int block_size = sendsize * ppn;
    if (Intra_Gather_Scatter_ON)
    {
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            source = sendbuf + shift * block_size;
            MPI_Gather(source, block_size, MPI_CHAR, bufS + shift * block_size * ppn, block_size, MPI_CHAR, 0, Comm_intra);
        }
    }

    if (intra_rank == 0)
    {
        if (COMMUNICATION_ON)
        {
            // MPI_Alltoall(bufS, block_size * ppn, MPI_CHAR, bufR, block_size * ppn, MPI_CHAR, Comm_inter);
            for (int shift = 0; shift < inter_procn; ++shift)
            {
                int target = (inter_rank + shift) % inter_procn;
                MPI_Isend(bufS + target * block_size * ppn, block_size * ppn, MPI_CHAR,
                          target, 0, Comm_inter, &(reqSend[shift]));
                int source = (inter_procn + inter_rank - shift) % inter_procn;
                MPI_Irecv(bufR + block_size * ppn * source, block_size * ppn, MPI_CHAR,
                          source,
                          0, Comm_inter, &(reqRecv[shift]));
            }
            MPI_Waitall(inter_procn, reqSend, status);
            MPI_Waitall(inter_procn, reqRecv, status);
        }

        //将消息转置后scatter
        if (MATRIX_Tanfform_ON)
            for (int shift = 0; shift < inter_procn; ++shift)
            {
                source = bufR + shift * block_size * ppn;
                MATRIX_transform(source, ppn, sendsize);
            }
    }
    if (Intra_Gather_Scatter_ON)
    {
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            source = bufR + shift * block_size * ppn;
            target = recvbuf + shift * block_size;
            MPI_Scatter(source, block_size, MPI_CHAR, target, block_size, MPI_CHAR, 0, Comm_intra);
        }
    }
}

/*
*/

void DIRECT_Kleader_NODE_AWARE_RDMA_alltoall(void *sendbuf,
                                             int sendsize,
                                             void *recvbuf,
                                             int recvsize,
                                             MPI_Comm comm)
{
    MPI_Request reqs[inter_procn];
    MPI_Request reqSend[inter_procn];
    MPI_Request reqRecv[inter_procn];
    MPI_Status status[inter_procn];
    char *source, *target;
    int block_size = sendsize * ppn;
    int BufScount = 0;
    if (Intra_Gather_Scatter_ON)
    {
        char *p = bufS;
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            int block_id = (inter_rank + shift) % inter_procn;
            int leader = (shift % leaderN) << 2;
            source = sendbuf + block_id * block_size;
            //MPI_Igather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra, &(reqs[shift]));
            MPI_Gather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra);

            if (intra_rank == leader)
            {
                p += block_size * ppn;
                BufScount++;
            }
        }
    }
    BufScount = inter_procn;
    if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
    {
        if (COMMUNICATION_ON)
        {
            for (int c = 0; c < BufScount; ++c)
            {
                int shift = (intra_rank >> 2) + leaderN * c;
                int target = (inter_rank + shift) % inter_procn;
                rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
                // //puts("check 225");
                rdma_req.local_mh.v = send_mh.v;
                rdma_req.local_offset = c * block_size * ppn;
                rdma_req.len = block_size * ppn;
                rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
                rdma_req.rmt_offset = c * ppn * block_size;
                // //puts("check 231");
                rdma_req.type = GLEX_RDMA_TYPE_PUT;
                rdma_req.rmt_evt.cookie_0 = 0x9696969696969696ULL;
                rdma_req.rmt_evt.cookie_1 = 0x9696969696969696ULL;
                rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
                if (c % num_of_ongoing_msg == 0)
                    rdma_req.flag = GLEX_FLAG_REMOTE_EVT | GLEX_FLAG_FENCE;
                else
                    rdma_req.flag = GLEX_FLAG_REMOTE_EVT;
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
            for (int c = 0; c < BufScount; ++c)
            {
                // glex_ep_addr_t tmp;
                // int ret = glex_receive_mp(_GLEXCOLL.ep, -1, &tmp, GLEXCOLL_databuf, &GLEXCOLL_databuf_len);
                // printf("recv %c\n",*(char *)&(GLEXCOLL_databuf));
                // glex_ep_addr_t ep_addr;
                // glex_get_ep_addr(_GLEXCOLL.ep, &ep_addr);
                // printf("local ep_addr = %#llx %#llx\n", (long long)ep_addr.v,(long long)_GLEXCOLL.ep_addrs[1].v);
                // printf("local ep_mem_handle = %#llx\n",
                //        (long long)_GLEXCOLL.local_mh.v);
                while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                    ;
                if (event->cookie_1 != 0x9696969696969696ULL)
                {
                    printf("probed a new event, but cookie_1 is invalid: %#llx\n",
                           (long long)event->cookie_1);
                }
                glex_discard_probed_event(_GLEXCOLL.ep);
            }
        }
        //将消息转置后scatter
        if (MATRIX_Tanfform_ON)
            for (int c = 0; c < BufScount; ++c)
            {
                source = bufR + c * block_size * ppn;
                MATRIX_transform(source, ppn, sendsize);
            }
    }

    if (Intra_Gather_Scatter_ON)
    {
        char *p = bufR;
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            int recv_from = (inter_procn + inter_rank - shift) % inter_procn;
            int leader = (shift % leaderN) << 2;

            target = recvbuf + recv_from * block_size;
            MPI_Scatter(p, block_size, MPI_CHAR, target, block_size, MPI_CHAR, leader, Comm_intra);
            if (intra_rank == leader)
                p += block_size * ppn;
        }
        //MPI_Waitall(inter_procn, reqs, status);
    }
}
int slice_size;
void DIRECT_One_slice_corePNODE_AWARE_RDMA_alltoall(void *sendbuf,
                                                    int sendsize,
                                                    void *recvbuf,
                                                    int recvsize,
                                                    MPI_Comm comm)
{
    //puts("check");
    int ret;
    int ongoin_msgN = 0;
    // int total_msgN =
    //基本策略是将每条消息分解成slice_size的大小然后流水化传输。
    //Linear exchange pattern
    int size_per_rank = sendsize * global_procn;
    int slice_count = 1;
    if (sendsize > slice_size)
        slice_count = sendsize / slice_size + ((sendsize % slice_size > 0) ? 1 : 0);

    //性能感知排序
    for (int shift = 1; shift < global_procn; shift++)
    {
        int target = (global_rank + shift) % global_procn;
        int remain_size = sendsize;
        int addr_shift = target * sendsize;
        int slice_id = 0;
        while (slice_id < slice_count)
        {
            int sizeS = mmin(remain_size, slice_size);
            //发送切片并流水化
            memcpy(bufS + addr_shift, sendbuf + addr_shift, sizeS);
            {
                rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
                // if(global_rank == 1) printf("rmt_ep_addr = %lld\n",rdma_req.rmt_ep_addr.v);
                // if(global_rank == 0) printf("local_ep_addr = %lld\n",_GLEXCOLL.ep_addrs[global_rank].v);
                // //puts("check 225");
                rdma_req.local_mh.v = send_mh.v;
                rdma_req.local_offset = addr_shift;
                rdma_req.len = sizeS;
                rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
                rdma_req.rmt_offset = global_rank * sendsize + slice_id * slice_size;
                // //puts("check 231");
                rdma_req.type = GLEX_RDMA_TYPE_PUT;
                rdma_req.rmt_evt.cookie_0 = 1 + global_rank;
                rdma_req.rmt_evt.cookie_1 = 1 + slice_id;
                rdma_req.flag = GLEX_FLAG_REMOTE_EVT;
                rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
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

            addr_shift += sizeS;
            slice_id++;
            remain_size -= sizeS;
        }
    }
    //printf("check %d\n",global_rank);
    int received_count = 0;
    while (received_count < global_procn - 1)
    {
        int received_slice = 0;
        while (received_slice < slice_count)
        {
            /* code */
            while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                ;
            int recv_from = event->cookie_0 - 1;
            int slice_id = event->cookie_1 - 1;
            int shift = recv_from * sendsize + slice_id * slice_size;
            received_slice++;
            glex_discard_probed_event(_GLEXCOLL.ep);
            int sizeTmp = slice_size;
            if (slice_id == slice_count - 1 && sendsize % slice_size != 0)
                sizeTmp = sendsize % slice_size;
            memcpy(recvbuf + shift, bufR + shift, sizeTmp);
        }
        received_count++;
    }
    memcpy(recvbuf + global_rank * sendsize, sendbuf + global_rank * sendsize, sendsize);
}

void XOR_EXCHANGE_RDMA_ONGOING_alltoall(void *sendbuf,
                                        int sendsize,
                                        void *recvbuf,
                                        int recvsize,
                                        MPI_Comm comm)
{

    //puts("check");
    int ret;
    // int total_msgN =
    //基本策略是将每条消息分解成slice_size的大小然后流水化传输。
    //Linear exchange pattern
    int size_per_rank = sendsize * global_procn;
    //memcpy(bufS,sendbuf,size_per_rank);
    //性能感知排序
    for (int shift = 1; shift < global_procn; shift++)
    {
        int target = (global_rank + shift) % global_procn;
        int remain_size = sendsize;
        int addr_shift = target * sendsize;
        if (Memcpy_On)
            memcpy(bufS + addr_shift, sendbuf + addr_shift, sendsize);
        rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
        rdma_req.local_mh.v = send_mh.v;
        rdma_req.local_offset = addr_shift;

        // for (int i = 0; i < 1; i++)
        //     printf("%d send %f\n",global_rank, ((double *)(bufS + addr_shift))[i]);
        rdma_req.len = sendsize;
        rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
        rdma_req.rmt_offset = global_rank * sendsize;
        rdma_req.type = GLEX_RDMA_TYPE_PUT;
        rdma_req.rmt_evt.cookie_0 = 1 + global_rank;
        rdma_req.rmt_evt.cookie_1 = 996; //*((double*)(bufS + addr_shift));
        rdma_req.local_evt.cookie_1 = 997;
        rdma_req.flag = GLEX_FLAG_LOCAL_EVT | GLEX_FLAG_REMOTE_EVT;
        rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
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

    //printf("check %d\n",global_rank);
    int received_count = 0;
    int sended_count = 0;
    while (received_count < global_procn - 1 || sended_count < global_procn - 1)
    {
        /* code */
        while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
            ;
        int recv_from = event->cookie_0 - 1;
        int cookie_1 = event->cookie_1;
        int shift = recv_from * sendsize;
        glex_discard_probed_event(_GLEXCOLL.ep);
        // for (int i = 0; i < 2; i++)
        //     printf("recv %f recv_from = %d\n", ((double *)(bufR))[i],recv_from);
        if (cookie_1 == 996)
        {
            if (Memcpy_On)
                memcpy(recvbuf + shift, bufR + shift, sendsize);
            received_count++;
        }
        else if (cookie_1 == 997)
            sended_count++;
        else
        {
            puts("error");
            exit(0);
        }

        //printf("%d check recv\n",global_rank);
    }
    //memcpy(recvbuf, bufR, global_procn*sendsize);
    if (Memcpy_On)
        memcpy(recvbuf + global_rank * sendsize, sendbuf + global_rank * sendsize, sendsize);
    MPI_Barrier(MPI_COMM_WORLD);
}

void DIRECT_One_corePNODE_AWARE_RDMA_alltoall(void *sendbuf,
                                              int sendsize,
                                              void *recvbuf,
                                              int recvsize,
                                              MPI_Comm comm)
{
    //puts("check");
    int ret;
    // int total_msgN =
    //Linear exchange pattern
    int size_per_rank = sendsize * global_procn;
    //memcpy(bufS,sendbuf,size_per_rank);
    for (int shift = 1; shift < global_procn; shift++)
    {

        int target = (global_rank + shift) % global_procn;
        int remain_size = sendsize;
        int addr_shift = target * sendsize;
        if (Memcpy_On)
            memcpy(bufS + addr_shift, sendbuf + addr_shift, sendsize);
        rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
        rdma_req.local_mh.v = send_mh.v;
        rdma_req.local_offset = addr_shift;

        // for (int i = 0; i < 1; i++)
        //     printf("%d send %f\n",global_rank, ((double *)(bufS + addr_shift))[i]);
        rdma_req.len = sendsize;
        rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
        rdma_req.rmt_offset = global_rank * sendsize;
        rdma_req.type = GLEX_RDMA_TYPE_PUT;
        rdma_req.rmt_evt.cookie_0 = 1 + global_rank;
        rdma_req.rmt_evt.cookie_1 = 996; //*((double*)(bufS + addr_shift));
        rdma_req.flag = GLEX_FLAG_REMOTE_EVT;
        if (shift % num_of_ongoing_msg == 0)
            rdma_req.flag = GLEX_FLAG_FENCE | GLEX_FLAG_REMOTE_EVT;
        rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
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

    //printf("check %d\n",global_rank);
    int received_count = 0;
    int sended_count = 0;
    while (received_count < global_procn - 1)
    {
        /* code */
        while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
            ;
        int recv_from = event->cookie_0 - 1;
        int cookie_1 = event->cookie_1;
        int shift = recv_from * sendsize;
        // for (int i = 0; i < 2; i++)
        //     printf("recv %f recv_from = %d\n", ((double *)(bufR))[i],recv_from);
       // printf("%d check recv\n",global_rank);
        if (cookie_1 == 996)
        {
            if (Memcpy_On)
                memcpy(recvbuf + shift, bufR + shift, sendsize);
            received_count++;
        }
        else if (cookie_1 == 997)
            sended_count++;
        else
        {
            puts("error");
            exit(0);
        }
    }
    glex_discard_probed_event(_GLEXCOLL.ep);
    //memcpy(recvbuf, bufR, global_procn*sendsize);
    if (Memcpy_On)
        memcpy(recvbuf + global_rank * sendsize, sendbuf + global_rank * sendsize, sendsize);

    // while (glex_probe_next_event(_GLEXCOLL.ep, &event) != GLEX_NO_EVENT){
    //         glex_discard_probed_event(_GLEXCOLL.ep);
    // }
    //printf("check F%d\n",global_rank);
}
int RT_Self_Adapting_on = 0;
void DIRECT_RDMA_NUM_ONGOING_Self_Adapting(void *sendbuf,
                                           int sendsize,
                                           void *recvbuf,
                                           int recvsize,
                                           MPI_Comm comm)
{
    //每节点一进程
    //控制同时发送的消息数量
    //自适应拥塞避免
    //puts("check");

    int ret;
    int onGoingMax = num_of_ongoing_msg;
    int onGoingNum = 0;
    int received_count = 0;
    int sended_count = 0;
    //for (int shift = 1; shift < global_procn; shift++)
    for (int location = 0; location < global_procn - 1; location++)
    {
        //int shift = (global_procn + RT_Collections[location].target - global_rank)%global_procn;

        int target;

        if (RT_Self_Adapting_on)
            target = RT_Collections[location].target; //(global_rank + shift) % global_procn;//
        else
            target = (global_rank + location + 1) % global_procn;
        int remain_size = sendsize;
        int addr_shift = target * sendsize;
        memcpy(bufS + addr_shift, sendbuf + addr_shift, sendsize);
        rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
        rdma_req.local_mh.v = send_mh.v;
        rdma_req.local_offset = addr_shift;
        rdma_req.len = sendsize;
        rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
        rdma_req.rmt_offset = global_rank * sendsize;
        rdma_req.type = GLEX_RDMA_TYPE_PUT;
        rdma_req.flag = GLEX_FLAG_LOCAL_EVT | GLEX_FLAG_REMOTE_EVT;

        rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
        while (onGoingNum == onGoingMax)
        {
            //当没有空余发送信用的时候，等待发送信用
            {
                /* code */
                while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                    ;
                int cookie_1 = event->cookie_1;
                if (cookie_1 == 999999999) //这是RMT EVNT
                {
                    int recv_fromORTarget = event->cookie_0 - 1;
                    int shift = recv_fromORTarget * sendsize;
                    memcpy(recvbuf + shift, bufR + shift, sendsize);
                    received_count++;
                }
                else
                {
                    double startT = event->cookie_0;
                    int location = event->cookie_1 - 1;
                    //puts("check local event");
                    sended_count++;
                    onGoingNum--;
                    double endT = MPI_Wtime();
                    if (RT_Self_Adapting_on)
                        RT_Collections[location].time = endT - startT;
                }
                glex_discard_probed_event(_GLEXCOLL.ep);
                //printf("%d check recv\n",global_rank);
            }
        }
        rdma_req.rmt_evt.cookie_0 = 1 + global_rank;
        rdma_req.rmt_evt.cookie_1 = 999999999;      //是RMT
        rdma_req.local_evt.cookie_0 = MPI_Wtime();  //请求进入队列的时间
        rdma_req.local_evt.cookie_1 = 1 + location; //007是RMT
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
        onGoingNum++;
    }
    //printf("check %d\n",global_rank);
    while (received_count < (global_procn - 1) || sended_count < (global_procn - 1))
    {
        /* code */
        while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
            ;
        int cookie_1 = event->cookie_1;
        if (cookie_1 == 999999999) //这是RMT EVNT
        {
            int recv_fromORTarget = event->cookie_0 - 1;
            int shift = recv_fromORTarget * sendsize;
            memcpy(recvbuf + shift, bufR + shift, sendsize);
            received_count++;
        }
        else
        {
            double startT = event->cookie_0;
            int target = event->cookie_1 - 1;
            //puts("check local event");
            sended_count++;
            onGoingNum--;
            double endT = MPI_Wtime();
            RT_Collections[target].time = endT - startT;
        }
        glex_discard_probed_event(_GLEXCOLL.ep);
    }

    if (RT_Self_Adapting_on)
        qsort(RT_Collections, global_procn - 1, sizeof(*RT_Collections), Runtime_Data_cmp);
    memcpy(recvbuf + global_rank * sendsize, sendbuf + global_rank * sendsize, sendsize);
    MPI_Barrier(MPI_COMM_WORLD);
}

//每节点多进程
void DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE_alltoall(void *sendbuf,
                                                      int sendsize,
                                                      void *recvbuf,
                                                      int recvsize,
                                                      MPI_Comm comm)
{
    MPI_Win win;
    MPI_Win_create(recvbuf, sendsize * global_procn, 1, MPI_INFO_NULL, Comm_intra, &win);
    MPI_Request reqs[inter_procn];
    MPI_Request reqSend[inter_procn];
    MPI_Request reqRecv[inter_procn];
    MPI_Status status[inter_procn];
    char *source, *target;
    int block_size = sendsize * ppn;
    int BufScount = 0;
    int received_count = 0;
    int sendsed_count = 0;
    int shift = 0;
    int shifts[100];
    char *p_send = bufS;
    char *p_recv = bufR;
    int rmt_offset = 0;
    MPI_Win_fence(0, win);
    // MPI_Barrier(MPI_COMM_WORLD);
    // puts("check 321");
    while (shift < inter_procn)
    {
        int BufScount = mmin(num_of_ongoing_msg, inter_procn - shift);
        int MySendCount = 0;
        char *p = p_send;
        if (Intra_Gather_Scatter_ON)
            for (int i = 0; i < BufScount; i++)
            {
                //现在进行shift个消息的阻塞式Gather
                int block_id = (inter_rank + shift) % inter_procn;
                int leader = (shift % leaderN) << 2;
                source = sendbuf + block_id * block_size;
                //MPI_Igather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra, &(reqs[shift]));
                MPI_Gather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra);

                if (intra_rank == leader)
                {

                    //printf("%d recv a msg from %d \n", inter_rank, recv_from);
                    p += block_size * ppn;
                    shifts[MySendCount] = shift;
                    MySendCount++;
                }
                shift++;
            }
        if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
        {

            for (int i = 0; i < MySendCount; i++)
            {
                //现在进行shift个消息的通信请求发送
                int target = (inter_rank + shifts[i]) % inter_procn;
                rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
                // //puts("check 225");
                rdma_req.local_mh.v = send_mh.v;
                rdma_req.local_offset = p_send - bufS;
                // if (global_rank == 32)
                // {
                //     for (int i = 0; i < ppn * ppn; i++)
                //     {
                //         printf("%d %f\n", inter_rank, ((double *)p_send)[i]);
                //     }
                // }

                rdma_req.len = block_size * ppn;
                rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
                rdma_req.rmt_offset = rmt_offset * ppn * block_size;
                // //puts("check 231");
                rdma_req.type = GLEX_RDMA_TYPE_PUT;
                rdma_req.rmt_evt.cookie_0 = 1 + inter_rank;
                rdma_req.rmt_evt.cookie_1 = 1 + rmt_offset;
                rmt_offset++;
                rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
                if (i == MySendCount - 1)
                    rdma_req.flag = GLEX_FLAG_REMOTE_EVT | GLEX_FLAG_FENCE;
                else
                    rdma_req.flag = GLEX_FLAG_REMOTE_EVT;
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
                p_send += ppn * block_size;
                sendsed_count++;
            }
        }
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    // puts("check 393");
    if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
        while (received_count < sendsed_count)
        {
            /* code */

            while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                ;
            received_count++;
            int recv_from = event->cookie_0 - 1;
            int offset = (event->cookie_1) - 1;
            int shift = (inter_procn + inter_rank - recv_from) % inter_procn;
            glex_discard_probed_event(_GLEXCOLL.ep);

            char *received_buffer = bufR + offset * ppn * ppn * sendsize;
            // if (global_rank == 32)
            // {
            //     printf("offset =%d\n", offset);
            //     for (int i = 0; i < ppn * ppn; i++)
            //     {
            //         printf("%d %f\n", inter_rank, ((double *)bufR)[i]);
            //     }
            // }
            if (MATRIX_Tanfform_ON)
                MATRIX_transform(received_buffer, ppn, sendsize);
            if (Intra_Gather_Scatter_ON)
                for (int child = 0; child < ppn; child++)
                {
                    int leader = (shift % leaderN) << 2;
                    MPI_Put(received_buffer + child * block_size, block_size, MPI_CHAR,
                            child, recv_from * ppn * sendsize, block_size, MPI_CHAR, win);
                }
        }

    // MPI_Barrier(MPI_COMM_WORLD);
    // puts("check 427");
    MPI_Win_fence(0, win);
    MPI_Win_free(&win);
}

int leaderN = 1;
void DIRECT_Kleader_NODE_AWARE_alltoall(void *sendbuf,
                                        int sendsize,
                                        void *recvbuf,
                                        int recvsize,
                                        MPI_Comm comm)
{
    MPI_Request reqs[inter_procn];
    MPI_Request reqSend[inter_procn];
    MPI_Request reqRecv[inter_procn];
    MPI_Status status[inter_procn];
    char *source, *target;
    int block_size = sendsize * ppn;
    int BufScount = 0;
    if (Intra_Gather_Scatter_ON)
    {
        char *p = bufS;
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            int block_id = (inter_rank + shift) % inter_procn;
            int leader = (shift % leaderN) << 2;
            source = sendbuf + block_id * block_size;
            //MPI_Igather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra, &(reqs[shift]));
            MPI_Gather(source, block_size, MPI_CHAR, p, block_size, MPI_CHAR, leader, Comm_intra);

            if (intra_rank == leader)
            {
                p += block_size * ppn;
                BufScount++;
            }
        }
        // if(inter_rank == 1 && intra_rank == 4)
        // {
        //     for(int i = 0;i<ppn*ppn*BufScount;i++)
        //         printf("%f\n",((double *)bufS)[i]);
        // }
    }

    if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
    {
        if (COMMUNICATION_ON)
        {
            //MPI_Alltoall(bufS, block_size * ppn, MPI_CHAR, bufR, block_size * ppn, MPI_CHAR, Comm_inter);
            for (int c = 0; c < BufScount; ++c)
            {
                int shift = (intra_rank >> 2) + leaderN * c;
                int target = (inter_rank + shift) % inter_procn;
                MPI_Isend(bufS + c * block_size * ppn, block_size * ppn, MPI_CHAR,
                          target, 0, Comm_inter, &(reqSend[c]));
                int source = (inter_procn + inter_rank - shift) % inter_procn;
                MPI_Irecv(bufR + c * ppn * block_size, block_size * ppn, MPI_CHAR,
                          source, 0, Comm_inter, &(reqRecv[c]));
            }
            MPI_Waitall(BufScount, reqSend, status);
            MPI_Waitall(BufScount, reqRecv, status);
        }
        // if(global_rank == 68)
        // {
        //     for(int i = 0;i<ppn*ppn*BufScount;i++)
        //         printf("%f\n",((double *)bufR)[i]);
        // }
        //将消息转置后scatter
        if (MATRIX_Tanfform_ON)
            for (int c = 0; c < BufScount; ++c)
            {
                source = bufR + c * block_size * ppn;
                MATRIX_transform(source, ppn, sendsize);
            }
        // if(global_rank == 32)
        // {
        //     for(int i = 0;i<ppn*ppn*inter_procn;i++)
        //         printf("%f\n",((double *)bufR)[i]);
        // }
        //printf("check %d\n",global_rank);
    }

    if (Intra_Gather_Scatter_ON)
    {
        char *p = bufR;
        for (int shift = 0; shift < inter_procn; ++shift)
        {
            int recv_from = (inter_procn + inter_rank - shift) % inter_procn;
            int leader = (shift % leaderN) << 2;

            target = recvbuf + recv_from * block_size;
            MPI_Scatter(p, block_size, MPI_CHAR, target, block_size, MPI_CHAR, leader, Comm_intra);
            if (intra_rank == leader)
                p += block_size * ppn;
        }
        //MPI_Waitall(inter_procn, reqs, status);
    }
}

static MPI_Request reqSV[32];
static MPI_Request reqRV[32];
static MPI_Status  statusV[32];
//适用于每节点任意进程
void DIRECT_alltoall(void *sendbuf,
                     int sendsize,
                     void *recvbuf,
                     int recvsize,
                     MPI_Comm comm)
{
    //MPI_Alltoall(sendbuf,sendsize,MPI_DOUBLE,recvbuf,recvsize,MPI_DOUBLE,comm);
    // MPI_Request req_sends[global_procn];
    // MPI_Request req_recvs[global_procn];
    // MPI_Status status[global_procn];

    // if(global_rank ==1)
    //     printf("sendbuf[0] = %f,sendbuf[1] = %f\n",((double *)sendbuf)[0],((double *)sendbuf)[1]);
    for (int shift_start=0;shift_start<global_procn;shift_start+=num_of_ongoing_msg)
    {
        int s;
        for (s = 0; s < num_of_ongoing_msg && (s + shift_start) < global_procn; s++)
        {
            int shift = shift_start + s;
            {
                int target = (global_rank + shift) % global_procn;
                MPI_Isend(sendbuf + sendsize * target, sendsize, MPI_CHAR,
                          target, global_rank, comm, &(reqSV[s]));
                int source = (global_procn + global_rank - shift) % global_procn;
                MPI_Irecv(recvbuf + recvsize * source, sendsize, MPI_CHAR,
                          source,
                          source, comm, &(reqRV[s]));
                // MPI_Wait(&(req_sends[shift]), status);
                // MPI_Wait(&(req_recvs[shift]), status);
            }
        }
        MPI_Waitall(s, reqSV, statusV);
        MPI_Waitall(s, reqRV, statusV);
    }
    

}

void BRUCK_RMA_alltoall(void *sendbuf,
                        int sendsize,
                        void *recvbuf,
                        int recvsize,
                        MPI_Comm comm)
{
    int data_send = 0;
    // char * start = sendbuf + global_rank*sendsize;
    char *tmpbuf = (char *)malloc(sizeof(char) * global_procn * sendsize);
    for (int i = 0; i < global_procn; i++)
    {
        char *source = sendbuf + sendsize * ((global_rank + i) % global_procn);
        char *target = tmpbuf + sendsize * i;
        memcpy(target, source, sendsize);
    }
    int tmp = global_procn;
    char *bufPack = (char *)malloc(sizeof(char) * global_procn * sendsize);
    // if(global_rank == 1)
    //     printf("BruckStepN= %d\n",BruckStepN);
    // if(global_rank==2){
    //     printf("BEFORE: tmpbuf[0]= %f tmpbuf[1]=%f tmpbuf[2]=%f\n",((double*)tmpbuf)[0],((double*)tmpbuf)[1],((double*)tmpbuf)[2]);
    // }
    // if( global_rank == 0)
    // {
    //     printf("global rank = %d, BruckStepN =%d\n",global_rank,BruckStepN);
    // }
    for (int step = 0; step < BruckStepN; step++)
    {
        char *bufPackP = bufPack;
        unsigned int checkValue = (1 << step);
        // if (global_rank == 1)
        //     printf("-----checkValue = %d -------\n",checkValue);
        //接下来将消息打包到bufPack
        for (unsigned int shift = 1; shift < global_procn; ++shift)
        {
            // if (global_rank == 1)
            //     printf("shift = %d\n",shift);
            if ((shift & checkValue))
            {
                char *source = tmpbuf + sendsize * shift;
                memcpy(bufPackP, source, sendsize);
                bufPackP += sendsize;
                // if (global_rank == 1)
                // {
                //     printf("pack %f  ", ((double *)bufPack)[0]);
                // }
            }
        }
        if (COMMUNICATION_ON)
        {
            MPI_Request req;
            MPI_Status status;
            //消息打包完毕后,开始数据传输。
            // MPI_Put(bufPack, (bufPackP - bufPack), MPI_CHAR,
            //         (global_rank + checkValue) % global_procn,
            //         0,
            //         (bufPackP - bufPack),
            //         MPI_CHAR,
            //         ALLTOALL_win);
            MPI_Irecv(Bruck_buffer, (bufPackP - bufPack), MPI_CHAR, (global_procn + global_rank - checkValue) % global_procn, 0, MPI_COMM_WORLD, &req);
            MPI_Send(bufPack, (bufPackP - bufPack), MPI_CHAR, (global_rank + checkValue) % global_procn, 0, MPI_COMM_WORLD);
            MPI_Wait(&req, &status);
        }
        //每一步传输完成后开始消息解包
        char *bufUnpackP = Bruck_buffer;
        for (int shift = 1; shift < global_procn; ++shift)
        {
            if ((shift & checkValue))
            {
                char *target = tmpbuf + sendsize * shift;
                memcpy(target, bufUnpackP, recvsize);
                bufUnpackP += recvsize;
            }
        }
    }
    // if (global_rank == 2)
    // {
    //     printf("tmpbuf[0]= %f tmpbuf[1]=%f tmpbuf[2]=%f\n", ((double *)tmpbuf)[0], ((double *)tmpbuf)[1], ((double *)tmpbuf)[2]);
    // }
    //最后将数据还原到recvbuf中去
    for (int i = 0; i < global_procn; i++)
    {
        char *target = recvbuf + sendsize * i;
        char *source = tmpbuf + sendsize * ((global_procn + global_rank - i) % global_procn);
        memcpy(target, source, sendsize);
    }
    // if (global_rank == 2)
    // {
    //     printf("recvbuf[0]= %f recvbuf[1]=%f recvbuf[2]=%f\n", ((double *)recvbuf)[0], ((double *)recvbuf)[1], ((double *)recvbuf)[2]);
    // }

    free(tmpbuf);
    free(bufPack);
}

void BRUCK_RDMA_alltoall(void *sendbuf,
                         int sendsize,
                         void *recvbuf,
                         int recvsize,
                         MPI_Comm comm)
{
    int data_send = 0;
    char *tmpbuf = (char *)malloc(sizeof(char) * global_procn * sendsize);
    for (int i = 0; i < global_procn; i++)
    {
        char *source = sendbuf + sendsize * ((global_rank + i) % global_procn);
        char *target = tmpbuf + sendsize * i;
        if (Memcpy_On)
            memcpy(target, source, sendsize);
    }
    int tmp = global_procn;
    int recv_flag[20];
    memset(recv_flag, 0, sizeof(int) * 20);
    for (int step = 0; step < BruckStepN; step++)
    {
        int memshift = sendsize * global_procn * step;
        char *bufPackP = bufS + memshift;
        unsigned int checkValue = (1 << step);
        for (unsigned int shift = checkValue; shift < global_procn; shift += (checkValue << 1))
        {
            int lenb = sendsize * mmin(checkValue, global_procn - shift);
            if (Memcpy_On)
                memcpy(bufPackP, tmpbuf + sendsize * shift, lenb);
            bufPackP += lenb;
        }
        if (COMMUNICATION_ON)
        {
            // MPI_Request req;
            // MPI_Status status;
            // MPI_Irecv(bufR, (bufPackP - bufS), MPI_CHAR, (global_procn + global_rank - checkValue) % global_procn, 0, MPI_COMM_WORLD, &req);
            // MPI_Send(bufS,(bufPackP - bufS), MPI_CHAR, (global_rank + checkValue) % global_procn, 0, MPI_COMM_WORLD);
            // MPI_Wait(&req, &status);

            int target = (global_rank + checkValue) % global_procn;
            rdma_req.rmt_ep_addr.v = _GLEXCOLL.ep_addrs[target].v;
            rdma_req.local_mh.v = send_mh.v;
            rdma_req.local_offset = memshift;
            // for (int i = 0; i < 1; i++)
            //     printf("%d send %f\n",global_rank, ((double *)(bufS + addr_shift))[i]);
            rdma_req.len = (bufPackP - bufS - memshift);
            rdma_req.rmt_mh.v = _GLEXCOLL.rmt_mhs[target].v;
            rdma_req.rmt_offset = memshift;
            rdma_req.type = GLEX_RDMA_TYPE_PUT;
            rdma_req.rmt_evt.cookie_0 = 1 + global_rank;
            rdma_req.rmt_evt.cookie_1 = 1 + step;   //*((double*)(bufS + addr_shift));
            rdma_req.local_evt.cookie_1 = 1 + step; //*((double*)(bufS + addr_shift));
            rdma_req.flag = GLEX_FLAG_REMOTE_EVT;
            rdma_req.rmt_key = _GLEXCOLL.ep_attr.key;
            int ret;
            while ((ret = glex_rdma(_GLEXCOLL.ep, &rdma_req, &bad_rdma_req)) == GLEX_BUSY)
                ;
            {
            }
            if (ret != GLEX_SUCCESS)
            {
                if (ret == GLEX_INVALID_PARAM)
                    printf("%d, _rdma() 非法参数", global_rank);
                printf("_rdma(), return: %d\n", ret);
                exit(1);
            }
            // int recv_from = event->cookie_0 - 1;
            // int shift = recv_from * sendsize;
            //等待传输完成
        }
        while (recv_flag[step] != 1)
        {
            /* code */
            while (glex_probe_next_event(_GLEXCOLL.ep, &event) == GLEX_NO_EVENT)
                ;
            int step_rmt = event->cookie_1 - 1;
            //printf("%d step_rmt = %d\n",global_rank,step_rmt);
            recv_flag[step_rmt] = 1;
            glex_discard_probed_event(_GLEXCOLL.ep);
        }

        //每一步传输完成后开始消息解包
        char *bufUnpackP = bufR + memshift;
        for (int shift = 1; shift < global_procn; ++shift)
        {
            if ((shift & checkValue))
            {
                char *target = tmpbuf + sendsize * shift;
                if (Memcpy_On)
                    memcpy(target, bufUnpackP, recvsize);
                bufUnpackP += recvsize;
            }
        }
    }
    //最后将数据还原到recvbuf中去

    for (int i = 0; i < global_procn; i++)
    {
        char *target = recvbuf + sendsize * i;
        char *source = tmpbuf + sendsize * ((global_procn + global_rank - i) % global_procn);
        if (Memcpy_On)
            memcpy(target, source, sendsize);
    }
    free(tmpbuf);
    MPI_Barrier(MPI_COMM_WORLD);
}

int GLEXCOLL_Alltoall(void *sendbuf,
                      int sendsize,
                      void *recvbuf,
                      int recvsize,
                      MPI_Comm comm)
{
    switch (Alltoall_algorithm)
    {
    case DIRECT:
        /* code */
        DIRECT_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case BRUCK:
        BRUCK_RMA_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case BRUCK_RDMA:
        BRUCK_RDMA_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case DIRECT_NODE_AWARE:
        if (intra_procn != 1)
            DIRECT_NODE_AWARE_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case DIRECT_Kleader_NODE_AWARE:
        if (intra_procn != 1)
            DIRECT_Kleader_NODE_AWARE_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case DIRECT_Kleader_NODE_AWARE_RDMA:
        //if(global_rank == 0) printf("intra_procn = %d\n",intra_procn);
        if (intra_procn != 1)
            DIRECT_Kleader_NODE_AWARE_RDMA_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        else if (intra_procn == 1)
        {
            if (Num_of_ongoing_and_SelfAdapting_ON)
            {
                DIRECT_RDMA_NUM_ONGOING_Self_Adapting(sendbuf, sendsize, recvbuf, recvsize, comm);
            }
            else
            {
                //puts("check");
                DIRECT_One_corePNODE_AWARE_RDMA_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
            }
        }
        //DIRECT_One_slice_corePNODE_AWARE_RDMA_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE:
        if (inter_procn != 1)
            DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
        break;
    case XOR_EXCHANGE_RDMA_ONGOING:
        if (intra_procn == 1)
            XOR_EXCHANGE_RDMA_ONGOING_alltoall(sendbuf, sendsize, recvbuf, recvsize, comm);
    default:
        break;
    }
}

void GLEXCOLL_AlltoallFinalize()
{
    free(Bruck_buffer);
    if (intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
    {
        free(bufS);
        free(bufR);
        free(block_tmp);
    }
    //
    if (intra_rank == 0)
        free(RT_Collections);
    if (inter_procn > 1 && intra_rank % 4 == 0 && (intra_rank >> 2) < leaderN)
    {

        int ret = glex_deregister_mem(_GLEXCOLL.ep, _GLEXCOLL.local_mh);
        if (ret != GLEX_SUCCESS)
        {
            printf("_deregister_mem(), return: %d\n", ret);
            exit(1);
        }

        ret = glex_deregister_mem(_GLEXCOLL.ep, send_mh);
        if (ret != GLEX_SUCCESS)
        {
            printf("_deregister_mem(), return: %d\n", ret);
            exit(1);
        }
    }
}
