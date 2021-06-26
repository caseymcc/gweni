/*
  * Gweni
 *
  * Copyright (c) 2010 Facepunch Studios
 *
  * MIT License
 *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
 *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
 *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE.
 */


#ifndef _gweni_gweni_h_
#define _gweni_gweni_h_

#include <gweni/controls/base.h>

namespace gweni
{
extern GWENI_EXPORT controls::Base *HoveredControl;
extern GWENI_EXPORT controls::Base *KeyboardFocus;
extern GWENI_EXPORT controls::Base *MouseFocus;

namespace input
{
inline void unfocus()
{
    if(KeyboardFocus)
        KeyboardFocus->unfocus();
}

}

} // namespace gweni

#endif//_gweni_gweni_h_
