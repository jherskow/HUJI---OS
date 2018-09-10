cmake_minimum_required(VERSION 3.8)
project(OS_ex2)

set(CMAKE_CXX_STANDARD 11)

set(LIBSRC uthreads.cpp uthreads.h thread.h thread.cpp)
add_library(libuthreads.a ${LIBSRC})


set(TSTSRC uthreads.cpp uthreads.h thread.h thread.cpp)
add_executable(test_run ${TSTSRC})

#set(TSTLIB libuthreads.a uthreads.h test1.cpp)
#add_executable(test_from_lib ${TSTLIB})