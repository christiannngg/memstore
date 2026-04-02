cmake_minimum_required(VERSION 3.20)
project(imdb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_executable(imdb
    IMDb/main.cpp
    IMDb/server.cpp
)

target_include_directories(imdb PRIVATE IMDb)
