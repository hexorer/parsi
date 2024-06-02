find_package(Catch2 CONFIG REQUIRED)

add_executable(${PROJECT_NAME}-tests)

target_sources(${PROJECT_NAME}-tests
    PRIVATE
        parsers.cpp
        bitset.cpp
        charset.cpp
        rtparser.cpp
        fixed_string.cpp
        parsi-c.cpp
        extra_msgpack.cpp
)

target_link_libraries(${PROJECT_NAME}-tests
    PRIVATE
        ${PROJECT_NAME}
        ${PROJECT_NAME}-options
        parsi-c
        Catch2::Catch2
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CTest)
include(Catch)

if (NOT ANDROID)
    catch_discover_tests(${PROJECT_NAME}-tests)
endif()

if (PARSI_ENABLE_COVERAGE)
    find_program(GCOVR_PROGRAM gcovr REQUIRED)

    set(COVERAGE_DIR ${PROJECT_BINARY_DIR}/coverage)

    add_custom_target(coverage
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        COMMAND ctest -C Debug --output-on-failure
	    COMMAND ${GCOVR_PROGRAM}
                -r "${PROJECT_SOURCE_DIR}/include"
                --print-summary
                --fail-under-line 70
                --fail-under-branch 70
                --html "${COVERAGE_DIR}/index.html" --html-details
                --xml "${COVERAGE_DIR}/coverage.xml"
                --json "${COVERAGE_DIR}/coverage.json"
                --object-directory=${PROJECT_BINARY_DIR}
        DEPENDS ${PROJECT_NAME}-tests
        BYPRODUCTS ${COVERAGE_DIR}/coverage.xml
        VERBATIM
    )
endif()
