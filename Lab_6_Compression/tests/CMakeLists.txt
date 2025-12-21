cmake_minimum_required(VERSION 3.16)
project(lab6_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

set(BOOLEAN_DIR ${CMAKE_CURRENT_LIST_DIR}/../cpp/boolean)

add_executable(unit_tests
  unit_tests.cpp
  ${BOOLEAN_DIR}/src/core/tokenizer.cpp
  ${BOOLEAN_DIR}/src/core/fwd_index.cpp
  ${BOOLEAN_DIR}/src/core/inv_index.cpp
  ${BOOLEAN_DIR}/src/core/boolean_ops.cpp
  ${BOOLEAN_DIR}/src/core/query_parser.cpp
)

target_include_directories(unit_tests PRIVATE
  ${BOOLEAN_DIR}/include
)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(unit_tests EXTRA_ARGS -s)
