#!/bin/bash
source_venv(){
    vname=$1
    vdp=$2
    [ "$vdp" == "" ] && vdp="."
    vp="${HOME}/.$1/venv"
    if [[ ! -d "$vp" ]];then
        virtualenv --no-site-packages $vp --python=python2.7
    fi
    source ${vp}/bin/activate
    pip install -r $vdp/requirements.txt  | grep -v 'equirement already satisfied'
}

