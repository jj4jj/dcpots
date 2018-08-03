#!/bin/bash

src_dir=$(cd $(dirname $0);pwd)
build_dir="${src_dir}/../build"

build(){
    cd ${build_dir} && cmake ${src_dir}
    cd -
}

rebuild(){
    mkdir -p ${build_dir} 
    cmdir="{{autocmake_dir}}"
    {{python}} $cmdir/autocmake.py {{cmake_conf}} $@
    build
}
###########################
clean(){
    echo "clean files ...(tips)"
    find . -name *.pyc
    find . -name *.pyc | xargs rm -f
    find . -name '*.pb.cc'
    find . -name '*.pb.h'
    echo "make clean ..."
    cd ${build_dir} && make clean
}

inst_all(){
    cd ${build_dir} && make install
}

list(){
    echo "usage:$1 <option> [autocmake.py tools dir] "
    echo "option as follow:"
    echo -e "\trebuild"
    echo -e "\tbuild"
    echo -e "\tclean"
    echo -e "\tlist"
    echo -e "\tinstall"
}

if [[ ! -d ${build_dir} ]];then
    echo "init project compiling ..."
    rebuild
fi

case $1 in
    build)
        build
        ;;
    rebuild)
        rebuild ${@:2:9}
        ;;
    clean)
        clean
        ;;
    install)
        inst_all
        ;;
    *)
        list
esac
