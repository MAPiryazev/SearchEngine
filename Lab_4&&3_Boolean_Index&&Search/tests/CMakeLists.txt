cmake_minimum_required(VERSION 3.16)
project(lab4_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

find_package(mongocxx REQUIRED CONFIG)
find_package(bsoncxx REQUIRED CONFIG)

set(TOKENIZER_DIR ${CMAKE_CURRENT_LIST_DIR}/../cpp/tokenizer)

add_executable(unit_tests
  unit_tests.cpp   
  ${TOKENIZER_DIR}/src/tokenizer.cpp
  ${TOKENIZER_DIR}/src/fwd_index.cpp
  ${TOKENIZER_DIR}/src/inv_index.cpp
  ${TOKENIZER_DIR}/src/boolean_ops.cpp
  ${TOKENIZER_DIR}/src/query_parser.cpp
)

target_include_directories(unit_tests PRIVATE ${TOKENIZER_DIR}/include)
target_link_libraries(unit_tests PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)

add_test(NAME unit_tests COMMAND unit_tests)
