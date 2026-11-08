/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Color is four plain bytes. SlotRenderer puts it into a vertex as the two
 *  16-bit halves rg() and ba(), which the shader's unpackColor splits again,
 *  so those halves must be (r<<8)|g and (b<<8)|a whatever the byte order.
 */
#include <doctest/doctest.h>

#include <gweni/platforms/platformTypes.h>

using gweni::Color;

TEST_CASE("Color packs its vertex halves high byte first")
{
    const Color c(0x11, 0x22, 0x33, 0x44);

    CHECK(c.rg() == 0x1122);
    CHECK(c.ba() == 0x3344);

    CHECK(Color(255, 0, 0, 0).rg() == 0xff00);
    CHECK(Color(0, 255, 0, 0).rg() == 0x00ff);
    CHECK(Color(0, 0, 255, 0).ba() == 0xff00);
    CHECK(Color(0, 0, 0, 255).ba() == 0x00ff);
}

TEST_CASE("Color copies and chains assignment")
{
    Color a, b;
    const Color c(1, 2, 3, 4);

    a=b=c;

    CHECK(a == c);
    CHECK(b == c);
    CHECK(a.r == 1);
    CHECK(a.g == 2);
    CHECK(a.b == 3);
    CHECK(a.a == 4);
}
