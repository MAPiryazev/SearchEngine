cmake_minimum_required(VERSION 3.16)
project(search_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)

add_library(tokenizer_lib
  src/core/tokenizer.cpp
)
target_include_directories(tokenizer_lib PUBLIC include)

add_executable(tokenizer
  src/apps/tokenizer_main.cpp
)
target_link_libraries(tokenizer PRIVATE tokenizer_lib mongo::mongocxx_shared mongo::bsoncxx_shared)

add_executable(index_builder
  src/apps/index_builder_main.cpp
)
target_link_libraries(index_builder PRIVATE tokenizer_lib mongo::mongocxx_shared mongo::bsoncxx_shared)

add_executable(bool_search
  src/apps/bool_search_main.cpp
  src/core/fwd_index.cpp
  src/core/inv_index.cpp
  src/core/boolean_ops.cpp
  src/core/query_parser.cpp
)
target_link_libraries(bool_search PRIVATE tokenizer_lib)

add_executable(index_builder6
  src/apps/lab6/index_builder6_main.cpp
  src/core/lab6/vbyte.cpp
)
target_link_libraries(index_builder6 PRIVATE tokenizer_lib mongo::mongocxx_shared mongo::bsoncxx_shared)

add_executable(bool_search6
  src/apps/lab6/bool_search6_main.cpp
  src/core/fwd_index.cpp
  src/core/boolean_ops.cpp
  src/core/query_parser.cpp
  src/core/lab6/inv_index6.cpp
  src/core/lab6/vbyte.cpp
)
target_link_libraries(bool_search6 PRIVATE tokenizer_lib)

add_executable(rank_search8
  src/apps/lab8/rank_search8_main.cpp
  src/core/fwd_index.cpp
  src/core/boolean_ops.cpp
  src/core/query_parser.cpp
  src/core/lab6/inv_index6.cpp
  src/core/lab6/vbyte.cpp
)
target_link_libraries(rank_search8 PRIVATE tokenizer_lib)