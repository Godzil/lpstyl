cmake_minimum_required (VERSION 3.0)

project (lpstyl)

option(BUILD_ATALK "Build AppleTalk support" OFF)

if (BUILD_ATALK)
 find_library(LIB_ATALK atalk)
 add_definition(-DATALK=1)
endif()

add_executable(lpstyl lpstyl.c adsp.c)
target_link_libraries(lpstyl ${LIB_ATALK})
