/* See LICENSE file for copyright and license details. */

#define FG(dc, col)  ((col)[(dc)->invert ? ColBG : ColFG])
#define BG(dc, col)  ((col)[(dc)->invert ? ColFG : ColBG])

enum { ColBG, ColFG, ColBorder, ColLast };

typedef struct {
	int x, y, w, h;
	Bool invert;
	Display *dpy;
	GC gc;
	Pixmap canvas;
	struct {
		int ascent;
		int descent;
		int height;
		int width;
		XFontSet set;
		XFontStruct *xfont;
	} font;
} DC;  /* draw context */

void drawrect(DC *dc, int x, int y, unsigned int w, unsigned int h, Bool fill, unsigned long color);
void drawtext(DC *dc, const char *text, unsigned long col[ColLast]);
void drawtextn(DC *dc, const char *text, size_t n, unsigned long col[ColLast]);
void eprintf(const char *fmt, ...);
void freedc(DC *dc);
unsigned long getcolor(DC *dc, const char *colstr);
DC *initdc(void);
void initfont(DC *dc, const char *fontstr);
void mapdc(DC *dc, Window win, unsigned int w, unsigned int h);
void resizedc(DC *dc, unsigned int w, unsigned int h);
int textnw(DC *dc, const char *text, size_t len);
int textw(DC *dc, const char *text);
