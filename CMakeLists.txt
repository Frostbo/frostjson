cmake_minimum_required(VERSION 3.0.0)
project(frostjson VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_library(frostjson_lib frostjson.cpp)
#add_executable(frostjson frostjson.cpp)
add_executable(frostjson_test test.cpp)
target_link_libraries(frostjson_test frostjson_lib)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
