#Unit tests for src/conversions.c using Zephyr's ztest framework.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(luntnode_conversions_test)

target_include_directories(app PRIVATE ../../src)
target_sources(app PRIVATE 
src/test_conversions.c
../../src/conversions.c
)