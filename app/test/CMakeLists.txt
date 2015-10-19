#projects genearte by cmaketools 
file(GLOB_RECURSE cur_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.cc *.hpp)

#base dir depend on itself and some otherlib
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
)

#base dir linked lib dir
link_directories(

/usr/local/lib
${PROJECT_SOURCE_DIR}/lib)

#it will generate a exe

ADD_EXECUTABLE(testagent ${cur_SRCS})

#assign custrom link lib
target_link_libraries(testagent
dagent
dcnode
protobuf)

#installing
INSTALL(TARGETS testagent DESTINATION bin/)
