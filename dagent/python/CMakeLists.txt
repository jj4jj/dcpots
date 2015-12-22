
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dagent cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dagent/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/3rd/pbjson cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
/usr/include
${PROJECT_SOURCE_DIR}/base
${PROJECT_SOURCE_DIR}/dcnode
${PROJECT_SOURCE_DIR}/dagent
/usr/local/include
/usr/local/include/libbson-1.0
${PROJECT_SOURCE_DIR}/3rd)

add_library(dagent_py SHARED ${cur_SRCS})

link_directories(
/usr/local/lib)

target_link_libraries(dagent_py
protobuf
python2.7)

