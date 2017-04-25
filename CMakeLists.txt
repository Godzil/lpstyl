cmake_minimum_required (VERSION 3.0)

project (lpstyl)

option(BUILD_ATALK "Build AppleTalk support" OFF)

if (BUILD_ATALK)
    find_library(LIB_ATALK atalk)
    add_definition(-DATALK=1)
    set(ATALK_SRC adsp.c)
endif()

add_executable(lpstyl lpstyl.c ${ATALK_SRC})
target_link_libraries(lpstyl ${LIB_ATALK})
