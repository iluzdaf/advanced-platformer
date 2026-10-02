set(COVERAGE_DIR "${BINARY_DIR}/coverage")
file(REMOVE_RECURSE "${COVERAGE_DIR}")
file(MAKE_DIRECTORY "${COVERAGE_DIR}")

cmake_host_system_information(RESULT PROCESSORS QUERY NUMBER_OF_LOGICAL_CORES)
set(ENV{LLVM_PROFILE_FILE} "${COVERAGE_DIR}/tests-%4m.profraw")
execute_process(
    COMMAND "${CTEST}" --test-dir "${BINARY_DIR}" --parallel ${PROCESSORS} --output-on-failure
    RESULT_VARIABLE TESTS_RESULT
)
if(NOT TESTS_RESULT EQUAL 0)
    message(FATAL_ERROR "The tests failed, so coverage was not measured")
endif()

file(GLOB PROFILES "${COVERAGE_DIR}/*.profraw")
execute_process(
    COMMAND "${LLVM_PROFDATA}" merge -sparse -o "${COVERAGE_DIR}/tests.profdata" ${PROFILES}
    COMMAND_ERROR_IS_FATAL ANY
)

set(IGNORED "/(external|tests|build)/")
execute_process(
    COMMAND
        "${LLVM_COV}" report "${TESTS}" "-instr-profile=${COVERAGE_DIR}/tests.profdata"
        "-ignore-filename-regex=${IGNORED}"
    OUTPUT_FILE "${COVERAGE_DIR}/report.txt"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND
        "${LLVM_COV}" show "${TESTS}" "-instr-profile=${COVERAGE_DIR}/tests.profdata"
        "-ignore-filename-regex=${IGNORED}" -format=html "-output-dir=${COVERAGE_DIR}/html"
    COMMAND_ERROR_IS_FATAL ANY
)
file(READ "${COVERAGE_DIR}/report.txt" REPORT)
message("${REPORT}")
message("Line-by-line coverage: ${COVERAGE_DIR}/html/index.html")
