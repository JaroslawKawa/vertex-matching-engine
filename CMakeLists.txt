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

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_library(vertex_engine STATIC
    src/application/exchange.cpp
    src/application/trade_history.cpp
    src/domain/user.cpp
    src/domain/wallet.cpp
    src/domain/trade.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
)

target_include_directories(vertex_engine PUBLIC include)

add_executable(vertex_app app/main.cpp)

target_link_libraries(vertex_app PRIVATE vertex_engine)

if (MSVC)
    target_compile_options(vertex_app PRIVATE /W4)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vertex_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(tests)
