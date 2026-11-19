#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  app_headless and app_headless_vulkan: gweni_app's app::Headless
#  (tests/app/headless.cpp, whose header lists its checks), then the keys of
#  its stats() against the ones ControlTest --stats prints on the same
#  backend: the test fails unless the exe exited 0, printed
#  "app_headless: passed" and "app_headless: keys=...", and those keys are
#  ControlTest's line's, in its order.
#
#  -DEXE=<gweni_app_headless_test> -DCONTROLTEST=<ControlTest>
#  -DRESOURCES=<resources dir> -DSCRATCH=<scratch dir>
#
cmake_minimum_required(VERSION 3.16)

function(fail message)
    message(FATAL_ERROR "app_headless: FAIL: ${message}")
endfunction()

file(REMOVE_RECURSE ${SCRATCH})
file(MAKE_DIRECTORY ${SCRATCH})

execute_process(
    COMMAND ${EXE} ${RESOURCES} ${SCRATCH}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 240)
message("${output}${errors}")
if(NOT result EQUAL 0)
    fail("the headless test exited ${result}")
endif()
if(NOT output MATCHES "app_headless: passed")
    fail("the headless test did not pass its own checks")
endif()
string(REGEX MATCH "app_headless: keys=([^\n]*)" keys "${output}")
if(NOT keys)
    fail("the headless test printed no keys")
endif()
set(headless_keys "${CMAKE_MATCH_1}")

execute_process(
    COMMAND ${CONTROLTEST} --headless --control ButtonDemo --stats --frames 1
        --resources ${RESOURCES}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE controltest
    ERROR_VARIABLE errors
    TIMEOUT 120)
if(NOT result EQUAL 0)
    fail("ControlTest --stats exited ${result}: ${errors}")
endif()
string(REGEX MATCH "frame=0 [^\n]*" line "${controltest}")
if(NOT line)
    fail("ControlTest --stats printed no frame line")
endif()
string(REGEX MATCHALL "[a-z_]+=" pairs "${line}")
set(controltest_keys "")
foreach(pair IN LISTS pairs)
    string(REGEX REPLACE "=$" "" key "${pair}")
    list(APPEND controltest_keys ${key})
endforeach()
string(REPLACE ";" "," controltest_keys "${controltest_keys}")
if(NOT headless_keys STREQUAL controltest_keys)
    fail("app::Headless::stats()'s keys are not ControlTest --stats's:\n  ${headless_keys}\n  ${controltest_keys}")
endif()
message("app_headless: the stats keys are ControlTest's (${controltest_keys})")
