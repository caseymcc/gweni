/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/skin.h>
#include <gweni/utility.h>
#include <gweni/platforms/platform.h>


namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(TextBoxNumeric)
{
    setText("0");
}

bool TextBoxNumeric::isTextAllowed(const String &str, int pos)
{
    const String &strString=getText();

    if(str.length() == 0)
        return true;

    for(size_t i=0; i < str.length(); i++)
    {
        if(str[i] == '-')
        {
            // Has to be at the start
            if(i != 0 || pos != 0)
                return false;

            // Can only be one
            if(std::count(strString.begin(), strString.end(), '-') > 0)
                return false;

            continue;
        }

        if(str[i] >= '0' && str[i] <= '9')
            continue;

        if(str[i] == '.')
        {
            // Already a fullstop
            if(std::count(strString.begin(), strString.end(), '.') > 0)
                return false;

            continue;
        }

        return false;
    }

    return true;
}

float TextBoxNumeric::GetFloatFromText()
{
    float temp=toFloat(getText());
    return temp;
}

}//namespace controls
}//namespace gweni
