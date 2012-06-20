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

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "draw.h"

#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define MIN(a, b)  ((a) < (b) ? (a) : (b))

void
drawrect(DC *dc, int x, int y, unsigned int w, unsigned int h, Bool fill, unsigned long color) {
	XSetForeground(dc->dpy, dc->gc, color);
	if(fill)
		XFillRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x, dc->y + y, w, h);
	else
		XDrawRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x, dc->y + y, w-1, h-1);
}

void
drawtext(DC *dc, const char *text, ColorSet *col) {
	char buf[BUFSIZ];
	size_t mn, n = strlen(text);

	/* shorten text if necessary */
	for(mn = MIN(n, sizeof buf); textnw(dc, text, mn) + dc->font.height/2 > dc->w; mn--)
		if(mn == 0)
			return;
	memcpy(buf, text, mn);
	if(mn < n)
		for(n = MAX(mn-3, 0); n < mn; buf[n++] = '.');

	drawrect(dc, 0, 0, dc->w, dc->h, True, col->BG);
	drawtextn(dc, buf, mn, col);
}

void
drawtextn(DC *dc, const char *text, size_t n, ColorSet *col) {
	int x = dc->x + dc->font.height/2;
	int y = dc->y + dc->font.ascent+1;

	XSetForeground(dc->dpy, dc->gc, col->FG);
	if(dc->font.xft_font) {
		if (!dc->xftdraw)
			eprintf("error, xft drawable does not exist");
		XftDrawStringUtf8(dc->xftdraw, &col->FG_xft,
			dc->font.xft_font, x, y, (unsigned char*)text, n);
	} else if(dc->font.set) {
        printf("XmbDrawString\n");
		XmbDrawString(dc->dpy, dc->canvas, dc->font.set, dc->gc, x, y, text, n);
	} else {
		XSetFont(dc->dpy, dc->gc, dc->font.xfont->fid);
		XDrawString(dc->dpy, dc->canvas, dc->gc, x, y, text, n);
	}
}

void
eprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	}
	exit(EXIT_FAILURE);
}

void
freecol(DC *dc, ColorSet *col) {
    if(col) {
        if(&col->FG_xft)
            XftColorFree(dc->dpy, DefaultVisual(dc->dpy, DefaultScreen(dc->dpy)),
                DefaultColormap(dc->dpy, DefaultScreen(dc->dpy)), &col->FG_xft);
        free(col);
    }
}

void
freedc(DC *dc) {
    if(dc->font.xft_font) {
        XftFontClose(dc->dpy, dc->font.xft_font);
        XftDrawDestroy(dc->xftdraw);
    }
	if(dc->font.set)
		XFreeFontSet(dc->dpy, dc->font.set);
    if(dc->font.xfont)
		XFreeFont(dc->dpy, dc->font.xfont);
    if(dc->canvas)
		XFreePixmap(dc->dpy, dc->canvas);
	if(dc->gc)
        XFreeGC(dc->dpy, dc->gc);
	if(dc->dpy)
        XCloseDisplay(dc->dpy);
	if(dc)
        free(dc);
}

unsigned long
getcolor(DC *dc, const char *colstr) {
	Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
	XColor color;

	if(!XAllocNamedColor(dc->dpy, cmap, colstr, &color, &color))
		eprintf("cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

ColorSet *
initcolor(DC *dc, const char * foreground, const char * background) {
	ColorSet * col = (ColorSet *)malloc(sizeof(ColorSet));
	if(!col)
		eprintf("error, cannot allocate memory for color set");
	col->BG = getcolor(dc, background);
	col->FG = getcolor(dc, foreground);
	if(dc->font.xft_font)
		if(!XftColorAllocName(dc->dpy, DefaultVisual(dc->dpy, DefaultScreen(dc->dpy)),
			DefaultColormap(dc->dpy, DefaultScreen(dc->dpy)), foreground, &col->FG_xft))
			eprintf("error, cannot allocate xft font color '%s'\n", foreground);
	return col;
}

DC *
initdc(void) {
	DC *dc;

	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("no locale support\n", stderr);
	if(!(dc = calloc(1, sizeof *dc)))
		eprintf("cannot malloc %u bytes:", sizeof *dc);
	if(!(dc->dpy = XOpenDisplay(NULL)))
		eprintf("cannot open display\n");

	dc->gc = XCreateGC(dc->dpy, DefaultRootWindow(dc->dpy), 0, NULL);
	XSetLineAttributes(dc->dpy, dc->gc, 1, LineSolid, CapButt, JoinMiter);
	return dc;
}

void
initfont(DC *dc, const char *fontstr) {
	char *def, **missing, **names;
	int i, n;
	XFontStruct **xfonts;

	missing = NULL;
	if((dc->font.xfont = XLoadQueryFont(dc->dpy, fontstr))) {
		dc->font.ascent = dc->font.xfont->ascent;
		dc->font.descent = dc->font.xfont->descent;
		dc->font.width   = dc->font.xfont->max_bounds.width;
	} else if((dc->font.set = XCreateFontSet(dc->dpy, fontstr, &missing, &n, &def))) {
		n = XFontsOfFontSet(dc->font.set, &xfonts, &names);
		for(i = 0; i < n; i++) {
			dc->font.ascent  = MAX(dc->font.ascent,  xfonts[i]->ascent);
			dc->font.descent = MAX(dc->font.descent, xfonts[i]->descent);
			dc->font.width   = MAX(dc->font.width,   xfonts[i]->max_bounds.width);
		}
	} else if((dc->font.xft_font = XftFontOpenName(dc->dpy, DefaultScreen(dc->dpy), fontstr))) {
		dc->font.ascent = dc->font.xft_font->ascent;
		dc->font.descent = dc->font.xft_font->descent;
		dc->font.width = dc->font.xft_font->max_advance_width;
	} else {
		eprintf("cannot load font '%s'\n", fontstr);
	}
	if(missing)
		XFreeStringList(missing);
	dc->font.height = dc->font.ascent + dc->font.descent;
	return;
}

void
mapdc(DC *dc, Window win, unsigned int w, unsigned int h) {
	XCopyArea(dc->dpy, dc->canvas, win, dc->gc, 0, 0, w, h, 0, 0);
}

void
resizedc(DC *dc, unsigned int w, unsigned int h) {
	int screen = DefaultScreen(dc->dpy);
	if(dc->canvas)
		XFreePixmap(dc->dpy, dc->canvas);

	dc->w = w;
	dc->h = h;
	dc->canvas = XCreatePixmap(dc->dpy, DefaultRootWindow(dc->dpy), w, h,
	                           DefaultDepth(dc->dpy, screen));
    if(dc->xftdraw) {
        XftDrawDestroy(dc->xftdraw);
    }
	if(dc->font.xft_font) {
		dc->xftdraw = XftDrawCreate(dc->dpy, dc->canvas, DefaultVisual(dc->dpy,screen), DefaultColormap(dc->dpy,screen));
		if(!(dc->xftdraw))
			eprintf("error, cannot create xft drawable\n");
	}
}

int
textnw(DC *dc, const char *text, size_t len) {
	if(dc->font.xft_font) {
		XGlyphInfo gi;
		XftTextExtentsUtf8(dc->dpy, dc->font.xft_font, (const FcChar8*)text, len, &gi);
		return gi.width;
	} else if(dc->font.set) {
		XRectangle r;
		XmbTextExtents(dc->font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc->font.xfont, text, len);
}

int
textw(DC *dc, const char *text) {
	return textnw(dc, text, strlen(text)) + dc->font.height;
}
