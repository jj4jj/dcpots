#!/bin/python
#-*- coding:utf-8 -*-
import os
import sys
from jinja2 import Template 
cdr=os.path.dirname(__file__)
pctf=os.path.join(cdr,'project.cmake')
ptf=open(pctf).read(1024*1024)
pct = Template(ptf.decode('utf8')) 
##
uctf=os.path.join(cdr,'unit.cmake')
utf=open(uctf).read(1024*1024)
uct = Template(utf.decode('utf8')) 

##
bctf=os.path.join(cdr,'autobuild.sh')
btf=open(bctf).read(1024*1024)
bct = Template(btf.decode('utf8')) 

def render(f,t,e):
    print 'render file:%s ...'%(f,)
    open(f,'w').write(Template(Template(t.render(e)).render(e)).render(e).encode('utf8'))

def run(wdr, config, mdf, cenv):
    prjc='/'.join((wdr,'CMakeLists.txt'))
    keys=filter(lambda xk:xk[0:2] != '__', dir(config))
    #print dir(config)
    env={}
    env.update({'autocmake_dir':cdr,"cmake_conf":os.path.join(wdr,mdf)})
    env['python'] = 'python'
    ########################################
    env['cdir']='${CMAKE_CURRENT_SOURCE_DIR}'
    env['root']='${PROJECT_SOURCE_DIR}'
    #reserve
    for k in keys:
        if k != 'env':
            env[k]=getattr(config,k)
        else:
            fmte=getattr(config,k)
            env.update(fmte)
    ###command env############################
    env.update(cenv)
    ##########################################
    #project
    render(prjc, pct, env)

    #units
    units = getattr(config,'units',[])
    for unit in units:
        env['unit']=unit
        render('/'.join((wdr, unit['subdir'], 'CMakeLists.txt')) ,uct, env)
    #######################################################################
    #autobuild
    autobuild='/'.join((wdr,'autobuild.sh'))
    render(autobuild, bct, env)

    
if __name__ == '__main__':
    wdr='.'
    mdf='cmake_conf.py'
    cenv = {}
    if len(sys.argv) >= 2:
        if os.path.isfile(sys.argv[1]):
            wdr=os.path.dirname(sys.argv[1])
            mdf=os.path.basename(sys.argv[1])
        else:
            wdr=sys.argv[1]
        if len(sys.argv) >= 3:
            for kiv in sys.argv[2:]:
                kivk , kivv = tuple(kiv.split('='))
                cenv[kivk]=kivv
            
    print sys.argv,wdr,mdf,cenv
    sys.path.append(wdr)
    mdc=__import__(mdf.split('.')[0])
    run(wdr, mdc, mdf, cenv)

