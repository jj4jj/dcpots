#@/bin/bash
build()
{
    cd tools/cmaketools/
    python generate.py ../../cmake_conf
    cd -
    make -C dcnode/proto/
    #cd -
    make -C dagent/proto/
    cd build && cmake ../ && make
    cd -
}
###########################
clean()
{
    find . -name *.pyc | rm
}

list()
{
    echo "usage:./automake.sh <option>  "
    echo "option as follow:"
    echo -e "\trebuild"
    echo -e "\tbuild"
    echo -e "\tclean"
    echo -e "\tlist"
}

case $1 in
    build)
        cd build && make
        ;;
    rebuild)
        build
        ;;
    clean)
        clean
        ;;
    *)
        list
esac
