cmake_minimum_required(VERSION 3.9)
project(libe2c)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/salticidae/cmake/Modules/")

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include(ExternalProject)
# Configure CMake for salticidae
add_subdirectory(salticidae)
include_directories(salticidae/include)

# Build salticidae

# Build secp256k1
include_directories(secp256k1/include)
ExternalProject_Add(libsecp256k1
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/secp256k1"
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/autogen.sh
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/configure --disable-shared --with-pic --with-bignum=no --enable-module-recovery
    BUILD_COMMAND make
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1)

add_library(secp256k1 STATIC IMPORTED)
set_target_properties(
    secp256k1
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/.libs/libsecp256k1.a)
add_dependencies(secp256k1 libsecp256k1)

# Add E2C Libraries

include_directories(./)
include_directories(include)
add_library(libe2c
    OBJECT
    src/util.cpp
    src/client.cpp
    src/crypto.cpp
    src/entity.cpp
    src/consensus.cpp
    src/e2c.cpp
    )

# E2C Build Options

option(BUILD_SHARED "build shared library." OFF)
if(BUILD_SHARED)
    set_property(TARGET libe2c PROPERTY POSITION_INDEPENDENT_CODE 1)
    add_library(libe2c_shared SHARED $<TARGET_OBJECTS:libe2c>)
    set_target_properties(libe2c_shared PROPERTIES OUTPUT_NAME "libe2c")
    target_link_libraries(libe2c_shared salticidae_static secp256k1 crypto ${CMAKE_THREAD_LIBS_INIT})
endif()
add_library(libe2c_static STATIC $<TARGET_OBJECTS:libe2c>)
set_target_properties(libe2c_static PROPERTIES OUTPUT_NAME "libe2c")
target_link_libraries(libe2c_static salticidae_static secp256k1 crypto ${CMAKE_THREAD_LIBS_INIT})

# # Building tests
add_subdirectory(test)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# TODO Fix Logging
option(E2C_ENABLE_PROT_LOG  "Enable Protocol logging"   OFF )
option(E2C_DEBUG            "Enable Debugging"          OFF )

configure_file(src/config.h.in include/libe2c/config.h @ONLY)

# Setup Documentation

find_package(Doxygen)
if (DOXYGEN_FOUND)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} "${CMAKE_SOURCE_DIR}/doc/doxygen.conf" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif(DOXYGEN_FOUND)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -W -Wall -Wextra -pedantic -Wsuggest-override")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -W -Wall -Wextra -pedantic -Wsuggest-override")

macro(remove_cxx_flag flag)
  string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endmacro()

remove_cxx_flag("-DNDEBUG")
