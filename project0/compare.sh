#!/bin/bash

for file in $(find ./ -name "trace*.txt"); do
    echo "Tracing "$file
    ./sdriver.pl -t $file -s ./msh > msh-trace.out -a "-p"
    ./sdriver.pl -t $file -s ./mshref > mshref-trace.out -a "-p"
    diff -y -a msh-trace.out mshref-trace.out
    echo "====================================================================="
done | tee trace-diff.out