#!/bin/bash

# for((noden=$GLEX_COLL_NODEN_MIN;noden<=$GLEX_COLL_NODEN_MAX;noden*=2))
for((noden=$GLEX_COLL_NODEN_MIN;noden<=$GLEX_COLL_NODEN_MAX;noden+=50))
# for((noden=$GLEX_COLL_NODEN_MAX;noden>=$GLEX_COLL_NODEN_MIN;noden-=10))
do
    for((k=2;k<=16;k+=2))
    do
        for((loopn=0;loopn<5;loopn++))
        do 
            echo "------------------------------------noden=$noden  K=$k-------------------------------------------"
            yhrun -N $noden -n $noden ./build/test/$procName 13 $k
            wait
            sleep 1.5
        done
    done
done