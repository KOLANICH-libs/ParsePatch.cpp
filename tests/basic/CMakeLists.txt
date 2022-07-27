find_package(GTest)

add_executable("tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests.cpp")
target_link_libraries("tests" GTest::GTest GTest::Main "lib${PROJECT_NAME}")

harden("tests")
add_sanitizers("tests")
#gtest_discover_tests("tests")
