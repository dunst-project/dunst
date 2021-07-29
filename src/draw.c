#include "draw.h"

#include <assert.h>
#include <math.h>
#include <pango/pango-attributes.h>
#include <pango/pangocairo.h>
#include <pango/pango-font.h>
#include <pango/pango-layout.h>
#include <pango/pango-types.h>
#include <stdlib.h>
#include <inttypes.h>
#include <glib.h>

#include "dunst.h"
#include "icon.h"
#include "log.h"
#include "markup.h"
#include "notification.h"
#include "queues.h"
#include "output.h"
#include "settings.h"
#include "utils.h"

struct color {
        double r;
        double g;
        double b;
        double a;
};

struct colored_layout {
        PangoLayout *l;
        struct color fg;
        struct color bg;
        struct color highlight;
        struct color frame;
        char *text;
        PangoAttrList *attr;
        cairo_surface_t *icon;
        const struct notification *n;
};

const struct output *output;
window win;

PangoFontDescription *pango_fdesc;

#define UINT_MAX_N(bits) ((1 << bits) - 1)

void draw_setup(void)
{
        const struct output *out = output_create(settings.force_xwayland);
        output = out;

        win = out->win_create();

        pango_fdesc = pango_font_description_from_string(settings.font);
}

static struct color hex_to_color(uint32_t hexValue, int dpc)
{
        const int bpc = 4 * dpc;
        const unsigned single_max = UINT_MAX_N(bpc);

        struct color ret;
        ret.r = ((hexValue >> 3 * bpc) & single_max) / (double)single_max;
        ret.g = ((hexValue >> 2 * bpc) & single_max) / (double)single_max;
        ret.b = ((hexValue >> 1 * bpc) & single_max) / (double)single_max;
        ret.a = ((hexValue)            & single_max) / (double)single_max;

        return ret;
}

static struct color string_to_color(const char *str)
{
        if (STR_FULL(str)) {
                char *end;
                uint_fast32_t val = strtoul(str+1, &end, 16);
                if (end[0] != '\0' && end[1] != '\0') {
                        LOG_W("Invalid color string: '%s'", str);
                }

                switch (end - (str+1)) {
                        case 3:  return hex_to_color((val << 4) | 0xF, 1);
                        case 6:  return hex_to_color((val << 8) | 0xFF, 2);
                        case 4:  return hex_to_color(val, 1);
                        case 8:  return hex_to_color(val, 2);
                }
        }

        /* return black on error */
        LOG_W("Invalid color string: '%s'", str);
        return hex_to_color(0xF, 1);
}

static inline double color_apply_delta(double base, double delta)
{
        base += delta;
        if (base > 1)
                base = 1;
        if (base < 0)
                base = 0;

        return base;
}

static struct color calculate_foreground_color(struct color bg)
{
        double c_delta = 0.1;
        struct color fg = bg;

        /* do we need to darken or brighten the colors? */
        bool darken = (bg.r + bg.g + bg.b) / 3 > 0.5;

        int signedness = darken ? -1 : 1;

        fg.r = color_apply_delta(fg.r, c_delta * signedness);
        fg.g = color_apply_delta(fg.g, c_delta * signedness);
        fg.b = color_apply_delta(fg.b, c_delta * signedness);

        return fg;
}

static struct color layout_get_sepcolor(struct colored_layout *cl,
                                        struct colored_layout *cl_next)
{
        switch (settings.sep_color.type) {
        case SEP_FRAME:
                if (cl_next->n->urgency > cl->n->urgency)
                        return cl_next->frame;
                else
                        return cl->frame;
        case SEP_CUSTOM:
                return string_to_color(settings.sep_color.sep_color);
        case SEP_FOREGROUND:
                return cl->fg;
        case SEP_AUTO:
                return calculate_foreground_color(cl->bg);
        default:
                LOG_E("Invalid %s enum value in %s:%d", "sep_color", __FILE__, __LINE__);
                break;
        }
}

