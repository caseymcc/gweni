/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
*  See license in Gweni.h
 */

#ifndef GWENI_RENDERERS_OPENGL_DEBUGFONT_H
#define GWENI_RENDERERS_OPENGL_DEBUGFONT_H

#include <gweni/Gweni.h>
#include <gweni/Renderers/OpenGL.h>

namespace gweni
{
    namespace renderer
    {
        class OpenGL_DebugFont : public gweni::renderer::OpenGL
        {
        public:

            OpenGL_DebugFont();
            ~OpenGL_DebugFont();

            void init();

            void renderText(gweni::Font* font, gweni::Point pos,
                            const gweni::String& text);
            gweni::Point measureText(gweni::Font* font, const gweni::String& text);

        protected:

            void CreateDebugFont();
            void DestroyDebugFont();

            gweni::Texture*  m_fontTexture;
            float m_fFontScale[2];
            float m_fLetterSpacing;

        };


    }
}
#endif //GWENI_RENDERERS_OPENGL_DEBUGFONT_H
