/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  getrandom() for gweni_bench under valgrind: the same bytes every run.
 *
 *  Mesa parses its drirc files with expat, which salts its hash tables with
 *  getrandom() (five calls in all before the first frame). A different salt
 *  lays the heap out differently, and from then on glibc malloc and LLVM's
 *  pointer-keyed sets do different work, so a profiled scene's phases cost a
 *  few percent more or less instructions from run to run (build at N=1000:
 *  update 191.9M to 197.7M over 10 runs). Under valgrind this getrandom() hands out a fixed
 *  sequence instead, and the counts are the same every run. Outside valgrind
 *  it is the system call, unchanged.
 *
 *  The exe exports this one symbol (bench/exports.list), so it interposes the
 *  libc function for the libraries Mesa loads.
 */
#include "callgrind.h"

#if defined(__linux__)

#include <cstddef>
#include <cstdint>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" ssize_t getrandom(void *buffer, size_t length, unsigned int flags)
{
    if(RUNNING_ON_VALGRIND)
    {
        // xorshift64*, seeded the same every run: fixed, but not the same
        // bytes for every call
        static uint64_t state=0x9e3779b97f4a7c15ull;
        unsigned char *bytes=static_cast<unsigned char *>(buffer);

        for(size_t i=0; i<length; ++i)
        {
            state^=state>>12;
            state^=state<<25;
            state^=state>>27;
            bytes[i]=static_cast<unsigned char>((state*0x2545f4914f6cdd1dull)>>56);
        }
        return static_cast<ssize_t>(length);
    }
    return syscall(SYS_getrandom, buffer, length, flags);
}

#endif