static void layout_setup_pango(PangoLayout *layout, int width)
{
        int scale = output->get_scale();
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_width(layout, width * scale * PANGO_SCALE);
        pango_layout_set_font_description(layout, pango_fdesc);
        pango_layout_set_spacing(layout, settings.line_height * scale * PANGO_SCALE);

        PangoAlignment align;
        switch (settings.align) {
        case ALIGN_LEFT:
        default:
                align = PANGO_ALIGN_LEFT;
                break;
        case ALIGN_CENTER:
                align = PANGO_ALIGN_CENTER;
                break;
        case ALIGN_RIGHT:
                align = PANGO_ALIGN_RIGHT;
                break;
        }
        pango_layout_set_alignment(layout, align);

}

static void free_colored_layout(void *data)
{
        struct colored_layout *cl = data;
        g_object_unref(cl->l);
        pango_attr_list_unref(cl->attr);
        g_free(cl->text);
        if (cl->icon) cairo_surface_destroy(cl->icon);
        g_free(cl);
}

static bool have_dynamic_width(void)
{
        return (settings.geometry.width_set && settings.geometry.w == 0);
}

static int get_text_icon_padding()
{
        if (settings.text_icon_padding) {
                return settings.text_icon_padding;
        } else {
                return settings.h_padding;
        }
}

static bool have_progress_bar(const struct notification *n)
{
        return (n->progress >= 0 && settings.progress_bar == true);
}

static void get_text_size(PangoLayout *l, int *w, int *h, int scale) {
        pango_layout_get_pixel_size(l, w, h);
        // scale the size down, because it may be rendered at higher DPI

        if (w)
                *w /= scale;
        if (h)
                *h /= scale;
}

static struct dimensions calculate_dimensions(GSList *layouts)
{
        struct dimensions dim = { 0 };
        int scale = output->get_scale();

        const struct screen_info *scr = output->get_active_screen();
        if (have_dynamic_width()) {
                /* dynamic width */
                dim.w = 0;
        } else if (settings.geometry.width_set) {
                /* fixed width */
                if (settings.geometry.negative_width) {
                        dim.w = scr->w - settings.geometry.w;
                } else {
                        dim.w = settings.geometry.w;
                }
        } else {
                /* across the screen */
                dim.w = scr->w;
        }

        dim.h += 2 * settings.frame_width;
        dim.h += (g_slist_length(layouts) - 1) * settings.separator_height;

        dim.corner_radius = settings.corner_radius;

        int text_width = 0, total_width = 0;
        for (GSList *iter = layouts; iter; iter = iter->next) {
                struct colored_layout *cl = iter->data;
                int w=0,h=0;
                get_text_size(cl->l, &w, &h, scale);
                if (cl->icon) {
                        h = MAX(get_icon_height(cl->icon, scale), h);
                        w += get_icon_width(cl->icon, scale) + settings.h_padding;
                }
                h = MAX(settings.notification_height, h + settings.padding * 2);
                dim.h += h;
                text_width = MAX(w, text_width);

                if (have_dynamic_width() || settings.shrink) {
                        /* dynamic width */
                        total_width = MAX(text_width + 2 * settings.h_padding, total_width);

                        /* subtract height from the unwrapped text */
                        dim.h -= h;

                        if (total_width > scr->w) {
                                /* set width to screen width */
                                dim.w = scr->w - settings.geometry.x * 2;
                        } else if (have_dynamic_width() || (total_width < settings.geometry.w && settings.shrink)) {
                                /* set width to text width */
                                dim.w = total_width + 2 * settings.frame_width;
                        }

                        /* re-setup the layout */
                        w = dim.w;
                        w -= 2 * settings.h_padding;
                        w -= 2 * settings.frame_width;
                        if (cl->icon) {
                                w -= get_icon_width(cl->icon, scale) + get_text_icon_padding();
                        }
                        layout_setup_pango(cl->l, w);

                        /* re-read information */
                        get_text_size(cl->l, &w, &h, scale);
                        if (cl->icon) {
                                h = MAX(get_icon_height(cl->icon, scale), h);
                                w += get_icon_width(cl->icon, scale) + settings.h_padding;
                        }
                        h = MAX(settings.notification_height, h + settings.padding * 2);
                        dim.h += h;
                        text_width = MAX(w, text_width);
                }

                if (have_progress_bar(cl->n)){
                        dim.h += settings.progress_bar_height + settings.padding;
                        dim.w = MAX(dim.w, settings.progress_bar_min_width);
                }

                dim.corner_radius = MIN(dim.corner_radius, h/2);
        }

