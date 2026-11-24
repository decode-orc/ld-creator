cmake_minimum_required(VERSION 3.20)

# Builds must be out of source. An in-source configure scatters CMakeCache.txt,
# CMakeFiles/ and generated headers through the tree, and a later out-of-source
# configure then silently picks up the stale cache. Refuse it up front.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Configure into a separate build "
        "directory, e.g.:\n"
        "  cmake -S . -B build -G Ninja\n"
        "Remove the CMakeCache.txt and CMakeFiles/ just created in the source "
        "tree first.")
endif()

project(ld-creator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# RF modulation is compute bound, so an unoptimised default build is never what
# a developer wants. Single-config generators (Makefiles, plain Ninja) leave
# CMAKE_BUILD_TYPE empty unless told otherwise, which compiles with no -O flag
# at all; default those to Release. Multi-config generators (Ninja Multi-Config,
# Visual Studio, Xcode) pick the configuration at build time and are left alone.
get_property(LD_CREATOR_MULTI_CONFIG_GENERATOR GLOBAL
    PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(LD_CREATOR_MULTI_CONFIG_GENERATOR)
    message(STATUS "Build type: selected per build by the multi-config generator")
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# Export compile_commands.json for IDE tooling (clangd, VS Code cpptools).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Version string: PROJECT_VERSION_OVERRIDE (e.g. from a Nix build) wins,
# otherwise the short git commit hash with a "-dirty" suffix when the working
# tree has uncommitted changes.
if(DEFINED PROJECT_VERSION_OVERRIDE)
    set(LD_CREATOR_VERSION "${PROJECT_VERSION_OVERRIDE}")
else()
    find_package(Git QUIET)
    set(GIT_COMMIT_HASH "unknown")
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(GIT_COMMIT_HASH STREQUAL "")
            set(GIT_COMMIT_HASH "unknown")
        else()
            execute_process(
                COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_DIRTY
                ERROR_QUIET
            )
            if(NOT GIT_DIRTY EQUAL 0)
                set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
            endif()
        endif()
    endif()
    set(LD_CREATOR_VERSION "${GIT_COMMIT_HASH}")
endif()
message(STATUS "ld-creator version: ${LD_CREATOR_VERSION}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ld_creator_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ld_creator_version.h
    @ONLY
)

# The test suites are a development and CI concern. Packaging builds may turn
# them off so neither Google Test nor the test sources are needed to produce the
# shipped binary.
option(LD_CREATOR_BUILD_TESTS "Build the unit and functional test suites" ON)

find_package(spdlog REQUIRED)
# CVBS metadata sidecars (.meta) are SQLite databases (CVBS file format
# specification, docs-tech/cvbs-file-format-specification).
find_package(SQLite3 REQUIRED)
# Project files are YAML, as in videosynth.
find_package(yaml-cpp REQUIRED)

# yaml-cpp 0.8 exports the namespaced target; older packages define only the
# bare name, which reaches the linker as "-lyaml-cpp" and resolves only if the
# library sits in a directory already being searched. Link the imported target
# where it exists.
if(TARGET yaml-cpp::yaml-cpp)
    set(LD_CREATOR_YAML_CPP_TARGET yaml-cpp::yaml-cpp)
else()
    set(LD_CREATOR_YAML_CPP_TARGET yaml-cpp)
endif()

if(LD_CREATOR_BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

# Core library: everything except the CLI entry point, so the test suites link
# the same code the application runs. Add new sources here as modules land
# (see docs-tech/design/high-level-design.md, "Module layout").
add_library(ld_creator_core
    src/cli/command_line.cpp
    src/cvbs_input/cvbs_metadata.cpp
    src/cvbs_input/cvbs_video_input.cpp
    src/diagnostics/byte_sink.cpp
    src/diagnostics/tap_writer.cpp
    src/optical_readout/readout_transfer.cpp
    src/pipeline/run_setup.cpp
    src/project/number_format.cpp
    src/project/project_loader.cpp
    src/project/project_parser.cpp
    src/project/project_validator.cpp
    src/project/resolved_project.cpp
    src/project/sha256.cpp
    src/rf_sample_writer/rf_sample_writer.cpp
)

target_include_directories(ld_creator_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ld_creator_core
    PUBLIC
        spdlog::spdlog
        SQLite::SQLite3
    PRIVATE
        ${LD_CREATOR_YAML_CPP_TARGET}
)

add_executable(ld-creator
    src/main.cpp
)

target_include_directories(ld-creator
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(ld-creator
    PRIVATE
        ld_creator_core
)

include(GNUInstallDirs)
install(TARGETS ld-creator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(LD_CREATOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# The clang-format gate is a dev/CI check. Packaging builds turn it off: the
# formatter available there is an unrelated version whose output differs, and a
# style difference must not fail a release build.
option(LD_CREATOR_ENABLE_CLANG_FORMAT
    "Run clang-format checks as part of the build" ON)

if(LD_CREATOR_ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
else()
    set(CLANG_FORMAT_EXE "")
    message(STATUS "clang-format disabled (LD_CREATOR_ENABLE_CLANG_FORMAT=OFF)")
endif()

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
    )

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/clang-format-stamps")

    set(FORMAT_STAMPS)
    foreach(source IN LISTS ALL_FORMAT_SOURCES)
        file(RELATIVE_PATH rel_source "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
        string(REPLACE "/" "_" stamp_name "${rel_source}")
        string(REPLACE "." "_" stamp_name "${stamp_name}")
        set(stamp "${CMAKE_CURRENT_BINARY_DIR}/clang-format-stamps/${stamp_name}.stamp")
        add_custom_command(
            OUTPUT "${stamp}"
            COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror "${source}"
            COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
            DEPENDS "${source}"
            COMMENT "clang-format: ${rel_source}"
            VERBATIM
        )
        list(APPEND FORMAT_STAMPS "${stamp}")
    endforeach()

    add_custom_target(clang_format_check ALL DEPENDS ${FORMAT_STAMPS})
    add_dependencies(ld_creator_core clang_format_check)
elseif(LD_CREATOR_ENABLE_CLANG_FORMAT)
    message(STATUS "clang-format not found — format checks disabled")
endif()

# clang-tidy runs as part of the build for local development and CI, where the
# dev shell supplies a matching, wrapped clang-tools. The package build (nix
# build / nix run) must leave this OFF: its sandbox only exposes the stdenv's
# unwrapped clang-tidy, which cannot resolve libc++ headers.
option(LD_CREATOR_ENABLE_CLANG_TIDY
    "Run clang-tidy static analysis as part of the build" ON)

if(LD_CREATOR_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)

    if(CLANG_TIDY_EXE)
        set_target_properties(ld_creator_core ld-creator
            ${ld_creator_test_targets}
            PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
    else()
        message(STATUS "clang-tidy not found — static analysis disabled")
    endif()
else()
    message(STATUS "clang-tidy disabled (LD_CREATOR_ENABLE_CLANG_TIDY=OFF)")
endif()
