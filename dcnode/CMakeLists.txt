#the compiling files
file(GLOB_RECURSE cur_SRCS RELATIVE
${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.cc *.c)

#base dir depend on itself and some otherlib
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${PROJECT_SOURCE_DIR}
                    ${PROJECT_SOURCE_DIR}/3rd/protobuf/include
                    )

#base dir linked lib dir
link_directories(/usr/local/lib
                 ${PROJECT_SOURCE_DIR}/lib)

#it will generate a static lib for comiling faster
add_library(dcnode STATIC ${cur_SRCS})
