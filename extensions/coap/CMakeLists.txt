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
file(GLOB SOURCES "*.cpp" "protocols/*.cpp" "controllerservice/*.cpp" "server/*.cpp" )
set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
add_definitions("-DWITH_POSIX=1")

add_library(minifi-coap STATIC ${SOURCES})
add_library(nanofi-coap-c STATIC ${CSOURCES})

target_include_directories(nanofi-coap-c PUBLIC ${COAP_INCLUDE_DIRS})
target_include_directories(minifi-coap PUBLIC ${COAP_INCLUDE_DIRS})

add_dependencies(minifi-coap nanofi-coap-c)
add_dependencies(nanofi-coap-c COAP::libcoap) 

target_link_libraries(nanofi-coap-c PUBLIC ${COAP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
target_link_libraries(minifi-coap PUBLIC nanofi-coap-c ${COAP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

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
