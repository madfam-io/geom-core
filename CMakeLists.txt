cmake_minimum_required(VERSION 3.15)
project(geom-core VERSION 1.0.0 LANGUAGES CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" ON)
option(BUILD_WASM_BINDINGS "Build WebAssembly bindings using Embind" OFF)

# ===========================
# Core Library (geom_core_lib)
# ===========================
add_library(geom_core_lib SHARED
    src/Analyzer.cpp
    src/Mesh.cpp
    src/Spatial.cpp
)

target_include_directories(geom_core_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set library output name
set_target_properties(geom_core_lib PROPERTIES
    OUTPUT_NAME "geom_core"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ===========================
# Python Bindings
# ===========================
if(BUILD_PYTHON_BINDINGS)
    # FetchContent to download pybind11
    include(FetchContent)

    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )

    FetchContent_MakeAvailable(pybind11)

    # Create Python module
    pybind11_add_module(geom_core_py
        bindings/python/PyBindEntry.cpp
    )

    # Link the core library
    target_link_libraries(geom_core_py PRIVATE geom_core_lib)

    # Set output directory for easier testing
    set_target_properties(geom_core_py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
    )

    message(STATUS "Python bindings enabled")
endif()

# ===========================
# WebAssembly Bindings
# ===========================
if(BUILD_WASM_BINDINGS)
    # WASM build: create a standalone module
    add_executable(geom_core_wasm
        src/Analyzer.cpp
        src/Mesh.cpp
        src/Spatial.cpp
        bindings/wasm/WasmEntry.cpp
    )

    target_include_directories(geom_core_wasm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Emscripten linker flags
    set_target_properties(geom_core_wasm PROPERTIES
        LINK_FLAGS "\
            -lembind \
            -s MODULARIZE=1 \
            -s EXPORT_NAME='GeomCore' \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
            -s ENVIRONMENT=web \
            --bind"
        OUTPUT_NAME "geom_core"
        SUFFIX ".js"
    )

    # Set output directory
    set_target_properties(geom_core_wasm PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
    )

    message(STATUS "WebAssembly bindings enabled")
endif()

# ===========================
# Summary
# ===========================
message(STATUS "")
message(STATUS "===================================")
message(STATUS "geom-core Configuration Summary")
message(STATUS "===================================")
message(STATUS "Version:              ${PROJECT_VERSION}")
message(STATUS "C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "Python Bindings:      ${BUILD_PYTHON_BINDINGS}")
message(STATUS "WASM Bindings:        ${BUILD_WASM_BINDINGS}")
message(STATUS "===================================")
message(STATUS "")
