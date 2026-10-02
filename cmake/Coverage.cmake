find_program(LLVM_PROFDATA_EXECUTABLE NAMES llvm-profdata HINTS "${LLVM_BIN_DIR}" REQUIRED)
find_program(LLVM_COV_EXECUTABLE NAMES llvm-cov HINTS "${LLVM_BIN_DIR}" REQUIRED)

add_custom_target(
    coverage
    COMMAND
        ${CMAKE_COMMAND} -DBINARY_DIR=${CMAKE_BINARY_DIR}
        -DTESTS=$<TARGET_FILE:advanced_platformer_tests> -DCTEST=${CMAKE_CTEST_COMMAND}
        -DLLVM_PROFDATA=${LLVM_PROFDATA_EXECUTABLE} -DLLVM_COV=${LLVM_COV_EXECUTABLE} -P
        ${PROJECT_SOURCE_DIR}/cmake/RunCoverage.cmake
    DEPENDS advanced_platformer_tests
    COMMENT "Measuring which of the project's lines the tests run"
    USES_TERMINAL
    VERBATIM
)
