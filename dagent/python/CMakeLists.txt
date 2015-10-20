
aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

message(STATUS "cur: ${cur_SRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/dagent/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/dagent cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")


#message(STATUS "cur: ${cur_SRCS}")

#base dir depend on itself and some otherlib
include_directories(
    /usr/include/
    ${PROJECT_SOURCE_DIR}/dcbase
    ${PROJECT_SOURCE_DIR}/dcnode
    ${PROJECT_SOURCE_DIR}/dagent
    ${PROJECT_SOURCE_DIR}
)

link_directories(
 /usr/local/lib)

add_library(dagent_py SHARED ${cur_SRCS})
target_link_libraries(dagent_py protobuf python2.7)

