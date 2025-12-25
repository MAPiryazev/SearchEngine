cmake_minimum_required(VERSION 3.16)
project(search_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)

# Common library (tokenizer, fwd_index, boolean_ops, query_parser)
add_library(common_lib
  common/src/tokenizer.cpp
  common/src/fwd_index.cpp
  common/src/boolean_ops.cpp
  common/src/query_parser.cpp
)
target_include_directories(common_lib PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

# Base applications (using old inv_index)
add_executable(tokenizer
  base/apps/tokenizer_main.cpp
)
target_link_libraries(tokenizer PRIVATE common_lib mongo::mongocxx_shared mongo::bsoncxx_shared)
target_include_directories(tokenizer PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/base/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

add_executable(index_builder
  base/apps/index_builder_main.cpp
)
target_link_libraries(index_builder PRIVATE common_lib mongo::mongocxx_shared mongo::bsoncxx_shared)
target_include_directories(index_builder PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/base/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

add_executable(bool_search
  base/apps/bool_search_main.cpp
  base/inv_index.cpp
)
target_link_libraries(bool_search PRIVATE common_lib)
target_include_directories(bool_search PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/base/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

# Lab6 applications
add_executable(index_builder6
  lab6/apps/index_builder6_main.cpp
  lab6/src/vbyte.cpp
)
target_link_libraries(index_builder6 PRIVATE common_lib mongo::mongocxx_shared mongo::bsoncxx_shared)
target_include_directories(index_builder6 PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/lab6/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

add_executable(bool_search6
  lab6/apps/bool_search6_main.cpp
  lab6/src/inv_index6.cpp
  lab6/src/vbyte.cpp
)
target_link_libraries(bool_search6 PRIVATE common_lib)
target_include_directories(bool_search6 PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/lab6/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

# Lab8 applications
add_executable(rank_search8
  lab8/apps/rank_search8_main.cpp
  lab6/src/inv_index6.cpp
  lab6/src/vbyte.cpp
)
target_link_libraries(rank_search8 PRIVATE common_lib)
target_include_directories(rank_search8 PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/lab6/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)
