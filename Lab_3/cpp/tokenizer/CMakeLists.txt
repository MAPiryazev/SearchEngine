cmake_minimum_required(VERSION 3.16)
project(tokenizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)

add_executable(tokenizer src/main.cpp)

target_include_directories(tokenizer PRIVATE include)
target_link_libraries(tokenizer PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)
