/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::ImageHandle, the image a Markdown document shows whose bytes the app
 *  supplies later, headless (no renderer: the handle holds the bytes and reads
 *  only their header):
 *  - its states: None; setSize Sized (a width or height under 1 refused);
 *    setBytes of a PNG Bytes, the size the header's whatever setSize said, a
 *    setSize after it refused; setFailed Failed, the bytes dropped and the size
 *    kept; bytes that are no PNG or JPEG Failed; a Failed handle filled in
 *    again; every call that changes nothing (the same size, the same bytes,
 *    failing a failed handle) notifying nothing and keeping serial();
 *  - its watches: every watch on it called once per change with the handle,
 *    two documents' worth of watches on one handle alike; a deleted watch (and
 *    one reset() to nothing or to another handle) no longer called; a watch
 *    deleted by an earlier watch's call during a notification not called in it,
 *    one made during it first called on the next change; a watch whose call
 *    deletes every watch, which hold the handle's last references, ends the
 *    notification with no use of the freed handle (the ASAN tree checks it);
 *  - a fill made while a controls::RenderWalkScope is open is recorded in
 *    renderWalkMutations() as "ImageHandle <operation> (rendering none)", once
 *    per operation, and still applied; the same fills between frames record
 *    nothing.
 */
#include <gweni/imageHandle.h>
#include <gweni/controls/base.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using gweni::ImageHandle;

namespace
{

uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc=0xffffffffu;

    for(size_t i=0; i < size; ++i)
    {
        crc^=data[i];
        for(int bit=0; bit < 8; ++bit)
            crc=(crc >> 1)^(0xedb88320u & (0u-(crc & 1u)));
    }
    return crc^0xffffffffu;
}

void putBigEndian(std::vector<uint8_t> &bytes, uint32_t value)
{
    for(int shift=24; shift >= 0; shift-=8)
        bytes.push_back(uint8_t(value >> shift));
}

//! A PNG's signature and IHDR chunk (CRC right) for a width x height RGBA
//! image: all the handle reads of it.
std::vector<uint8_t> pngHeader(uint32_t width, uint32_t height)
{
    std::vector<uint8_t> bytes={0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    std::vector<uint8_t> chunk={'I', 'H', 'D', 'R'};

    putBigEndian(chunk, width);
    putBigEndian(chunk, height);
    chunk.insert(chunk.end(), {8, 6, 0, 0, 0});
    putBigEndian(bytes, 13);
    bytes.insert(bytes.end(), chunk.begin(), chunk.end());
    putBigEndian(bytes, crc32(chunk.data(), chunk.size()));
    return bytes;
}

bool recorded(const std::string &entry)
{
    const std::vector<std::string> entries=gweni::controls::renderWalkMutations();

    return std::find(entries.begin(), entries.end(), entry) != entries.end();
}

}//namespace

TEST_CASE("ImageHandle: None, Sized, Bytes and Failed, a change notified once")
{
    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    int calls=0;
    ImageHandle::Watch watch(handle, [&](ImageHandle &changed) { CHECK((&changed == handle.get())); ++calls; });

    CHECK((handle->state() == ImageHandle::State::None));
    CHECK_FALSE(handle->hasSize());
    CHECK(handle->serial() == 0);

    CHECK_FALSE(handle->setSize(0, 20));
    CHECK_FALSE(handle->setSize(20, -1));
    CHECK((handle->state() == ImageHandle::State::None));
    CHECK(calls == 0);

    CHECK(handle->setSize(64, 32));
    CHECK((handle->state() == ImageHandle::State::Sized));
    CHECK(handle->width() == 64);
    CHECK(handle->height() == 32);
    CHECK(calls == 1);
    CHECK(handle->setSize(64, 32));
    CHECK(calls == 1);
    CHECK(handle->setSize(80, 40));
    CHECK(calls == 2);

    const std::vector<uint8_t> png=pngHeader(40, 30);

    CHECK(handle->setBytes(png));
    CHECK((handle->state() == ImageHandle::State::Bytes));
    CHECK(handle->bytes() == png);
    // the header's size, not the one the app said
    CHECK(handle->width() == 40);
    CHECK(handle->height() == 30);
    CHECK(calls == 3);
    CHECK(handle->setBytes(png.data(), png.size()));
    CHECK(calls == 3);
    CHECK_FALSE(handle->setSize(10, 10));
    CHECK(handle->width() == 40);
    CHECK(calls == 3);

    const uint64_t serial=handle->serial();

    handle->setFailed();
    CHECK((handle->state() == ImageHandle::State::Failed));
    CHECK(handle->bytes().empty());
    CHECK(handle->width() == 40);
    CHECK(calls == 4);
    CHECK(handle->serial() == serial+1);
    handle->setFailed();
    CHECK(calls == 4);
    CHECK(handle->serial() == serial+1);

    // filled in again after failing
    CHECK(handle->setBytes(pngHeader(7, 9)));
    CHECK((handle->state() == ImageHandle::State::Bytes));
    CHECK(handle->width() == 7);
    CHECK(handle->height() == 9);
    CHECK(calls == 5);
}

