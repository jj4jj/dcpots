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
            'name':'dcnode',
            'subdir':'dcnode',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':['base','utility/drs','dcnode/proto','3rd/pbjson'],
        },
        {
            'name':'dcagent',
            'subdir':'dcagent',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':['base', 'dcagent/proto'],
        },
        {
            'name':'mongoproxyapi',
            'subdir':'app/mongoproxy/api',
            'includes':['/usr/local/include/libbson-1.0','3rd'],
            'linkpaths':['/usr/local/lib'],
            'src_dirs': ['app/mongoproxy/proto','utility/mongo'],
        },
        {
            'name': 'dcrepoter',
            'subdir': 'reporter',
            'includes': ['base','dcnode','dcagent','/usr/local/include/libbson-1.0','3rd'],
            'src_dirs': ['base', 'utility/drs', 'dcnode', 'dcnode/proto', 'dagent', 'dagent/proto','3rd/pbjson']
        },
        {
            'name': 'pbjson',
            'subdir': '3rd/pbjson/',
            'includes': ['3rd'],
        },
        {
            'name': 'dcutil-redis',
            'subdir': 'utility/redis',
            'includes': [],
        },
        {
            'name': 'dcrpc',
            'subdir': 'dcrpc',
            'includes': [],
            'src_dirs': ['./dcrpc/client/','./dcrpc/server/','./dcrpc/share/'],
        },
		{
            'name':'dcutil-mysql',
            'subdir':'utility/mysql',
            'linklibs' : [],
            'includes':['/usr/local/include'],
            'src_dirs':[''],
            'extra_srcs': ['3rd/pbjson'],
        },
		{
            'name':'dcutil-script',
            'subdir':'utility/script',
            'linklibs' : [],
            'includes':['/usr/local/include'],
            'src_dirs':[''],
            'extra_srcs': ['3rd/pbjson'],
        },
		{
            'name':'dcutil-mongo',
            'subdir':'utility/mongo',
            'linklibs' : [],
            'includes':['/usr/local/include/libbson-1.0','3rd'],
            'src_dirs':[''],
            'extra_srcs': ['3rd/pbjson'],
        },
		{
            'name':'dcutil-drs',
            'subdir':'utility/drs',
            'linklibs' : [],
            'includes':['/usr/local/include','3rd'],
            'src_dirs':[''],
            'extra_srcs': ['3rd/pbjson'],
        },


]
EXES = [
        {
            'name':'test',
            'subdir':'app/test',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linkpaths':['/usr/local/lib'],
            'linklibs' : [
				'dcbase',
				'dcnode',
                'dcagent',
				'dcutil-drs',
				'dcutil-mysql',
				'dcutil-mongo',
				'dcutil-script',
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
                'mongoproxyapi',
				'dcnode',
                'dcagent',
				'dcutil-mongo',
				'dcutil-drs',
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
                'dcrepoter',
				'dcagent',
				'dcnode',
				'dcutil-script',
				'dcutil-drs',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'collector',
            'subdir':'app/collector',
            'includes':['/usr/local/include','/usr/local/include/libbson-1.0'],
            'linklibs' : [
                'dcrepoter',
				'dcagent',
				'dcnode',
				'dcutil-script',
				'dcutil-drs',
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
