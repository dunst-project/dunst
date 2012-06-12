/* See LICENSE file for copyright and license details. */

#ifndef DRAW_H
#define DRAW_H

#include <X11/Xft/Xft.h>

typedef struct {
	int x, y, w, h;
	Bool invert;
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
} DC;  /* draw context */

typedef struct {
	unsigned long FG;
	XftColor FG_xft;
	unsigned long BG;
} ColorSet;

void drawrect(DC *dc, int x, int y, unsigned int w, unsigned int h, Bool fill, unsigned long color);
void drawtext(DC *dc, const char *text, ColorSet *col);
void drawtextn(DC *dc, const char *text, size_t n, ColorSet *col);
void freecol(DC *dc, ColorSet *col);
void eprintf(const char *fmt, ...);
void freedc(DC *dc);
unsigned long getcolor(DC *dc, const char *colstr);
ColorSet *initcolor(DC *dc, const char *foreground, const char *background);
DC *initdc(void);
void initfont(DC *dc, const char *fontstr);
void mapdc(DC *dc, Window win, unsigned int w, unsigned int h);
void resizedc(DC *dc, unsigned int w, unsigned int h);
int textnw(DC *dc, const char *text, size_t len);
int textw(DC *dc, const char *text);

#endif
