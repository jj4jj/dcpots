PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 1    #0/1
DEFS = []
VERBOSE = 'on'    #on/off
EXTRA_C_FLAGS = '-Wno-unused-parameter'
EXTRA_CXX_FLAGS = '--std=c++11'
LIBS = [
        {
            'name':'dcbase',
            'subdir':'base',
            'linklibs' : [],
            'includes':[],
            'src_dirs':['base'],
        },
        {
            'name':'dcnode',
            'subdir':'dcnode',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0','3rd'],
            'src_dirs':['dcnode/proto','3rd/pbjson'],
            'genobj': {
                'out':'${CMAKE_CURRENT_SOURCE_DIR}/proto/dcnode.pb.cc',
                'dep':'${CMAKE_CURRENT_SOURCE_DIR}/proto/dcnode.proto',
                'cmd':'protoc ${CMAKE_CURRENT_SOURCE_DIR}/proto/dcnode.proto -I${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/proto'
            }
        },
        {
            'name':'dcagent',
            'subdir':'dcagent',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0','3rd'],
            'src_dirs':['dcagent/proto'],
            'genobj': {
                'out':'${CMAKE_CURRENT_SOURCE_DIR}/proto/dcagent.pb.cc',
                'dep':'${CMAKE_CURRENT_SOURCE_DIR}/proto/dcagent.proto',
                'cmd':'protoc ${CMAKE_CURRENT_SOURCE_DIR}/proto/dcagent.proto -I${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/proto'
            }
        },
        {
            'name':'mongoproxyapi',
            'subdir':'app/mongoproxy/api',
            'includes':['/usr/local/include/libmongoc-1.0','3rd'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': ['app/mongoproxy/proto'],
            'genobj': {
                'out':'${CMAKE_CURRENT_SOURCE_DIR}/../proto/mongo.pb.cc',
                'dep':'${CMAKE_CURRENT_SOURCE_DIR}/../proto/mongo.proto',
                'cmd':'protoc ${CMAKE_CURRENT_SOURCE_DIR}/proto/mongo.proto -I${CMAKE_CURRENT_SOURCE_DIR}/../proto --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/../proto'
            }
        },
        {
            'name': 'dcrepoter',
            'subdir': 'reporter',
            'includes': ['base','dcnode','dcagent','/usr/local/include/libmongoc-1.0','3rd'],
        },
        {
            'name': 'pbjson',
            'subdir': '3rd/pbjson/',
            'includes': ['3rd'],
        },
        {
            'name': 'dcutil-redis',
            'subdir': 'utility/redis',
            'includes': ['3rd'],
        },
        {
            'name': 'dcrpc',
            'subdir': 'dcrpc',
            'includes': [],
            'src_dirs': ['./dcrpc/client/','./dcrpc/server/','./dcrpc/share/','./dcrpc/share/dcrpc.pb.cc'],
            'genobj': {
                'out':'${CMAKE_CURRENT_SOURCE_DIR}/share/dcrpc.pb.cc',
                'dep':'${CMAKE_CURRENT_SOURCE_DIR}/proto/dcrpc.proto',
                'cmd':'protoc ${CMAKE_CURRENT_SOURCE_DIR}/proto/dcrpc.proto -I${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/share'
            }
        },
        {
            'name':'dcutil-mysql',
            'subdir':'utility/mysql',
            'linklibs' : [],
            'includes':['/usr/local/include'],
        },
        {
            'name':'dcutil-script',
            'subdir':'utility/script',
            'linklibs' : [],
            'includes':['/usr/local/include'],
        },
        {
            'name':'dcutil-mongo',
            'subdir':'utility/mongo',
            'linklibs' : [],
            'includes':['/usr/local/include/libbson-1.0','3rd'],
        },
        {
            'name':'dcutil-drs',
            'subdir':'utility/drs',
            'linklibs' : [],
            'includes':['/usr/local/include','3rd'],
        },
        {
            'name':'dcutil-crypt',
            'subdir':'utility/crypt',
            'linklibs' : [],
            'includes':[],
            'src_dirs':[],
            'extra_srcs': [],
        },

]
EXES = [
        {
            'name':'dctest',
            'subdir':'app/test',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0'],
            'linkpaths':['/usr/local/lib'],
            'linklibs' : [
                'dcnode',
                'dcagent',
                'dcutil-drs',
                'dcutil-mysql',
                'dcutil-mongo',
                'dcutil-script',
                'dcbase',
                'pbjson',
                'python2.7',
                'libprotobuf.a',
                'mysqlclient',
                'mongoc-1.0',
                'bson-1.0',
            ],
            'genobj': {
                'out':'${CMAKE_CURRENT_SOURCE_DIR}/test_conf.pb.cc',
                'dep':'${CMAKE_CURRENT_SOURCE_DIR}/test_conf.proto',
                'cmd':'protoc ${CMAKE_CURRENT_SOURCE_DIR}/test_conf.proto -I${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}'
            }
        },
        {
            'name':'mongoproxy',
            'subdir':'app/mongoproxy',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': ['app/mongoproxy/proto'],
            'linklibs' : [
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
            'subdir':'app/mongoproxy/testapi',
            'includes':['/usr/local/include'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': [],
            'linklibs' : [
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
            'name':'reporter',
            'subdir':'app/reporter',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0'],
            'linklibs' : [
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
            'subdir':'app/collector',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0'],
            'linklibs' : [
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
            'subdir':'app/pingpong',
            'includes':['/usr/local/include','/usr/local/include/libmongoc-1.0'],
            'linklibs' : [
                'dcnode',
                'dcutil-drs',
                'dcbase',
                'pbjson',
                'protobuf',
            ]
        },
]