        if (dim.w <= 0) {
                dim.w = text_width + 2 * settings.h_padding;
                dim.w += 2 * settings.frame_width;
        }

        return dim;
}

static PangoLayout *layout_create(cairo_t *c)
{
        const struct screen_info *screen = output->get_active_screen();

        PangoContext *context = pango_cairo_create_context(c);
        pango_cairo_context_set_resolution(context, screen->dpi);

        PangoLayout *layout = pango_layout_new(context);

        g_object_unref(context);

        return layout;
}

static struct colored_layout *layout_init_shared(cairo_t *c, const struct notification *n)
{
        struct colored_layout *cl = g_malloc(sizeof(struct colored_layout));
        cl->l = layout_create(c);
        int scale = output->get_scale();

        if (!settings.word_wrap) {
                PangoEllipsizeMode ellipsize;
                switch (settings.ellipsize) {
                case ELLIPSE_START:
                        ellipsize = PANGO_ELLIPSIZE_START;
                        break;
                case ELLIPSE_MIDDLE:
                        ellipsize = PANGO_ELLIPSIZE_MIDDLE;
                        break;
                case ELLIPSE_END:
                        ellipsize = PANGO_ELLIPSIZE_END;
                        break;
                default:
                        LOG_E("Invalid %s enum value in %s:%d", "ellipsize", __FILE__, __LINE__);
                        break;
                }
                pango_layout_set_ellipsize(cl->l, ellipsize);
        }

        if (settings.icon_position != ICON_OFF && n->icon) {
                cl->icon = gdk_pixbuf_to_cairo_surface(n->icon);
        } else {
                cl->icon = NULL;
        }

        if (cl->icon && cairo_surface_status(cl->icon) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(cl->icon);
                cl->icon = NULL;
        }

        cl->fg = string_to_color(n->colors.fg);
        cl->bg = string_to_color(n->colors.bg);
        cl->highlight = string_to_color(n->colors.highlight);
        cl->frame = string_to_color(n->colors.frame);

        cl->n = n;

        struct dimensions dim = calculate_dimensions(NULL);
        int width = dim.w;

        if (have_dynamic_width()) {
                layout_setup_pango(cl->l, -1);
        } else {
                width -= 2 * settings.h_padding;
                width -= 2 * settings.frame_width;
                if (cl->icon) {
                        width -= get_icon_width(cl->icon, scale) + get_text_icon_padding();
                }
                layout_setup_pango(cl->l, width);
        }

        return cl;
}

static struct colored_layout *layout_derive_xmore(cairo_t *c, const struct notification *n, int qlen)
{
        struct colored_layout *cl = layout_init_shared(c, n);
        cl->text = g_strdup_printf("(%d more)", qlen);
        cl->attr = NULL;
        pango_layout_set_text(cl->l, cl->text, -1);
        return cl;
}

static struct colored_layout *layout_from_notification(cairo_t *c, struct notification *n)
{

        struct colored_layout *cl = layout_init_shared(c, n);
        int scale = output->get_scale();

        /* markup */
        GError *err = NULL;
        pango_parse_markup(n->text_to_render, -1, 0, &(cl->attr), &(cl->text), NULL, &err);

