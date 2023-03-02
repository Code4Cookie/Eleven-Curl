cmake_minimum_required(VERSION 3.5)

project(eleven_curl)

find_package(CURL REQUIRED)
find_package(cjson REQUIRED)

include_directories(${CURL_INCLUDE_DIR})

#static lib
add_library(eleven_curl STATIC 
    src/elevencurl.c
	include/elevencurl.h
)

target_include_directories(eleven_curl
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
		${CURL_INCLUDE_DIR}
)

