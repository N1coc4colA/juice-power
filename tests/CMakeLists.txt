find_package(GTest REQUIRED)

add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE CXX_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")
file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

add_executable(tests ${C_SOURCES} ${CXX_HEADERS} ${CXX_SOURCES})

target_link_libraries(
tests
    PRIVATE
        GTest::GTest
        juice-power
)

add_test(juice-power_gtest tests)
