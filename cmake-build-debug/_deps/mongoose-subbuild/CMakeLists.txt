# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.20.2)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(mongoose-populate NONE)



include(ExternalProject)
ExternalProject_Add(mongoose-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/cesanta/mongoose/archive/6.18.tar.gz" "URL_HASH" "SHA256=f5c10346abc9c72f7cac7885d853ca064fb09aad57580433941a8fd7a3543769"
                    SOURCE_DIR          "/Users/xawatso/Desktop/Projects/metadata/cmake-build-debug/_deps/mongoose-src"
                    BINARY_DIR          "/Users/xawatso/Desktop/Projects/metadata/cmake-build-debug/_deps/mongoose-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


