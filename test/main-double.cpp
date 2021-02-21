
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

double fun(int calc)
{
    if (calc == 0)
    {
        return 0.0;
    }
    else
    {
        int n = 1 << calc;
        int i = 0;
        double var = 0.0;
        for (i = 0; i < n; i += 1)
        {
            if (i % 2 == 0)
                var = sin(var);
            else
                var = cos(var);
        }
        return var;
    }
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    //puts("lets start");

    int calc = atoi(argv[1]);
    int sizeV = 1 << atoi(argv[2]);
    int testmode = 3; //1 GLEXCOLL, 2 MPICH, 3 GLEXCOLL+MPICH
    if (argc >= 4)
        CorePerNuma = 1 << atoi(argv[3]);
    else
    {
        CorePerNuma = 16;
    }
    if (CorePerNuma <= 0 || CorePerNuma > 32)
    {
        puts("CorePerNuma value error");
        exit(0);
    }
    if (argc >= 5)
        Childn_K = atoi(argv[4]);
    else
        Childn_K = 2;

    if (Childn_K <= 0 || Childn_K > 15)
    {
        puts("Childn_K value error");
        exit(0);
    }
    // if(argc >= 6)
    // {
    //     testmode = atoi(argv[5]);

    // }
    GLEXCOLL_Init(argc, argv);
    GLEXCOLL_InitAllreduce();
    //puts("check 74");
    // MPI_Finalize();
    // return EXIT_SUCCESS;
    //printf("CorePerNuma =%d\n ",CorePerNuma);
    //puts("GLEXCOLL_Init finished");
    // Get the size of the communicator
    int size = 0;
    MPI_Request req;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // if(size != 4)
    // {
    //     printf("This application is meant to be run with 4 MPI processes.\n");
    //     MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    // }
    // Get my rank
    int my_rank;
    int nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    srand(my_rank);
    // if(my_rank == 0)
    //     printf("CorePerNuma = %d\n",CorePerNuma);
    double *source = (double *)malloc(sizeof(double) * sizeV);
    double *source1 = (double *)malloc(sizeof(double) * sizeV);

    //printf("sizeof(Payload) = %d\n",sizeof(struct Payload)); 64

    //int sources[sizeV];
    //随机数产生，
    static default_random_engine e;
    uniform_real_distribution<double> u0(-0.01, 0.01);
    double range = u0(e);
    uniform_real_distribution<double> u1(-range, range);
    uniform_int_distribution<unsigned> uinit(0, 1000);

    int i;
    int loopN_GLEX = 1000;
    int loopN_MPI = 1000;
    double *targets = (double *)malloc(sizeof(*targets) * sizeV);
    double *targets1 = (double *)malloc(sizeof(*targets1) * sizeV);

    //int targets[sizeV];
    // Each MPI process sends its rank to reduction, root MPI process collects the result
    //warmup
    cout.precision(30); //设置精度
    int warmupN = 10;

    MPI_Barrier(MPI_COMM_WORLD);
    // GLEXCOLL_Iallreduce(source1, targets1, sizeV,MPI_DOUBLE,MPI_SUM);
    // GLEXCOLL_Wait_Iallreduce();
    // puts("check 122");
    // GLEXCOLL_AllreduceFinalize();
    // GLEXCOLL_Finalize();
    // MPI_Finalize();

    for (int loop = 0; loop < warmupN; loop++)
    {
        for (i = 0; i < sizeV; i++)
        {
            source[i] = source1[i] = (loop % 2 + 1.0) / 32.0;
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if ((testmode & 2) == 2)
        {
            // if(global_rank == 0)
            //     puts("check mpi");
            MPI_Iallreduce(source, targets, sizeV, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, &status);
        }
        if ((testmode & 1) == 1)
        {
            // if(global_rank == 0)
            //     puts("check");
            {

                //INTRA_ALLREDUCE_TYPE = 0;
                //printf("TreeNumber = %d\n",TreeNumber);
                //MPI_Barrier(MPI_COMM_WORLD);
                _TreeID = 0;
                inter_comm_mode = OFFLOAD;
                //usleep(uinit(e));
                GLEXCOLL_Iallreduce(source1, targets1, sizeV, MPI_DOUBLE, MPI_SUM);
                //usleep(uinit(e));
                GLEXCOLL_Wait_Iallreduce();

                // if(my_rank == 0)
                // printf("rank%d=%lf\n",my_rank,*targets1);
                //puts("check 145");
                //GLEXCOLL_Allreduce(source1,targets1,sizeV,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
                //MPI_Barrier(MPI_COMM_WORLD);
                //usleep(1000);
                // if(my_rank == 0)
                // {
                //     printf("check _TreeID = %d\n",_TreeID);
                // }
                // if(my_rank == 9)
                // {
                //     cout<<"GLEX:\t"<<targets1[0]<<'\t'<<"MPI:\t"<<targets[0]<<endl;
                // }
            }
        }
        // if(my_rank == 0)
        //          cout<<"GLEX:\t"<<targets1[0]<<'\t'<<"MPI:\t"<<targets[0]<<endl;
        //printf("%3.30f \t %lf relative error = %20e\n",*targets,*targets1,2.0*(*targets1 - *targets)/(*targets1 + *targets));
    }
    //printf("rank%d=%lf\n",my_rank,*targets1);

    MPI_Barrier(MPI_COMM_WORLD);
    if ((testmode & 2) == 2)
    {
        //测试MPI_IAllreduce
        double time_start1 = MPI_Wtime();
        for (int i = 0; i < loopN_MPI; i++)
        {
            //MPI_Allreduce(source, targets, sizeV, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            //if(my_rank == 159){ printf("i=%d\n",i);usleep(200);}
            MPI_Iallreduce(source, targets, sizeV, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD, &req);
            //fun(calc);
            MPI_Wait(&req, &status);
        }
        double time_end1 = MPI_Wtime();

        double time_local1 = (time_end1 - time_start1) / loopN_MPI;
        double time_global1 = 0.0;
        MPI_Reduce(&time_local1, &time_global1, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (my_rank == 0)
        {
            printf("%f\t", time_global1 * 1e6);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if ((testmode & 1) == 1)
    {
        //for(INTRA_ALLREDUCE_TYPE = 3;INTRA_ALLREDUCE_TYPE<=6;INTRA_ALLREDUCE_TYPE+=3)
        {
            inter_comm_mode = OFFLOAD; //测试non offload,NON_OFFLOAD
            MPI_Barrier(MPI_COMM_WORLD);
            //GLEXCOLL_Iallreduce
            for (int treeid = 0; treeid < 1 /*TreeNumber*/; treeid++)
            {
                _TreeID = 0;
                inter_comm_mode = OFFLOAD;
                double time_start = MPI_Wtime();
                for (int i = 0; i < loopN_GLEX; i++)
                {
                    for (int j = 0; j < sizeV; j++)
                    {
                        source[j] = source1[j] = (i % 2 + 1.0) / 32.0;
                    }
                    //usleep(uinit(e));
                    GLEXCOLL_Iallreduce(source1, targets1, sizeV, MPI_DOUBLE, MPI_SUM);
                    //usleep(uinit(e));
                    //fun(calc);
                    GLEXCOLL_Wait_Iallreduce();
                    // if(i % 10000 == 0)
                }
                double time_end = MPI_Wtime();

                double time_local = (time_end - time_start) / loopN_GLEX;
                double time_global = 0.0;
                MPI_Reduce(&time_local, &time_global, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                if (my_rank == 0)
                {
                    printf("%f\t", time_global * 1e6);
                }
                MPI_Barrier(MPI_COMM_WORLD);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }
    //puts("");
    //puts("check 245");
    MPI_Barrier(MPI_COMM_WORLD);
    //puts("check 245");

    GLEXCOLL_AllreduceFinalize();
    GLEXCOLL_Finalize();
    MPI_Finalize();
    return EXIT_SUCCESS;
}
