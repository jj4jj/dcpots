PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 0	#0/1
DEFS = []
VERBOSE = 'off'	#on/off
EXTRA_C_FLAGS = ''
EXTRA_CXX_FLAGS = '-std=c++11'
EXTRA_LD_FLAGS = '-ldl -lm -lrt -pthread'
LIBS = [
        {
            'name':'dagent_py',#lib name
            'subdir':'dagent/python', #lib file dir
            'type': 'SHARED',#lib type -shared or static
            'includes':['base','dcnode','dagent'], #include src
            'linkpaths':[], #link
            'src_dirs':['base','dcnode','dcnode/proto','dagent','dagent/proto'], #src dir add src
            'linklibs' : [
                'protobuf','python2.7'
            ],
            'extra_srcs':['file.cpp'], #extra cl src
        },

]
EXES = [
        {
            'name':'testagent',
            'subdir':'app/test',
            'linklibs' : [
                'dagent',
                'python2.7',
                'protobuf',
            ]
        },
]
