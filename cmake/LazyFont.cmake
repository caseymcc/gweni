#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The CJK face lives outside resources/: web/page.cmake preloads that whole
#  directory. A configure writes only to its own build tree. Native runtime
#  copies and the web's separate lazy package take this directory as input.
#
include(${CMAKE_CURRENT_LIST_DIR}/fetchPinnedFile.cmake)

set(GWENI_LAZY_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources-lazy")
set(GWENI_CJK_FONT_NAME DroidSansFallbackFull)
set(GWENI_CJK_FONT_SHA256 2392015530438bafc48edfc4aee6d9de2387f627a6134d8ab3dfcc99d21c8240)
set(GWENI_CJK_FONT_URL
    "https://raw.githubusercontent.com/aosp-mirror/platform_frameworks_base/82454e029f05bf1795ac8704fb828ce9ed76767a/data/fonts/${GWENI_CJK_FONT_NAME}.ttf")
set(GWENI_CJK_FONT_FILE "" CACHE FILEPATH
    "Existing Droid Sans Fallback Full file for offline configuration (SHA-256 verified)")

file(MAKE_DIRECTORY "${GWENI_LAZY_RESOURCE_DIR}")
set(cjk_destination "${GWENI_LAZY_RESOURCE_DIR}/${GWENI_CJK_FONT_NAME}.ttf")
if(GWENI_CJK_FONT_FILE)
    if(NOT EXISTS "${GWENI_CJK_FONT_FILE}")
        message(FATAL_ERROR "GWENI_CJK_FONT_FILE does not exist: ${GWENI_CJK_FONT_FILE}")
    endif()
    # Existing files are verified before reuse, without a network request.
    gweni_fetch_pinned_file("${GWENI_CJK_FONT_FILE}" "${GWENI_CJK_FONT_URL}" "${GWENI_CJK_FONT_SHA256}")
    configure_file("${GWENI_CJK_FONT_FILE}" "${cjk_destination}" COPYONLY)
else()
    gweni_fetch_pinned_file("${cjk_destination}" "${GWENI_CJK_FONT_URL}" "${GWENI_CJK_FONT_SHA256}")
endif()
foreach(suffix LICENSE.txt ranges.json)
    configure_file(
        "${GWENI_SOURCE_DIR}/resources-lazy/${GWENI_CJK_FONT_NAME}-${suffix}"
        "${GWENI_LAZY_RESOURCE_DIR}/${GWENI_CJK_FONT_NAME}-${suffix}" COPYONLY)
endforeach()
