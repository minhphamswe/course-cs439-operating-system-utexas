#!/usr/bin/env bash
path=$(cd ./bochs-2.2.6-pintos/bin; pwd)
echo "Adding to \$PATH "$path
export PATH=$path:$PATH

path=$(cd ./pintos; pwd)
echo "Adding to \$PATH "$path
export PATH=$path:$PATH

