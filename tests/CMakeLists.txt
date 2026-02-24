add_executable(vertex_tests
    domain/wallet_tests.cpp
)

target_link_libraries(vertex_tests
    PRIVATE
        vertex_engine
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vertex_tests)