project='dcpots'
version='0.1.1'
debug = 1 
defs = []
verbose = 'on'
extra_c_flags = '-wno-unused-parameter'
extra_cxx_flags = '--std=c++11 -lpthread -lrt -ldl'
env={
'protoc':'protoc',
'protoi':'/usr/local/include',
'protol':'/usr/local/lib',
}
units = [{
            'name':'dcbase',
            'subdir':'base',
        },
        {
            'name': 'pbjson',
            'subdir': '3rd/pbjson',
            'incs': ['3rd'],
        },
        {
            'name': 'dcutil-redis',
            'subdir': 'utility/redis',
            'incs': ['3rd'],
        },
        {
            'name': 'dcrpc',
            'subdir': 'dcrpc',
            'incs': ['{{protoi}}'],
            'dsrcs': ['{{cdir}}/client/','{{cdir}}/server/','{{cdir}}/share/'],
            'srcs': ['share/dcrpc.pb.cc'],
            'objs': [{
                'out':'{{cdir}}/share/dcrpc.pb.cc',
                'dep':'{{cdir}}/proto/dcrpc.proto',
                'cmd':'{{protoc}} {{cdir}}/proto/dcrpc.proto -I{{cdir}}/proto --cpp_out={{cdir}}/share'
            }]
        },
        {
            'name':'dcutil-mysql',
            'subdir':'utility/mysql',
            'incs':['/usr/local/include'],
        },
        {
            'name':'dcutil-script',
            'subdir':'utility/script',
            'incs':['/usr/local/include'],
        },
        {
            'name':'dcutil-drs',
            'subdir':'utility/drs',
            'incs':['3rd','{{protoi}}'],
            'srcs': ['extensions.pb.cc'],
            'objs': [{
                'out':'{{cdir}}/extensions.pb.cc',
                'dep':'{{cdir}}/extensions.proto',
                'cmd':'{{protoc}} {{cdir}}/dcxconfig.proto {{cdir}}/extensions.proto -I{{cdir}}/ -I{{protoi}} -I/usr/include -I/usr/local/include --cpp_out={{cdir}}/'
            }]
        },
        {
            'name':'dcutil-crypt',
            'subdir':'utility/crypt',
        },
]
