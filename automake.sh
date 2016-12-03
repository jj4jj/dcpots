#!/bin/bash

build()
{
    make -C dcnode/proto/
    #cd -
    make -C dagent/proto/
    make -C app/mongoproxy/
    cd build && cmake ../ && make -j 6
    cd -
}

rebuild()
{
    mkdir -p build
    python tools/cmaketools/ptcmake.py
    make -C dcnode/proto/
    #cd -
    make -C dagent/proto/
    cd build && cmake ../ && make
    cd -
}
###########################
clean()
{
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
    sh tools/clean.sh
}

inst_all()
{
    cd build && make install
}

run_test()
{
    echo "todo"
    cd bin && ./dcagent test
}

list()
{
    echo "usage:./automake.sh <option>  "
    echo "option as follow:"
    echo -e "\trebuild"
    echo -e "\tbuild"
    echo -e "\tclean"
    echo -e "\tlist"
    echo -e "\tinstall"
    echo -e "\ttest"
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
        rebuild
        ;;
    clean)
        clean
        ;;
    install)
        inst_all
        ;;
    test)
        run_test
        ;;
    *)
        list
esac
