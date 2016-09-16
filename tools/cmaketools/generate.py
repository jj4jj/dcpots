#!/bin/python
#-*-coding:utf8-*- 
import sys
import os

template_path=os.path.join(os.path.dirname(__file__),'templates')

def copy_replace_file(srcfile, dstfile, replaces):
    print("generate file:"+dstfile+" ...")
    with open(srcfile, "r") as f:
        data = f.read()
        for key in replaces:
            data = data.replace(key, str(replaces[key]))
        with open(dstfile, "w") as of:
            of.write(data)

def path_fixing(path):
    if path.find('/') == 0:
        return path
    else:
        return '${PROJECT_SOURCE_DIR}/'+path

def src_files(path):
    incpath = path
    if path.find('/') != 0:
        incpath = '${PROJECT_SOURCE_DIR}'+'/'+path;
    aux_source='aux_source_directory(' + incpath + ' CSRCS)\nlist(APPEND CSRCS "${extra}")'
    return aux_source

def src_extra_files(path):
    incpath = path
    if path.find('/') != 0:
        incpath = '${PROJECT_SOURCE_DIR}'+'/'+path;
    aux_source='list(APPEND CSRCS "' + incpath + '")'
    return aux_source


def generate(desc , root_path):
    rootf = os.path.join(template_path,'root.txt')
    libf = os.path.join(template_path,'lib.txt')
    exef = os.path.join(template_path,'exe.txt')
    definations = '\n'.join(map(lambda s:'ADD_DEFINITIONS(-D'+s+')',desc.DEFS))
    if len(desc.DEFS) == 0 :
        definations=''
    if len(desc.LIBS) == 0 and len(desc.EXES) == 0:
        print("not found lib or exe modules")
        sys.exit(-1)
    subdirs = ''
    if len(desc.LIBS) > 0 :
        subdirs = subdirs + '\n'.join(map(lambda l:'add_subdirectory('+l['subdir']+')', desc.LIBS))
    subdirs = subdirs + '\n';
    if len(desc.EXES) > 0 :
        subdirs = subdirs + '\n'.join(map(lambda l:'add_subdirectory('+l['subdir']+')', desc.EXES))
        extra_statements = ''

        copy_replace_file(rootf, root_path+'/CMakeLists.txt',
            {'<definations>': definations,
             '<debug_mode>': desc.DEBUG,
             '<project_name>': desc.PROJECT,
             '<extra_c_flags>': desc.EXTRA_C_FLAGS,
             '<extra_cxx_flags>': desc.EXTRA_CXX_FLAGS,
             '<verbose>': desc.VERBOSE,
             #'<extra_ld_flags>': desc.EXTRA_LD_FLAGS,
             '<add_subdirectory_area>': subdirs,
             '<project_version>': desc.VERSION,
                         '<extra_statements>':extra_statements})

    for lib in desc.LIBS:
        extra_includes = getattr(desc, 'EXTRA_INCLUDES', None) or []
        subf=os.path.join(root_path,lib['subdir'],'CMakeLists.txt')

        includes = ''
        if lib.has_key('includes') and len(lib['includes']) > 0:
            extra_includes.extend(lib['includes'])
        includes = '\n'.join(map(path_fixing,extra_includes))

        linkpaths = ''
        if lib.has_key('linkpaths') and len(lib['linkpaths']) > 0:
            linkpaths = '\n'.join(map(path_fixing,lib['linkpaths']))

        linklibs = ''
        if lib.has_key('linklibs') and len(lib['linklibs']) > 0:
            linklibs = '\n'.join(lib['linklibs'])

        extra_srcs = ''
        if lib.has_key('src_dirs') and len(lib['src_dirs']) > 0:
            extra_srcs = '\n'.join(map(src_files, lib['src_dirs']))

        if lib.has_key('extra_srcs') and len(lib['extra_srcs']) > 0:
            extra_srcs += '\n'
            extra_srcs += '\n'.join(map(src_extra_files, lib['extra_srcs']))

        lib_type = 'STATIC'
        if lib.has_key('type'):
            lib_type = lib['type']

        extra_statements = ''
        if lib.has_key('genobj'):
            extra_statements += 'add_custom_command(OUTPUT %s\nCOMMAND %s\nDEPENDS %s\n)\nlist(APPEND CSRCS %s)\n' % (lib["genobj"]["out"],lib["genobj"]["cmd"],lib["genobj"]["dep"], lib["genobj"]["out"])

        copy_replace_file(libf, subf,
            {'<lib_name>': lib['name'],
             '<includes>': includes,
             '<lib_type>': lib_type,
             '<linkpaths>': linkpaths,
             '<linklibs>': linklibs,
             '<extra_srcs>': extra_srcs,
             '<extra_statements>':extra_statements})

    for exe in desc.EXES:
        subf=os.path.join(root_path,exe['subdir'],'CMakeLists.txt')
        extra_includes = getattr(desc, 'EXTRA_INCLUDES', None) or []
        includes = ''
        if exe.has_key('includes') and len(exe['includes']) > 0:
            extra_includes.extend(exe['includes'])
        includes = '\n'.join(map(path_fixing,extra_includes))
 
        linkpats = ''
        if exe.has_key('linkpaths') and len(exe['linkpaths']) > 0:
            linkpaths = '\n'.join(map(path_fixing,exe['linkpaths']))

        linklibs = ''
        if exe.has_key('linklibs') and len(exe['linklibs']) > 0:
            linklibs = '\n'.join(exe['linklibs'])

        extra_srcs = ''
        if exe.has_key('src_dirs') and len(exe['src_dirs']) > 0:
            extra_srcs = '\n'.join(map(src_files, exe['src_dirs']))

        if exe.has_key('extra_srcs') and len(exe['extra_srcs']) > 0:
            extra_srcs += '\n'
            extra_srcs += '\n'.join(map(src_extra_files, exe['extra_srcs']))

        extra_statements = ''
        if exe.has_key('genobj'):
            extra_statements += 'add_custom_command(OUTPUT %s\nCOMMAND %s\nDEPENDS %s\n)\nlist(APPEND CSRCS %s)\n' % (exe["genobj"]["out"],exe["genobj"]["cmd"],exe["genobj"]["dep"], exe["genobj"]["out"])

        copy_replace_file(exef, subf,
            {'<exe_name>': exe['name'],
             '<includes>': includes,
             '<linkpaths>': linkpaths,
             '<linklibs>': linklibs,
             '<extra_srcs>': extra_srcs,
                         '<extra_statements>': extra_statements})


def main(desc_file_path):
    modfile = desc_file_path+'.py'
    modf='cmake_conf'
    if os.path.exists(modfile):
        modf = os.path.basename(desc_file_path)
        desc_file_path = os.path.dirname(modfile)

    if desc_file_path and len(desc_file_path) > 0:
        sys.path.append(desc_file_path)
    else:
        sys.path.append('.') 
    desc=__import__(modf)
    generate(desc, desc_file_path or '.')

def usage():
    print("./generate.py <description filepath>")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        usage()
        sys.exit(-1)
    main(sys.argv[1])
