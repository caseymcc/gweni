#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  A capture from incomplete resources writes its degraded frame and exits 2,
#  run as a cmake -P script (tests/CMakeLists.txt registers one test per CASE):
#
#      cmake -DEXE=<ControlTest|OpenGLCore> -DTOOL=<ControlTest|OpenGLCore> \
#            -DSOURCE_DIR=<repo root> -DSCRATCH_DIR=<dir> -DCASE=<case> \
#            -P missingResources.cmake
#
#  CASE is what the --resources directory lacks, in a copy of the repo's
#  resources/ made here:
#    nodir     the directory itself (not created)
#    noskin    DefaultSkin.json: every surface magenta, the text still drawn
#    nofont    OpenSans.ttf, the default face: the text in the built-in one
#    complete  nothing: exit 0 and nothing on stderr
#
#  Both tools capture the demo shell (ControlTest's --control Demo), which
#  builds every page up front, so a page that logs, as ImagePanelDemo did
#  asking for a missing image, fails the complete case.
#
#  A degraded case passes only if the exe exits 2, writes the PNG, names the
#  missing path on stderr and ends with "<TOOL>: resources incomplete, exiting 2".
#
#  In the web tree EXE is the page (<TOOL>.html) and -DRUNNER=<node;run.mjs> the
#  browser runner (tests/web/run.mjs): the copy is mounted into the page's file
#  system (--mount) at the same path, the shot written under its /out and copied
#  back, and the runner exits with the page's status and passes its stderr on.
#

foreach(var EXE TOOL SOURCE_DIR SCRATCH_DIR CASE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "missingResources.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

set(resources "${SCRATCH_DIR}/resources")

if(CASE STREQUAL "nodir")
    set(resources "${SCRATCH_DIR}/no_such_resources")
    set(missing "resource directory not found: [^\n]*/no_such_resources")
elseif(CASE STREQUAL "noskin")
    set(removed DefaultSkin.json)
    set(missing "skin config [^\n]*/resources/DefaultSkin.json")
elseif(CASE STREQUAL "nofont")
    set(removed OpenSans.ttf)
    set(missing "Font file not found: [^\n]*/resources/OpenSans.ttf")
elseif(CASE STREQUAL "complete")
    set(removed)
else()
    message(FATAL_ERROR "missingResources.cmake: unknown CASE '${CASE}'")
endif()

if(NOT CASE STREQUAL "nodir")
    file(COPY "${SOURCE_DIR}/resources/" DESTINATION "${resources}")
    foreach(name ${removed})
        file(REMOVE "${resources}/${name}")
    endforeach()
endif()

set(shot "${SCRATCH_DIR}/shot.png")

if(TOOL STREQUAL "ControlTest")
    set(args --headless --control Demo --shot "${shot}" --resources "${resources}")
elseif(TOOL STREQUAL "OpenGLCore")
    set(args --headless --screenshot "${shot}" --resources "${resources}")
else()
    message(FATAL_ERROR "missingResources.cmake: unknown TOOL '${TOOL}'")
endif()

if(DEFINED RUNNER)
    # the page sees the copy at the same absolute path and writes its shot to
    # /out/shot.png, which the runner copies to SCRATCH_DIR/shot.png
    set(mount)
    if(NOT CASE STREQUAL "nodir")
        set(mount --mount "${resources}:${resources}")
    endif()
    list(TRANSFORM args REPLACE "^${shot}$" "/out/shot.png")
    set(command ${RUNNER} --out "${SCRATCH_DIR}" ${mount} "${EXE}" -- ${args})
else()
    set(command "${EXE}" ${args})
endif()
execute_process(
    COMMAND ${command}
    WORKING_DIRECTORY "${SCRATCH_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

set(log "stdout:\n${out}\nstderr:\n${err}")

if(NOT EXISTS "${shot}")
    message(FATAL_ERROR "${CASE}: ${TOOL} exited with '${result}' and wrote no ${shot}\n${log}")
endif()

if(CASE STREQUAL "complete")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${CASE}: ${TOOL} exited with '${result}', not 0\n${log}")
    endif()
    if(err MATCHES "resources incomplete")
        message(FATAL_ERROR "${CASE}: ${TOOL} reported incomplete resources\n${log}")
    endif()
    # the runner passes the page's stderr on and adds none of its own to a
    # passing run, so this holds for the browser too
    if(NOT err STREQUAL "")
        message(FATAL_ERROR "${CASE}: ${TOOL} wrote to stderr\n${log}")
    endif()
    return()
endif()

if(NOT result EQUAL 2)
    message(FATAL_ERROR "${CASE}: ${TOOL} exited with '${result}', not 2\n${log}")
endif()
if(NOT err MATCHES "${missing}")
    message(FATAL_ERROR "${CASE}: stderr does not match '${missing}'\n${log}")
endif()
if(NOT err MATCHES "${TOOL}: resources incomplete, exiting 2\n$")
    message(FATAL_ERROR "${CASE}: stderr does not end with the exit-2 line\n${log}")
endif()
