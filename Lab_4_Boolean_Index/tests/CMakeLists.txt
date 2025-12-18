cmake_minimum_required(VERSION 3.16)
project(lab4_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

set(TOKENIZER_DIR ${CMAKE_CURRENT_LIST_DIR}/../cpp/tokenizer)
set(TOKENIZER_SRC ${TOKENIZER_DIR}/src)
set(TOKENIZER_INC ${TOKENIZER_DIR}/include)

add_library(tokenizer_core OBJECT
  ${TOKENIZER_SRC}/tokenizer.cpp
  ${TOKENIZER_SRC}/fwd_index.cpp
  ${TOKENIZER_SRC}/inv_index.cpp
  ${TOKENIZER_SRC}/boolean_ops.cpp
  ${TOKENIZER_SRC}/query_parser.cpp
)
target_include_directories(tokenizer_core PUBLIC ${TOKENIZER_INC})

add_executable(unit_tests
  unit_tests.cpp
  $<TARGET_OBJECTS:tokenizer_core>
)
target_include_directories(unit_tests PRIVATE ${TOKENIZER_INC})
add_test(NAME unit_tests COMMAND unit_tests)