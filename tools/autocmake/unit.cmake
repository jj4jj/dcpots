#sub cmake of {{unit.name}}

set(unit_srcs "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} unit_srcs)

##extra dir srcs
{%-for dsrc in unit.dsrcs%}
aux_source_directory({%if dsrc[0] != '/' and dsrc[0] != '{'%}{{root}}/{%endif%}{{dsrc}} extra_unit_srcs)
list(APPEND unit_srcs "${extra_unit_srcs}")
{%-endfor%}


##extra srcs
{%-for src in unit.srcs%}
list(APPEND unit_srcs "{%if src[0] != '/' and src[0] != '{'%}{{cdir}}/{%endif%}{{src}}")
{%-endfor%}

#include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
{%-for inc in unit.incs%}
{%if inc[0] != '/' and inc[0] != '{'%}{{root}}/{%endif%}{{inc}}
{%-endfor%}
{%-for inc in unit.dsrcs%}
{%if inc[0] != '/' and inc[0] != '{'%}{{root}}/{%endif%}{{inc}}
{%-endfor%}
)

#link lib dir
link_directories(
{%-for linc in unit.lincs%}
{%if linc[0] != '/' and linc[0] != '{'%}{{root}}/{%endif%}{{linc}}
{%-endfor%}
)

#custom command
{%for obj in unit.objs%}
{%-if obj.cmd%}
{%-if not obj.force%}
add_custom_command(OUTPUT {{obj.out}}
    COMMAND {{obj.cmd}}
    DEPENDS {{obj.dep}})
{%-if obj.build%}
list(APPEND unit_srcs {{obj.out}})
{%endif%}
{%-endif%}
{%-endif%}
{%-endfor%}

##########################main objects###################################
{%-if unit.type == 'exe'%}
#exe
add_executable({{unit.name}} ${unit_srcs})
    {%-if unit.pic%}
set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE ON)
    {%-else%}
#set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE OFF)
    {%-endif%}

#link lib
target_link_libraries({{unit.name}}
{%-for lib in unit.libs%}
{{lib}}
{%-endfor%})

install(TARGETS {{unit.name}} DESTINATION bin)
{%-elif unit.type == 'share'%}
#share lib
add_library({{unit.name}} SHARED ${unit_srcs})
    {%-if not unit.pic%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE OFF)
    {%-else%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE ON)
    {%-endif%}
install(TARGETS {{unit.name}} DESTINATION lib)
{%-else%}
#static lib
add_library({{unit.name}} STATIC ${unit_srcs})
{%-endif%}

#custom command
{%for obj in unit.objs%}
{%-if obj.cmd%}
{%-if obj.force%}
add_custom_command(TARGET {{unit.name}}
    PRE_BUILD
    COMMAND {{obj.cmd}}
    DEPENDS {{obj.dep}})
{%-if obj.build%}
list(APPEND unit_srcs {{obj.out}})
{%endif%}
{%-endif%}
{%-endif%}
{%-endfor%}



#depends
{%-for dep in unit.deps%}
add_dependencies({{unit.name}} {{dep}})
{%-endfor%}


