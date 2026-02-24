add_executable(vertex_tests
    domain/user_tests.cpp
    domain/wallet_tests.cpp
    domain/order_tests.cpp
    domain/trade_tests.cpp
    engine/order_book_tests.cpp
    engine/matching_engine_tests.cpp
)

target_link_libraries(vertex_tests
    PRIVATE
        vertex_engine
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vertex_tests)
