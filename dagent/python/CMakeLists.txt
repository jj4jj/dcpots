
aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

#message(STATUS "cur: ${cur_SRCS}")

#aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode/proto cur_aSRCS)
#set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dagent cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

#message(STATUS "cur: ${cur_SRCS}")


include_directories(
${PROJECT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}/base
${PROJECT_SOURCE_DIR}/dcnode
${PROJECT_SOURCE_DIR}/dagent
/usr/include)

add_library(dagent_py SHARED ${cur_SRCS})

link_directories(

/usr/local/lib)

target_link_libraries(dagent_py
protobuf
python2.7)

