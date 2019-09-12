#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

cmake_minimum_required(VERSION 2.6)

IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

if(MSVC)
    set(CBOR_RESTRICT_SPECIFIER "")
elseif(APPLE)
    set(CBOR_RESTRICT_SPECIFIER "restrict")
else()
    set(CBOR_RESTRICT_SPECIFIER "__restrict__")
endif()

if (WIN32)
    set(BYPRODUCT "src/${CMAKE_BUILD_TYPE}/cbor.lib")
else()
    set(BYPRODUCT "src/libcbor.a")
endif()

set(CBOR_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libcbor-src")
set(CBOR_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/libcbor-build")

include(ExternalProject)
ExternalProject_Add(
    libcbor-external
    GIT_REPOSITORY "https://github.com/PJK/libcbor.git"
    GIT_TAG "82512d851205fbc7f65d96a0b4a8e1bad2e4f3c6"
    SOURCE_DIR "${CBOR_SOURCE_DIR}"
    BINARY_DIR "${CBOR_BYPRODUCT_DIR}"
    INSTALL_COMMAND ${CMAKE_COMMAND}  -E echo "Skipping install step."
    BUILD_BYPRODUCTS "${CBOR_BYPRODUCT_DIR}/${BYPRODUCT}"
)
add_definitions("-Drestrict=${CBOR_RESTRICT_SPECIFIER}")

set(CBOR_FOUND "YES" CACHE STRING "" FORCE)
set(CBOR_INCLUDE_DIRS "${CBOR_SOURCE_DIR}/src" "${CBOR_BYPRODUCT_DIR}" CACHE STRING "" FORCE)
set(CBOR_LIBRARY "${CBOR_BYPRODUCT_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

include_directories(${CMAKE_SOURCE_DIR}/nanofi/include)
include_directories(${CMAKE_SOURCE_DIR}/libminifi/include ${CMAKE_SOURCE_DIR}/thirdparty/cron ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/ut)
include_directories(${CBOR_INCLUDE_DIRS})

add_library(cbor STATIC IMPORTED)
add_dependencies(cbor libcbor-external)
set_target_properties(cbor PROPERTIES IMPORTED_LOCATION "${CBOR_BYPRODUCT_DIR}/${BYPRODUCT}")

if(WIN32)
    include_directories(../libminifi/opsys/win)
else()
    include_directories(../libminifi/opsys/posix)
endif()

file(GLOB NANOFI_SOURCES  "src/api/*.c*" "src/core/*.c*" "src/cxx/*.cpp" "src/sitetosite/*.c*" "src/ecu_api/*.c*" "src/processors/*.c*")

if(WIN32)
    list(REMOVE_ITEM NANOFI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/api/ecu.c ${CMAKE_CURRENT_SOURCE_DIR}/src/core/flowfiles.c)
endif()

file(GLOB C2_COAP_SOURCES "")
set(NANOFI_COAP_LIBS "")

if (ENABLE_COAP STREQUAL "ON")
    include_directories(${COAP_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/extensions/coap)
    file(GLOB C2_COAP_SOURCES "src/coap/*.c*" "src/c2_api/*.c*" "src/heartbeat/*.c*")
    set(NANOFI_COAP_LIBS ${NANOFI_COAP_LIBS} nanofi-coap-c ${COAP_LIBRARIES})
endif()

add_library(nanofi STATIC ${NANOFI_SOURCES} ${C2_COAP_SOURCES})
add_dependencies(nanofi  minifi-standard-processors minifi-coap cbor)

if (APPLE)
    target_link_libraries(nanofi -Wl,-all_load core-minifi minifi minifi-standard-processors ${NANOFI_COAP_LIBS} ${CBOR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
elseif(NOT WIN32)
    target_link_libraries(nanofi -Wl,--whole-archive core-minifi minifi minifi-standard-processors -Wl,--no-whole-archive ${NANOFI_COAP_LIBS} ${CBOR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
else()
	target_link_libraries(nanofi PUBLIC core-minifi minifi minifi-standard-processors ${NANOFI_COAP_LIBS} PRIVATE ${CBOR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
endif()

if (ENABLE_PYTHON AND NOT STATIC_BUILD)

add_library(nanofi-shared SHARED ${NANOFI_SOURCES})

if (APPLE)
    target_link_libraries (nanofi-shared -Wl,-all_load core-minifi-shared minifi-shared minifi-standard-processors minifi-coap ${NANOFI_COAP_LIBS} ${CBOR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
elseif(NOT WIN32)
    target_link_libraries (nanofi-shared -Wl,--whole-archive core-minifi-shared minifi-shared  minifi-standard-processors -Wl,--no-whole-archive minifi-coap ${NANOFI_COAP_LIBS} ${CBOR_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
else()
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:core-minifi-shared")
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:minifi-shared")
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:minifi-standard-processors")
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:nanofi-coap-c")
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:${CBOR_LIBRARY}")
endif ()

add_dependencies(nanofi-shared minifi-standard-processors)

if(WIN32)
    set_target_properties(nanofi-shared PROPERTIES LINK_FLAGS "${WIN32_ARCHIVES}")
endif()

set_property(TARGET nanofi-shared PROPERTY POSITION_INDEPENDENT_CODE ON)

endif(ENABLE_PYTHON AND NOT STATIC_BUILD)

if (NOT DISABLE_CURL)
add_subdirectory(examples)
add_subdirectory(ecu)
endif()
