
aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

#message(STATUS "cur: ${cur_SRCS}")

#aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode/proto cur_aSRCS)
#set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")


#message(STATUS "cur: ${cur_SRCS}")


include_directories(
${PROJECT_SOURCE_DIR}

/usr/include)

add_library(dagent STATIC ${cur_SRCS})

link_directories(

/usr/local/lib)

target_link_libraries(dagent
dcnode)

