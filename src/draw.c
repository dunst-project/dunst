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

struct colored_layout {
        PangoLayout *l;
        char *text;
        PangoAttrList *attr;
        cairo_surface_t *icon;
        struct notification *n;
        bool is_xmore;
};

const struct output *output;
window win;

PangoFontDescription *pango_fdesc;
PangoContext *pango_ctx;

// NOTE: Saves some characters
#define COLOR(cl, field) (cl)->n->colors.field

void load_icon_themes(void)
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

char *color_to_string(struct color c, char buf[10])
{
        if (!COLOR_VALID(c)) return NULL;

        g_snprintf(buf, 10, "#%02x%02x%02x%02x",
                        (int)(c.r * 255),
                        (int)(c.g * 255),
                        (int)(c.b * 255),
                        (int)(c.a * 255));
        return buf;
}

struct gradient *gradient_alloc(size_t length)
{
        if (length == 0)
                return NULL;

        struct gradient *grad = g_malloc(sizeof(struct gradient) + length * sizeof(struct color));

        grad->length = length;
        grad->pattern = NULL;

        return grad;
}

void gradient_free(struct gradient *grad)
{
        if (grad == NULL) return;

        if (grad->pattern)
                cairo_pattern_destroy(grad->pattern);

        g_free(grad);
}

void gradient_pattern(struct gradient *grad)
{
        if (grad->length == 1) {
                grad->pattern = cairo_pattern_create_rgba(grad->colors[0].r,
                                                          grad->colors[0].g,
                                                          grad->colors[0].b,
                                                          grad->colors[0].a);
        } else {
                grad->pattern = cairo_pattern_create_linear(0, 0, 1, 0);
                for (int i = 0; i < grad->length; i++) {
                        double offset = i  / (double)(grad->length - 1);
                        cairo_pattern_add_color_stop_rgba(grad->pattern,
                                                          offset,
                                                          grad->colors[i].r,
                                                          grad->colors[i].g,
                                                          grad->colors[i].b,
                                                          grad->colors[i].a);
                }
        }
}

char *gradient_to_string(const struct gradient *grad)
{
        if (!GRADIENT_VALID(grad)) return NULL;

        char *buf = g_malloc(grad->length * 11);
        color_to_string(grad->colors[0], buf);
        char *ptr = buf + 9;

        for (int i = 1; i < grad->length; i++) {
                ptr += g_snprintf(ptr, 11, ", #%02x%02x%02x%02x",
                                  (int)(grad->colors[i].r * 255),
                                  (int)(grad->colors[i].g * 255),
                                  (int)(grad->colors[i].b * 255),
                                  (int)(grad->colors[i].a * 255));
        }

        return buf;
}

