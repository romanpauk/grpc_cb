cmake_minimum_required(VERSION 3.22)
project(grpc_cb VERSION 0.1)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS NO)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
enable_testing()
option(GRPCCB_ENABLE_TESTING "enable testing through googletest" ON)

FetchContent_Declare(grpc GIT_REPOSITORY https://github.com/grpc/grpc GIT_TAG master)
FetchContent_MakeAvailable(grpc)

if(GRPCCB_ENABLE_TESTING)
    if(WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG main)
    FetchContent_MakeAvailable(googletest)
endif()

add_library(grpc_cb INTERFACE)
target_include_directories(grpc_cb INTERFACE include)
target_link_libraries(grpc_cb INTERFACE grpc)

target_sources(grpc_cb INTERFACE
    README.md
)

if(GRPCCB_ENABLE_TESTING)
    add_executable(grpc_cb_test
        test/grpc_cb.cpp
    )

    add_test(NAME grpc_cb_test COMMAND grpc_cb_test)
    target_link_libraries(grpc_cb_test grpc_cb gtest_main)
    target_include_directories(grpc_cb_test PRIVATE test)
endif()