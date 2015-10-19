#projects genearte by cmaketools 
file(GLOB_RECURSE cur_SRCS RELATIVE
${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.cc *.hpp)

#base dir depend on itself and some otherlib
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${PROJECT_SOURCE_DIR}
)

#it will generate a static lib for comiling faster
add_library(base STATIC ${cur_SRCS})

