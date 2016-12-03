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
def render(f,t,e):
    print 'render file:%s ...'%f
    open(f,'w').write(t.render(e).encode('utf8'))

def run(wdr, config):
    prjc='/'.join((wdr,'CMakeLists.txt'))
    keys=filter(lambda xk:xk[0:2] != '__', dir(config))
    envs={}
    for k in keys:
        if k != 'envs':
            envs[k]=getattr(config,k)
        else:
            envs.update(getattr(config,k))
    render(prjc, pct, envs)
    units = getattr(config,'units',[])
    for unit in units:
        envs['unit']=unit
        render('/'.join((wdr, unit['subdir'], 'CMakeLists.txt')),uct,envs)
    
if __name__ == '__main__':
    wdr='.'
    mdf='cmake_conf.py'
    if len(sys.argv) == 2:
        if os.path.isfile(sys.argv[1]):
            wdr=os.path.dirname(sys.argv[1])
            mdf=os.path.basename(sys.argv[1])
        else:
            wdr=sys.argv[1]
    sys.path.append(wdr)
    mdc=__import__(mdf.split('.')[0])
    run(wdr, mdc)
