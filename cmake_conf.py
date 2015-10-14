PROJECT='dcagent'
VERSION='0.0.1'
DEBUG = 1
DEFS =['_DCAGENT_DEBUG']
LIBS = [
        {
            'name':'dcnode',
            'subdir':'dcnode',
            'includes':[
                #'3rd/protobuf/include'
            ]
        },
        {
            'name':'dagent',
            'subdir':'dagent',
            'includes':[
                #'3rd/protobuf/include'
            ]
        }
]
EXES = [
        {
            'name':'dcagent',
            'subdir':'app',
            'includes': [
                #'3rd/protobuf/include',
            ],
            'linkpaths' : [
                #'3rd/protobuf/include/lib',
            ],
            'linklibs' : [
                'dagent',
                'dcnode',
                'protobuf',
            ]
        }
]
