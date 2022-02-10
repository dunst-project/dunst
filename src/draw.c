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
#include "icon-lookup.h"

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
        struct notification *n;
        bool is_xmore;
};

const struct output *output;
window win;

PangoFontDescription *pango_fdesc;

#define UINT_MAX_N(bits) ((1 << bits) - 1)

void load_icon_themes()
{
        bool loaded_theme = false;

        for (int i = 0; settings.icon_theme[i] != NULL; i++) {
                char *theme = settings.icon_theme[i];
                int theme_index = load_icon_theme(theme);
                if (theme_index >= 0) {
                        LOG_I("Adding theme %s", theme);
                        add_default_theme(theme_index);
                        loaded_theme = true;
                }
        }
        if (!loaded_theme) {
                int theme_index = load_icon_theme("hicolor");
                add_default_theme(theme_index);
        }

}

void draw_setup(void)
{
        const struct output *out = output_create(settings.force_xwayland);
        output = out;

        win = out->win_create();

        pango_fdesc = pango_font_description_from_string(settings.font);

        if (settings.enable_recursive_icon_lookup)
                load_icon_themes();
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

static int get_horizontal_text_icon_padding(struct notification *n)
{
        bool horizontal_icon = (
                n->icon && (n->icon_position == ICON_LEFT || n->icon_position == ICON_RIGHT)
        );
        if (settings.text_icon_padding && horizontal_icon) {
                return settings.text_icon_padding;
        } else {
                return settings.h_padding;
        }
}

static int get_vertical_text_icon_padding(struct notification *n)
{
        bool vertical_icon = n->icon && (n->icon_position == ICON_TOP);
        if (settings.text_icon_padding && vertical_icon) {
                return settings.text_icon_padding;
        } else {
                return settings.padding;
        }
}

static bool have_progress_bar(const struct colored_layout *cl)
{
        return (cl->n->progress >= 0 && settings.progress_bar == true &&
                        !cl->is_xmore);
}

static void get_text_size(PangoLayout *l, int *w, int *h, double scale) {
        pango_layout_get_pixel_size(l, w, h);
        // scale the size down, because it may be rendered at higher DPI

        if (w)
                *w = ceil(*w / scale);
        if (h)
                *h = ceil(*h / scale);
}

// Set up pango for a given layout.
// @param width The avaiable text width in pixels, used for caluclating alignment and wrapping
// @param height The maximum text height in pixels.
static void layout_setup_pango(PangoLayout *layout, int width, int height,
                bool word_wrap, PangoEllipsizeMode ellipsize_mode,
                PangoAlignment alignment)
{
        double scale = output->get_scale();
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_width(layout, round(width * scale * PANGO_SCALE));

        // When no height is set, word wrap is turned off
        if (word_wrap)
                pango_layout_set_height(layout, round(height * scale * PANGO_SCALE));

        pango_layout_set_font_description(layout, pango_fdesc);
        pango_layout_set_spacing(layout, round(settings.line_height * scale * PANGO_SCALE));

        pango_layout_set_ellipsize(layout, ellipsize_mode);

        pango_layout_set_alignment(layout, alignment);
}

// Set up the layout of a single notification
// @param width Width of the layout
// @param height Height of the layout
static void layout_setup(struct colored_layout *cl, int width, int height, double scale)
{
        int horizontal_padding = get_horizontal_text_icon_padding(cl->n);
        int icon_width = cl->icon ? get_icon_width(cl->icon, scale) + horizontal_padding : 0;
        int text_width = width - 2 * settings.h_padding - (cl->n->icon_position == ICON_TOP ? 0 : icon_width);
        int progress_bar_height = have_progress_bar(cl) ? settings.progress_bar_height + settings.padding : 0;
        int max_text_height = MAX(0, settings.height - progress_bar_height - 2 * settings.padding);
        layout_setup_pango(cl->l, text_width, max_text_height, cl->n->word_wrap, cl->n->ellipsize, cl->n->alignment);
}

static void free_colored_layout(void *data)
{
        struct colored_layout *cl = data;
        g_object_unref(cl->l);
        pango_attr_list_unref(cl->attr);
        g_free(cl->text);
        g_free(cl);
}

// calculates the minimum dimensions of the notification excluding the frame
static struct dimensions calculate_notification_dimensions(struct colored_layout *cl, double scale)
{
        struct dimensions dim = { 0 };
        layout_setup(cl, settings.width.max, settings.height, scale);

        int horizontal_padding = get_horizontal_text_icon_padding(cl->n);
        int icon_width = cl->icon? get_icon_width(cl->icon, scale) + horizontal_padding : 0;
        int icon_height = cl->icon? get_icon_height(cl->icon, scale) : 0;
        int progress_bar_height = have_progress_bar(cl) ? settings.progress_bar_height + settings.padding : 0;

        int vertical_padding;
        if (cl->n->hide_text) {
                vertical_padding = 0;
                dim.text_width = 0;
                dim.text_height = 0;
        } else {
                get_text_size(cl->l, &dim.text_width, &dim.text_height, scale);
                vertical_padding = get_vertical_text_icon_padding(cl->n);
        }

        if (cl->n->icon_position == ICON_TOP && cl->n->icon) {
                dim.h = icon_height + dim.text_height + vertical_padding;
        } else {
                dim.h = MAX(icon_height, dim.text_height);
        }

        dim.h += progress_bar_height;
        dim.w = dim.text_width + icon_width + 2 * settings.h_padding;

        dim.h = MIN(settings.height, dim.h + settings.padding * 2);
        dim.w = MAX(settings.width.min, dim.w);
        if (have_progress_bar(cl))
                dim.w = MAX(settings.progress_bar_min_width, dim.w);

        dim.w = MIN(settings.width.max, dim.w);

        cl->n->displayed_height = dim.h;
        return dim;
}

static struct dimensions calculate_dimensions(GSList *layouts)
{
        struct dimensions dim = { 0 };
        double scale = output->get_scale();

        dim.h += 2 * settings.frame_width;
        dim.h += (g_slist_length(layouts) - 1) * settings.separator_height;

        dim.corner_radius = settings.corner_radius;

        for (GSList *iter = layouts; iter; iter = iter->next) {
                struct colored_layout *cl = iter->data;
                struct dimensions n_dim = calculate_notification_dimensions(cl, scale);
                dim.h += n_dim.h;
                LOG_D("Notification dimensions %ix%i", n_dim.w, n_dim.h);
                dim.w = MAX(dim.w, n_dim.w + settings.frame_width);
        }

        dim.w += 2 * settings.frame_width;
        dim.corner_radius = MIN(dim.corner_radius, dim.h/2);

        /* clamp max width to screen width */
        const struct screen_info *scr = output->get_active_screen();
        int max_width = scr->w - settings.offset.x;
        if (dim.w > max_width) {
                dim.w = max_width;
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

static struct colored_layout *layout_init_shared(cairo_t *c, struct notification *n)
{
        struct colored_layout *cl = g_malloc(sizeof(struct colored_layout));
        cl->l = layout_create(c);

        cl->fg = string_to_color(n->colors.fg);
        cl->bg = string_to_color(n->colors.bg);
        cl->highlight = string_to_color(n->colors.highlight);
        cl->frame = string_to_color(n->colors.frame);
        cl->is_xmore = false;

        cl->n = n;
        return cl;
}

static struct colored_layout *layout_derive_xmore(cairo_t *c, struct notification *n, int qlen)
{
        struct colored_layout *cl = layout_init_shared(c, n);
        cl->text = g_strdup_printf("(%d more)", qlen);
        cl->attr = NULL;
        cl->is_xmore = true;
        cl->icon = NULL;
        pango_layout_set_text(cl->l, cl->text, -1);
        return cl;
}

static struct colored_layout *layout_from_notification(cairo_t *c, struct notification *n)
{

        struct colored_layout *cl = layout_init_shared(c, n);

        if (n->icon_position != ICON_OFF && n->icon) {
                cl->icon = n->icon;
        } else {
                cl->icon = NULL;
        }

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


static int layout_get_height(struct colored_layout *cl, double scale)
{
        int h_text = 0;
        int h_icon = 0;
        int h_progress_bar = 0;

        int vertical_padding;
        if (cl->n->hide_text) {
                vertical_padding = 0;
        } else {
                get_text_size(cl->l, NULL, &h_text, scale);
                vertical_padding = get_vertical_text_icon_padding(cl->n);
        }

        if (cl->icon)
                h_icon = get_icon_height(cl->icon, scale);

        if (have_progress_bar(cl)) {
                h_progress_bar = settings.progress_bar_height + settings.padding;
        }


        if (cl->n->icon_position == ICON_TOP && cl->n->icon) {
                return h_icon + h_text + h_progress_bar + vertical_padding;
        } else {
                return MAX(h_text, h_icon) + h_progress_bar;
        }
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
void draw_rounded_rect(cairo_t *c, int x, int y, int width, int height, int corner_radius, double scale, bool first, bool last)
{
        width = round(width * scale);
        height = round(height * scale);
        x = round(x * scale);
        y = round(y * scale);
        corner_radius = round(corner_radius * scale);

        const double degrees = M_PI / 180.0;

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
static void draw_rect(cairo_t *c, double x, double y, double width, double height, double scale) {
        cairo_rectangle(c, round(x * scale), round(y * scale), round(width * scale), round(height * scale));
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
                                          double scale)
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

        return cairo_surface_create_for_rectangle(srf,
                                                  round(x * scale), round(y * scale),
                                                  round(width * scale), round(height * scale));
}

static void render_content(cairo_t *c, struct colored_layout *cl, int width, double scale)
{
        // Redo layout setup, while knowing the width. This is to make
        // alignment work correctly
        layout_setup(cl, width, settings.height, scale);

        const int h = layout_get_height(cl, scale);
        LOG_D("Layout height %i", h);
        int h_without_progress_bar = h;
        if (have_progress_bar(cl)) {
                 h_without_progress_bar -= settings.progress_bar_height + settings.padding;
        }

        if (!cl->n->hide_text) {
                int h_text = 0;
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
                        if (cl->n->icon_position == ICON_LEFT) {
                                text_x = get_icon_width(cl->icon, scale) + settings.h_padding + get_horizontal_text_icon_padding(cl->n);
                        } else if (cl->n->icon_position == ICON_TOP) {
                                text_y = get_icon_height(cl->icon, scale) + settings.padding + get_vertical_text_icon_padding(cl->n);
                        } // else ICON_RIGHT
                }
                cairo_move_to(c, round(text_x * scale), round(text_y * scale));

                cairo_set_source_rgba(c, cl->fg.r, cl->fg.g, cl->fg.b, cl->fg.a);
                pango_cairo_update_layout(c, cl->l);
                pango_cairo_show_layout(c, cl->l);
        }

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
                if (cl->n->icon_position == ICON_LEFT) {
                        image_x = settings.h_padding;
                } else if (cl->n->icon_position == ICON_TOP) {
                        image_y = settings.padding;
                        image_x = width/2 - image_width/2;
                } // else ICON_RIGHT

                cairo_set_source_surface(c, cl->icon, round(image_x * scale), round(image_y * scale));
                draw_rect(c, image_x, image_y, image_width, image_height, scale);
                cairo_fill(c);
        }

        // progress bar positioning
        if (have_progress_bar(cl)){
                int progress = MIN(cl->n->progress, 100);
                unsigned int frame_x = 0;
                unsigned int frame_width = settings.progress_bar_frame_width,
                             progress_width = MIN(width - 2 * settings.h_padding, settings.progress_bar_max_width),
                             progress_height = settings.progress_bar_height - frame_width,
                             frame_y = settings.padding + h - settings.progress_bar_height,
                             progress_width_without_frame = progress_width - 2 * frame_width,
                             progress_width_1 = progress_width_without_frame * progress / 100,
                             progress_width_2 = progress_width_without_frame - progress_width_1;

                switch (cl->n->progress_bar_alignment) {
                        case PANGO_ALIGN_LEFT:
                             frame_x = settings.h_padding;
                             break;
                        case PANGO_ALIGN_CENTER:
                             frame_x = width/2 - progress_width/2;
                             break;
                        case PANGO_ALIGN_RIGHT:
                             frame_x = width - progress_width - settings.h_padding;
                             break;
                }
                unsigned int x_bar_1 = frame_x + frame_width,
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
                // TODO draw_rect instead of cairo_rectangle resulted
                // in blurry lines due to rounding (half_frame_width
                // can be non-integer)
                cairo_rectangle(c,
                                (frame_x + half_frame_width) * scale,
                                (frame_y + half_frame_width) * scale,
                                (progress_width - frame_width) * scale,
                                progress_height * scale);
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
        double scale = output->get_scale();
        const int cl_h = layout_get_height(cl, scale);

        int h_text = 0;
        get_text_size(cl->l, NULL, &h_text, scale);

        int bg_width = 0;
        int bg_height = MIN(settings.height, (2 * settings.padding) + cl_h);

        cairo_surface_t *content = render_background(srf, cl, cl_next, dim.y, dim.w, bg_height, dim.corner_radius, first, last, &bg_width, scale);
        cairo_t *c = cairo_create(content);

        render_content(c, cl, bg_width, scale);

        /* adding frame */
        if (first)
                dim.y += settings.frame_width;

        if (!last)
                dim.y += settings.separator_height;


        if ((2 * settings.padding + cl_h) < settings.height)
                dim.y += cl_h + 2 * settings.padding;
        else
                dim.y += settings.height;

        cairo_destroy(c);
        cairo_surface_destroy(content);
        return dim;
}

/**
 * Calculates the position the window should be placed at given its width and
 * height and stores them in \p ret_x and \p ret_y.
 */
void calc_window_pos(const struct screen_info *scr, int width, int height, int *ret_x, int *ret_y)
{
        if(!ret_x || !ret_y)
                return;

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
        LOG_D("Window dimensions %ix%i", dim.w, dim.h);
        double scale = output->get_scale();

        cairo_surface_t *image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                                    round(dim.w * scale),
                                                                    round(dim.h * scale));

        bool first = true;
        for (GSList *iter = layouts; iter; iter = iter->next) {

                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                dim = layout_render(image_surface, cl_this, cl_next, dim, first, !cl_next);

                first = false;
        }

        output->display_surface(image_surface, win, &dim);

        cairo_surface_destroy(image_surface);
        g_slist_free_full(layouts, free_colored_layout);
}

void draw_deinit(void)
{
        output->win_destroy(win);
        output->deinit();
        if (settings.enable_recursive_icon_lookup)
                free_all_themes();
}

double draw_get_scale(void)
{
        if (output) {
                return output->get_scale();
        } else {
                LOG_W("Called draw_get_scale before output init");
                return 1;
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
