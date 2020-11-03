/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MARKUP_H
#define DUNST_MARKUP_H

enum markup_mode {
        MARKUP_NULL,
        MARKUP_NO,
        MARKUP_STRIP,
        MARKUP_FULL
};

/**
 * Strip any markup from text; turn it in to plain text.
 *
 * For well-formed markup, the following two commands should be
 * roughly equivalent:
 *
 *     out = markup_strip(in);
 *     pango_parse_markup(in, -1, 0, NULL, &out, NULL, NULL);
 *
 * However, `pango_parse_markup()` balks at invalid markup;
 * `markup_strip()` shouldn't care if there is invalid markup.
 */
char *markup_strip(char *str);

/**
 * Remove HTML hyperlinks of a string.
 *
 * @param str The string to replace a tags
 * @param urls (nullable) If any href-attributes found, an `\n` concatenated
 *        string of the URLs in format `[<text between tags>] <href>`
 */
void markup_strip_a(char **str, char **urls);

/**
 * Remove img-tags of a string. If alt attribute given, use this as replacement.
 *
 * @param str The string to replace img tags
 * @param urls (nullable) If any src-attributes found, an `\n` concatenated string of
 *        the URLs in format `[<alt>] <src>`
 */
void markup_strip_img(char **str, char **urls);

/**
 * Transform the string in accordance with `markup_mode` and
 * `settings.ignore_newline`
 */
char *markup_transform(char *str, enum markup_mode markup_mode);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
