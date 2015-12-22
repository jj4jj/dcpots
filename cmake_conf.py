PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 1    #0/1
DEFS = []
VERBOSE = 'off'    #on/off
EXTRA_C_FLAGS = ''
EXTRA_CXX_FLAGS = '-std=c++11'
EXTRA_LD_FLAGS = '-ldl -lm -lrt -pthread'
LIBS = [
        {
            'name':'dcbase',
            'subdir':'base',
            'linklibs' : [],
            'includes':[],
            'src_dirs':['base'],
            'extra_srcs': [''],
        },
        {
            'name':'dcutil',
            'subdir':'utility',
            'linklibs' : [],
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':[''],
            'extra_srcs': ['3rd/pbjson'],
        },
        {
            'name':'dcnode',
            'subdir':'dcnode',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':['base','utility','dcnode/proto','3rd/pbjson'],
        },
        {
            'name':'dagent',
            'subdir':'dagent',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':['base','utility','dcnode','dcnode/proto','dagent/proto','3rd/pbjson'],
        },
        {
            'name':'dagent_py',
            'subdir':'dagent/python',
            'type': 'SHARED',
            'includes':['base','dcnode','dagent','/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'linkpaths':[],
            'src_dirs':['base','dcnode','dcnode/proto','dagent','dagent/proto','3rd/pbjson'],
            'linklibs' : [
                'protobuf','python2.7'
            ]
        },
        {
            'name':'mongoproxyapi',
            'subdir':'app/mongoproxy/api',
            'includes':['/usr/local/include'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': ['app/mongoproxy/proto'],
            'linklibs' : [
                'dagent',
                'python2.7',
                'libprotobuf.a',
                'mongoc-1.0',
                'bson-1.0'
            ]
        },
]
EXES = [
        {
            'name':'testagent',
            'subdir':'app/test',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linkpaths':['/usr/local/lib'],
            'linklibs' : [
                'dagent',
                'python2.7',
                'libprotobuf.a',
                'mysqlclient',
                'mongoc-1.0',
                'bson-1.0'
            ]
        },
        {
            'name':'mongoproxy',
            'subdir':'app/mongoproxy',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': ['app/mongoproxy/proto'],
            'linklibs' : [
                'dagent',
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
                'dagent',
                'mongoproxyapi',
                'python2.7',
                'libprotobuf.a',
                'mongoc-1.0',
                'bson-1.0'
            ]
        },
        {
            'name':'reporter',
            'subdir':'app/reporter',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linklibs' : [
                'dagent',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'collector',
            'subdir':'app/collector',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linklibs' : [
                'dagent',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'pingpong',
            'subdir':'app/pingpong',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linklibs' : [
                'dcnode',
                'protobuf',
            ]
        },
]
