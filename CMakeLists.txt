cmake_minimum_required(VERSION 3.2)

############################################################################
#
#  Duel 6 Reloaded MasterServer
#
#############################################################################

# project name
project(Duel6rMasterServer)

#########################################################################
# Platform detection
#########################################################################

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    EXEC_PROGRAM(cat ARGS "/proc/cpuinfo" OUTPUT_VARIABLE CPUINFO)

    STRING(REGEX REPLACE "^.*(arm).*$" "\\1" ARM_THERE ${CPUINFO})
    STRING(COMPARE EQUAL "arm" "${ARM_THERE}" ARM_TRUE)
    if (ARM_TRUE)
        set(D6R_PLATFORM "arm")
    else (ARM_TRUE)
        set(D6R_PLATFORM "x86")
    endif (ARM_TRUE)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(D6R_PLATFORM "x86")
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

#########################################################################
# Compiler flags
#########################################################################

# visual studio
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)
endif (MSVC)

# gnu compiler or clang
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -ggdb3 -D_FORTIFY_SOURCE=2 -std=c++17")
    if (D6R_PLATFORM STREQUAL "x86")

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -msse2")
    elseif (D6R_PLATFORM STREQUAL "arm")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard -mlittle-endian -munaligned-access")
    endif (D6R_PLATFORM STREQUAL "x86")
endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")

# MinGW
if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -static-libgcc -static-libstdc++")
endif (MINGW)

set (D6R_SOURCES
	source/main.cpp
	)
set (D6R_TEST_SOURCES
	source/test.cpp
	)
set(D6R_APP_NAME "duel6r-masterserver" CACHE STRING "Filename of the application.")
set(D6R_APP_DEBUG_NAME "duel6rd-masterserver" CACHE STRING "Filename of the debug version of the application.")

set(D6R_TESTAPP_NAME "duel6r-masterserver-test" CACHE STRING "Filename of the test application.")
set(D6R_TESTAPP_DEBUG_NAME "duel6rd-masterserver-test" CACHE STRING "Filename of the debug version of the test application.")

add_executable(${D6R_APP_NAME} ${D6R_SOURCES})
add_executable(${D6R_TESTAPP_NAME} ${D6R_TEST_SOURCES})
set_target_properties(${D6R_APP_NAME} PROPERTIES VERSION 1.0.0 DEBUG_OUTPUT_NAME ${D6R_APP_DEBUG_NAME})


#########################################################################
# External dependencies
#########################################################################

# Platform
if (MINGW)
    target_link_libraries(${D6R_APP_NAME} mingw32)
endif (MINGW)


find_path(HEADERS_ENET enet/enet.h DOC "Path to ENet headers")
include_directories(${HEADERS_ENET})
find_library(LIB_ENET NAMES enet enet_static libenet libenet_static DOC "Path to ENet library")
target_link_libraries(${D6R_APP_NAME} ${LIB_ENET})
target_link_libraries(${D6R_TESTAPP_NAME} ${LIB_ENET})

