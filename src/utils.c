/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

/* see utils.h */
char *string_replace_char(char needle, char replacement, char *haystack)
{
        ASSERT_OR_RET(haystack, NULL);

        char *current = haystack;
        while ((current = strchr(current, needle)))
                *current++ = replacement;
        return haystack;
}

/* see utils.h */
char *string_replace_at(char *buf, int pos, int len, const char *repl)
{
        assert(buf);
        assert(repl);

        char *tmp;
        int size, buf_len, repl_len;

        buf_len = strlen(buf);
        repl_len = strlen(repl);
        size = (buf_len - len) + repl_len + 1;

        if (repl_len <= len) {
                tmp = buf;
        } else {
                tmp = g_malloc(size);
                memcpy(tmp, buf, pos);
        }

        memcpy(tmp + pos, repl, repl_len);
        memmove(tmp + pos + repl_len, buf + pos + len, buf_len - (pos + len) + 1);

        if (tmp != buf) {
                g_free(buf);
        }

        return tmp;
}

/* see utils.h */
char *string_replace_all(const char *needle, const char *replacement, char *haystack)
{
        ASSERT_OR_RET(haystack, NULL);
        assert(needle);
        assert(replacement);

        char *start;
        int needle_pos;
        int needle_len, repl_len;

        needle_len = strlen(needle);
        if (needle_len == 0) {
                return haystack;
        }

        start = strstr(haystack, needle);
        repl_len = strlen(replacement);

        while (start) {
                needle_pos = start - haystack;
                haystack = string_replace_at(haystack, needle_pos, needle_len, replacement);
                start = strstr(haystack + needle_pos + repl_len, needle);
        }
        return haystack;
}

/* see utils.h */
char *string_append(char *a, const char *b, const char *sep)
{
        if (STR_EMPTY(a)) {
                g_free(a);
                return g_strdup(b);
        }
        if (STR_EMPTY(b))
                return a;

        char *new;
        if (!sep)
                new = g_strconcat(a, b, NULL);
        else
                new = g_strconcat(a, sep, b, NULL);
        g_free(a);

        return new;
}

/* see utils.h */
char *string_strip_quotes(const char *value)
{
        ASSERT_OR_RET(value, NULL);

        size_t len = strlen(value);
        char *s;

        if (value[0] == '"' && value[len-1] == '"')
                s = g_strndup(value + 1, len-2);
        else
                s = g_strdup(value);

        return s;
}

/* see utils.h */
void string_strip_delimited(char *str, char a, char b)
{
        assert(str);

        int iread=-1, iwrite=0, copen=0;
        while (str[++iread] != 0) {
                if (str[iread] == a) {
                        ++copen;
                } else if (str[iread] == b && copen > 0) {
                        --copen;
                } else if (copen == 0) {
                        str[iwrite++] = str[iread];
                }
        }
        str[iwrite] = 0;
}

/* see utils.h */
char *string_to_path(char *string)
{

        if (string && STRN_EQ(string, "~/", 2)) {
                char *home = g_strconcat(user_get_home(), "/", NULL);

                string = string_replace_at(string, 0, 2, home);

                g_free(home);
        }

        return string;
}

/* see utils.h */
gint64 string_to_time(const char *string)
{
        assert(string);

        errno = 0;
        char *endptr;
        gint64 val = strtoll(string, &endptr, 10);

        if (errno != 0) {
                LOG_W("Time: '%s': %s.", string, strerror(errno));
                return 0;
        } else if (string == endptr) {
                LOG_W("Time: '%s': No digits found.", string);
                return 0;
        } else if (errno != 0 && val == 0) {
                LOG_W("Time: '%s': Unknown error.", string);
                return 0;
        } else if (errno == 0 && !*endptr) {
                return S2US(val);
        }

        // endptr may point to a separating space
        while (isspace(*endptr))
                endptr++;

        if (STRN_EQ(endptr, "ms", 2))
                return val * 1000;
        else if (STRN_EQ(endptr, "s", 1))
                return S2US(val);
        else if (STRN_EQ(endptr, "m", 1))
                return S2US(val) * 60;
        else if (STRN_EQ(endptr, "h", 1))
                return S2US(val) * 60 * 60;
        else if (STRN_EQ(endptr, "d", 1))
                return S2US(val) * 60 * 60 * 24;
        else
                return 0;
}

/* see utils.h */
gint64 time_monotonic_now(void)
{
        struct timespec tv_now;

        /* On Linux, BOOTTIME is the correct monotonic time,
         * as BOOTTIME counts onwards during sleep. For all other
         * POSIX compliant OSes, MONOTONIC should also count onwards
         * during system sleep. */
#ifdef __linux__
        clock_gettime(CLOCK_BOOTTIME, &tv_now);
#else
        clock_gettime(CLOCK_MONOTONIC, &tv_now);
#endif
        return S2US(tv_now.tv_sec) + tv_now.tv_nsec / 1000;
}

/* see utils.h */
const char *user_get_home(void)
{
        static const char *home_directory = NULL;
        ASSERT_OR_RET(!home_directory, home_directory);

        // Check the HOME variable for the user's home
        home_directory = getenv("HOME");
        ASSERT_OR_RET(!home_directory, home_directory);

        // Check the /etc/passwd entry for the user's home
        home_directory = getpwuid(getuid())->pw_dir;

        return home_directory;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
