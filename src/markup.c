/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "markup.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "settings.h"
#include "utils.h"

static char *markup_quote(char *str)
{
        assert(str != NULL);

        str = string_replace_all("&", "&amp;", str);
        str = string_replace_all("\"", "&quot;", str);
        str = string_replace_all("'", "&apos;", str);
        str = string_replace_all("<", "&lt;", str);
        str = string_replace_all(">", "&gt;", str);

        return str;
}

static char *markup_unquote(char *str)
{
        assert(str != NULL);

        str = string_replace_all("&quot;", "\"", str);
        str = string_replace_all("&apos;", "'", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);
        str = string_replace_all("&amp;", "&", str);

        return str;
}

static char *markup_br2nl(char *str)
{
        assert(str != NULL);

        str = string_replace_all("<br>", "\n", str);
        str = string_replace_all("<br/>", "\n", str);
        str = string_replace_all("<br />", "\n", str);
        return str;
}

/*
 * Remove HTML hyperlinks of a string.
 *
 * @str: The string to replace a tags
 * @urls: (nullable): If any href-attributes found, an '\n' concatenated
 *        string of the URLs in format '[<text between tags>] <href>'
 */
void markup_strip_a(char **str, char **urls)
{
        char *tag1 = NULL;

        if (urls)
                *urls = NULL;

        while ((tag1 = strstr(*str, "<a"))) {
                // use href=" as stated in the notification spec
                char *href = strstr(tag1, "href=\"");
                char *tag1_end = strstr(tag1, ">");
                char *tag2 = strstr(tag1, "</a>");

                // the tag is broken, ignore it
                if (!tag1_end) {
                        LOG_W("Given link is broken: '%s'",
                              tag1);
                        string_replace_at(*str, tag1-*str, strlen(tag1), "");
                        break;
                }
                if (tag2 && tag2 < tag1_end) {
                        int repl_len =  (tag2 - tag1) + strlen("</a>");
                        LOG_W("Given link is broken: '%.*s.'",
                              repl_len, tag1);
                        string_replace_at(*str, tag1-*str, repl_len, "");
                        break;
                }

                // search contents of href attribute
                char *plain_url = NULL;
                if (href && href < tag1_end) {

                        // shift href to the actual begin of the value
                        href = href+6;

                        const char *quote = strstr(href, "\"");

                        if (quote && quote < tag1_end) {
                                plain_url = g_strndup(href, quote-href);
                        }
                }

                // text between a tags
                int text_len;
                if (tag2)
                        text_len = tag2 - (tag1_end+1);
                else
                        text_len = strlen(tag1_end+1);

                char *text = g_strndup(tag1_end+1, text_len);

                int repl_len = text_len + (tag1_end-tag1) + 1;
                repl_len += tag2 ? strlen("</a>") : 0;

                *str = string_replace_at(*str, tag1-*str, repl_len, text);

                // if there had been a href attribute,
                // add it to the URLs
                if (plain_url && urls) {
                        text = string_replace_all("]", "", text);
                        text = string_replace_all("[", "", text);

                        char *url = g_strdup_printf("[%s] %s", text, plain_url);

                        *urls = string_append(*urls, url, "\n");
                        g_free(url);
                }

                g_free(plain_url);
                g_free(text);
        }
}

/*
 * Remove img-tags of a string. If alt attribute given, use this as replacement.
 *
 * @str: The string to replace img tags
 * @urls: (nullable): If any src-attributes found, an '\n' concatenated string of
 *        the URLs in format '[<alt>] <src>'
 */
void markup_strip_img(char **str, char **urls)
{
        const char *start = *str;

        if (urls)
                *urls = NULL;

        while ((start = strstr(*str, "<img"))) {
                const char *end = strstr(start, ">");

                // the tag is broken, ignore it
                if (!end) {
                        LOG_W("Given image is broken: '%s'", start);
                        string_replace_at(*str, start-*str, strlen(start), "");
                        break;
                }

                // use attribute=" as stated in the notification spec
                const char *alt_s = strstr(start, "alt=\"");
                const char *src_s = strstr(start, "src=\"");

                char *text_alt = NULL;
                char *text_src = NULL;

                const char *src_e = NULL, *alt_e = NULL;
                if (alt_s)
                        alt_e = strstr(alt_s + strlen("alt=\""), "\"");
                if (src_s)
                        src_e = strstr(src_s + strlen("src=\""), "\"");

                // Move pointer to the actual start
                alt_s = alt_s ? alt_s + strlen("alt=\"") : NULL;
                src_s = src_s ? src_s + strlen("src=\"") : NULL;

                /* check if alt and src attribute are given
                 * If both given, check the alignment of all pointers */
                if (   alt_s && alt_e
                    && src_s && src_e
                    && (  (alt_s < src_s && alt_e < src_s-strlen("src=\"") && src_e < end)
                        ||(src_s < alt_s && src_e < alt_s-strlen("alt=\"") && alt_e < end)) ) {

                        text_alt = g_strndup(alt_s, alt_e-alt_s);
                        text_src = g_strndup(src_s, src_e-src_s);

                /* check if single valid alt attribute is available */
                } else if (alt_s && alt_e && alt_e < end && (!src_s || src_s < alt_s || alt_e < src_s - strlen("src=\""))) {
                        text_alt = g_strndup(alt_s, alt_e-alt_s);

                /* check if single valid src attribute is available */
                } else if (src_s && src_e && src_e < end && (!alt_s || alt_s < src_s || src_e < alt_s - strlen("alt=\""))) {
                        text_src = g_strndup(src_s, src_e-src_s);

                } else {
                         LOG_W("Given image argument is broken: '%.*s'",
                               (int)(end-start), start);
                }

                // replacement text for alt
                int repl_len = end - start + 1;

                if (!text_alt)
                        text_alt = g_strdup("[image]");

                *str = string_replace_at(*str, start-*str, repl_len, text_alt);

                // if there had been a href attribute,
                // add it to the URLs
                if (text_src && urls) {
                        text_alt = string_replace_all("]", "", text_alt);
                        text_alt = string_replace_all("[", "", text_alt);

                        char *url = g_strdup_printf("[%s] %s", text_alt, text_src);

                        *urls = string_append(*urls, url, "\n");
                        g_free(url);
                }

                g_free(text_src);
                g_free(text_alt);
        }
}

/*
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
char *markup_strip(char *str)
{
        if (str == NULL) {
                return NULL;
        }

        /* strip all tags */
        string_strip_delimited(str, '<', '>');

        /* unquote the remainder */
        str = markup_unquote(str);

        return str;
}

/*
 * Transform the string in accordance with `markup_mode` and
 * `settings.ignore_newline`
 */
char *markup_transform(char *str, enum markup_mode markup_mode)
{
        if (str == NULL) {
                return NULL;
        }

        switch (markup_mode) {
        case MARKUP_NULL:
                /* `assert(false)`, but with a meaningful error message */
                assert(markup_mode != MARKUP_NULL);
                break;
        case MARKUP_NO:
                str = markup_quote(str);
                break;
        case MARKUP_STRIP:
                str = markup_br2nl(str);
                str = markup_strip(str);
                str = markup_quote(str);
                break;
        case MARKUP_FULL:
                str = markup_br2nl(str);
                markup_strip_a(&str, NULL);
                markup_strip_img(&str, NULL);
                break;
        }

        if (settings.ignore_newline) {
                str = string_replace_all("\n", " ", str);
        }

        return str;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
