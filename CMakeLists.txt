cmake_minimum_required(VERSION 3.20)

project(test)

set(CMAKE_CXX_STANDARD 20)

add_executable(test src/main.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
    set(CMAKE_MACOSX_RPATH 1)
endif()

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG f4622efcb59d84071ae11404ae61bd821c1c344b) # the commit hash for 1.6.2
FetchContent_MakeAvailable(cpr)
target_link_libraries(test PRIVATE cpr::cpr)

include_directories("/usr/local/include")