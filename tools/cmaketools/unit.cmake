{%-if unit.sdef%}
set(cur_srcs "")
{%-else%}
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_srcs)
{%-endif%}
##extra dir srcs
{%-for dsrc in unit.dsrcs%}
aux_source_directory({%if dsrc[0] != '/' and dsrc[0] != '{'%}{{env.root}}/{%endif%}{{dsrc}} aux_cur_srcs)
list(APPEND cur_srcs "${aux_cur_srcs}")
{%-endfor%}
##extra srcs
{%-for src in unit.srcs%}
list(APPEND cur_srcs "{%if src[0] != '/' and src[0] != '{'%}{{env.cdir}}/{%endif%}{{src}}")
{%-endfor%}

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
{%-for inc in unit.incs%}
{%if inc[0] != '/' and inc[0] != '{'%}{{env.root}}/{%endif%}{{inc}}
{%-endfor%}
/usr/local/include
/usr/include)

link_directories(
{%-for linc in unit.lincs%}
{%if linc[0] != '/' and linc[0] != '{'%}{{env.root}}/{%endif%}{{linc}}
{%-endfor%}
/usr/local/lib
/usr/lib
/lib)

##########################main objects###################################
{%-if unit.type == 'exe'%}
add_executable({{unit.name}} ${cur_srcs})
{%-if unit.pic%}
set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE ON)
{%-else%}
#set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE OFF)
{%-endif%}
target_link_libraries({{unit.name}}
{%-for lib in unit.libs%}
{{lib}}
{%-endfor%})
{%-elif unit.type == 'share'%}
add_library({{unit.name}} SHARED ${cur_srcs})
{%-if not unit.pic%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE OFF)
{%-else%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE ON)
{%-endif%}
{%-else%}
add_library({{unit.name}} STATIC ${cur_srcs})
{%-endif%}

{%-for obj in unit.objs%}
{%-if obj.cmd%}
{%-if obj.force%}
add_custom_target({{obj.name}}
	COMMAND {{obj.cmd}}
	DEPENDS {{obj.dep}})
{%-else%}
add_custom_command(OUTPUT {{obj.out}}
	COMMAND {{obj.cmd}}
	DEPENDS {{obj.dep}})
{%-endif%}
{%-else%}

##########################sub objects###################################
set(sub_obj_srcs "")

{%-for dsrc in obj.dsrcs%}
aux_source_directory({%if dsrc[0] != '/' and dsrc[0] != '{'%}{{env.root}}/{%endif%}{{dsrc}} aux_sub_obj_srcs)
list(APPEND sub_obj_srcs "${aux_sub_obj_srcs}")
{%-endfor%}

{%-for src in obj.srcs%}
list(APPEND sub_obj_srcs "{%if src[0] != '/' and src[0] != '{'%}{{env.cdir}}/{%endif%}{{src}}")
{%-endfor%}


{%-if obj.type == 'exe'%}
add_executable({{obj.name}} ${sub_obj_srcs})
{%-if obj.pic%}
set_property(TARGET {{obj.name}} PROPERTY POSITION_INDEPENDENT_CODE ON)
{%-else%}
#set_property(TARGET {{obj.name}} PROPERTY POSITION_INDEPENDENT_CODE OFF)
{%-endif%}
target_link_libraries({{obj.name}}
{%-for lib in obj.libs%}
{{lib}}
{%-endfor%})
{%-elif obj.type == 'share'%}
add_library({{obj.name}} SHARED ${sub_obj_srcs})
{%-if not obj.pic%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE OFF)
{%-else%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE ON)
{%-endif%}
{%-else%}
add_library({{obj.name}} STATIC ${sub_obj_srcs})
{%-endif%}
{%-endif%}
{%-endfor%}

#install({{unit.name}})
