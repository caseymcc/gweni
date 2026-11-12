/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Canvas::post() and its gweni::PostQueue, the one thread-safe thing in the
 *  library: 8 threads posting 10 000 functions each while the main thread
 *  updates the canvas, every function run exactly once on the updating thread,
 *  each thread's in the order it posted them and, with the posts serialised by
 *  a lock of the test's own that takes a sequence number, all of them in the
 *  order of that number; a post made by a running post runs in the next
 *  update, not this one; the wake handler called on the posting thread when
 *  the queue goes from empty to holding a function, and only then; and a
 *  worker's postHandle() outliving the canvas, whose post() is then dropped
 *  and returns false, with what was queued destroyed by ~Canvas, run never.
 *  The ASAN tree runs these as every other unit case. The web build has no
 *  threads, so there the cases run without their other threads.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/postQueue.h>

#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace gweni;

namespace
{


struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }
};

#if !defined(__EMSCRIPTEN__)
const int Threads=8;
const int PostsPerThread=10000;

struct Record
{
    int thread;
    int index;
    long sequence;
};

//! Posts Threads*PostsPerThread functions from Threads workers while the main
//! thread updates the canvas; \p serialise takes the test's lock and a
//! sequence number around each post().
std::vector<Record> postFromThreads(controls::Canvas *canvas, bool serialise, bool &allOnMain)
{
    std::vector<Record> records;
    records.reserve(size_t(Threads)*PostsPerThread);

    const std::thread::id main=std::this_thread::get_id();
    std::mutex order;
    long sequence=0;
    std::atomic<int> finished{0};
    std::atomic<bool> go{false};
    allOnMain=true;

    std::vector<std::thread> workers;

    for(int t=0; t < Threads; ++t)
    {
        workers.emplace_back([&, t]() {
            while(!go.load())
                std::this_thread::yield();
            for(int i=0; i < PostsPerThread; ++i)
            {
                auto post=[&](long seq) {
                    canvas->post([&records, &allOnMain, main, t, i, seq]() {
                        if(std::this_thread::get_id() != main)
                            allOnMain=false;
                        records.push_back({t, i, seq});
                    });
                };

                if(serialise)
                {
                    std::lock_guard<std::mutex> lock(order);
                    post(sequence++);
                }
                else
                    post(-1);
            }
            ++finished;
        });
    }

    go=true;
    // the main thread is the UI thread: it updates while they post
    while(finished.load() < Threads)
        canvas->updateCanvas();
    for(std::thread &worker:workers)
        worker.join();
    canvas->updateCanvas();

    return records;
}

//! Every thread's functions ran once each, in the order it posted them.
void checkPerThread(const std::vector<Record> &records)
{
    REQUIRE(records.size() == size_t(Threads)*PostsPerThread);

    std::vector<int> next(Threads, 0);
    bool inOrder=true;

    for(const Record &record:records)
    {
        if(record.index != next[record.thread])
            inOrder=false;
        next[record.thread]=record.index+1;
    }
    CHECK(inOrder);
    for(int t=0; t < Threads; ++t)
        CHECK(next[t] == PostsPerThread);
}
#endif

} // namespace

#if !defined(__EMSCRIPTEN__)
TEST_CASE("Post queue: 8 threads' posts run once each on the updating thread, FIFO per thread")
{
    Scene scene;
    bool allOnMain=false;
    const std::vector<Record> records=postFromThreads(scene.canvas, false, allOnMain);

    CHECK(allOnMain);
    checkPerThread(records);
    CHECK_FALSE(scene.canvas->postHandle()->pending());
}

TEST_CASE("Post queue: across threads the posts run in the order they took the lock")
{
    Scene scene;
    bool allOnMain=false;
    const std::vector<Record> records=postFromThreads(scene.canvas, true, allOnMain);

    CHECK(allOnMain);
    checkPerThread(records);

    bool inOrder=true;

    for(size_t i=0; i < records.size(); ++i)
    {
        if(records[i].sequence != long(i))
            inOrder=false;
    }
    CHECK(inOrder);
}
#endif

