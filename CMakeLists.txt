cmake_minimum_required(VERSION 3.13.4)
project(Obfuscator)

find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

add_library(ObfuscationPass MODULE 
    src/ObfuscationPass.cpp
    src/RulesMBA.cpp
    src/LinearMBA.cpp
    src/Flattening.cpp
)

set_target_properties(ObfuscationPass PROPERTIES
    CXX_STANDARD 17
    COMPILE_FLAGS "-fno-rtti"
)
