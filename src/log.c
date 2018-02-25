/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/** @file log.c
 *  @brief logging wrapper to use GLib's logging capabilities
 */

#include "log.h"

#include <glib.h>

#include "settings.h"

/* see log.h */
const char *log_level_to_string(GLogLevelFlags level)
{
        switch (level) {
        case G_LOG_LEVEL_ERROR: return "ERROR";
        case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
        case G_LOG_LEVEL_WARNING: return "WARNING";
        case G_LOG_LEVEL_MESSAGE: return "MESSAGE";
        case G_LOG_LEVEL_INFO: return "INFO";
        case G_LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
        }
}

/* see log.h */
GLogLevelFlags string_parse_loglevel(const char *level, GLogLevelFlags def)
{
        if (!level)
                return def;

        if (g_ascii_strcasecmp(level, "critical") == 0)
                return G_LOG_LEVEL_CRITICAL;
        else if (g_ascii_strcasecmp(level, "crit") == 0)
                return G_LOG_LEVEL_CRITICAL;
        else if (g_ascii_strcasecmp(level, "warning") == 0)
                return G_LOG_LEVEL_WARNING;
        else if (g_ascii_strcasecmp(level, "warn") == 0)
                return G_LOG_LEVEL_WARNING;
        else if (g_ascii_strcasecmp(level, "message") == 0)
                return G_LOG_LEVEL_MESSAGE;
        else if (g_ascii_strcasecmp(level, "mesg") == 0)
                return G_LOG_LEVEL_MESSAGE;
        else if (g_ascii_strcasecmp(level, "info") == 0)
                return G_LOG_LEVEL_INFO;
        else if (g_ascii_strcasecmp(level, "debug") == 0)
                return G_LOG_LEVEL_DEBUG;
        else if (g_ascii_strcasecmp(level, "deb") == 0)
                return G_LOG_LEVEL_DEBUG;

        LOG_W("Unknown log level: '%s'", level);
        return def;
}

/**
 * Log handling function for GLib's logging wrapper
 *
 * @param log_domain Used only by GLib
 * @param message_level Used only by GLib
 * @param message Used only by GLib
 * @param testing If not `NULL` (here: `true`), do nothing
 */
static void dunst_log_handler(
                const gchar    *log_domain,
                GLogLevelFlags  message_level,
                const gchar    *message,
                gpointer        testing)
{
        if (testing)
                return;

/* if you want to have a debug build, you want to log anything,
 * unconditionally, without specifying debug log level again */
#ifndef DEBUG_BUILD
        if (settings.log_level < message_level)
                return;
#endif
        const char *log_level_str =
                log_level_to_string(message_level & G_LOG_LEVEL_MASK);

        /* Use stderr for warnings and higher */
        if (message_level <= G_LOG_LEVEL_WARNING)
                g_printerr("%s: %s\n", log_level_str, message);
        else
                g_print("%s: %s\n", log_level_str, message);
}

/* see log.h */
void dunst_log_init(bool testing)
{
        g_log_set_default_handler(dunst_log_handler, (void*)testing);
        settings.log_level = G_LOG_LEVEL_MESSAGE;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
