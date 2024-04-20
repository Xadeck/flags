add_library(flags INTERFACE flags.h)

add_executable(
  flags_test
  flags_test.cc
)

target_link_libraries(
  flags_test
  flags
  GTest::gmock
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(flags_test)
