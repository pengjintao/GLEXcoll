#!/bin/sh
# for((leadn=6;leadn>=1;leadn-=1))
# do
#    for((ongoing=4;ongoing>=1;ongoing-=2))
#    do
#       echo "-------------------ongoing = 1<<$ongoing --leadn = $leadn----------------------------- "
#       #yhrun -N $GLEX_COLL_NODEN -n $GLEX_COLL_PROCN ./build/test/$procName $leadn $ongoing
#       yhrun -N $GLEX_COLL_NODEN -n $GLEX_COLL_NODEN ./build/test/$procName $leadn $ongoing
#       wait
#       sleep 1
#    done 
# done

# export  ALLTOALL_TYPE=DIRECT
# yhrun -N $GLEX_COLL_NODEN -n $GLEX_COLL_NODEN  ./build/test/$procName 6 32
# sleep 2
# export  ALLTOALL_TYPE=DIRECT_Kleader_NODE_AWARE_RDMA
# yhrun -N $GLEX_COLL_NODEN -n $GLEX_COLL_NODEN  ./build/test/$procName 6 32

GLEX_COLL_NODEN_MAX=$GLEX_COLL_NODEN 
GLEX_COLL_NODEN_MIN=2

for((noden=$GLEX_COLL_NODEN_MIN;noden<=$GLEX_COLL_NODEN_MAX;noden*=2))
do
    echo "------------node $noden msg_size=[0,1,2,...,20]*double ongoing=[1,2,4,8,16,32]------------"
    # export  ALLTOALL_TYPE=DIRECT
    # yhrun -N $noden -n $noden  ./build/test/$procName 6 32
    # sleep 1.5
    export  ALLTOALL_TYPE=DIRECT_Kleader_NODE_AWARE_RDMA
    yhrun -N $noden -n $noden  ./build/test/$procName 6 32
    sleep 1.5
done
