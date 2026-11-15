#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Configure-time download, with an immutable SHA-256 and a verified cache.
#  Download into a temporary file so an interrupted or failed fetch cannot
#  leave a partial resource at the path an app will open.
#
function(gweni_fetch_pinned_file destination url sha256)
    if(EXISTS "${destination}")
        file(SHA256 "${destination}" actual)
        if(NOT actual STREQUAL sha256)
            message(FATAL_ERROR
                "Cached resource SHA-256 mismatch: ${destination}\n"
                "Expected ${sha256}, got ${actual}. Remove it and configure again.")
        endif()
        return()
    endif()

    get_filename_component(directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    set(temporary "${destination}.download")
    message(STATUS "Fetching ${url}")
    file(DOWNLOAD "${url}" "${temporary}"
        TLS_VERIFY ON TIMEOUT 120 INACTIVITY_TIMEOUT 30 STATUS status)
    list(GET status 0 code)
    list(GET status 1 detail)
    if(NOT code EQUAL 0)
        file(REMOVE "${temporary}")
        message(FATAL_ERROR "Could not fetch ${destination}: ${detail}")
    endif()
    file(SHA256 "${temporary}" actual)
    if(NOT actual STREQUAL sha256)
        file(REMOVE "${temporary}")
        message(FATAL_ERROR
            "Downloaded resource SHA-256 mismatch: ${destination}\n"
            "Expected ${sha256}, got ${actual}.")
    endif()
    file(RENAME "${temporary}" "${destination}")
endfunction()
