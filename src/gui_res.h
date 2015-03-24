/*
 * Copyright 2014-2015 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

// Font.
//-----

#if !defined(FONT_DESC_FILE)
    #define FONT_DESC_FILE(_name, _fontSize, _path)
#endif //!defined(FONT_DESC_FILE)
//FONT_DESC_FILE( DroidSansMono, 14.0f, "font/DroidSansMono.ttf" )
//FONT_DESC_FILE( StatusFont,    18.0f, "font/DroidSans.ttf"     )
#undef FONT_DESC_FILE

#if !defined(FONT_DESC_MEM)
    #define FONT_DESC_MEM(_name, _fontSize, _data)
#endif //!defined(FONT_DESC_MEM)
FONT_DESC_MEM( DroidSansMono, 14.0f, g_droidSansMono )
FONT_DESC_MEM( StatusFont,    18.0f, g_droidSans     )
#undef FONT_DESC_MEM

// Resources.
//-----

#ifndef RES_DESC
    #define RES_DESC(_defaultFont, _defaultFontDataSize, _defaultFontSize, _sunIconData, _sunIconDataSize)
#endif //!defined(RES_DESC)
RES_DESC(g_droidSans, g_droidSansSize, 15.0f, g_sunIcon, g_sunIconSize)
#undef RES_DESC

// About text.
//-----

#ifndef ABOUT_LINE
    #define ABOUT_LINE(_str)
#endif //!defined(ABOUT_LINE)
ABOUT_LINE("-------------------------------------------------------------------------------")
ABOUT_LINE("cmftStudio v1.1")
ABOUT_LINE("Copyright 2014-2015 Dario Manesku. All rights reserved.")
ABOUT_LINE("https://github.com/dariomanesku/cmftStudio")
ABOUT_LINE("-------------------------------------------------------------------------------")
ABOUT_LINE("")
ABOUT_LINE("Controls:")
ABOUT_LINE("        Ctrl/Meta + F   ->  Toggle full screen.")
ABOUT_LINE("        Ctrl/Meta + Q  ->  Quit application.")
ABOUT_LINE("")
ABOUT_LINE("All features related to cubemap filtering are also available for use from the command-line interface.")
ABOUT_LINE("Check out cmft project - https://github.com/dariomanesku/cmft")
ABOUT_LINE("")
ABOUT_LINE("")
ABOUT_LINE("Feel free to reach me at any time on Github or Twitter @dariomanesku.")
#undef ABOUT_LINE

/* vim: set sw=4 ts=4 expandtab: */
