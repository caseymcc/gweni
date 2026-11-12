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
        //OpenSans has none of these but the bullet: they draw from its glyph
        //fallback chain (the skin's "fontFallback", DejaVu Sans Mono), and the
        //U+FE0F of the second warning sign draws nothing
        "Symbols: \u2192 \u2190 \u2191 \u2193 \u2713 \u2717 \u26A0 \u26A0\uFE0F \u2022 \u2605", // → ← ↑ ↓ ✓ ✗ ⚠ ⚠️ • ★
        //neither OpenSans nor its chain has CJK: these draw as the face's missing glyph
        "CJK: \u5355\u5143\u6D4B\u8BD5",                     // CJK: 单元测试
    };

    for(const char *line:lines)
    {
        controls::Label *label=newChild<controls::Label>();
        label->setText(line);
        label->sizeToContents();
    }

    {
        //a log table in the skin's mono face, laid out in terminal cells: every
        //row is 27 cells, the \u26A1 two of them (a terminal's wcwidth), so its
        //rules line up from row to row
        //                ┌───────┬─────────┬───────┐
        //                │ step  │ status  │  time │
        //                ├───────┼─────────┼───────┤
        //                │ build │ ✓ ok    │  12 s │
        //                │ test  │ ✗ fail  │   3 s │
        //                │ lint  │ ⚠ warn  │ ⚡1 s │
        //                │ docs  │ → next  │     – │
        //                └───────┴─────────┴───────┘
        const char *const log=
            "\u250C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u252C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n"
            "\u2502 step  \u2502 status  \u2502  time \u2502\n"
            "\u251C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u253C\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2524\n"
            "\u2502 build \u2502 \u2713 ok    \u2502  12 s \u2502\n"
            "\u2502 test  \u2502 \u2717 fail  \u2502   3 s \u2502\n"
            "\u2502 lint  \u2502 \u26A0 warn  \u2502 \u26A11 s \u2502\n"
            "\u2502 docs  \u2502 \u2192 next  \u2502     \u2013 \u2502\n"
            "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2534\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518";

        controls::Label *label=newChild<controls::Label>();
        label->setFont(getSkin()->getDefaultMonoFont());
        label->setText(log);
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
