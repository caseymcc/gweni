/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_imageHandle_h_
#define _gweni_imageHandle_h_

#include <gweni/export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace gweni
{

//! An image whose bytes the app supplies after the control showing it was laid
//! out: a Markdown document's ![alt](url), asked of the app's resolver while it
//! lays out, the bytes fetched over HTTP some frames later. Shared
//! (std::shared_ptr): one handle may be shown by several documents and blocks,
//! each watching it through an ImageHandle::Watch, which every change notifies.
//!
//! Its life: None (nothing known), then optionally Sized (setSize: the app knows
//! the dimensions, so a placeholder can take the image's box), then Bytes
//! (setBytes: an encoded PNG or JPEG whose header was read, its width and height
//! now the header's) or Failed (setFailed, or bytes whose header is no PNG's or
//! JPEG's). A Failed handle may be filled in again (a retry). A call that changes
//! nothing (the same size, the same bytes, failing a failed handle) notifies
//! nothing. The pixels are decoded by each watcher's renderer as it loads them
//! (IResourceLoader::loadTexture's bytes overload), not here: the handle holds
//! the bytes, so it needs no renderer and outlives any canvas.
//!
//! The app fills a handle in between frames, from the UI thread, never inside
//! renderCanvas(): a watcher marks its control for layout from the
//! notification, which belongs to the update phase. A fill made while a
//! controls::RenderWalkScope is open is reported as a render-walk mutation
//! ("ImageHandle setBytes (rendering <Type>)" in
//! gweni::controls::renderWalkMutations(), one line on stderr) and still
//! applied.
class GWENI_EXPORT ImageHandle: public std::enable_shared_from_this<ImageHandle>
{
public:
    enum class State
    {
        None,       //!< nothing known yet
        Sized,      //!< the dimensions are known, the bytes not yet
        Bytes,      //!< the encoded bytes are here, their header read
        Failed      //!< the app gave up, or the bytes were no PNG or JPEG
    };

    //! Watches one handle for as long as it lives: its function is called after
    //! each change of the handle, with the handle. Deleting it (or reset())
    //! unregisters it, from inside a notification too: a watch deleted by an
    //! earlier one's call is not called. A watch made during a notification is
    //! first called on the next change. It holds the handle, so the handle
    //! outlives every watch on it.
    class GWENI_EXPORT Watch
    {
    public:
        using Function=std::function<void(ImageHandle &)>;

        Watch()=default;
        Watch(std::shared_ptr<ImageHandle> handle, Function function);
        ~Watch();
        Watch(const Watch &)=delete;
        Watch &operator=(const Watch &)=delete;

        //! Watches another handle (null: none)
        void reset(std::shared_ptr<ImageHandle> handle=nullptr, Function function=nullptr);

        const std::shared_ptr<ImageHandle> &handle() const { return m_handle; }

    private:
        friend class ImageHandle;

        std::shared_ptr<ImageHandle> m_handle;
        Function m_function;
    };

    ImageHandle()=default;
    ~ImageHandle();
    ImageHandle(const ImageHandle &)=delete;
    ImageHandle &operator=(const ImageHandle &)=delete;

    static std::shared_ptr<ImageHandle> create() { return std::make_shared<ImageHandle>(); }

    //! The image's dimensions in image px, before its bytes come. Ignored (false)
    //! for a width or height under 1 and once the bytes are here, whose header
    //! gives the size.
    bool setSize(int width, int height);
    //! The encoded image, a PNG or JPEG: its header gives the size (Bytes), or the
    //! handle fails when it can be read as neither (false, Failed). The bytes are
    //! copied.
    bool setBytes(const uint8_t *bytes, size_t size);
    bool setBytes(std::vector<uint8_t> bytes);
    //! The image will not come: the bytes, if any, dropped, the size kept
    void setFailed();

    State state() const { return m_state; }
    //! 0 until known
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool hasSize() const { return m_width > 0 && m_height > 0; }
    //! Empty unless state() is Bytes
    const std::vector<uint8_t> &bytes() const { return m_bytes; }
    //! Counts every change, from 0, so a watcher can tell it has seen one
    uint64_t serial() const { return m_serial; }
    //! The watches on it now
    size_t watchCount() const;

private:
    void changed(const char *operation);
    void add(Watch *watch);
    void remove(Watch *watch);

    State m_state=State::None;
    int m_width=0;
    int m_height=0;
    std::vector<uint8_t> m_bytes;
    uint64_t m_serial=0;

    //! Null where a watch went away during a notification, compacted after it
    std::vector<Watch *> m_watches;
    int m_notifying=0;
};

}//namespace gweni

#endif//_gweni_imageHandle_h_
