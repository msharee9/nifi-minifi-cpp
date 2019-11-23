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


include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(protocols nanofi controllerservice server)
include_directories(../http-curl/)

file(GLOB CSOURCES "nanofi/*.c")
file(GLOB SOURCES "*.cpp" "protocols/*.cpp" "processors/*.cpp" "controllerservice/*.cpp" "server/*.cpp" )

set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(DIR "${BASE_DIR}/thirdparty/libcoap-src")

add_library(minifi-coap STATIC ${SOURCES})

if (NOT WIN32)
    set(BYPRODUCT "${BASE_DIR}/thirdparty/libcoap-src/.libs/libcoap-2.a")

    ExternalProject_Add(
    coap-external
    GIT_REPOSITORY "https://github.com/obgm/libcoap.git"
    GIT_TAG "00486a4f46e0278dd24a8ff3411416ff420cde29"
    PREFIX "${BASE_DIR}/thirdparty/libcoap"
    BUILD_IN_SOURCE true
    SOURCE_DIR "${DIR}"
    BUILD_COMMAND make
    CMAKE_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND}  -E echo "Skipping install step."
    CONFIGURE_COMMAND ""
    PATCH_COMMAND ./autogen.sh && ./configure --disable-examples --disable-dtls --disable-tests --disable-documentation
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
    )
    add_definitions("-DWITH_POSIX=1")
else()
    configure_file(DownloadCoapCMakeLists.txt.in "${CMAKE_CURRENT_BINARY_DIR}/coap_download/CMakeLists.txt" @ONLY)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libcoap_windows.patch" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/coap_download")
    execute_process(COMMAND "${CMAKE_COMMAND}" -G "Visual Studio 15 2017 Win64" . WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/coap_download")
    execute_process(COMMAND "${CMAKE_COMMAND}" --build . WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/coap_download")

    set(COAP_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/coap_download/libcoap-src")

    file(GLOB COAP_SOURCE_FILES "${COAP_SOURCE_DIR}/src/*.c")
    list(REMOVE_ITEM COAP_SOURCE_FILES ${COAP_SOURCE_DIR}/src/coap_io_lwip.c)

	configure_file(${COAP_SOURCE_DIR}/coap_config.h.windows ${COAP_SOURCE_DIR}/coap_config.h)
	configure_file(${COAP_SOURCE_DIR}/include/coap2/coap.h.windows ${COAP_SOURCE_DIR}/include/coap2/coap.h)

    add_library(coap STATIC ${COAP_SOURCE_FILES})
    
    target_include_directories(coap PUBLIC "${COAP_SOURCE_DIR}")
    target_include_directories(coap PUBLIC "${COAP_SOURCE_DIR}/include")
    target_include_directories(coap PUBLIC "${COAP_SOURCE_DIR}/include/coap2")

endif()
 
set(COAP_LIBRARIES coap CACHE STRING "" FORCE)

add_library(nanofi-coap-c STATIC ${CSOURCES})
add_dependencies(minifi-coap nanofi-coap-c)
add_dependencies(nanofi-coap-c coap)
target_link_libraries(nanofi-coap-c PUBLIC ${COAP_LIBRARIES} ${CMAKE_DL_LIBS})

target_link_libraries (minifi-coap PUBLIC nanofi-coap-c PRIVATE ${CMAKE_DL_LIBS})
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(minifi-coap PUBLIC "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (WIN32)
    set_target_properties(minifi-coap PROPERTIES
         LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-coap PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-coap PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

SET (COAP-EXTENSION minifi-coap PARENT_SCOPE)
register_extension(minifi-coap)