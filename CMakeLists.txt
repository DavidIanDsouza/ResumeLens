cmake_minimum_required(VERSION 3.10)
project(ResumeAI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include_directories(include)

add_executable(resume_ai 
    src/main.cpp
    src/FileHandler.cpp
    src/FormattingAnalyzer.cpp
    src/JobMatch.cpp
    src/KeywordAlerts.cpp
    src/KeywordExtractor.cpp
    src/PDFExporter.cpp
    src/ResumeAI.cpp
)