void draw_setup(void)
{
        const struct output *out = output_create(settings.force_xwayland);
        output = out;

        win = out->win_create();

        LOG_D("Trying to load font: '%s'", settings.font);
        pango_fdesc = pango_font_description_from_string(settings.font);
        LOG_D("Loaded closest matching font: '%s'", pango_font_description_get_family(pango_fdesc));

        pango_ctx = pango_cairo_create_context(out->win_get_context(win));

        if (settings.enable_recursive_icon_lookup)
                load_icon_themes();
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
                        return COLOR(cl_next->n->urgency > cl->n->urgency ? cl_next : cl, frame);
                case SEP_CUSTOM:
                        return settings.sep_color.color;
                case SEP_FOREGROUND:
                        return COLOR(cl, fg);
                case SEP_AUTO:
                        return calculate_foreground_color(COLOR(cl, bg));
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

static void get_text_size(PangoLayout *l, int *w, int *h) {
        pango_layout_get_pixel_size(l, w, h);
}

// Set up pango for a given layout.
// @param width The avaiable text width in pixels, used for caluclating alignment and wrapping
// @param height The maximum text height in pixels.
static void layout_setup_pango(PangoLayout *layout, int width, int height,
                bool word_wrap, PangoEllipsizeMode ellipsize_mode,
                PangoAlignment alignment)
{
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_width(layout, round(width * PANGO_SCALE));

        // When no height is set, word wrap is turned off
        if (word_wrap)
                pango_layout_set_height(layout, round(height * PANGO_SCALE));

        pango_layout_set_font_description(layout, pango_fdesc);
        pango_layout_set_spacing(layout, round(settings.line_height * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, ellipsize_mode);
        pango_layout_set_alignment(layout, alignment);
}

// Set up the layout of a single notification
// @param width Width of the layout
// @param height Height of the layout
static void layout_setup(struct colored_layout *cl, int width, int height)
{
        int horizontal_padding = get_horizontal_text_icon_padding(cl->n);
        int icon_width = cl->icon ? get_icon_width(cl->icon) + horizontal_padding : 0;
        int text_width = width - 2 * settings.h_padding - (cl->n->icon_position == ICON_TOP ? 0 : icon_width);
        int progress_bar_height = have_progress_bar(cl) ? settings.progress_bar_height + settings.padding : 0;
        int max_text_height = MAX(0, height - progress_bar_height - 2 * settings.padding);
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
static struct dimensions calculate_notification_dimensions(struct colored_layout *cl)
{
        struct dimensions dim = { 0 };
        layout_setup(cl, settings.width.max, settings.height.max);

        int horizontal_padding = get_horizontal_text_icon_padding(cl->n);
        int icon_width = cl->icon? get_icon_width(cl->icon) + horizontal_padding : 0;
        int icon_height = cl->icon? get_icon_height(cl->icon) : 0;
        int progress_bar_height = have_progress_bar(cl) ? settings.progress_bar_height + settings.padding : 0;

        int vertical_padding;
        if (cl->n->hide_text) {
                vertical_padding = 0;
                dim.text_width = 0;
                dim.text_height = 0;
        } else {
                get_text_size(cl->l, &dim.text_width, &dim.text_height);
                vertical_padding = get_vertical_text_icon_padding(cl->n);
        }

        if (cl->n->icon_position == ICON_TOP && cl->n->icon) {
                dim.h = icon_height + dim.text_height + vertical_padding;
        } else {
                dim.h = MAX(icon_height, dim.text_height);
        }

        dim.h += progress_bar_height;
        dim.w = dim.text_width + icon_width + 2 * settings.h_padding;

        if (have_progress_bar(cl))
                dim.w = MAX(settings.progress_bar_min_width, dim.w);

        dim.h = MIN(settings.height.max, dim.h + settings.padding * 2);
        dim.h = MAX(settings.height.min, dim.h);

        dim.w = MAX(settings.width.min, dim.w);
        dim.w = MIN(settings.width.max, dim.w);

        cl->n->displayed_height = dim.h;
        return dim;
}

static struct dimensions calculate_dimensions(GSList *layouts)
{
        int layout_count = g_slist_length(layouts);
        struct dimensions dim = { 0 };

        dim.corner_radius = settings.corner_radius;

        for (GSList *iter = layouts; iter; iter = iter->next) {
                struct colored_layout *cl = iter->data;
                struct dimensions n_dim = calculate_notification_dimensions(cl);
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

        if (settings.gap_size) {
                int extra_frame_height = layout_count * (2 * settings.frame_width);
                int extra_gap_height = (layout_count * settings.gap_size) - settings.gap_size;
                int total_extra_height = extra_frame_height + extra_gap_height;
                dim.h += total_extra_height;
        } else {
                dim.h += 2 * settings.frame_width;
                dim.h += (layout_count - 1) * settings.separator_height;
        }

        return dim;
}

static struct colored_layout *layout_init_shared(cairo_t *c, struct notification *n)
{
        struct colored_layout *cl = g_malloc(sizeof(struct colored_layout));
        cl->l = pango_layout_new(pango_ctx);
        cl->is_xmore = false;
        cl->n = n;

        // Invalid colors should never reach this point!
        assert(settings.frame_width == 0 || COLOR_VALID(COLOR(cl, frame)));
        assert(!have_progress_bar(cl) || COLOR(cl, highlight) != NULL);
        assert(COLOR_VALID(COLOR(cl, fg)));
        assert(COLOR_VALID(COLOR(cl, bg)));
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
                pango_attr_list_insert(cl->attr, pango_attr_fallback_new(true));
                pango_layout_set_attributes(cl->l, cl->attr);
        } else {
                /* remove markup and display plain message instead */
                n->text_to_render = markup_strip(n->text_to_render);
                cl->text = NULL;
                cl->attr = pango_attr_list_new();
                pango_attr_list_insert(cl->attr, pango_attr_fallback_new(true));
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


static int layout_get_height(struct colored_layout *cl)
{
        int h_text = 0;
        int h_icon = 0;
        int h_progress_bar = 0;

        int vertical_padding;
        if (cl->n->hide_text) {
                vertical_padding = 0;
        } else {
                get_text_size(cl->l, NULL, &h_text);
                vertical_padding = get_vertical_text_icon_padding(cl->n);
        }

        if (cl->icon)
                h_icon = get_icon_height(cl->icon);

        if (have_progress_bar(cl)) {
                h_progress_bar = settings.progress_bar_height + settings.padding;
        }


        return (cl->n->icon_position == ICON_TOP && cl->n->icon)
                ? h_icon + h_text + h_progress_bar + vertical_padding
                : MAX(h_text, h_icon) + h_progress_bar;
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
 * A small wrapper around cairo_rectange for drawing a scaled rectangle.
 */
static inline void draw_rect(cairo_t *c, int x, int y, int width, int height)
{
        cairo_rectangle(c, x, y, width, height);
}

/**
 * Create a path on the given cairo pango_ctx to draw the background of a notification.
 * The top corners will get rounded by `corner_radius`, if `first` is set.
 * Respectably the same for `last` with the bottom corners.
 *
 * TODO: Pass additional frame width information to fix blurry lines due to fractional scaling
 *       X and Y can then be calculated as:  x = round(x*scale) + half_frame_width
 */
void draw_rounded_rect(cairo_t *c, int x, int y, int width, int height, int corner_radius, enum corner_pos corners)
{
        // Fast path for simple rects
        if (corners == C_NONE || corner_radius <= 0) {
                draw_rect(c, x, y, width, height);
                return;
        }

        // Nothing valid to draw
        if (width <= 0 || height <= 0)
                return;

        const double degrees = M_PI / 180.0;

        // This seems to be needed for a problem that occurs mostly when using cairo_stroke
        // and not cairo_fill. Since the only case where we have partial angles is the progress
        // bar and there we use fill, maybe this can be removed completely?
        enum corner_pos skip = C_NONE;

        float top_y_off = 0, bot_y_off = 0, top_x_off, bot_x_off;
        top_x_off = bot_x_off = MAX(width - corner_radius, corner_radius);

        double bot_left_angle1 = degrees * 90;
        double bot_left_angle2 = degrees * 180;

        double top_left_angle1 = degrees * 180;
        double top_left_angle2 = degrees * 270;

        double top_right_angle1 = degrees * 270;
        double top_right_angle2 = degrees * 360;

        double bot_right_angle1 = degrees * 0;
        double bot_right_angle2 = degrees * 90;

        // The trickiest cases to handle are when the width is less than corner_radius and corner_radius * 2,
        // because we have to split the angle for the arc in the rounded corner
        if (width <= corner_radius) {
                double angle1 = 0, angle2 = 0;

                // If there are two corners on the top/bottom they occupy half of the width
                if ((corners & C_TOP) == C_TOP)
                        angle1 = acos(1.0 - ((double)width / 2.0) / (double)corner_radius);
                else
                        angle1 = acos(1.0 - (double)width / (double)corner_radius);

                if ((corners & C_BOT) == C_BOT)
                        angle2 = acos(1.0 - ((double)width / 2.0) / (double)corner_radius);
                else
                        angle2 = acos(1.0 - (double)width / (double)corner_radius);

                if ((corners & (C_TOP_RIGHT | C_BOT_LEFT)) == (C_TOP_RIGHT | C_BOT_LEFT) && !(corners & C_TOP_LEFT)) {
                        top_y_off -= corner_radius * (1.0 - sin(angle1));
                }

                if ((corners & (C_TOP_LEFT | C_BOT_RIGHT)) == (C_TOP_LEFT | C_BOT_RIGHT) && !(corners & C_BOT_LEFT)) {
                        bot_y_off = corner_radius * (1.0 - sin(angle2));
                }

                top_left_angle2 = degrees * 180 + angle1;
                top_right_angle1 = degrees * 360 - angle1;
                bot_left_angle1 = degrees * 180 - angle2;
                bot_right_angle2 = angle2;

                top_x_off = -(corner_radius - width);
                bot_x_off = -(corner_radius - width);

                if (corners != C_TOP && corners != C_BOT)
                        skip = ~corners;

        } else if (width <= corner_radius * 2 && (corners & C_LEFT && corners & C_RIGHT)) {
                double angle1 = 0, angle2 = 0;
                if (!(corners & C_TOP_LEFT) && corners & C_TOP_RIGHT)
                        top_x_off = width - corner_radius;
                else
                        angle1 = acos((double)width / (double)corner_radius - 1.0);

                if (!(corners & C_BOT_LEFT) && corners & C_BOT_RIGHT)
                        bot_x_off = width - corner_radius;
                else
                        angle2 = acos((double)width / (double)corner_radius - 1.0);

                top_right_angle2 = degrees * 360 - angle1;
                bot_right_angle1 = angle2;
        }

        cairo_new_sub_path(c);

        // bottom left
        if (!(skip & C_BOT_LEFT)) {
                if (corners & C_BOT_LEFT) {
                        cairo_arc(c,
                                  x + corner_radius,
                                  y + height - corner_radius,
                                  corner_radius,
                                  bot_left_angle1,
                                  bot_left_angle2);
                } else {
                        cairo_line_to(c, x, y + height);
                }
        }

        // top left
        if (!(skip & C_TOP_LEFT)) {
                if (corners & C_TOP_LEFT) {
                        cairo_arc(c,
                                  x + corner_radius,
                                  y + corner_radius,
                                  corner_radius,
                                  top_left_angle1,
                                  top_left_angle2);
                } else {
                        cairo_line_to(c, x, y);
                }
        }

        // top right
        if (!(skip & C_TOP_RIGHT)) {
                if (corners & C_TOP_RIGHT) {
                        cairo_arc(c,
                                  x + top_x_off,
                                  y + corner_radius + top_y_off,
                                  corner_radius,
                                  top_right_angle1,
                                  top_right_angle2);
                } else {
                        cairo_line_to(c, x + width, y);
                }
        }

        // bottom right
        if (!(skip & C_BOT_RIGHT)) {
                if (corners & C_BOT_RIGHT) {
                        cairo_arc(c,
                                  x + bot_x_off,
                                  y + height - corner_radius + bot_y_off,
                                  corner_radius,
                                  bot_right_angle1,
                                  bot_right_angle2);
                } else {
                        cairo_line_to(c, x + width, y + height);
                }
        }

        cairo_close_path(c);
}

static cairo_surface_t *render_background(cairo_surface_t *srf,
                                          struct colored_layout *cl,
                                          struct colored_layout *cl_next,
                                          int y,
                                          int width,
                                          int height,
                                          int corner_radius,
                                          enum corner_pos corners,
                                          int *ret_width)
{
        int x = 0;
        int radius_int = corner_radius;

        cairo_t *c = cairo_create(srf);

        /* stroke area doesn't intersect with main area */
        cairo_set_fill_rule(c, CAIRO_FILL_RULE_EVEN_ODD);

        /* for correct combination of adjacent areas */
        cairo_set_operator(c, CAIRO_OPERATOR_ADD);

        if (corners & (C_TOP | _C_FIRST))
                height += settings.frame_width;
        if (corners & (C_BOT | _C_LAST))
                height += settings.frame_width;
        else
                height += settings.separator_height;

        draw_rounded_rect(c, x, y, width, height, corner_radius, corners);

        /* adding frame */
        x += settings.frame_width;
        if (corners & (C_TOP | _C_FIRST)) {
                y += settings.frame_width;
                height -= settings.frame_width;
        }

        width -= 2 * settings.frame_width;

        if (corners & (C_BOT | _C_LAST))
                height -= settings.frame_width;
        else
                height -= settings.separator_height;

        radius_int = frame_internal_radius(corner_radius, settings.frame_width, height);

        draw_rounded_rect(c, x, y, width, height, radius_int, corners);
        cairo_set_source_rgba(c, COLOR(cl, frame.r), COLOR(cl, frame.g), COLOR(cl, frame.b), COLOR(cl, frame.a));
        cairo_fill(c);

        draw_rounded_rect(c, x, y, width, height, radius_int, corners);
        cairo_set_source_rgba(c, COLOR(cl, bg.r), COLOR(cl, bg.g), COLOR(cl, bg.b), COLOR(cl, bg.a));
        cairo_fill(c);

        cairo_set_operator(c, CAIRO_OPERATOR_SOURCE);

        if (   settings.sep_color.type != SEP_FRAME
            && settings.separator_height > 0
            && (corners & (C_BOT | _C_LAST)) == 0) {
                struct color sep_color = layout_get_sepcolor(cl, cl_next);
                cairo_set_source_rgba(c, sep_color.r, sep_color.g, sep_color.b, sep_color.a);

                draw_rect(c, settings.frame_width, y + height, width, settings.separator_height);

                cairo_fill(c);
        }

        cairo_destroy(c);

        if (ret_width)
                *ret_width = width;

        return cairo_surface_create_for_rectangle(srf, x, y, width, height);
}

static void render_content(cairo_t *c, struct colored_layout *cl, int width, int height)
{
        // Redo layout setup, while knowing the width. This is to make
        // alignment work correctly
        layout_setup(cl, width, height);

        // NOTE: Includes paddings!
        int h_without_progress_bar = height;

        if (have_progress_bar(cl)) {
                h_without_progress_bar -= settings.progress_bar_height + settings.padding;
        }

        int text_h = 0;
        if (!cl->n->hide_text) {
                get_text_size(cl->l, NULL, &text_h);
        }

        // text vertical alignment
        int text_x = settings.h_padding,
            text_y = settings.padding;

        if (settings.vertical_alignment == VERTICAL_CENTER) {
                text_y = h_without_progress_bar / 2 - text_h / 2;
        } else if (settings.vertical_alignment == VERTICAL_BOTTOM) {
                text_y = h_without_progress_bar - settings.padding - text_h;
        } // else VERTICAL_TOP

        // icon positioning
        if (cl->icon && cl->n->icon_position != ICON_OFF) {
                int image_width = get_icon_width(cl->icon),
                    image_height = get_icon_height(cl->icon),
                    image_x = width - settings.h_padding - image_width,
                    image_y = text_y,
                    v_padding = get_vertical_text_icon_padding(cl->n);

                // vertical alignment
                switch (settings.vertical_alignment) {
                        case VERTICAL_TOP:
                                if (cl->n->icon_position == ICON_TOP) {
                                        // Shift text downward
                                        text_y += image_height + v_padding;
                                }
                                break;
                        case VERTICAL_CENTER:
                                if (cl->n->icon_position == ICON_TOP) {
                                        // Adjust text and image by half
                                        image_y -= (image_height + v_padding) / 2;
                                        text_y += (image_height + v_padding) / 2;
                                } else {
                                        image_y += text_h / 2 - image_height / 2;
                                }
                                break;
                        case VERTICAL_BOTTOM:
                                if (cl->n->icon_position == ICON_TOP) {
                                        image_y -= image_height + v_padding;
                                } else {
                                        image_y -= image_height - text_h;
                                }
                                break;
                }

                // icon position
                if (cl->n->icon_position == ICON_TOP) {
                        image_x = (width - image_width) / 2;
                } else if (cl->n->icon_position == ICON_LEFT) {
                        image_x = settings.h_padding;
                        text_x += image_width + get_horizontal_text_icon_padding(cl->n);
                } // else ICON_RIGHT

                cairo_set_source_surface(c, cl->icon, image_x, image_y);
                draw_rounded_rect(c, image_x, image_y, image_width, image_height, settings.icon_corner_radius, settings.icon_corners);
                cairo_fill(c);
        }

        // text positioning
        if (!cl->n->hide_text) {
                cairo_move_to(c, text_x, text_y);
                cairo_set_source_rgba(c, COLOR(cl, fg.r), COLOR(cl, fg.g), COLOR(cl, fg.b), COLOR(cl, fg.a));
                pango_cairo_update_layout(c, cl->l);
                pango_cairo_show_layout(c, cl->l);
        }

        // progress bar positioning
        if (have_progress_bar(cl)) {
                int progress = MIN(cl->n->progress, 100);
                unsigned int frame_x = 0;
                unsigned int frame_width = settings.progress_bar_frame_width,
                             progress_width = MIN(width - 2 * settings.h_padding, settings.progress_bar_max_width),
                             progress_height = settings.progress_bar_height - frame_width,
                             frame_y = h_without_progress_bar,
                             progress_width_without_frame = progress_width - 2 * frame_width,
                             progress_width_1 = progress_width_without_frame * progress / 100,
                             progress_width_2 = progress_width_without_frame - 1;

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
                             x_bar_2 = x_bar_1 + 0.5;

                double half_frame_width = (double)frame_width / 2.0;

                /* Draw progress bar
                * TODO: Modify draw_rounded_rect to fix blurry lines due to fractional scaling
                * Note: for now the bar background is drawn a little bit smaller than the fill, however
                *       this solution is not particularly solid (basically subracting a pixel or two)
                */

                // back layer (background)
                cairo_set_source_rgba(c, COLOR(cl, bg.r), COLOR(cl, bg.g), COLOR(cl, bg.b), COLOR(cl, bg.a));
                draw_rounded_rect(c, x_bar_2, frame_y, progress_width_2, progress_height,
                        settings.progress_bar_corner_radius, settings.progress_bar_corners);
                cairo_fill(c);

                // top layer (fill)
                cairo_matrix_t matrix;
                cairo_matrix_init_scale(&matrix, 1.0 / width, 1.0);
                cairo_pattern_set_matrix(COLOR(cl, highlight->pattern), &matrix);
                cairo_set_source(c, COLOR(cl, highlight->pattern));

                draw_rounded_rect(c, x_bar_1, frame_y, progress_width_1, progress_height,
                        settings.progress_bar_corner_radius, settings.progress_bar_corners);
                cairo_fill(c);

                // border
                cairo_set_source_rgba(c, COLOR(cl, frame.r), COLOR(cl, frame.g), COLOR(cl, frame.b), COLOR(cl, frame.a));
                cairo_set_line_width(c, frame_width);
                draw_rounded_rect(c,
                                frame_x + half_frame_width + 1,
                                frame_y,
                                progress_width - frame_width - 2,
                                progress_height,
                                settings.progress_bar_corner_radius,
                                settings.progress_bar_corners);
                cairo_stroke(c);
        }
}

static struct dimensions layout_render(cairo_surface_t *srf,
                                       struct colored_layout *cl,
                                       struct colored_layout *cl_next,
                                       struct dimensions dim,
                                       enum corner_pos corners)
{
        const int cl_h = layout_get_height(cl);

        int h_text = 0;
        get_text_size(cl->l, NULL, &h_text);

        int bg_width = 0;
        int bg_height = MIN(settings.height.max, (2 * settings.padding) + cl_h);
        bg_height = MAX(settings.height.min, bg_height);

        cairo_surface_t *content = render_background(srf, cl, cl_next, dim.y, dim.w, bg_height, dim.corner_radius, corners, &bg_width);
        cairo_t *c = cairo_create(content);

        render_content(c, cl, bg_width, bg_height);

        /* adding frame */
        if (corners & (C_TOP | _C_FIRST))
                dim.y += settings.frame_width;

        if (corners & (C_BOT | _C_LAST))
                dim.y += settings.frame_width;

        dim.y += bg_height;

        if (settings.gap_size)
                dim.y += settings.gap_size;
        else
                dim.y += settings.separator_height;

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

        cairo_t *c = output->win_get_context(win);
        cairo_surface_t *srf = output->win_get_surface(win);

        if (c == NULL || srf == NULL) {
                return;
        }

        GSList *layouts = create_layouts(c);

        struct dimensions dim = calculate_dimensions(layouts);
        LOG_D("Window dimensions %ix%i", dim.w, dim.h);

        cairo_surface_t *image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dim.w, dim.h);

        // Scaling
        double scale = output->get_scale();
        cairo_surface_set_device_scale(srf, scale, scale);
        // NOTE: is this better or worse than screen->dpi?
        pango_cairo_context_set_resolution(pango_ctx, scale * 96);
        pango_cairo_update_context(c, pango_ctx);

        enum corner_pos corners = (settings.corners & C_TOP) | _C_FIRST;
        for (GSList *iter = layouts; iter; iter = iter->next) {

                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                if (settings.gap_size)
                        corners = settings.corners;
                else if (!cl_next)
                        corners |= (settings.corners & C_BOT) | _C_LAST;

                dim = layout_render(image_surface, cl_this, cl_next, dim, corners);
                corners &= ~(C_TOP | _C_FIRST);
        }

        // NOTE: Currently display_surface scales the dimension on its own. Otherwise these
        //       logical dimensions (of the backbuffer surface) should have been scaled to
        //       the real pixel size
        output->display_surface(image_surface, win, &dim);

        cairo_surface_destroy(image_surface);
        g_slist_free_full(layouts, free_colored_layout);
}

void draw_deinit(void)
{
        g_object_unref(pango_ctx);
        pango_font_description_free(pango_fdesc);

        output->win_destroy(win);
        output->deinit();
        if (settings.enable_recursive_icon_lookup)
                free_all_themes();
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
