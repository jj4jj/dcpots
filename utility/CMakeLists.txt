
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/ cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")
set(cur_SRCS "${cur_SRCS};${PROJECT_SOURCE_DIR}/")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
/usr/include
/usr/local/include
/usr/local/include/libbson-1.0)

add_library(dcutil STATIC ${cur_SRCS})

link_directories(
/usr/local/lib)

target_link_libraries(dcutil
)

