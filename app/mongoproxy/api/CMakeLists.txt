
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")

aux_source_directory(${PROJECT_SOURCE_DIR}/app/mongoproxy/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
/usr/include
/usr/local/include
${PROJECT_SOURCE_DIR}/3rd)

add_library(mongoproxyapi STATIC ${cur_SRCS})

link_directories(/usr/local/lib
/usr/local/lib)

target_link_libraries(mongoproxyapi
dagent
python2.7
libprotobuf.a
mongoc-1.0
bson-1.0)