TEST_CASE("Post queue: a post made by a running post runs in the next update")
{
    Scene scene;
    std::vector<std::string> log;

    scene.canvas->post([&]() {
        log.push_back("first");
        scene.canvas->post([&]() { log.push_back("reposted"); });
    });
    scene.canvas->post([&]() { log.push_back("second"); });

    scene.canvas->updateCanvas();
    CHECK(log == std::vector<std::string>{"first", "second"});
    CHECK(scene.canvas->postHandle()->pending());

    scene.canvas->updateCanvas();
    CHECK(log == std::vector<std::string>{"first", "second", "reposted"});
    CHECK_FALSE(scene.canvas->postHandle()->pending());

    // a function that posts itself again runs once an update
    int runs=0;
    std::function<void()> again=[&]() {
        ++runs;
        scene.canvas->post(again);
    };
    scene.canvas->post(again);
    for(int i=0; i < 5; ++i)
        scene.canvas->updateCanvas();
    CHECK(runs == 5);
}

TEST_CASE("Post queue: the wake handler is called on the posting thread when the queue fills")
{
    Scene scene;
    std::atomic<int> wakes{0};
    std::thread::id wokenOn;

    scene.canvas->setWakeHandler([&]() {
        wokenOn=std::this_thread::get_id();
        ++wakes;
    });

    // empty to one queued: a wake; a second post into a queue holding one: none
    scene.canvas->post([]() {});
    CHECK(wakes == 1);
    CHECK(wokenOn == std::this_thread::get_id());
    scene.canvas->post([]() {});
    CHECK(wakes == 1);

    scene.canvas->updateCanvas();
    CHECK(wakes == 1);

#if !defined(__EMSCRIPTEN__)
    // from another thread, on that thread
    std::thread::id worker;
    std::thread thread([&]() {
        worker=std::this_thread::get_id();
        scene.canvas->post([]() {});
    });
    thread.join();
    CHECK(wokenOn == worker);
#else
    scene.canvas->post([]() {});
#endif
    CHECK(wakes == 2);

    // a post made by a running post fills the swapped-out queue: a wake, so the
    // loop updates again for it
    scene.canvas->updateCanvas();
    scene.canvas->post([&]() { scene.canvas->post([]() {}); });
    CHECK(wakes == 3);
    scene.canvas->updateCanvas();
    CHECK(wakes == 4);
    scene.canvas->updateCanvas();

    // PostQueue::wake() calls it too; an empty handler calls nothing
    scene.canvas->postHandle()->wake();
    CHECK(wakes == 5);
    scene.canvas->setWakeHandler(nullptr);
    scene.canvas->post([]() {});
    scene.canvas->postHandle()->wake();
    CHECK(wakes == 5);
}

TEST_CASE("Post queue: a handle outliving its canvas drops posts and returns false")
{
    std::shared_ptr<PostQueue> handle;
    std::weak_ptr<int> captured;
    int wakes=0;
    bool ran=false;

    {
        Scene scene;
        handle=scene.canvas->postHandle();
        scene.canvas->setWakeHandler([&]() { ++wakes; });

        auto data=std::make_shared<int>(7);
        captured=data;
        CHECK(handle->post([data, &ran]() { ran=true; }));
        CHECK(wakes == 1);
    }

    // ~Canvas closed the queue and destroyed what it held, unrun
    CHECK(handle->closed());
    CHECK_FALSE(ran);
    CHECK(captured.expired());
    CHECK_FALSE(handle->pending());

    // a worker's post now is dropped: no wake, nothing kept
    auto data=std::make_shared<int>(8);
    captured=data;
    CHECK_FALSE(handle->post([data, &ran]() { ran=true; }));
    data.reset();
    CHECK(captured.expired());
    CHECK(wakes == 1);
    CHECK(handle->run() == 0);
    CHECK_FALSE(ran);

#if !defined(__EMSCRIPTEN__)
    // and from another thread
    bool posted=true;
    std::thread thread([&]() { posted=handle->post([]() {}); });
    thread.join();
    CHECK_FALSE(posted);
#endif
}
