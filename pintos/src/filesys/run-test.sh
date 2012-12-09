#!/bin/bash
NUM_TIMES=$1
TIME_STAMP=$(date "+%y-%m-%d_%H-%M-%S")

for (( i=1; i<=$NUM_TIMES; i++ ))
do
  file=build_"$TIME_STAMP"_"$i".log
  make check 2>&1 > $file
done