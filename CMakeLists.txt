cmake_minimum_required(VERSION 3.20)
project(vertex_matching_engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

option(VERTEX_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_library(vertex_engine STATIC
    src/application/exchange/exchange_core.cpp
    src/application/exchange/exchange_order.cpp
    src/application/exchange/exchange_settlement.cpp
    src/application/exchange/exchange_wallet.cpp
    src/application/trade_history.cpp
    src/application/order_meta_store.cpp
    src/application/order_history.cpp
    src/domain/user.cpp
    src/domain/wallet.cpp
    src/domain/trade.cpp
    src/engine/order_book.cpp
    src/engine/market_worker.cpp
    src/engine/market_dispatcher.cpp
)

target_include_directories(vertex_engine PUBLIC include)

add_executable(vertex_app 
    app/main.cpp
    app/cli/cli_app.cpp
    app/cli/printer.cpp
    app/cli/tokenizer.cpp
    app/cli/parser.cpp
)

target_link_libraries(vertex_app PRIVATE vertex_engine)

add_executable(vertex_bench 
    bench/main.cpp
    bench/benchmark_runner.cpp
)

target_link_libraries(vertex_bench PRIVATE vertex_engine)

if (MSVC)
    target_compile_options(vertex_app PRIVATE /W4)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vertex_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (VERTEX_ENABLE_TSAN)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_library(vertex_tsan_flags INTERFACE)
        target_compile_options(vertex_tsan_flags INTERFACE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
            -O1
        )
        target_link_options(vertex_tsan_flags INTERFACE -fsanitize=thread)
    else()
        message(FATAL_ERROR "TSAN is supported only on Linux with GCC/Clang")
    endif()
endif()

if (TARGET vertex_tsan_flags)
    target_link_libraries(vertex_engine PRIVATE vertex_tsan_flags)
endif()

add_subdirectory(tests)
