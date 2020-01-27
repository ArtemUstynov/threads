cmake_minimum_required(VERSION 3.13)
project(sample)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
link_libraries(/Users/ustynov/Downloads/sample/x86_64-darwin17/libprogtest_solver.a)

add_executable(sample
        x86_64-darwin17/libprogtest_solver.a
        common.h
        progtest_solver.h
        sample_tester.cpp
        sample_tester.h
        solution.cpp)
