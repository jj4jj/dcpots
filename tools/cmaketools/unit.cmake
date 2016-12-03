aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} CSRCS)

{%for dsrc in unit.dsrcs%}aux_source_directory({%if dsrc[0] != '/' and dsrc[0] != '{'%}{{env.root}}/{%endif%}{{dsrc}} aux_CSRCS)
list(APPEND CSRCS "${aux_CSRCS}")
{%endfor%}

{%for src in unit.srcs%}list(APPEND CSRCS "{%if src[0] != '/' and src[0] != '{'%}{{env.cdir}}/{%endif%}{{src}}")
{%endfor%}

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
{%for inc in unit.incs%}{%if inc[0] != '/' and inc[0] != '{'%}{{env.root}}/{%endif%}{{inc}}
{%endfor%}/usr/local/include
/usr/include)

{%if unit.type == 'exe'%}
add_executable({{unit.name}} ${CSRCS})
{%if unit.pic%}
set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE ON)
{%else%}
set_property(TARGET {{unit.name}} PROPERTY POSITION_INDEPENDENT_CODE OFF)
{%endif%}
link_directories(
{%for linc in unit.lincs%}{%if linc[0] != '/' and linc[0] != '{'%}{{env.root}}/{%endif%}{{linc}}
{%endfor%}/usr/local/lib
/usr/lib
/lib)
target_link_libraries({{unit.name}}
{%for lib in unit.libs%}{{lib}}
{%endfor%})
{%elif unit.type == 'share'%}
add_library({{unit.name}} SHARED ${CSRCS})
{%if not unit.pic%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE OFF)
{%else%}
set_property(TARGET {{uniq.name}} PROPERTY INTERFACE_POSITION_INDEPENDENT_CODE ON)
{%endif%}
{%else%}
add_library({{unit.name}} STATIC ${CSRCS})
{%endif%}

{%for obj in unit.objs%}
{%if obj.force%}
add_custom_target({{obj.name}}
	COMMAND {{obj.cmd}}
	DEPENDS {{obj.dep}})
{%else%}
add_custom_command(OUTPUT {{obj.out}}
	COMMAND {{obj.cmd}}
	DEPENDS {{obj.dep}})
{%endif%}
{%endfor%}

#install({{unit.name}})
