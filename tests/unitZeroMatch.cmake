#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni_unit_tests's zero-match guard (tests/unit/main.cpp), run as a
#  cmake -P script (tests/CMakeLists.txt registers it as
#  unit_tests_zero_match_fails):
#
#      cmake -DUNIT_TESTS=<gweni_unit_tests> [-DEMULATOR=<node>] \
#            -P unitZeroMatch.cmake
#
#  A run whose --test-case matches no case must exit non-zero and say so on
#  stderr. doctest alone exits 0 there, so a case name split at a ';' by
#  doctest_discover_tests registered two tests that passed while the case
#  never ran. Both the exit status and the message are required: a CTest
#  PASS_REGULAR_EXPRESSION alone would ignore the exit status.
#  EMULATOR is the web tree's CMAKE_CROSSCOMPILING_EMULATOR (node), which runs
#  gweni_unit_tests.js as it runs the discovered cases.
#

if(NOT DEFINED UNIT_TESTS)
    message(FATAL_ERROR "unitZeroMatch.cmake: pass -DUNIT_TESTS=...")
endif()

set(message "no test case matched the filter")

execute_process(
    COMMAND ${EMULATOR} "${UNIT_TESTS}" "--test-case=gweni: no test case has this name"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

if(result EQUAL 0)
    message(FATAL_ERROR "a run matching no test case exited 0; "
        "tests/unit/main.cpp's guard must fail it\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT result MATCHES "^[0-9]+$")
    message(FATAL_ERROR "gweni_unit_tests did not run: ${result}\nstderr:\n${err}")
endif()
string(FIND "${err}" "${message}" at)
if(at EQUAL -1)
    message(FATAL_ERROR "a run matching no test case exited ${result} but its "
        "stderr does not say \"${message}\"\nstdout:\n${out}\nstderr:\n${err}")
endif()
message(STATUS "a run matching no test case exits ${result} and says \"${message}\"")