TEST_CASE("ImageHandle: bytes that are no PNG or JPEG fail the handle")
{
    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    int calls=0;
    ImageHandle::Watch watch(handle, [&](ImageHandle &) { ++calls; });
    const std::vector<uint8_t> text={'G', 'I', 'F', '8', '9', 'a', 0, 0, 0, 0, 0, 0};

    CHECK(handle->setSize(20, 10));
    CHECK_FALSE(handle->setBytes(text));
    CHECK((handle->state() == ImageHandle::State::Failed));
    CHECK(handle->bytes().empty());
    CHECK(calls == 2);
    CHECK_FALSE(handle->setBytes(nullptr, 0));
    CHECK(calls == 2);

    // a PNG cut before its IHDR ends
    std::shared_ptr<ImageHandle> cut=ImageHandle::create();
    std::vector<uint8_t> png=pngHeader(5, 5);

    png.resize(20);
    CHECK_FALSE(cut->setBytes(png));
    CHECK((cut->state() == ImageHandle::State::Failed));
}

TEST_CASE("ImageHandle: every watch is notified, a deleted one no more")
{
    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    std::vector<std::string> calls;
    // two documents showing one handle, one of them in two blocks
    auto first=std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &) { calls.push_back("first"); });
    auto second=std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &) { calls.push_back("second"); });
    ImageHandle::Watch third(handle, [&](ImageHandle &) { calls.push_back("third"); });

    CHECK(handle->watchCount() == 3);
    CHECK(handle->setSize(10, 10));
    CHECK(calls == std::vector<std::string>{"first", "second", "third"});

    calls.clear();
    second.reset();
    CHECK(handle->watchCount() == 2);
    CHECK(handle->setSize(11, 10));
    CHECK(calls == std::vector<std::string>{"first", "third"});

    // reset() to nothing, then onto another handle
    std::shared_ptr<ImageHandle> other=ImageHandle::create();

    calls.clear();
    first->reset();
    CHECK((first->handle() == nullptr));
    handle->setFailed();
    CHECK(calls == std::vector<std::string>{"third"});
    first->reset(other, [&](ImageHandle &) { calls.push_back("first on other"); });
    calls.clear();
    other->setFailed();
    CHECK(calls == std::vector<std::string>{"first on other"});
    CHECK(handle->watchCount() == 1);
    CHECK(other->watchCount() == 1);
}

TEST_CASE("ImageHandle: watches deleted and made during a notification")
{
    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    std::vector<std::string> calls;
    std::unique_ptr<ImageHandle::Watch> victim;
    std::unique_ptr<ImageHandle::Watch> late;

    ImageHandle::Watch killer(handle, [&](ImageHandle &h)
    {
        calls.push_back("killer");
        victim.reset();
        if(!late)
            late=std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &) { calls.push_back("late"); });
    });
    victim=std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &) { calls.push_back("victim"); });

    CHECK(handle->setSize(3, 3));
    // the victim deleted before its turn, the late one not called yet
    CHECK(calls == std::vector<std::string>{"killer"});
    CHECK(handle->watchCount() == 2);

    calls.clear();
    CHECK(handle->setSize(4, 3));
    CHECK(calls == std::vector<std::string>{"killer", "late"});
}

TEST_CASE("ImageHandle: a watch deleting the handle's last holders mid-notification")
{
    std::weak_ptr<ImageHandle> weak;
    std::vector<std::unique_ptr<ImageHandle::Watch>> watches;
    int calls=0;
    ImageHandle *raw=nullptr;

    {
        std::shared_ptr<ImageHandle> handle=ImageHandle::create();

        weak=handle;
        raw=handle.get();
        watches.push_back(std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &)
        {
            ++calls;
            watches.clear();
        }));
        watches.push_back(std::make_unique<ImageHandle::Watch>(handle, [&](ImageHandle &) { ++calls; }));
    }
    // only the watches hold it now
    CHECK_FALSE(weak.expired());
    CHECK(raw->setSize(5, 5));
    CHECK(calls == 1);
    CHECK(weak.expired());
}

TEST_CASE("ImageHandle: a fill inside a RenderWalkScope is a render-walk mutation")
{
    gweni::controls::resetRenderWalkMutations();

    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    int calls=0;
    ImageHandle::Watch watch(handle, [&](ImageHandle &) { ++calls; });

    // between frames: nothing recorded
    CHECK(handle->setSize(8, 8));
    CHECK(gweni::controls::renderWalkMutations().empty());

    {
        gweni::controls::RenderWalkScope walk;

        CHECK(handle->setSize(16, 8));
        CHECK(handle->setBytes(pngHeader(2, 2)));
    }
    // still applied
    CHECK((handle->state() == ImageHandle::State::Bytes));
    CHECK(calls == 3);
    CHECK(recorded("ImageHandle setSize (rendering none)"));
    CHECK(recorded("ImageHandle setBytes (rendering none)"));
    CHECK_FALSE(recorded("ImageHandle setFailed (rendering none)"));

    {
        gweni::controls::RenderWalkScope walk;

        handle->setFailed();
    }
    CHECK(recorded("ImageHandle setFailed (rendering none)"));
    CHECK(gweni::controls::renderWalkMutations().size() == 3);

    // a call that changes nothing inside the walk records nothing more
    gweni::controls::resetRenderWalkMutations();
    {
        gweni::controls::RenderWalkScope walk;

        handle->setFailed();
    }
    CHECK(gweni::controls::renderWalkMutations().empty());
    gweni::controls::resetRenderWalkMutations();
}
