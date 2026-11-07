/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench's replacement of the global operator new and delete: the same
 *  malloc/free underneath, plus a count of the allocations and their bytes.
 *  Defined in the executable, so it replaces the default for the whole
 *  process, the gweni library and the vendored C++ code included.
 *
 *  Only the thread that called countAllocationsOnThisThread() is counted: Mesa
 *  allocates from its own threads too, and how much depends on scheduling, so
 *  counting them would make the counts differ between identical runs.
 */
#include "bench.h"

#include <pthread.h>

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{

std::atomic<size_t> g_allocs{0};
std::atomic<size_t> g_allocBytes{0};
std::atomic<bool> g_threadSet{false};
pthread_t g_thread;

void count(std::size_t size)
{
    if(!g_threadSet.load(std::memory_order_acquire) || !pthread_equal(pthread_self(), g_thread))
        return;
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    g_allocBytes.fetch_add(size, std::memory_order_relaxed);
}

void *allocate(std::size_t size)
{
    count(size);
    return std::malloc(size ? size : 1);
}

void *allocateAligned(std::size_t size, std::align_val_t alignment)
{
    count(size);

    std::size_t align=static_cast<std::size_t>(alignment);
    if(align < sizeof(void *))
        align=sizeof(void *);

    void *memory=nullptr;
    if(posix_memalign(&memory, align, size ? size : 1) != 0)
        return nullptr;
    return memory;
}

}//namespace

namespace gweni
{
namespace bench
{

void countAllocationsOnThisThread()
{
    g_thread=pthread_self();
    g_threadSet.store(true, std::memory_order_release);
}

size_t allocCount()
{
    return g_allocs.load(std::memory_order_relaxed);
}

size_t allocBytes()
{
    return g_allocBytes.load(std::memory_order_relaxed);
}

}//namespace bench
}//namespace gweni

void *operator new(std::size_t size)
{
    if(void *memory=allocate(size))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size)
{
    if(void *memory=allocate(size))
        return memory;
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new(std::size_t size, std::align_val_t alignment)
{
    if(void *memory=allocateAligned(size, alignment))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
    if(void *memory=allocateAligned(size, alignment))
        return memory;
    throw std::bad_alloc();
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    return allocateAligned(size, alignment);
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    return allocateAligned(size, alignment);
}

void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void *memory, const std::nothrow_t &) noexcept { std::free(memory); }
void operator delete[](void *memory, const std::nothrow_t &) noexcept { std::free(memory); }
void operator delete(void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::align_val_t, const std::nothrow_t &) noexcept { std::free(memory); }
void operator delete[](void *memory, std::align_val_t, const std::nothrow_t &) noexcept { std::free(memory); }
