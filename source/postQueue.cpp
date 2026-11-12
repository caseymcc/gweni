/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/postQueue.h>

#include <utility>

namespace gweni
{

bool PostQueue::post(Function function)
{
    std::function<void()> wake;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_closed)
            return false;
        if(m_queued.empty())
            wake=m_wakeHandler;
        m_queued.push_back(std::move(function));
    }

    if(wake)
        wake();
    return true;
}

size_t PostQueue::run()
{
    std::vector<Function> running;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_closed)
            return 0;
        running.swap(m_queued);
    }

    for(Function &function:running)
    {
        if(function)
            function();
    }
    return running.size();
}

bool PostQueue::pending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_queued.empty();
}

void PostQueue::close()
{
    std::vector<Function> dropped;
    std::function<void()> handler;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_closed=true;
        dropped.swap(m_queued);
        handler.swap(m_wakeHandler);
    }
    // destroyed here, outside the lock: a capture's destructor may post
}

bool PostQueue::closed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closed;
}

void PostQueue::setWakeHandler(std::function<void()> handler)
{
    std::function<void()> old;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        old=std::move(m_wakeHandler);
        m_wakeHandler=std::move(handler);
    }
}

void PostQueue::wake()
{
    if(std::function<void()> handler=wakeHandler())
        handler();
}

std::function<void()> PostQueue::wakeHandler() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_wakeHandler;
}

}//namespace gweni