        if (!err) {
                pango_layout_set_text(cl->l, cl->text, -1);
                pango_layout_set_attributes(cl->l, cl->attr);
        } else {
                /* remove markup and display plain message instead */
                n->text_to_render = markup_strip(n->text_to_render);
                cl->text = NULL;
                cl->attr = NULL;
                pango_layout_set_text(cl->l, n->text_to_render, -1);
                if (n->first_render) {
                        LOG_W("Unable to parse markup: %s", err->message);
                }
                g_error_free(err);
        }


        get_text_size(cl->l, NULL, &(n->displayed_height), scale);
        if (cl->icon) n->displayed_height = MAX(get_icon_height(cl->icon, scale), n->displayed_height);

        n->displayed_height = n->displayed_height + settings.padding * 2;

        // progress bar
        if (have_progress_bar(n)) n->displayed_height += settings.progress_bar_height + settings.padding;

        n->displayed_height = MAX(settings.notification_height, n->displayed_height);

        n->first_render = false;
        return cl;
}

static GSList *create_layouts(cairo_t *c)
{
        GSList *layouts = NULL;

        int qlen = queues_length_waiting();
        bool xmore_is_needed = qlen > 0 && settings.indicate_hidden;

        for (const GList *iter = queues_get_displayed();
                        iter; iter = iter->next)
        {
                struct notification *n = iter->data;

                notification_update_text_to_render(n);

                if (!iter->next && xmore_is_needed && settings.notification_limit == 1) {
                        char *new_ttr = g_strdup_printf("%s (%d more)", n->text_to_render, qlen);
                        g_free(n->text_to_render);
                        n->text_to_render = new_ttr;
                }
                layouts = g_slist_append(layouts,
                                layout_from_notification(c, n));
        }

        if (xmore_is_needed && settings.notification_limit != 1) {
                /* append xmore message as new message */
                layouts = g_slist_append(layouts,
                        layout_derive_xmore(c, queues_get_head_waiting(), qlen));
        }

        return layouts;
}


static int layout_get_height(struct colored_layout *cl, int scale)
{
        int h;
        int h_icon = 0;
        int h_progress_bar = 0;
        get_text_size(cl->l, NULL, &h, scale);
        if (cl->icon)
                h_icon = get_icon_height(cl->icon, scale);
        if (have_progress_bar(cl->n)){
                h_progress_bar = settings.progress_bar_height + settings.padding;
        }

        int res = MAX(h, h_icon) + h_progress_bar;
        return res;
}

/* Attempt to make internal radius more organic.
 * Simple r-w is not enough for too small r/w ratio.
 * simplifications: r/2 == r - w + w*w / (r * 2) with (w == r)
 * r, w - corner radius & frame width,
 * h  - box height
 */
static int frame_internal_radius (int r, int w, int h)
{
        if (r == 0 || h + (w - r) * 2 == 0)
                return 0;

        // Integer precision scaler, using 1/4 of int size
        const int s = 2 << (8 * sizeof(int) / 4);

        int r1, r2, ret;
        h *= s;
        r *= s;
        w *= s;
        r1 = r - w + w * w / (r * 2);    // w  <  r
        r2 = r * h / (h + (w - r) * 2);  // w  >= r

        ret = (r > w) ? r1 : (r / 2 < r2) ? r / 2 : r2;

        return ret / s;
}

/**
 * Create a path on the given cairo context to draw the background of a notification.
 * The top corners will get rounded by `corner_radius`, if `first` is set.
 * Respectably the same for `last` with the bottom corners.
 */
