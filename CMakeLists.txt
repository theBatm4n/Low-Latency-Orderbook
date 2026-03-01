cmake_minimum_required(VERSION 3.14)
project(OrderBookProject VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
enable_testing()

# Add the googletest subdirectory
add_subdirectory(googletest)

# Main executable
add_executable(orderbook_main 
    main.cpp 
    orderbook.cpp
    OrderLockFree.cpp
    OrderbookLockFree.cpp
    )

# Test executable
add_executable(OrderBookTest
    test/OrderBookTest.cpp
    orderbook.cpp
    OrderLockFree.cpp
    OrderbookLockFree.cpp
)

target_link_libraries(OrderBookTest gtest_main) 
target_include_directories(OrderBookTest PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR})  

include(GoogleTest)
gtest_discover_tests(OrderBookTest)