/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tests_unit_curveEditorFixture_h_
#define _gweni_tests_unit_curveEditorFixture_h_

namespace test
{
// The input handler's last click is process-wide, including across both
// CurveEditor fixture translation units. A new scene must never rewind time.
inline double curveEditorClock=10.0;
}

#endif
