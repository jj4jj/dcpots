
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/base cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/utility cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dcnode/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/dagent/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
/usr/include
/usr/local/include
/usr/local/include/libbson-1.0)

add_library(dagent STATIC ${cur_SRCS})

link_directories(
/usr/local/lib)

target_link_libraries(dagent
)

