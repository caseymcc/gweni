# Warning flags for gweni's own targets. Vendored trees (deps/freetype-gl) keep
# their own flags and are not passed through here.
#
# -Werror=switch is an error rather than a warning on purpose: a switch over an
# imglib/gweni enum that misses an enumerator silently returns a wrong value
# (imglib::sizeOfPixel returning 0 for Format::BGRA once killed the process
# inside libjpeg), so a new enumerator must not compile until it is handled.

function(gweni_set_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Werror=switch>
    )
endfunction()
