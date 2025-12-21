cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(tokenizer_tests
  test_tokenizer.cpp
)

target_link_libraries(tokenizer_tests PRIVATE Catch2::Catch2WithMain)

enable_testing()
add_test(NAME tokenizer_tests COMMAND tokenizer_tests)
