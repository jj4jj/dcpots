PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 0    #0/1
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
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'src_dirs':[''],
            'extra_srcs': [''],
        },
        {
            'name':'dcnode',
            'subdir':'dcnode',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'src_dirs':['base','utility','dcnode/proto'],
        },
        {
            'name':'dagent',
            'subdir':'dagent',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'src_dirs':['base','utility','dcnode','dcnode/proto','dagent/proto'],
        },
        {
            'name':'dagent_py',
            'subdir':'dagent/python',
            'type': 'SHARED',
            'includes':['base','dcnode','dagent','/usr/local/include','/usr/local/include/libbson-1.0'],
            'linkpaths':[],
            'src_dirs':['base','dcnode','dcnode/proto','dagent','dagent/proto'],
            'linklibs' : [
                'protobuf','python2.7'
            ]
        },
		        {
            'name':'mongoproxyapi',
            'subdir':'app/mongoproxy/api',
            'includes':['/usr/local/include','3rd'],
            'linkpaths':['/usr/local/lib'],
			'src_dirs': ['3rd/pbjson'],
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
			'src_dirs': ['proto'],
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
