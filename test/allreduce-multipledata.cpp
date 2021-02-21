
#include <random>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <string>

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "glexcoll.h"
#include "glexallreduce.h"
}

using namespace std;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    GLEXCOLL_Init(argc, argv);
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&my_rank);
    CorePerNuma = 1;
    Childn_K = (atoi(argv[2]));
    _TreeID=0;
    GLEXCOLL_InitAllreduce();
    extern int SoftWare_Allreduce_Algorithm_Power_of_2;
    extern int allreduce_slice_num;
    //puts("lets start");
    int argv1 = (atoi(argv[1]));
    int size_max = 1 << argv1;
    double *sendbuf_MPI = (double *)malloc(size_max * sizeof(double));
    double *recvbuf_MPI = (double *)malloc(size_max * sizeof(double));
    double *sendbuf_GLEX = (double *)malloc(size_max * sizeof(double));
    double *recvbuf_GLEX = (double *)malloc(size_max * sizeof(double));

    static default_random_engine e;
    uniform_real_distribution<double> u0(-10, 10);
    double range = u0(e);
    for (int i = 0; i < size_max; i++)
    {
        sendbuf_MPI[i] = sendbuf_GLEX[i] = 1.0;//u0(e);
    }
    int loopN;
    for (int sz = 1; sz <= argv1; sz++)
    {
        if (sz <= 10)
            loopN = 10000;
        else
            loopN = 2000;
        MPI_Barrier(MPI_COMM_WORLD);
        int size = 1 << sz;
        {
            //测试mpi
            double reT = 0.0;
            for (int i = 0; i < loopN; i++)
            {
                double startT = MPI_Wtime();
                // MPI_Allreduce(sendbuf_MPI, recvbuf_MPI, size, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
                MPI_Reduce(sendbuf_MPI,recvbuf_MPI,size,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
                double endT = MPI_Wtime();
                reT += (endT - startT);
                MPI_Barrier(MPI_COMM_WORLD);
                MPI_Barrier(MPI_COMM_WORLD);
            }
            reT /= loopN;
            double re = 0.0;
            MPI_Reduce(&reT, &re, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
            if (global_rank == 0)
                printf("1<<%d\t %f \t", sz, re * 1e6);
        }

        SoftWare_Allreduce_Algorithm_Power_of_2 =K_nomial_tree_OMP;// Recursize_doubling_OMP; //
        {
            {
                //测试GLEX
                double reT = 0.0;
                for (int i = 0; i < loopN; i++)
                {
                    double startT = MPI_Wtime();
                    GLEXCOLL_Allreduce(sendbuf_GLEX, recvbuf_GLEX, size, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
                    //GLEXCOLL_Allreduce_NonOffload(sendbuf_GLEX, size, recvbuf_GLEX);
                    double endT = MPI_Wtime();
                    reT += (endT - startT);
                    MPI_Barrier(MPI_COMM_WORLD);
                    MPI_Barrier(MPI_COMM_WORLD);
                }
                reT /= loopN;
                double re = 0.0;
                MPI_Reduce(&reT, &re, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                if (global_rank == 0)
                    printf("%f ", re * 1e6);
            }
            if(my_rank == 0)
            {
//正确性检查
#pragma omp parallel for
                for (int i = 0; i < size_max; i++)
                {
                    if (abs(recvbuf_GLEX[i] - recvbuf_MPI[i]) > 0.001)
                    {
                        puts("check error a");
                        exit(0);
                    }
                }
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        SoftWare_Allreduce_Algorithm_Power_of_2 = K_nomial_tree_OMP;
        //for(allreduce_slice_num=(1<<10);allreduce_slice_num<=(1<<size_max);allreduce_slice_num*=2)
        extern int allreduce_k;
        //for(int t= 10;t<=argv1;t++)
        //for (allreduce_k = 2; allreduce_k <= 64; allreduce_k *= 2)
        if(0)
        {
            //allreduce_slice_num = (1<<t);
            MPI_Barrier(MPI_COMM_WORLD);
            {
                //测试GLEX
                double reT = 0.0;
                for (int i = 0; i < loopN; i++)
                {
                    double startT = MPI_Wtime();
                    GLEXCOLL_Allreduce(sendbuf_GLEX, recvbuf_GLEX, size, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
                    double endT = MPI_Wtime();
                    reT += (endT - startT);
                    MPI_Barrier(MPI_COMM_WORLD);
                    MPI_Barrier(MPI_COMM_WORLD);
                }
                reT /= loopN;
                double re = 0.0;
                MPI_Reduce(&reT, &re, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                if (global_rank == 0)
                    printf("%f ", re * 1e6);
            }
            {
//正确性检查
#pragma omp parallel for
                for (int i = 0; i < size_max; i++)
                {
                    if (abs(recvbuf_GLEX[i] - recvbuf_MPI[i]) > 0.001)
                    {
                        puts("check error b");
                        exit(0);
                    }
                }
            }
        }
        if (global_rank == 0)
            puts("");
    }

    free(sendbuf_MPI);
    free(recvbuf_MPI);
    free(sendbuf_GLEX);
    free(recvbuf_GLEX);
    GLEXCOLL_AllreduceFinalize();
    GLEXCOLL_Finalize();
    MPI_Finalize();
    return EXIT_SUCCESS;
}
