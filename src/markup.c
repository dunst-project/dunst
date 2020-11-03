/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "markup.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "settings.h"
#include "utils.h"

/**
 * Convert all HTML special symbols to HTML entities.
 * @param str (nullable)
 */
static char *markup_quote(char *str)
{
        ASSERT_OR_RET(str, NULL);

        str = string_replace_all("&", "&amp;", str);
        str = string_replace_all("\"", "&quot;", str);
        str = string_replace_all("'", "&apos;", str);
        str = string_replace_all("<", "&lt;", str);
        str = string_replace_all(">", "&gt;", str);

        return str;
}

/**
 * Convert all HTML special entities to their actual char.
 * @param str (nullable)
 */
static char *markup_unquote(char *str)
{
        ASSERT_OR_RET(str, NULL);

        str = string_replace_all("&quot;", "\"", str);
        str = string_replace_all("&apos;", "'", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);
        str = string_replace_all("&amp;", "&", str);

        return str;
}

/**
 * Convert all HTML linebreak tags to a newline character
 * @param str (nullable)
 */
static char *markup_br2nl(char *str)
{
        ASSERT_OR_RET(str, NULL);

        str = string_replace_all("<br>", "\n", str);
        str = string_replace_all("<br/>", "\n", str);
        str = string_replace_all("<br />", "\n", str);
        return str;
}

/* see markup.h */
void markup_strip_a(char **str, char **urls)
{
        assert(*str);
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

/* see markup.h */
void markup_strip_img(char **str, char **urls)
{
        const char *start;

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

/* see markup.h */
char *markup_strip(char *str)
{
        ASSERT_OR_RET(str, NULL);

        /* strip all tags */
        string_strip_delimited(str, '<', '>');

        /* unquote the remainder */
        str = markup_unquote(str);

        return str;
}

/**
 * Determine if an & character pointed to by \p str is a markup & entity or
 * part of the text
 *
 * @retval true: \p str is an entity
 * @retval false: It's no valid entity
 */
static bool markup_is_entity(const char *str)
{
        assert(str);
        assert(*str == '&');

        char *end = strchr(str, ';');
        ASSERT_OR_RET(end, false);

        // Parse (hexa)decimal entities with the format &#1234; or &#xABC;
        if (str[1] == '#') {
                const char *cur = str + 2;

                if (*cur == 'x') {
                        cur++;

                        // Reject &#x;
                        if (*cur == ';')
                                return false;

                        while (isxdigit(*cur) && cur < end)
                                cur++;
                } else {

                        // Reject &#;
                        if (*cur == ';')
                                return false;

                        while (isdigit(*cur) && cur < end)
                                cur++;
                }

                return (cur == end);
        } else {
                const char *supported_tags[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"};
                for (int i = 0; i < sizeof(supported_tags)/sizeof(*supported_tags); i++) {
                        if (g_str_has_prefix(str, supported_tags[i]))
                                return true;
                }
                return false;
        }
}

/**
 * Escape all unsupported and invalid &-entities in a string. If the resulting
 * string does not fit it will be reallocated.
 *
 * @param str The string to be transformed
 */
static char *markup_escape_unsupported(char *str)
{
        ASSERT_OR_RET(str, NULL);

        char *match = str;
        while ((match = strchr(match, '&'))) {
                if (!markup_is_entity(match)) {
                        int pos = match - str;
                        str = string_replace_at(str, pos, 1, "&amp;");
                        match = str + pos + strlen("&amp;");
                } else {
                        match++;
                }
        }

        return str;
}

/* see markup.h */
char *markup_transform(char *str, enum markup_mode markup_mode)
{
        ASSERT_OR_RET(str, NULL);

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
                str = markup_escape_unsupported(str);
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

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
