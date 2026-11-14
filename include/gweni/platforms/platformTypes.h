/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */



#ifndef _gweni_platformtypes_h_
#define _gweni_platformtypes_h_

#ifdef _MSC_VER
#   pragma warning( disable : 4244 )
#   pragma warning( disable : 4251 )
#endif

#include "gweni/config.h"
#include "gweni/export.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <list>
#include <memory>
#include <vector>
#include <limits>
#include <type_traits>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gweni
{
namespace controls
{
class Base;
class Canvas;
}

//
// All strings are UTF-8 for Unicode. This is backwards compatible with ASCII.
// See: http://www.utf8everywhere.org
//
typedef std::string String;
typedef char32_t UnicodeChar;    //!< One typed character: a Unicode codepoint.

namespace cursor
{
enum
{
    Normal=0,
    Beam=1,
    SizeNS=2,
    SizeWE=3,
    SizeNWSE=4,
    SizeNESW=5,
    SizeAll=6,
    No=7,
    Wait=8,
    Finger=9,
    Cursor_Count=10
};
}

struct GWENI_EXPORT Point
{
    Point(int x_=0, int y_=0)
        : x{x_}, y{y_}
    {}

    void set(int x_, int y_)
    {
        x=x_, y=y_;
    }

    void operator+= (const Point &p)
    {
        x+=p.x;
        y+=p.y;
    }

    Point operator+ (const Point &p) const
    {
        return Point(x + p.x, p.y + y);
    }

    void operator-= (const Point &p)
    {
        x-=p.x;
        y-=p.y;
    }

    Point operator- (const Point &p) const
    {
        return Point(x - p.x, y - p.y);
    }

    union
    {
        struct { int x, y; };
        int v[2];
    };
};

//! A point with fractional coordinates, in canvas units like Point: what a
//! line's endpoints are given in (renderer::Base::drawLine), so a plot can put
//! its samples between whole units. A Point converts to it.
struct GWENI_EXPORT PointF
{
    PointF(float x_=0.0f, float y_=0.0f)
        : x{x_}, y{y_}
    {}

    PointF(const Point &p)
        : x{float(p.x)}, y{float(p.y)}
    {}

    bool operator==(const PointF &p) const { return x == p.x && y == p.y; }
    bool operator!=(const PointF &p) const { return !(*this == p); }

    float x, y;
};

struct GWENI_EXPORT Size
{
    Size():width{0}, height{0} {}
    Size(int width, int height):width{width}, height{height} {}

    union
    {
        struct { int width, height; };
        int value[2];
    };
};

struct GWENI_EXPORT Rect
{
    Rect(int x_=0, int y_=0, int w_=0, int h_=0):
        x{x_}, y{y_}, w{w_}, h{h_}
    {}

    Rect(const Point &o, const Point &sz):
        x{o.x}, y{o.y}, w{sz.x}, h{sz.y}
    {}

    Rect(const glm::ivec4 &that):
        x{that.x}, y{that.y}, w{that.z}, h{that.w}
    {}

    Rect operator=(const glm::ivec4 &that)
    {
        std::copy(glm::value_ptr(that), glm::value_ptr(that)+4, v);
        return *this;
    }

    int left() const { return x; }
    int right() const { return x + w; }
    int top() const { return y; }
    int bottom() const { return y + h; }

    bool operator== (const Rect &other) const
    {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }

    Rect operator+ (const Rect &rct) const
    {
        const Rect m(x + rct.x, y + rct.y, w + rct.w, h + rct.h);
        return m;
    }

    Point getSize() const { return Point(w, h); }

    union
    {
        struct { int x, y, w, h; };
        int v[4];
    };
};

struct GWENI_EXPORT BoundingBox
{
    BoundingBox(int left_=0, int top_=0, int right_=0, int bottom_=0)
        : left(left_)
        , top(top_)
        , right(right_)
        , bottom(bottom_)
    {}

    BoundingBox(const glm::ivec4 &that)
        : left(that.z)
        , top(that.x)
        , right(that.w)
        , bottom(that.y)
    {}

    BoundingBox operator=(const glm::ivec4 &that)
    {
        std::copy(glm::value_ptr(that), glm::value_ptr(that)+4, v);
        return *this;
    }

    BoundingBox operator+(const BoundingBox &margin) const
    {
        BoundingBox m;

        m.top=top + margin.top;
        m.bottom=bottom + margin.bottom;
        m.left=left + margin.left;
        m.right=right + margin.right;

        return m;
    }

    int x() { return left; }
    int y() { return top; }
    int width() { return right-left; }
    int height() { return bottom-top; }

    union
    {
        struct { int left, top, right, bottom; };
        int v[4];
    };
};

struct GWENI_EXPORT Color
{
    constexpr Color(unsigned char r_=255,
        unsigned char g_=255,
        unsigned char b_=255,
        unsigned char a_=255)
        : r(r_), g(g_), b(b_), a(a_)
    {}

    void operator+= (const Color &c)
    {
        r=static_cast<unsigned char>(r + c.r);
        g=static_cast<unsigned char>(g + c.g);
        b=static_cast<unsigned char>(b + c.b);
        a=static_cast<unsigned char>(a + c.a);
    }

    void operator-= (const Color &c)
    {
        r=static_cast<unsigned char>(r - c.r);
        g=static_cast<unsigned char>(g - c.g);
        b=static_cast<unsigned char>(b - c.b);
        a=static_cast<unsigned char>(a - c.a);
    }

    void operator*= (float f)
    {
        *this=*this * f;
    }

    Color operator* (float f) const
    {
        return Color(
            static_cast<unsigned char>(f*r),
            static_cast<unsigned char>(f*g),
            static_cast<unsigned char>(f*b),
            static_cast<unsigned char>(f*a)
        );
    }

    Color operator- (const Color &c) const
    {
        return Color(
            static_cast<unsigned char>(r - c.r),
            static_cast<unsigned char>(g - c.g),
            static_cast<unsigned char>(b - c.b),
            static_cast<unsigned char>(a - c.a)
        );
    }

    Color operator+ (const Color &c) const
    {
        return Color(
            static_cast<unsigned char>(r + c.r),
            static_cast<unsigned char>(g + c.g),
            static_cast<unsigned char>(b + c.b),
            static_cast<unsigned char>(a + c.a)
        );
    }

    bool operator== (const Color &c) const
    {
        return c.r==r && c.g==g && c.b==b && c.a==a;
    }

    // The two 16-bit halves OpenGLCore packs into a vertex (unpacked by the
    // shader's unpackColor), built with shifts so they do not depend on byte order.
    constexpr uint16_t rg() const { return uint16_t(r << 8 | g); }
    constexpr uint16_t ba() const { return uint16_t(b << 8 | a); }

    unsigned char r, g, b, a;
};

static_assert(Color(0x11, 0x22, 0x33, 0x44).rg() == 0x1122 &&
              Color(0x11, 0x22, 0x33, 0x44).ba() == 0x3344,
              "Color::rg()/ba() must pack as (r<<8)|g and (b<<8)|a");
static_assert(std::is_trivially_copyable_v<Color>, "Color must be trivially copyable");

namespace colors
{
static const Color Black(0, 0, 0, 255);
static const Color Red(255, 0, 0, 255);
static const Color Yellow(255, 255, 0, 255);
static const Color White(255, 255, 255, 255);
static const Color Blue(0, 0, 255, 255);
static const Color Green(0, 255, 0, 255);
static const Color Grey(200, 200, 200, 255);
static const Color GreyLight(230, 230, 230, 255);
//! What a missing asset is drawn in: a missing image, a missing skin config's
//! surfaces. MissingDark is their borders, so controls stay apart.
static const Color Missing(255, 0, 255, 255);
static const Color MissingDark(128, 0, 128, 255);
}

struct Font
{
    typedef std::list<Font *> List;

    enum class Status
    {
        Unloaded,               //!< As yet, unloaded.
        Loaded,                 //!< Loaded successful.
        ErrorFileNotFound,      //!< File requested was not found.
        ErrorBadData,           //!< Resource was bad data.
        //! The file was missing or bad data, and the face's text is drawn in the
        //! renderer's built-in fallback font instead: text works, the face is wrong.
        LoadedFallback,
        MaxStatus
    };

    Font()
        : facename("?")
        , size(10)
        , bold(false)
    {}

    Font(const Font &)=default;
    Font &operator=(const Font &)=default;

    inline bool operator==(const Font &rhs) const
    {
        return facename == rhs.facename && size == rhs.size && bold == rhs.bold;
    }

    inline bool operator!=(const Font &rhs) const
    {
        return  bold != rhs.bold || size != rhs.size || facename != rhs.facename;
    }

    String facename;
    float size;
    bool bold;
};

struct Texture
{
    typedef std::list<Texture *> List;

    //! Status of load operation.
    enum class Status
    {
        Unloaded,               //!< As yet, unloaded.
        Loaded,                 //!< Loaded successful.
        ErrorFileNotFound,      //!< File requested was not found.
        ErrorBadData,           //!< Resource was bad data.
        MaxStatus
    };

    Texture()
        : readable(false),
        dataIndex(std::numeric_limits<size_t>::max())
    {}

    Texture(const Texture &)=default;
    Texture &operator=(const Texture &)=default;

    inline bool operator==(const Texture &rhs) const { return name == rhs.name; }
    inline bool operator!=(const Texture &rhs) const { return name != rhs.name; }

    String  name;
    bool readable;
    size_t dataIndex;
};

struct TextureData
{
    TextureData()
        : width{0.f}, height{0.f}, readable{false}
    {}

    float width;
    float height;
    bool readable;
};


//! Base class for resource path calculation.
//!
//! Can be implemented by different platforms for different file layouts.
class ResourcePaths
{
public:
    //! Resource type.
    enum class Type
    {
        Font,           //!< Font resource.
        Texture,        //!< Texture resource.
        Other,          //!< Other type of resource.
        MaxType
    };

    virtual ~ResourcePaths() {}
    virtual String getPath(Type type, String const &name)=0;
};


//! Interface for resource loaders.
//!
//! These are used to load, create and destroy resource needed for Gweni.
class IResourceLoader
{
public:

    //! Types of notification that the loader might want to know about.
    enum class NotificationType
    {
        DeviceLost,             //!< Device lost, perhaps release resources.
        DeviceAcquired,         //!< Device reappeared, perhaps reload resources.
        ReleaseResources,       //!< Release any resources held.
        MaxMessages,
        UserMessageStart=100
    };

    //! loadFont and freeFont are a counted pair: every load that succeeds (Loaded
    //! or LoadedFallback) holds the font until one freeFont gives it back, so two
    //! callers holding an equal Font each load it and each free it. A loader may
    //! also keep a font while text is still laid out in it, freeing it once the
    //! last of that text is gone.
    virtual gweni::Font::Status loadFont(const gweni::Font &font)=0;
    virtual void freeFont(const gweni::Font &font)=0;

    //! The glyph fallback chain of a face: the faces, in order, a codepoint the
    //! face has no glyph for is drawn from; an empty face name sets the default
    //! chain of every face without one of its own. A loader that draws no text
    //! from faces of its own ignores it (renderer::SlotRenderer's is the one).
    virtual void setGlyphFallback(const gweni::String &face, const std::vector<gweni::String> &chain) {}

    virtual gweni::Texture::Status loadTexture(gweni::Texture &texture)=0;
    //! An image from encoded bytes (a PNG or JPEG, the format found from its first
    //! bytes) or from rows of RGBA8 texels stride bytes apart, kept at a longest
    //! side of at most maxSize image px (0: as it is). A loader that loads no
    //! images from memory fails them (renderer::SlotRenderer's is the one).
    virtual gweni::Texture::Status loadTexture(gweni::Texture &texture, const uint8_t *bytes, size_t size,
        int maxSize=0)
    {
        return gweni::Texture::Status::ErrorBadData;
    }
    virtual gweni::Texture::Status loadTexturePixels(gweni::Texture &texture, int width, int height, size_t stride,
        const uint8_t *rgba, int maxSize=0)
    {
        return gweni::Texture::Status::ErrorBadData;
    }
    //! The textures loaded and not freed yet, for a leak check (0 where not counted)
    virtual size_t loadedTextureCount() const { return 0; }
    virtual void freeTexture(const gweni::Texture &texture)=0;
    virtual gweni::TextureData getTextureData(const gweni::Texture &texture) const=0;

    //! Notification of certain events. May be platform specific.
    //! Loader can deal with the events accordingly.
    virtual void notify(NotificationType msg) {}
};

template <class T>
static inline void hash_combine(size_t &seed, const T &v)
{
    seed^=std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} // namespace gweni

// Hashers
namespace std
{
template<> struct hash<gweni::Font>
{
    typedef gweni::Font argument_type;
    typedef std::size_t result_type;


    result_type operator()(argument_type const &f) const noexcept
    {
        result_type res=std::hash<gweni::String>{}(f.facename);

        gweni::hash_combine<decltype(f.size)>(res, f.size);
        gweni::hash_combine<decltype(f.bold)>(res, f.bold);

        return res;
    }
};
template<> struct hash<gweni::Texture>
{
    typedef gweni::Texture argument_type;
    typedef std::size_t result_type;


    result_type operator()(argument_type const &f) const noexcept
    {
        return std::hash<decltype(f.name)>{}(f.name);
    }
};
} // namespace std

#endif //_gweni_platformtypes_h_
