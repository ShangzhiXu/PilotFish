cmake_minimum_required(VERSION 3.26)
project(dynamorio C)

set(CMAKE_C_STANDARD 11)

add_executable(dynamorio
        my_client.c
        my_client.cpp
        my_client.h
        CFGExtractor/graph.cpp
        CFGExtractor/graph.h
        CFGExtractor/node.cpp
        CFGExtractor/node.h
        CFGExtractor/client.cpp
        CFGExtractor/staticAnalyzer.cpp
        CFGExtractor/staticAnalyzer.h
        srcML_test.cpp
        CFGExtractor/buffer_overflow.cpp
        taintAnalysisTest.cpp
        ValueExtractor/client.cpp
        ValueExtractor/utils.h
        ValueExtractor/utils.cpp
        ValueExtractor/temp.cpp
        CFGExtractor/extract_vars.cpp
        CFGExtractor/preprocess.cpp
        backup/staticAnalyzer_11_6.cpp
)
