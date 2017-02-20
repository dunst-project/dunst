/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
#include "markup.h"

#include <assert.h>
#include <stdbool.h>

#include "settings.h"
#include "utils.h"

/*
 * Quote a text string for rendering with pango
 */
static char *markup_quote(char *str)
{
        if (str == NULL) {
                return NULL;
        }

        str = string_replace_all("&", "&amp;", str);
        str = string_replace_all("\"", "&quot;", str);
        str = string_replace_all("'", "&apos;", str);
        str = string_replace_all("<", "&lt;", str);
        str = string_replace_all(">", "&gt;", str);

        return str;
}

/*
 * Strip any markup from text
 */
char *markup_strip(char *str)
{
        if (str == NULL) {
                return NULL;
        }

        /* strip all tags */
        string_strip_delimited(str, '<', '>');

        /* unquote the remainder */
        str = string_replace_all("&quot;", "\"", str);
        str = string_replace_all("&apos;", "'", str);
        str = string_replace_all("&amp;", "&", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);

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

        if (markup_mode == MARKUP_NO) {
                str = markup_quote(str);
        } else {
                if (settings.ignore_newline) {
                        str = string_replace_all("<br>", " ", str);
                        str = string_replace_all("<br/>", " ", str);
                        str = string_replace_all("<br />", " ", str);
                } else {
                        str = string_replace_all("<br>", "\n", str);
                        str = string_replace_all("<br/>", "\n", str);
                        str = string_replace_all("<br />", "\n", str);
                }

                if (markup_mode != MARKUP_FULL ) {
                        str = markup_strip(str);
                        str = markup_quote(str);
                }

        }

        if (settings.ignore_newline) {
                str = string_replace_all("\n", " ", str);
        }

        return str;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
