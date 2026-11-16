/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/imageHandle.h>
#include <gweni/controls/base.h>

#include <imglib/probe.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace gweni
{

ImageHandle::Watch::Watch(std::shared_ptr<ImageHandle> handle, Function function)
{
    reset(std::move(handle), std::move(function));
}

ImageHandle::Watch::~Watch()
{
    reset();
}

void ImageHandle::Watch::reset(std::shared_ptr<ImageHandle> handle, Function function)
{
    if(m_handle)
        m_handle->remove(this);
    m_handle=std::move(handle);
    m_function=std::move(function);
    if(m_handle)
        m_handle->add(this);
}

ImageHandle::~ImageHandle()
{
    // every watch holds the handle, so none is left here
}

bool ImageHandle::setSize(int width, int height)
{
    if(width < 1 || height < 1 || m_state == State::Bytes)
        return false;
    if(m_state == State::Sized && width == m_width && height == m_height)
        return true;

    m_width=width;
    m_height=height;
    m_state=State::Sized;
    changed("setSize");
    return true;
}

bool ImageHandle::setBytes(const uint8_t *bytes, size_t size)
{
    return setBytes(std::vector<uint8_t>(bytes, bytes+(bytes?size:0)));
}

bool ImageHandle::setBytes(std::vector<uint8_t> bytes)
{
    if(m_state == State::Bytes && bytes == m_bytes)
        return true;

    imglib::ImageHeader header;
    const int maxSide=std::numeric_limits<int>::max();

    if(!imglib::probeImage(bytes.data(), bytes.size(), header) ||
        header.width > size_t(maxSide) || header.height > size_t(maxSide))
    {
        // no PNG or JPEG: the handle fails, its size kept for nothing
        if(m_state != State::Failed)
        {
            m_bytes.clear();
            m_state=State::Failed;
            changed("setBytes");
        }
        return false;
    }

    m_bytes=std::move(bytes);
    m_width=int(header.width);
    m_height=int(header.height);
    m_state=State::Bytes;
    changed("setBytes");
    return true;
}

void ImageHandle::setFailed()
{
    if(m_state == State::Failed)
        return;

    m_bytes.clear();
    m_bytes.shrink_to_fit();
    m_state=State::Failed;
    changed("setFailed");
}

size_t ImageHandle::watchCount() const
{
    return m_watches.size()-std::count(m_watches.begin(), m_watches.end(), nullptr);
}

void ImageHandle::changed(const char *operation)
{
    controls::noteRenderWalkCall("ImageHandle", operation);
    ++m_serial;

    // a watch reset by an earlier one's call may have held the last reference
    const std::shared_ptr<ImageHandle> keep=weak_from_this().lock();

    // the watches there now; one removed meanwhile is nulled, one added is
    // appended past count
    const size_t count=m_watches.size();

    ++m_notifying;
    for(size_t i=0; i < count; ++i)
    {
        Watch *watch=m_watches[i];
        if(watch && watch->m_function)
            watch->m_function(*this);
    }
    --m_notifying;

    if(m_notifying == 0)
        m_watches.erase(std::remove(m_watches.begin(), m_watches.end(), nullptr), m_watches.end());
}

void ImageHandle::add(Watch *watch)
{
    m_watches.push_back(watch);
}

void ImageHandle::remove(Watch *watch)
{
    auto found=std::find(m_watches.begin(), m_watches.end(), watch);
    if(found == m_watches.end())
        return;
    if(m_notifying != 0)
        *found=nullptr;
    else
        m_watches.erase(found);
}

}//namespace gweni
