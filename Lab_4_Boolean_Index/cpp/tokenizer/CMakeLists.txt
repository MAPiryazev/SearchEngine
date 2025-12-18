cmake_minimum_required(VERSION 3.16)
project(tokenizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)

add_library(tokenizer_lib
  src/tokenizer.cpp
)
target_include_directories(tokenizer_lib PUBLIC include)

add_executable(tokenizer
  src/main.cpp
)
target_link_libraries(tokenizer PRIVATE tokenizer_lib mongo::mongocxx_shared mongo::bsoncxx_shared)

add_executable(index_builder
  src/index_builder.cpp
)
target_link_libraries(index_builder PRIVATE tokenizer_lib mongo::mongocxx_shared mongo::bsoncxx_shared)

add_executable(bool_search
  src/bool_search.cpp
  src/fwd_index.cpp
  src/inv_index.cpp
  src/boolean_ops.cpp
  src/query_parser.cpp
)
target_link_libraries(bool_search PRIVATE tokenizer_lib)
