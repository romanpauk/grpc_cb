cmake_minimum_required(VERSION 3.22)
project(grpc_cb VERSION 0.1)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS NO)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
enable_testing()
option(GRPCCB_ENABLE_TESTING "enable testing through googletest" ON)
option(GRPCCB_ENABLE_BENCHMARK "enable benchmarking through googlebenchmark" ON)
option(GRPCCB_ENABLE_ASIO "enable tests and benchmarks based on asio" ON)

FetchContent_Declare(grpc GIT_REPOSITORY https://github.com/grpc/grpc GIT_TAG master)
FetchContent_MakeAvailable(grpc)

if(GRPCCB_ENABLE_TESTING)
    if(WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG main)
    FetchContent_MakeAvailable(googletest)
endif()

if(GRPCCB_ENABLE_BENCHMARK)
    FetchContent_Declare(benchmark GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG master) 
       
    set(BENCHMARK_ENABLE_TESTING off)
    FetchContent_MakeAvailable(benchmark)
endif()

if(GRPCCB_ENABLE_ASIO)
    FetchContent_Declare(asio GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git GIT_TAG master)
    FetchContent_MakeAvailable(asio)
    add_library(asio INTERFACE)
    target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
endif()

add_library(grpc_cb INTERFACE)
target_include_directories(grpc_cb INTERFACE include)
target_link_libraries(grpc_cb INTERFACE grpc++ grpc)
if(GRPCCB_ENABLE_ASIO)
    target_compile_definitions(grpc_cb INTERFACE GRPCCB_ASIO_ENABLED)
    target_link_libraries(grpc_cb INTERFACE asio)
endif()

target_sources(grpc_cb INTERFACE
    include/grpc_cb/steady_timer.h
    include/grpc_cb/io_context.h
    include/grpc_cb/io_handler.h
    README.md
)

if(GRPCCB_ENABLE_TESTING)
    add_executable(grpc_cb_test
        test/grpc_cb.cpp
    )

    add_test(NAME grpc_cb_test COMMAND grpc_cb_test)
    target_link_libraries(grpc_cb_test grpc_cb gtest_main grpc)
    target_include_directories(grpc_cb_test PRIVATE test)
endif()

if(GRPCCB_ENABLE_BENCHMARK)
    add_executable(grpc_cb_benchmark
        benchmark/main.cpp
        benchmark/grpc_cb.cpp
    )

    target_link_libraries(grpc_cb_benchmark grpc_cb benchmark::benchmark)
    target_include_directories(grpc_cb_benchmark PRIVATE benchmark)
endif()

