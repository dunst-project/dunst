/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
#include "markup.h"

#include <assert.h>
#include <stdbool.h>

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
        str = string_replace_all("&amp;", "&", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);

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
                break;
        }

        if (settings.ignore_newline) {
                str = string_replace_all("\n", " ", str);
        }

        return str;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
