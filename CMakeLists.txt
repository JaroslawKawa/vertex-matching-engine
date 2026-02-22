cmake_minimum_required(VERSION 3.20)
project(vertex_matching_engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vertex_engine STATIC
    src/domain/user.cpp
    src/domain/wallet.cpp
    src/domain/order.cpp
    src/domain/limit_order.cpp
    src/application/exchange.cpp
)

target_include_directories(vertex_engine PUBLIC include)

add_executable(vertex_app app/main.cpp)

target_link_libraries(vertex_app PRIVATE vertex_engine)

target_compile_options(vertex_app PRIVATE /W4)