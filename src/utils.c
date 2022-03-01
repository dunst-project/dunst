/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "settings_data.h"

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
        int cskip = 0;
        while (str[++iread] != 0) {
                if (str[iread] == a) {
                        ++copen;
                } else if (str[iread] == b && copen > 0) {
                        --copen;
                } else if (copen == 0) {
                        cskip = 0;
                        str[iwrite++] = str[iread];
                }
                if (copen > 0){
                        cskip++;
                }
        }
        if (copen > 0) {
                iread -= cskip;
                for (int i = 0; i < cskip; i++) {
                        str[iwrite++] = str[iread++];
                }
        }
        str[iwrite] = 0;
}

/* see utils.h */
char **string_to_array(const char *string, const char *delimiter)
{
        char **arr = NULL;
        if (string) {
                arr = g_strsplit(string, delimiter, 0);
                for (int i = 0; arr[i]; i++){
                        g_strstrip(arr[i]);
                }
        }
        return arr;
}

/* see utils.h */
int string_array_length(char **s)
{
        if (!s)
                return -1;

        int len = 0;
        while (s[len])
                len++;

        return len;
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
bool safe_string_to_long_long(long long *in, const char *str) {
        errno = 0;
        char *endptr;
        long long val = g_ascii_strtoll(str, &endptr, 10);

        if (errno != 0) {
                LOG_W("'%s': %s.", str, strerror(errno));
                return false;
        } else if (str == endptr) {
                LOG_W("'%s': No digits found.", str);
                return false;
        } else if (*endptr != '\0') {
                LOG_W("'%s': String contains non-digits.", str);
                return false;
        }
        *in = val;
        return true;
}

/* see utils.h */
bool safe_string_to_int(int *in, const char *str) {
        long long l;
        if (!safe_string_to_long_long(&l, str))
                return false;

        // Check if it's in int range
        if (l < INT_MIN || l > INT_MAX) {
                errno = ERANGE;
                LOG_W("'%s': %s.", str, strerror(errno));
                return false;
        }

        *in = (int) l;
        return true;
}

/* see utils.h */
bool safe_string_to_double(double *in, const char *str) {
        errno = 0;
        char *endptr;
        double val = g_ascii_strtod(str, &endptr);
        if (errno != 0) {
                LOG_W("'%s': %s.", str, strerror(errno));
                return false;
        } else if (str == endptr) {
                LOG_W("'%s': No digits found.", str);
                return false;
        } else if (*endptr != '\0') {
                LOG_W("'%s': String contains non-digits.", str);
                return false;
        }
        *in = val;
        return true;
}

/* see utils.h */
gint64 string_to_time(const char *string)
{
        assert(string);

        errno = 0;
        char *endptr;
        gint64 val = strtol(string, &endptr, 10);

        if (errno != 0) {
                LOG_W("Time: '%s': %s.", string, strerror(errno));
                return 0;
        } else if (string == endptr) {
                errno = EINVAL;
                LOG_W("Time: '%s': No digits found.", string);
                return 0;
        } else if (val < -1) {
                // most times should not be negative, but show_age_threshhold
                // can be -1
                LOG_W("Time: '%s': Time should be positive (-1 is allowed too sometimes)",
                                string);
                errno = EINVAL;
        }
        else if (errno == 0 && !*endptr) {
                return S2US(val);
        }
        // endptr may point to a separating space
        while (isspace(*endptr))
                endptr++;

        if (val < 0) {
                LOG_W("Time: '%s' signal value -1 should not have a suffix", string);
                errno = EINVAL;
                return 0;
        }

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
        {
                errno = EINVAL;
                return 0;
        }
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

/* see utils.h */
bool safe_setenv(const char* key, const char* value){
        if (!key)
                return false;

        if (!value)
                setenv(key, "", 1);
        else
                setenv(key, value, 1);

        return true;
}

// These sections are not interpreted as rules
static const char* special_sections[] = {
        "global",
        "frame",
        "experimental",
        "shortcuts",
        "urgency_low",
        "urgency_normal",
        "urgency_critical",
};

static const char* deprecated_sections[] = {
        "frame",
        "shortcuts",
};

static const char* deprecated_sections_message[] = {
        "The settings from the frame section have been moved to the global section.", // frame
        "Settings in the shortcuts sections have been moved to the global section.\nAlternatively you can bind shortcuts in you window manager to dunstctl commands. For that, see the manual for dunstctl.", // shortcuts
};

/* see utils.h */
bool is_special_section(const char* s) {
        for (size_t i = 0; i < G_N_ELEMENTS(special_sections); i++) {
                if (STR_EQ(special_sections[i], s)) {
                        return true;
                }
        }
        return false;
}

/* see utils.h */
bool is_deprecated_section(const char* s) {
        for (size_t i = 0; i < G_N_ELEMENTS(deprecated_sections); i++) {
                if (STR_EQ(deprecated_sections[i], s)) {
                        return true;
                }
        }
        return false;
}

const char *get_section_deprecation_message(const char *s) {
        for (size_t i = 0; i < G_N_ELEMENTS(deprecated_sections); i++) {
                if (STR_EQ(deprecated_sections[i], s)) {
                        return deprecated_sections_message[i];
                }
        }
        return "";
}

/* see utils.h */
char *string_strip_brackets(const char* s) {
        if (!s)
                return NULL;

        size_t len = strlen(s);
        if (s[0] == '(' && s[len-1] == ')')
                return g_strndup(s + 1, len-2);
        else
                return NULL;

}

/* see utils.h */
bool is_readable_file(const char * const path)
{
        struct stat statbuf;
        bool result = false;

        if (0 == stat(path, &statbuf)) {
                /** See what intersting stuff can be done with FIFOs */
                if (!(statbuf.st_mode & (S_IFIFO | S_IFREG))) {
                        /** Sets errno if stat() was successful but @p path [in]
                         * does not point to a regular file or FIFO. This
                         * just in case someone queries errno which would
                         * otherwise indicate success. */
                        errno = EINVAL;
                } else if (0 == access(path, R_OK)) { /* must also be readable */
                        result = true;
                }
        }

        return result;
}

/* see utils.h */
FILE *fopen_verbose(const char * const path)
{
        FILE *f = NULL;
        char *real_path = string_to_path(strdup(path));

        if (is_readable_file(real_path) && (f = fopen(real_path, "r")))
                LOG_I(MSG_FOPEN_SUCCESS(path, f));
        else
                LOG_W(MSG_FOPEN_FAILURE(path));

        free(real_path);
        return f;
}

/* see utils.h */
void add_paths_from_env(GPtrArray *arr, char *env_name, char *subdir, char *alternative) {
        const char *xdg_data_dirs = g_getenv(env_name);
        if (!xdg_data_dirs)
                xdg_data_dirs = alternative;

        char **xdg_data_dirs_arr = string_to_array(xdg_data_dirs, ":");
        for (int i = 0; xdg_data_dirs_arr[i] != NULL; i++) {
                char *loc = g_build_filename(xdg_data_dirs_arr[i], subdir, NULL);
                g_ptr_array_add(arr, loc);
        }
        g_strfreev(xdg_data_dirs_arr);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
