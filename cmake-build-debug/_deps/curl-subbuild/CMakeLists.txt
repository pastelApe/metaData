# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.20.2)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(curl-populate NONE)



include(ExternalProject)
ExternalProject_Add(curl-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/curl/curl/releases/download/curl-7_75_0/curl-7.75.0.tar.xz" "URL_HASH" "SHA256=fe0c49d8468249000bda75bcfdf9e30ff7e9a86d35f1a21f428d79c389d55675"
                    SOURCE_DIR          "/Users/xawatso/Desktop/Projects/metadata/cmake-build-debug/_deps/curl-src"
                    BINARY_DIR          "/Users/xawatso/Desktop/Projects/metadata/cmake-build-debug/_deps/curl-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


