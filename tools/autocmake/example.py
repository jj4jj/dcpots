import os
project='u2'
version='0.1.1'
if os.path.exists('.debug.dev'):
    debug = 1 
else:
    debug = 0 
defs = []
cos= ['-Wno-unused-parameter']
incs= ['/usr/include','/usr/local/include','{{root}}/3rd/dcpots/3rd','{{root}}/3rd',
       '{{protoi}}','{{root}}/cspb/gen','{{root}}/base','{{root}}/comm',
       '{{root}}/comm/gen','{{mysqli}}', '{{root}}/3rd/dcpots/3rd/lua53/src']
links = ['{{protol}}','{{root}}/3rd/dcpots/3rd/lua53/lib']
libs = ['mysqlclient','protobuf','pthread','rt','dl']
verbose = 'on'
extra_c_flags = ''
extra_cxx_flags = '--std=c++11'
env={
    'protoc':'{{root}}/3rd/protobuf-2.6.1/bin/protoc',
    'protoi':'{{root}}/3rd/protobuf-2.6.1/include',
    'protol':'{{root}}/3rd/protobuf-2.6.1/lib',
    'mysqli':'/usr/include/mysql',
    'python':'~/.gsdev/venv/bin/python',
}

units =[
        {
            'name':'dcbase',
            'subdir':'3rd/dcpots/base',
        },
        {
            'name':'dcmysql',
            'subdir':'3rd/dcpots/utility/mysql',
            'incs':['3rd/dcpots'],
        },
        {
            'name':'dcredis',
            'subdir':'3rd/dcpots/utility/redis',
            'incs':['3rd/dcpots'],
        },
        {
            'name':'dccrypt',
            'subdir':'3rd/dcpots/utility/crypt',
            'incs':['3rd/dcpots'],
        },
        {
            'name':'pbjson',
            'subdir':'3rd/dcpots/3rd/pbjson',
            'incs':['{{protoi}}'],
        },

]

