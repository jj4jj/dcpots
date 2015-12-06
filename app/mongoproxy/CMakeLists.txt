#projects genearte by cmaketools
#file(GLOB_RECURSE cur_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.cc *.hpp)
#file(GLOB cur_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.cc *.hpp)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cur_aSRCS)
set(cur_SRCS "${cur_aSRCS}")
aux_source_directory(${PROJECT_SOURCE_DIR}/app/mongoproxy/proto cur_aSRCS)
set(cur_SRCS "${cur_SRCS};${cur_aSRCS}")

#base dir depend on itself and some otherlib
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
/usr/local/include
/usr/local/include/libbson-1.0)

#base dir linked lib dir
link_directories(
/usr/local/lib
/usr/local/lib
${PROJECT_SOURCE_DIR}/lib)

#it will generate a exe

ADD_EXECUTABLE(mongoproxy ${cur_SRCS})

#assign custrom link lib
target_link_libraries(mongoproxy
dagent
python2.7
libprotobuf.a
mongoc-1.0
bson-1.0)

#installing
INSTALL(TARGETS mongoproxy DESTINATION bin/)
