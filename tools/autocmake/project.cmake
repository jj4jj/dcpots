#CMake projects genearte by cmaketools 

project({{project}} C CXX)
set({{project}}_VERSION {{version}})

#min version
cmake_minimum_required(VERSION 3.0)

# building must be in not source

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" BUILDING_IN_SOURCE)

if(BUILDING_IN_SOURCE)
    message(FATAL_ERROR "compile dir must not be source dir , please remove 'CMakeCache.txt' in current dir , then create a building dir in which dir you can exec commands like this 'cmake <src dir>  [options]' ")
endif()

#compile option
option(DEBUG "Debug mode" on)
option(PCH "Use precompiled headers" off)

#common
set(CMAKE_COMM_FLAGS "-rdynamic")

#verbose
{%-if verbose or verbose == 1  or verbose == 'on' %}
set( CMAKE_VERBOSE_MAKEFILE on)
{%-else%}
set( CMAKE_VERBOSE_MAKEFILE off)
{%endif%}


{%-if deubg or debug == 1 or debug == 'on' %}
#debug
SET(DEBUG on)
set(CMAKE_BUILD_TYPE Debug)
add_definitions(-D_DEBUG_)
message("Build in debug-mode : Yes (default)")
set(CMAKE_COMM_FLAGS "${CMAKE_COMM_FLAGS} -g3 -ggdb3 -Wall -Wfatal-errors -Wextra -Wall")

{%-else%}
#release
SET(DEBUG off)
set(CMAKE_BUILD_TYPE Release)
add_definitions(-D_NDEBUG_)
message("Build in debug-mode : No")
set(CMAKE_COMM_FLAGS "${CMAKE_COMM_FLAGS} -g2 -ggdb -O2 -Wall")
{%endif%}


{%-if distcc or distcc == 1 or distcc == 'on' %}
#enable distcc
set(CMAKE_C_COMPILER "distcc")
set(CMAKE_C_COMPILER_ARG1 "gcc")
set(CMAKE_CXX_COMPILER "distcc")
set(CMAKE_CXX_COMPILER_ARG1 "g++")
{%endif%}

#common compile options
{%-for co in cos %}
set(CMAKE_COMM_FLAGS "${CMAKE_COMM_FLAGS} {{co}}")
{%-endfor%}


#debug compile
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_COMM_FLAGS} {{extra_c_flags}} ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_COMM_FLAGS} {{extra_cxx_flags}} ")

# release compile
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_COMM_FLAGS} {{extra_c_flags}} ")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_COMM_FLAGS} {{extra_cxx_flags}} ")

# output dir
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# install dir
set(CMAKE_INSTALL_PREFIX {{install_dir}})

#common define
{%-for def in defs %}
add_definitions(-D{{def}})
{%-endfor%}

#common include
{%-for inc in incs %}
include_directories({{inc}})
{%-endfor%}

#common link include
{%-for inc in links %}
link_directories({{inc}})
{%-endfor%}

#common link libs
{%-for lib in libs %}
link_libraries({{lib}})
{%-endfor%}

#sub units
{%-for unit in units%}
add_subdirectory({{unit.subdir}})
{%-endfor%}

#obj commands dependcies
{%-for obj in objs%}
{%-if obj.force%}
add_custom_target({{obj.name}}
    ALL
    COMMAND {{obj.cmd}}
    DEPENDS {{obj.dep}}
    SOURCES {{obj.srcs}})
{%-if obj.install%} 
install(FILES {{obj.out}} DESTINATION {{obj.dst}})
{%-endif%}
{%-else%}
add_custom_command(OUTPUT {{obj.out}}
    COMMAND {{obj.cmd}}
    DEPENDS {{obj.dep}})
{%-endif%}
{%-endfor%}

