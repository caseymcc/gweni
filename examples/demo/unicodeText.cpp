/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#include "demo/unicodeText.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/label.h"
#include "gweni/controls/textBox.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(UnicodeTextDemo)
{
    setLayout(new controls::VerticalLayout());

    //escaped rather than literal UTF-8, so the source charset does not matter
    const char *const lines[]=
    {
        "caf\u00E9 \u2014 na\u00EFve \u00BD \u20AC9",          // café — naïve ½ €9
        "\u201CDouble\u201D \u2018single\u2019 \u2026",         // “Double” ‘single’ …
        "\u00A33.50 \u00A5120 \u00A9 \u00B0C",                  // £3.50 ¥120 © °C
        "\u00FC \u00DF \u00E9 \u00F1 \u00E5 \u00F8 \u00E7",  // ü ß é ñ å ø ç
        //OpenSans has no CJK: these draw as the face's missing glyph
        "CJK: \u5355\u5143\u6D4B\u8BD5",                     // CJK: 单元测试
    };

    for(const char *line:lines)
    {
        controls::Label *label=newChild<controls::Label>();
        label->setText(line);
        label->sizeToContents();
    }

    {
        //a TextBox measures its text during layout, so these glyphs are rasterised by
        //measureText in the logic phase, before the frame that first draws them
        controls::TextBox *textBox=newChild<controls::TextBox>();
        textBox->setText("Gr\u00FC\u00DFe, se\u00F1or \u2013 \u00E0 bient\u00F4t"); // Grüße, señor – à bientôt
        textBox->setSize(200, 20);
    }
}

}//namespace gweni
