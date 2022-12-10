cmake_minimum_required(VERSION 3.23)
project(cpp_mython_interpreter)

set(CMAKE_CXX_STANDARD 17)

add_executable(cpp_mython_interpreter
        runtime.h
        runtime.cpp
        main.cpp
        lexer.cpp
        lexer.h
        parse.cpp
        parse.h
        lexer_test_open.cpp
        parse_test.cpp
        runtime_test.cpp
        statement.cpp
        statement.h
        statement_test.cpp
        test_runner_p.h)
