#projects genearte by cmaketools 
file(GLOB_RECURSE cur_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.cc *.hpp)

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

ADD_EXECUTABLE(reporter ${cur_SRCS})

#assign custrom link lib
target_link_libraries(reporter
dagent
python2.7
protobuf)

#installing
INSTALL(TARGETS reporter DESTINATION bin/)
