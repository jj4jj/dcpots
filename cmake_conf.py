PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 1
DEFS =['_DCAGENT_DEBUG']
LIBS = [
        {
            'name':'dcbase',
            'subdir':'base',
            'linklibs' : [
                'protobuf',
                'python2.7',
            ]
        },
        {
            'name':'dcnode',
            'subdir':'dcnode',
            'linklibs' : [
                'dcbase',
            ]
        },
        {
            'name':'dagent',
            'subdir':'dagent',
            'linklibs' : [
                'dcnode',
            ]
        },
        {
            'name':'dagent_py',
            'subdir':'dagent/python',
            'type': 'SHARED',
            'includes':['base','dcnode','dagent'],
            'linkpaths':[],
            'src_dirs':['base','dcnode','dagent'],
            'linklibs' : [
                'protobuf','python2.7'
            ]
        },

]
EXES = [
        {
            'name':'testagent',
            'subdir':'app/test',
            'linklibs' : [
                'dagent',
            ]
        },
        {
            'name':'reporter',
            'subdir':'app/reporter',
            'linklibs' : [
                'dagent',
                'dcnode',
                'dcbase',
                'python2.7',
                'protobuf',
            ]
        },
        {
            'name':'collector',
            'subdir':'app/collector',
            'linklibs' : [
                'dagent',
                'dcnode',
                'dcbase',
                'python2.7',
                'protobuf',
            ]
        },

]
