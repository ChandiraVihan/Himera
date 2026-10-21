cmake_minimum_required(VERSION 3.20)
project(HimeraTrooperTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Vendored AI core (plain C++, no Unreal). Never edit these files here.
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/HimeraAI/Source/HimeraAI/Core)

add_library(himera_core
    ${CORE_DIR}/src/goap.cpp
    ${CORE_DIR}/src/hollow.cpp
    ${CORE_DIR}/src/dstar.cpp
    ${CORE_DIR}/src/director.cpp)
target_include_directories(himera_core PUBLIC ${CORE_DIR}/include)
if(MSVC)
    target_compile_options(himera_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(himera_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(trooper_tests trooper_tests.cpp)
target_link_libraries(trooper_tests PRIVATE himera_core)

add_executable(astar_compare astar_compare.cpp)
target_link_libraries(astar_compare PRIVATE himera_core)
if(NOT MSVC)
    target_compile_options(trooper_tests PRIVATE -Wall -Wextra)
    target_compile_options(astar_compare PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME TrooperScenarios COMMAND trooper_tests)
add_test(NAME DStarVsAStarCompare COMMAND astar_compare ${CMAKE_CURRENT_SOURCE_DIR}/results.csv)

# Drift check: needs a path to the AI repo in HIMERA_AI_REPO. Skipped (not failed) when unset.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME VendoredCoreDriftCheck
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_sync.py)
    set_tests_properties(VendoredCoreDriftCheck PROPERTIES SKIP_RETURN_CODE 77)
endif()
