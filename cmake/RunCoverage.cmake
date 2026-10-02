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
execute_process(
    COMMAND
        "${LLVM_COV}" export "${TESTS}" "-instr-profile=${COVERAGE_DIR}/tests.profdata"
        "-ignore-filename-regex=${IGNORED}" -summary-only
    OUTPUT_VARIABLE EXPORTED
    COMMAND_ERROR_IS_FATAL ANY
)

set(SUMMARY "| Measure | Covered | Total | Cover |\n| --- | ---: | ---: | ---: |\n")
foreach(measure lines functions regions branches)
    string(
        JSON covered
        GET "${EXPORTED}"
        data
        0
        totals
        ${measure}
        covered
    )
    string(
        JSON count
        GET "${EXPORTED}"
        data
        0
        totals
        ${measure}
        count
    )
    if(count EQUAL 0)
        set(cover "-")
    else()
        math(EXPR hundredths "(${covered} * 20000 / ${count} + 1) / 2")
        math(EXPR whole "${hundredths} / 100")
        math(EXPR fraction "${hundredths} % 100")
        if(fraction LESS 10)
            set(fraction "0${fraction}")
        endif()
        set(cover "${whole}.${fraction}%")
    endif()
    string(SUBSTRING "${measure}" 0 1 first)
    string(TOUPPER "${first}" first)
    string(SUBSTRING "${measure}" 1 -1 rest)
    string(APPEND SUMMARY "| ${first}${rest} | ${covered} | ${count} | ${cover} |\n")
endforeach()
file(WRITE "${COVERAGE_DIR}/summary.md" "${SUMMARY}")

file(READ "${COVERAGE_DIR}/report.txt" REPORT)
message("${REPORT}")
message("${SUMMARY}")
message("Line-by-line coverage: ${COVERAGE_DIR}/html/index.html")