void draw_rounded_rect(cairo_t *c, int x, int y, int width, int height, int corner_radius, int scale, bool first, bool last)
{
        width *= scale;
        height *= scale;
        x *= scale;
        y *= scale;
        corner_radius *= scale;

        const float degrees = M_PI / 180.0;

        cairo_new_sub_path(c);

        if (last) {
                // bottom right
                cairo_arc(c,
                          x + width - corner_radius,
                          y + height - corner_radius,
                          corner_radius,
                          degrees * 0,
                          degrees * 90);
                // bottom left
                cairo_arc(c,
                          x + corner_radius,
                          y + height - corner_radius,
                          corner_radius,
                          degrees * 90,
                          degrees * 180);
        } else {
                cairo_line_to(c, x + width, y + height);
                cairo_line_to(c, x,         y + height);
        }

        if (first) {
                // top left
                cairo_arc(c,
                          x + corner_radius,
                          y + corner_radius,
                          corner_radius,
                          degrees * 180,
                          degrees * 270);
                // top right
                cairo_arc(c,
                          x + width - corner_radius,
                          y + corner_radius,
                          corner_radius,
                          degrees * 270,
                          degrees * 360);
        } else {
                cairo_line_to(c, x,         y);
                cairo_line_to(c, x + width, y);
        }

        cairo_close_path(c);
}

/**
 * A small wrapper around cairo_rectange for drawing a scaled rectangle.
 */
void draw_rect(cairo_t *c, int x, int y, int width, int height, int scale) {
        cairo_rectangle(c, x * scale, y * scale, width * scale, height * scale);
}

static cairo_surface_t *render_background(cairo_surface_t *srf,
                                          struct colored_layout *cl,
                                          struct colored_layout *cl_next,
                                          int y,
                                          int width,
                                          int height,
                                          int corner_radius,
                                          bool first,
                                          bool last,
                                          int *ret_width,
                                          int scale)
{
        int x = 0;
        int radius_int = corner_radius;

        cairo_t *c = cairo_create(srf);

        /* stroke area doesn't intersect with main area */
        cairo_set_fill_rule(c, CAIRO_FILL_RULE_EVEN_ODD);

        /* for correct combination of adjacent areas */
        cairo_set_operator(c, CAIRO_OPERATOR_ADD);

        if (first)
                height += settings.frame_width;
        if (last)
                height += settings.frame_width;
        else
                height += settings.separator_height;

        draw_rounded_rect(c, x, y, width, height, corner_radius, scale, first, last);

        /* adding frame */
        x += settings.frame_width;
        if (first) {
                y += settings.frame_width;
                height -= settings.frame_width;
        }

        width -= 2 * settings.frame_width;

        if (last)
                height -= settings.frame_width;
        else
                height -= settings.separator_height;

        radius_int = frame_internal_radius(corner_radius, settings.frame_width, height);

        draw_rounded_rect(c, x, y, width, height, radius_int, scale, first, last);
        cairo_set_source_rgba(c, cl->frame.r, cl->frame.g, cl->frame.b, cl->frame.a);
        cairo_fill(c);

        draw_rounded_rect(c, x, y, width, height, radius_int, scale, first, last);
        cairo_set_source_rgba(c, cl->bg.r, cl->bg.g, cl->bg.b, cl->bg.a);
        cairo_fill(c);

        cairo_set_operator(c, CAIRO_OPERATOR_SOURCE);

        if (   settings.sep_color.type != SEP_FRAME
            && settings.separator_height > 0
            && !last) {
                struct color sep_color = layout_get_sepcolor(cl, cl_next);
                cairo_set_source_rgba(c, sep_color.r, sep_color.g, sep_color.b, sep_color.a);

                draw_rect(c, settings.frame_width, y + height, width, settings.separator_height, scale);

                cairo_fill(c);
        }

        cairo_destroy(c);

        if (ret_width)
                *ret_width = width;

        return cairo_surface_create_for_rectangle(srf, x * scale, y * scale, width * scale, height * scale);
}

