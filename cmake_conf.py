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
            'name':'dcnode',
            'subdir':'dcnode',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','3rd','{{protoi}}'],
            'dsrcs':['dcnode/proto','3rd/pbjson'],
            'srcs':['proto/dcnode.pb.cc'],
            'objs': [{
                'out':'{{cdir}}/proto/dcnode.pb.cc',
                'dep':'{{cdir}}/proto/dcnode.proto',
                'cmd':'{{protoc}} {{cdir}}/proto/dcnode.proto -I{{cdir}}/proto --cpp_out={{cdir}}/proto'
            }]
        },
        {
            'name':'dcagent',
            'subdir':'dcagent',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','3rd'],
            'dsrcs':['dcagent/proto'],
            'objs': [{
                'out':'{{cdir}}/proto/dcagent.pb.cc',
                'dep':'{{cdir}}/proto/dcagent.proto',
                'cmd':'{{protoc}} {{cdir}}/proto/dcagent.proto -I{{cdir}}/proto --cpp_out={{cdir}}/proto'
            }]
        },
        {
            'name':'mongoproxyapi',
            'subdir':'app/mongoproxy/api',
            'incs':['/usr/local/include/libmongoc-1.0','3rd','{{root}}','{{protoi}}'],
            'lincs':['/usr/local/lib'],
            'dsrcs': ['app/mongoproxy/proto'],
            'srcs':['../proto/mongo.pb.cc'],
            'objs': [{
                'out':'{{cdir}}/../proto/mongo.pb.cc',
                'dep':'{{cdir}}/../proto/mongo.proto',
                'cmd':'{{protoc}} {{cdir}}/../proto/mongo.proto -I{{cdir}}/../proto --cpp_out={{cdir}}/../proto'
            }]
        },
        {
            'name': 'dcrepoter',
            'subdir': 'reporter',
            'incs': ['base','dcnode','dcagent','/usr/local/include/libmongoc-1.0','3rd'],
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
            'name':'dcutil-mongo',
            'subdir':'utility/mongo',
            'incs':['/usr/local/include/libbson-1.0','3rd','/usr/include'],
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
        {
            'name':'dctest',
            'subdir':'app/test',
            'type':'exe',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','{{root}}','{{protoi}}'],
            'lincs':['/usr/local/lib','{{protol}}'],
            'srcs':['test_conf.pb.cc'],
            'libs' : [
                'dcagent',
                'dcnode',
                'dcutil-drs',
                'dcutil-mysql',
                'dcutil-mongo',
                'dcutil-script',
                'dcutil-crypt',
                'dcbase',
                'pbjson',
                'python2.7',
                'libprotobuf.a',
                'mysqlclient',
                'mongoc-1.0',
                'bson-1.0',
                'ssl',
                'crypto',
            ],
            'objs': [{
                'out':'{{cdir}}/test_conf.pb.cc',
                'dep':'{{cdir}}/test_conf.proto',
                'cmd':'{{protoc}} {{cdir}}/test_conf.proto -I{{cdir}} --cpp_out={{cdir}}'
            }]
        },
        {
            'name':'mongoproxy',
            'type':'exe',
            'subdir':'app/mongoproxy',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','{{root}}','{{protoi}}'],
            'lincs':['/usr/local/lib','{{protol}}'],
            'dsrcs': ['app/mongoproxy/proto'],
            'libs' : [
                'mongoproxyapi',
                'dcnode',
                'dcagent',
                'dcutil-mongo',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'python2.7',
                'libprotobuf.a',
                'mongoc-1.0',
                'bson-1.0'
            ]
        },
        {
            'name':'mongoproxy_testapi',
            'type':'exe',
            'subdir':'app/mongoproxy/testapi',
            'incs':['/usr/local/include','{{root}}'],
            'lincs':['/usr/local/lib','{{protol}}'],
            'dsrcs': [],
            'libs' : [
                'mongoproxyapi',
                'dcnode',
                'dcagent',
                'dcutil-mongo',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'python2.7',
                'libprotobuf.a',
                'mongoc-1.0',
                'bson-1.0'
            ],
            'objs': [{
                'out':'{{cdir}}/test.pb.cc',
                'dep':'{{cdir}}/test.proto',
                'cmd':'{{protoc}} {{cdir}}/test.proto -I{{cdir}} --cpp_out={{cdir}}'
            }]
        },
        {
            'name':'reporter',
            'type':'exe',
            'subdir':'app/reporter',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','{{root}}','{{protoi}}'],
            'lincs': ['{{protol}}'],
            'libs' : [
                'dcrepoter',
                'dcagent',
                'dcnode',
                'dcutil-script',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'collector',
            'type':'exe',
            'subdir':'app/collector',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','{{root}}','{{protoi}}'],
            'lincs':['{{protol}}'],
            'libs' : [
                'dcrepoter',
                'dcagent',
                'dcnode',
                'dcutil-script',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'pingpong',
            'type':'exe',
            'subdir':'app/pingpong',
            'incs':['/usr/local/include','/usr/local/include/libmongoc-1.0','{{root}}'],
            'libs' : [
                'dcnode',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'protobuf',
            ]
        },
]
