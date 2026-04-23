cmake_minimum_required(VERSION 3.16)
project(hook_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HOOK_ENABLE_PAPI "Enable PAPI PMU support" ON)

add_library(hook SHARED
    hook.cpp
    src/common.cpp
    src/pmu_recorder.cpp
    src/relation_rules.cpp
    src/scope_registry.cpp
    src/symbol_resolver.cpp
    src/trace_logger.cpp
)

target_include_directories(hook
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hook
    PRIVATE
        dl
)

target_compile_definitions(hook
    PRIVATE
        _GNU_SOURCE
)

if(HOOK_ENABLE_PAPI)
    find_library(PAPI_LIBRARY NAMES papi)
    if(PAPI_LIBRARY)
        target_compile_definitions(hook PRIVATE HOOK_ENABLE_PAPI)
        target_link_libraries(hook PRIVATE ${PAPI_LIBRARY})
    else()
        message(WARNING "HOOK_ENABLE_PAPI is ON but libpapi was not found. Building without PAPI.")
    endif()
endif()

set_target_properties(hook PROPERTIES
    OUTPUT_NAME hook
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
