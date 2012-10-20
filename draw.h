/*

MIT/X Consortium License

© 2012 Sascha Kruse <knopwob@googlemail.com> and contributors
© 2010-2011 Connor Lane Smith <cls@lubutu.com>
© 2006-2011 Anselm R Garbe <anselm@garbe.us>
© 2009 Gottox <gottox@s01.de>
© 2009 Markus Schnalke <meillo@marmaro.de>
© 2009 Evan Gates <evan.gates@gmail.com>
© 2006-2008 Sander van Dijk <a dot h dot vandijk at gmail dot com>
© 2006-2007 Michał Janeczek <janeczek at gmail dot com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

 */
#ifndef DRAW_H
#define DRAW_H

#include <X11/Xft/Xft.h>

typedef struct {
        int x, y, w, h;
        bool invert;
        Display *dpy;
        GC gc;
        Pixmap canvas;
        XftDraw *xftdraw;
        struct {
                int ascent;
                int descent;
                int height;
                int width;
                XFontSet set;
                XFontStruct *xfont;
                XftFont *xft_font;
        } font;
} DC;                           /* draw context */

typedef struct {
        unsigned long FG;
        XftColor FG_xft;
        unsigned long BG;
} ColorSet;

void drawrect(DC * dc, int x, int y, unsigned int w, unsigned int h, bool fill,
              unsigned long color);
void drawtext(DC * dc, const char *text, ColorSet * col);
void drawtextn(DC * dc, const char *text, size_t n, ColorSet * col);
void freecol(DC * dc, ColorSet * col);
void eprintf(const char *fmt, ...);
void freedc(DC * dc);
unsigned long getcolor(DC * dc, const char *colstr);
ColorSet *initcolor(DC * dc, const char *foreground, const char *background);
DC *initdc(void);
void initfont(DC * dc, const char *fontstr);
void setopacity(DC *dc, Window win, unsigned long opacity);
void mapdc(DC * dc, Window win, unsigned int w, unsigned int h);
void resizedc(DC * dc, unsigned int w, unsigned int h);
int textnw(DC * dc, const char *text, size_t len);
int textw(DC * dc, const char *text);

#endif
/* vim: set ts=8 sw=8 tw=0: */
