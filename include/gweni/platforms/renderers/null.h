/*
 *  Gweni
 
*  See license in Gweni.h
 */



#ifndef _gweni_renderers_null_h_
#define _gweni_renderers_null_h_

#include <gweni/platforms/baseRender.h>

namespace gweni
{
    namespace renderer
    {
        //
        //! \brief Null renderer that does not render!
        //!
        //! This is used for build and unit testing.
        //
        class GWENI_EXPORT Null : public gweni::renderer::Base
        {
        public:
            Null(ResourcePaths& paths);
            virtual ~Null();

            
            Font::Status loadFont(const gweni::Font& font) final { return Font::Status::Loaded; }
            void freeFont(const gweni::Font& font) final {}
            bool ensureFont(const gweni::Font& font) final { return true; }

            Texture::Status loadTexture(const gweni::Texture& texture) final {  return Texture::Status::Loaded; }
            void freeTexture(const gweni::Texture& texture) final {}
            TextureData getTextureData(const gweni::Texture& texture) const final { return TextureData(); }
        };
    }
}
#endif // _gweni_renderers_null_h_
