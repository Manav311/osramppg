# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(max30101)

FILE(GLOB app_sources 
    src/*.c
    src/main.c
   
)
target_sources(app PRIVATE ${app_sources})



# --- Link prebuilt .a file ---
set(MYLIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libas7058a_hrm_b0.a)

add_library(mylib STATIC IMPORTED)
set_target_properties(mylib PROPERTIES IMPORTED_LOCATION ${MYLIB_PATH})

# If you also have headers:
# set_target_properties(mylib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

target_link_libraries(app PRIVATE mylib)