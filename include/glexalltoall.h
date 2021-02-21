#ifndef GLEX_ALLTOALL_H
#define GLEX_ALLTOALL_H
#include <stdio.h>
#include <mpi.h>


#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS

//alltoall算法选择变量，通过环境变量 ALLTOALL_TYPE 导入
enum ALGORITHM {BRUCK,BRUCK_RDMA,
                DIRECT,
                DIRECT_NODE_AWARE,
                DIRECT_Kleader_NODE_AWARE,
                DIRECT_Kleader_NODE_AWARE_RDMA,
                DIRECT_Kleader_NODE_AWARE_RDMA_PIPELINE,
                XOR_EXCHANGE_RDMA_ONGOING};
extern int Alltoall_algorithm;

void glexcoll_InitAlltoall();
int GLEXCOLL_Alltoall(void *sendbuf,
                int sendsize,
                void *recvbuf, 
                int recvsize, 
                MPI_Comm comm);
                
void GLEXCOLL_AlltoallFinalize();

//运行非MPI_Comm_world的alltoallv
//目前支持没节点单个进程
void GLEXCOLL_InitAlltoallV(MPI_Comm comm);
void  GLEXCOLL_AlltoallVFinalize();
void GLEXCOLL_Alltoallv(void *sendbuf, 
    uint64_t sendcounts[],
    uint64_t sdispls[],
    void *recvbuf, 
    uint64_t recvcounts[],
    uint64_t rdispls[]);
#endif

enum AlltoallvAlgorithmTYPE
{
    LINEAR_SHIFT,
    XOR,
    OPT_BANDWIDTH,
    LINEAR_SHIFT_REORDER,
    RDMA_LINEAR_SHIFT
};

extern void write_out_print_alltoallv_performance_data(int loopn,double *Step_max_sum);
extern void GLEXCOLL_Alltollv_RDMA(int mh_pair, void *sendbuf,void *recvbuf,uint64_t sendcounts[],uint64_t sdispls[]);
void GLEXCOLL_Alltoallv_regist_recv_disps(uint64_t * rdispls);
END_C_DECLS