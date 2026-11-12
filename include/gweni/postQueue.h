/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_postQueue_h_
#define _gweni_postQueue_h_

#include <gweni/export.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace gweni
{

//! Work handed to the UI thread from any thread: the one thread-safe thing in
//! the library. A canvas owns one (controls::Canvas::post(), postHandle()) and
//! runs it at the top of its update phase; what a posted function does runs on
//! the thread calling run(), so it may touch controls like any UI code.
//!
//! post() appends under a mutex, and run() swaps the whole vector out under it
//! and calls the functions outside it, in order: FIFO per posting thread, and
//! across threads the order in which they took the lock. A function posted
//! while run() is calling them (by one of them, or by another thread) waits for
//! the next run(), so a function that posts itself again cannot keep a frame
//! from ending. close() drops what is queued, and every post() after it is
//! dropped and returns false, so a worker holding the queue past its canvas's
//! life (a std::shared_ptr from postHandle()) does no harm.
//!
//! Without threads (Emscripten without pthreads) std::mutex does nothing and
//! the same code runs.
class GWENI_EXPORT PostQueue
{
public:
    using Function=std::function<void()>;

    PostQueue()=default;
    PostQueue(const PostQueue &)=delete;
    PostQueue &operator=(const PostQueue &)=delete;

    //! Queues \p function for the next run(), from any thread. When the queue
    //! goes from empty to holding it, calls the wake handler, on this thread
    //! and outside the lock. Returns false, dropping it, once close()d.
    bool post(Function function);

    //! Calls, in order, every function queued before the call, outside the
    //! lock; returns how many. Nothing once close()d.
    size_t run();

    //! Whether a function is queued for the next run().
    bool pending() const;

    //! Drops what is queued (destroying it on this thread), the wake handler
    //! and every later post().
    void close();
    bool closed() const;

    //! Called by post() when the queue goes from empty to non-empty, and by
    //! wake(): from the posting thread, outside the lock, so it must be safe to
    //! call from any thread (glfwPostEmptyEvent(), or setting a flag) and may
    //! be called once more by a post() racing close() or a new handler. An
    //! empty function calls nothing.
    void setWakeHandler(std::function<void()> handler);
    //! Calls the wake handler (the loop has work due sooner than it thought).
    void wake();

private:
    std::function<void()> wakeHandler() const;

    mutable std::mutex m_mutex;
    std::vector<Function> m_queued;
    std::function<void()> m_wakeHandler;
    bool m_closed=false;
};

}//namespace gweni

#endif//_gweni_postQueue_h_