static void render_content(cairo_t *c, struct colored_layout *cl, int width, int scale)
{
        const int h = layout_get_height(cl, scale);
        int h_without_progress_bar = h;
        if (have_progress_bar(cl->n)){
                 h_without_progress_bar -= settings.progress_bar_height + settings.padding;
        }
        int h_text;
        get_text_size(cl->l, NULL, &h_text, scale);

        int text_x = settings.h_padding,
            text_y = settings.padding + h_without_progress_bar / 2 - h_text / 2;

        // text positioning
        if (cl->icon) {
                // vertical alignment
                if (settings.vertical_alignment == VERTICAL_TOP) {
                        text_y = settings.padding;
                } else if (settings.vertical_alignment == VERTICAL_BOTTOM) {
                        text_y = h_without_progress_bar + settings.padding - h_text;
                        if (text_y < 0)
                                text_y = settings.padding;
                } // else VERTICAL_CENTER

                // icon position
                if (settings.icon_position == ICON_LEFT) {
                        text_x = get_icon_width(cl->icon, scale) + settings.h_padding + get_text_icon_padding();
                } // else ICON_RIGHT
        }
        cairo_move_to(c, text_x * scale, text_y * scale);

        cairo_set_source_rgba(c, cl->fg.r, cl->fg.g, cl->fg.b, cl->fg.a);
        pango_cairo_update_layout(c, cl->l);
        pango_cairo_show_layout(c, cl->l);


        // icon positioning
        if (cl->icon) {
                unsigned int image_width = get_icon_width(cl->icon, scale),
                             image_height = get_icon_height(cl->icon, scale),
                             image_x = width - settings.h_padding - image_width,
                             image_y = settings.padding + h_without_progress_bar/2 - image_height/2;

                // vertical alignment
                if (settings.vertical_alignment == VERTICAL_TOP) {
                        image_y = settings.padding;
                } else if (settings.vertical_alignment == VERTICAL_BOTTOM) {
                        image_y = h_without_progress_bar + settings.padding - image_height;
                        if (image_y < settings.padding || image_y > h_without_progress_bar)
                                image_y = settings.padding;
                } // else VERTICAL_CENTER

                // icon position
                if (settings.icon_position == ICON_LEFT) {
                        image_x = settings.h_padding;
                } // else ICON_RIGHT

                cairo_set_source_surface(c, cl->icon, image_x * scale, image_y * scale);
                draw_rect(c, image_x, image_y, image_width, image_height, scale);
                cairo_fill(c);
        }

        // progress bar positioning
        if (have_progress_bar(cl->n)){
                int progress = MIN(cl->n->progress, 100);
                unsigned int frame_width = settings.progress_bar_frame_width,
                             progress_width = MIN(width - 2 * settings.h_padding, settings.progress_bar_max_width),
                             progress_height = settings.progress_bar_height - frame_width,
                             frame_x = settings.h_padding,
                             frame_y = settings.padding + h - settings.progress_bar_height,
                             progress_width_without_frame = progress_width - 2 * frame_width,
                             progress_width_1 = progress_width_without_frame * progress / 100,
                             progress_width_2 = progress_width_without_frame - progress_width_1,
                             x_bar_1 = frame_x + frame_width,
                             x_bar_2 = x_bar_1 + progress_width_1;

                double half_frame_width = frame_width / 2.0;

                // draw progress bar
                // Note: the bar could be drawn a bit smaller, because the frame is drawn on top
                // left side
                cairo_set_source_rgba(c, cl->highlight.r, cl->highlight.g, cl->highlight.b, cl->highlight.a);
                draw_rect(c, x_bar_1, frame_y, progress_width_1, progress_height, scale);
                cairo_fill(c);
                // right side
                cairo_set_source_rgba(c, cl->bg.r, cl->bg.g, cl->bg.b, cl->bg.a);
                draw_rect(c, x_bar_2, frame_y, progress_width_2, progress_height, scale);
                cairo_fill(c);
                // border
                cairo_set_source_rgba(c, cl->frame.r, cl->frame.g, cl->frame.b, cl->frame.a);
                // TODO draw_rect instead of cairo_rectangle resulted in blurry lines. Why?
                cairo_rectangle(c, (frame_x + half_frame_width) * scale, (frame_y + half_frame_width) * scale, (progress_width - frame_width) * scale, progress_height * scale);
                cairo_set_line_width(c, frame_width * scale);
                cairo_stroke(c);
        }
}

