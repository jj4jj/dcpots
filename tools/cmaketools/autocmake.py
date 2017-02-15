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
def render(f,t,e,fe):
    print 'render file:%s ...'%f
    open(f,'w').write(Template(t.render(e)).render(fe).encode('utf8'))

def run(wdr, config):
    prjc='/'.join((wdr,'CMakeLists.txt'))
    keys=filter(lambda xk:xk[0:2] != '__', dir(config))
    env={}
    fmte={}
    for k in keys:
        if k != 'env':
            env[k]=getattr(config,k)
        else:
            fmte=getattr(config,k)
            env['env']=fmte
    #reserve
    fmte['cdir']='${CMAKE_CURRENT_SOURCE_DIR}'
    fmte['root']='${PROJECT_SOURCE_DIR}'
    ##########################################
    render(prjc, pct, env, fmte)
    units = getattr(config,'units',[])
    for unit in units:
        env['unit']=unit
        #print str(unit.get('objs',None))
        render('/'.join((wdr, unit['subdir'], 'CMakeLists.txt')) ,uct,env, fmte)
    
if __name__ == '__main__':
    wdr='.'
    mdf='cmake_conf.py'
    if len(sys.argv) == 2:
        if os.path.isfile(sys.argv[1]):
            wdr=os.path.dirname(sys.argv[1])
            mdf=os.path.basename(sys.argv[1])
        else:
            wdr=sys.argv[1]
            if not os.path.isdir(wdr):
                mdf=wdr
                wdr='.'
                if mdf.find('.') == -1:
                   mdf += '.py' 
    print 'generating CMakeLists with [dir=%s config=%s] ...' % (wdr, mdf)
    sys.path.append(wdr)
    mdc=__import__(mdf.split('.')[0])
    run(wdr, mdc)
    autogen=wdr+'/'+'autogen.sh'
    if not os.path.exists(autogen):
        at=Template(open(os.path.join(cdr,'autogen.sh')).read(1024*1024))
        render(autogen, at, {'autocmake_dir':cdr,'autocmake_conf':os.path.join(wdr,mdf)},{})

