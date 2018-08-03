#!/bin/bash
cdir="$(cd $(dirname $0); pwd)"
echo "tools initenv dir:$cdir "
sudo apt-get install python-virtualenv libmysqlclient-dev libtool automake libprotobuf-dev libev-dev -y
source ./vpyenv.sh
source_venv dcpots 
###############################################################################
dcpots_dir="$(cd ${cdir}/..; pwd)"
set -x
##init env
cd ${dcpots_dir} && python tools/autocmake/autocmake.py ./cmake_conf.py
#generate cmake
mkdir -p ${dcpots_dir}/build 
cd ${dcpots_dir}/build && cmake ${dcpots_dir}