static struct dimensions layout_render(cairo_surface_t *srf,
                                       struct colored_layout *cl,
                                       struct colored_layout *cl_next,
                                       struct dimensions dim,
                                       bool first,
                                       bool last)
{
        int scale = output->get_scale();
        const int cl_h = layout_get_height(cl, scale);

        int h_text = 0;
        get_text_size(cl->l, NULL, &h_text, scale);

        int bg_width = 0;
        int bg_height = MAX(settings.notification_height, (2 * settings.padding) + cl_h);

        cairo_surface_t *content = render_background(srf, cl, cl_next, dim.y, dim.w, bg_height, dim.corner_radius, first, last, &bg_width, scale);
        cairo_t *c = cairo_create(content);

        render_content(c, cl, bg_width, scale);

        /* adding frame */
        if (first)
                dim.y += settings.frame_width;

        if (!last)
                dim.y += settings.separator_height;


        if (settings.notification_height <= (2 * settings.padding) + cl_h)
                dim.y += cl_h + 2 * settings.padding;
        else
                dim.y += settings.notification_height;

        cairo_destroy(c);
        cairo_surface_destroy(content);
        return dim;
}

/**
 * Calculates the position the window should be placed at given its width and
 * height and stores them in \p ret_x and \p ret_y.
 */
static void calc_window_pos(int width, int height, int *ret_x, int *ret_y)
{
        if(!ret_x || !ret_y)
                return;

        const struct screen_info *scr = output->get_active_screen();

        // horizontal
        switch (settings.origin) {
                case ORIGIN_TOP_LEFT:
                case ORIGIN_LEFT_CENTER:
                case ORIGIN_BOTTOM_LEFT:
                        *ret_x = scr->x + settings.offset.x;
                        break;
                case ORIGIN_TOP_RIGHT:
                case ORIGIN_RIGHT_CENTER:
                case ORIGIN_BOTTOM_RIGHT:
                        *ret_x = scr->x + (scr->w - width) - settings.offset.x;
                        break;
                case ORIGIN_TOP_CENTER:
                case ORIGIN_CENTER:
                case ORIGIN_BOTTOM_CENTER:
                default:
                        *ret_x = scr->x + (scr->w - width) / 2;
                        break;
        }

        // vertical
        switch (settings.origin) {
                case ORIGIN_TOP_LEFT:
                case ORIGIN_TOP_CENTER:
                case ORIGIN_TOP_RIGHT:
                        *ret_y = scr->y + settings.offset.y;
                        break;
                case ORIGIN_BOTTOM_LEFT:
                case ORIGIN_BOTTOM_CENTER:
                case ORIGIN_BOTTOM_RIGHT:
                        *ret_y = scr->y + (scr->h - height) - settings.offset.y;
                        break;
                case ORIGIN_LEFT_CENTER:
                case ORIGIN_CENTER:
                case ORIGIN_RIGHT_CENTER:
                default:
                        *ret_y = scr->y + (scr->h - height) / 2;
                        break;
        }
}

void draw(void)
{
        assert(queues_length_displayed() > 0);

        GSList *layouts = create_layouts(output->win_get_context(win));

        struct dimensions dim = calculate_dimensions(layouts);
        int scale = output->get_scale();

        cairo_surface_t *image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dim.w * scale, dim.h * scale);

        bool first = true;
        for (GSList *iter = layouts; iter; iter = iter->next) {

                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                dim = layout_render(image_surface, cl_this, cl_next, dim, first, !cl_next);

                first = false;
        }

        calc_window_pos(dim.w, dim.h, &dim.x, &dim.y);
        output->display_surface(image_surface, win, &dim);

        cairo_surface_destroy(image_surface);
        g_slist_free_full(layouts, free_colored_layout);
}

void draw_deinit(void)
{
        output->win_destroy(win);
        output->deinit();
}

int draw_get_scale(void)
{
        if (output) {
                return output->get_scale();
        } else {
                LOG_W("Called draw_get_scale before output init");
                return 1;
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
