#!/bin/bash

build(){
    cd build && cmake ../ && make -j 6
    cd -
}

rebuild(){
    mkdir -p build
	cmdir=$1
	if [ -z "$1" ] cmdir={{autocmake_dir}}
    python $cmdir/autocmake.py
    cd build && cmake ../ && make
    cd -
}
###########################
clean(){
    echo "clean files ..."
    find . -name *.pyc
    find . -name *.pyc | xargs rm -f
    find . -name '*.pb.cc'
    find . -name '*.pb.cc' | xargs rm -f
    find . -name '*.pb.h'
    find . -name '*.pb.h' | xargs rm -f
    echo "make clean"
    cd build && make clean
    echo "clean shm and msg queue"
}

inst_all(){
    cd build && make install
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

if [[ ! -d build ]];then
    echo "init project compiling ..."
    rebuild
fi

case $1 in
    build)
        build
        ;;
    rebuild)
        rebuild $2
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
