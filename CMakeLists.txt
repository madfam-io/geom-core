cmake_minimum_required(VERSION 3.15)
project(geom-core VERSION 1.0.0 LANGUAGES CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" ON)
option(BUILD_WASM_BINDINGS "Build WebAssembly bindings using Embind" OFF)
option(USE_OCCT "Enable Open CASCADE Technology support for STEP files" ON)

# ===========================
# Optional Dependencies
# ===========================
set(OCCT_ENABLED FALSE)

if(USE_OCCT AND NOT EMSCRIPTEN)
    # Try to find OpenCASCADE
    find_package(OpenCASCADE QUIET)

    if(OpenCASCADE_FOUND)
        set(OCCT_ENABLED TRUE)
        message(STATUS "OpenCASCADE found: ${OpenCASCADE_VERSION}")
        message(STATUS "OpenCASCADE include dirs: ${OpenCASCADE_INCLUDE_DIR}")

        # Define OCCT-specific source files
        set(OCCT_SOURCES src/BRepLoader.cpp)
    else()
        # Try pkg-config as fallback (for Linux systems)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(OCCT QUIET OCCT)
            if(OCCT_FOUND)
                set(OCCT_ENABLED TRUE)
                set(OpenCASCADE_INCLUDE_DIR ${OCCT_INCLUDE_DIRS})
                set(OpenCASCADE_LIBRARY_DIR ${OCCT_LIBRARY_DIRS})
                message(STATUS "OpenCASCADE found via pkg-config")
                set(OCCT_SOURCES src/BRepLoader.cpp)
            endif()
        endif()
    endif()

    if(NOT OCCT_ENABLED)
        message(WARNING "OpenCASCADE not found. STEP file support will be disabled.")
        message(WARNING "To enable STEP support, install OpenCASCADE:")
        message(WARNING "  Ubuntu/Debian: sudo apt-get install libocct-data-exchange-dev libocct-ocaf-dev")
        message(WARNING "  macOS: brew install opencascade")
    endif()
endif()

# ===========================
# Core Library (geom_core_lib)
# ===========================
set(CORE_SOURCES
    src/Analyzer.cpp
    src/Mesh.cpp
    src/Spatial.cpp
)

# Add OCCT sources if enabled
if(OCCT_ENABLED)
    list(APPEND CORE_SOURCES ${OCCT_SOURCES})
endif()

add_library(geom_core_lib SHARED ${CORE_SOURCES})

target_include_directories(geom_core_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link OCCT libraries if enabled
if(OCCT_ENABLED)
    # Add OCCT compile definition
    target_compile_definitions(geom_core_lib PRIVATE GC_USE_OCCT)

    # Add OCCT include directories
    if(OpenCASCADE_INCLUDE_DIR)
        target_include_directories(geom_core_lib PRIVATE ${OpenCASCADE_INCLUDE_DIR})
    endif()

    # Link OCCT libraries
    # Required modules: STEP reading, BRep meshing, topology
    set(OCCT_LIBS
        TKernel       # Core kernel
        TKMath        # Math utilities
        TKBRep        # Boundary representation
        TKTopAlgo     # Topology algorithms
        TKMesh        # Meshing algorithms
        TKSTEP        # STEP reader/writer
        TKSTEPBase    # STEP base
        TKSTEPAttr    # STEP attributes
        TKSTEP209     # STEP AP209
        TKXSBase      # Data exchange base
    )

    # Try to link with full library names first
    foreach(lib ${OCCT_LIBS})
        find_library(${lib}_PATH NAMES ${lib}
            HINTS ${OpenCASCADE_LIBRARY_DIR} ${OCCT_LIBRARY_DIRS})
        if(${lib}_PATH)
            target_link_libraries(geom_core_lib PRIVATE ${${lib}_PATH})
        else()
            # Fallback: try linking with library name directly
            target_link_libraries(geom_core_lib PRIVATE ${lib})
        endif()
    endforeach()

    message(STATUS "OCCT support enabled")
endif()

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
message(STATUS "OCCT Support:         ${OCCT_ENABLED}")
message(STATUS "===================================")
message(STATUS "")
