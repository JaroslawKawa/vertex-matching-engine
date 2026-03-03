add_executable(vertex_tests
    ${CMAKE_SOURCE_DIR}/app/cli/tokenizer.cpp
    ${CMAKE_SOURCE_DIR}/app/cli/parser.cpp
    ${CMAKE_SOURCE_DIR}/app/cli/cli_app.cpp
    ${CMAKE_SOURCE_DIR}/app/cli/printer.cpp
    core/strong_id_tests.cpp
    core/id_generator_tests.cpp
    core/asset_market_tests.cpp
    application/trade_history_tests.cpp
    application/exchange_tests.cpp
    application/exchange_concurrency_tests.cpp
    cli/tokenizer_tests.cpp
    cli/parser_tests.cpp
    cli/cli_app_tests.cpp
    cli/printer_tests.cpp
    domain/user_tests.cpp
    domain/wallet_tests.cpp
    domain/trade_tests.cpp
    engine/order_book_tests.cpp
    engine/matching_engine_tests.cpp
    engine/market_worker_tests.cpp
    engine/market_dispatcher_tests.cpp
)

target_link_libraries(vertex_tests
    PRIVATE
        vertex_engine
        GTest::gtest_main

)
if (TARGET vertex_tsan_flags)
    target_link_libraries(vertex_tests PRIVATE vertex_tsan_flags)
endif()

target_include_directories(vertex_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

if (TARGET vertex_tsan_flags)
    add_custom_target(tsan
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS vertex_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
    )
endif()

include(GoogleTest)
gtest_discover_tests(vertex_tests)
