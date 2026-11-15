#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Pins configure-time downloads: verify a new file, reuse a verified cached
#  file without its source, reject a corrupt cache, and never publish a bad
#  download or leave a partial file. Local file URLs need no network.
#
cmake_minimum_required(VERSION 3.12)

if(DEFINED DESTINATION)
    include("${FETCH}")
    gweni_fetch_pinned_file("${DESTINATION}" "${URL}" "${SHA256}")
    return()
endif()

file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")
set(source "${SCRATCH_DIR}/source")
set(destination "${SCRATCH_DIR}/cached")
file(WRITE "${source}" "pinned resource\n")
file(SHA256 "${source}" pin)

function(fetch)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DFETCH=${FETCH}" "-DDESTINATION=${destination}"
        "-DURL=file://${source}" "-DSHA256=${pin}" -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(result "${result}" PARENT_SCOPE)
    set(error "${error}" PARENT_SCOPE)
endfunction()

fetch()
if(NOT result EQUAL 0 OR NOT EXISTS "${destination}")
    message(FATAL_ERROR "new download failed: ${error}")
endif()
file(SHA256 "${destination}" actual)
if(NOT actual STREQUAL pin)
    message(FATAL_ERROR "new download did not preserve bytes")
endif()

file(REMOVE "${source}")
fetch()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verified cache was not reused without its source: ${error}")
endif()

file(WRITE "${destination}" "corrupt cache\n")
fetch()
if(result EQUAL 0 OR NOT error MATCHES "Cached resource SHA-256 mismatch")
    message(FATAL_ERROR "corrupt cache accepted: expected SHA-256 rejection, got ${result}: ${error}")
endif()

file(REMOVE "${destination}")
file(WRITE "${source}" "bad download\n")
fetch()
if(result EQUAL 0 OR NOT error MATCHES "Downloaded resource SHA-256 mismatch")
    message(FATAL_ERROR "bad download accepted: expected SHA-256 rejection, got ${result}: ${error}")
endif()
if(EXISTS "${destination}" OR EXISTS "${destination}.download")
    message(FATAL_ERROR "bad download left a published or partial file")
endif()

file(REMOVE "${source}")
fetch()
if(result EQUAL 0 OR NOT error MATCHES "Could not fetch")
    message(FATAL_ERROR "missing source accepted: expected fetch failure, got ${result}: ${error}")
endif()
if(EXISTS "${destination}" OR EXISTS "${destination}.download")
    message(FATAL_ERROR "failed fetch left a published or partial file")
endif()
