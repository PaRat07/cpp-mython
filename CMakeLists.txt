cmake_minimum_required(VERSION 3.23)
project(cpp_mython_interpreter)

set(CMAKE_CXX_STANDARD 17)

add_executable(cpp_mython_interpreter
        lexer.cpp
        lexer.h
        lexer_test_open.cpp
        main.cpp
        test_runner_p.h)
