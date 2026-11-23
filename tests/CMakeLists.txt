# Test targets. Classification (AGENTS.md §3.1) follows the directory a test
# source lives in rather than a name pattern: everything under unit/ is built
# into the unit binary and labelled "unit", everything under functional/ into
# the functional binary and labelled "functional". Adding a test file is
# enough; there is no separate list to keep in step.

include(GoogleTest)

# Defines one test target: sources from `directory`, every discovered test
# labelled `label`. Extra arguments are added as further sources.
function(ld_creator_add_test_target target directory label)
    file(GLOB target_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/*.cpp")
    if(NOT target_sources)
        message(FATAL_ERROR "No test sources found in ${directory}")
    endif()

    add_executable(${target} ${target_sources} ${ARGN})

    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")

    target_link_libraries(${target}
        PRIVATE ld_creator_core GTest::gtest GTest::gtest_main)

    target_compile_definitions(${target}
        PRIVATE
            LD_CREATOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
            LD_CREATOR_BINARY_DIR="${CMAKE_BINARY_DIR}"
    )

    gtest_discover_tests(${target}
        PROPERTIES LABELS "${label}"
        DISCOVERY_TIMEOUT 60)
endfunction()

ld_creator_add_test_target(ld_creator_unit_tests unit unit)

ld_creator_add_test_target(ld_creator_functional_tests functional functional)
# Functional tests drive the real executable, so it must be built first.
add_dependencies(ld_creator_functional_tests ld-creator)
target_compile_definitions(ld_creator_functional_tests
    PRIVATE LD_CREATOR_EXECUTABLE="$<TARGET_FILE:ld-creator>")

# Aggregate: one stable name that builds every test binary. CI and README.md
# build this rather than naming each binary, so splitting or adding a suite
# does not break them.
add_custom_target(ld_creator_tests)
add_dependencies(ld_creator_tests
    ld_creator_unit_tests
    ld_creator_functional_tests)

set(ld_creator_test_targets
    ld_creator_unit_tests
    ld_creator_functional_tests
    PARENT_SCOPE)
