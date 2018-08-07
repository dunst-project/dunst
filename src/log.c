/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/** @file log.c
 *  @brief logging wrapper to use GLib's logging capabilities
 */

#include "log.h"

#include <glib.h>

static GLogLevelFlags log_level = G_LOG_LEVEL_WARNING;

/* see log.h */
static const char *log_level_to_string(GLogLevelFlags level)
{
        switch (level) {
        case G_LOG_LEVEL_ERROR: return "dunst: ERROR";
        case G_LOG_LEVEL_CRITICAL: return "dunst: CRITICAL";
        case G_LOG_LEVEL_WARNING: return "dunst: WARNING";
        case G_LOG_LEVEL_MESSAGE: return "dunst: MESSAGE";
        case G_LOG_LEVEL_INFO: return "dunst: INFO";
        case G_LOG_LEVEL_DEBUG: return "dunst: DEBUG";
        default: return "dunst: UNKNOWN";
        }
}

/* see log.h */
void log_set_level_from_string(const char *level)
{
        if (!level)
                return;

        if (g_ascii_strcasecmp(level, "critical") == 0)
                log_level = G_LOG_LEVEL_CRITICAL;
        else if (g_ascii_strcasecmp(level, "crit") == 0)
                log_level = G_LOG_LEVEL_CRITICAL;
        else if (g_ascii_strcasecmp(level, "warning") == 0)
                log_level = G_LOG_LEVEL_WARNING;
        else if (g_ascii_strcasecmp(level, "warn") == 0)
                log_level = G_LOG_LEVEL_WARNING;
        else if (g_ascii_strcasecmp(level, "message") == 0)
                log_level = G_LOG_LEVEL_MESSAGE;
        else if (g_ascii_strcasecmp(level, "mesg") == 0)
                log_level = G_LOG_LEVEL_MESSAGE;
        else if (g_ascii_strcasecmp(level, "info") == 0)
                log_level = G_LOG_LEVEL_INFO;
        else if (g_ascii_strcasecmp(level, "debug") == 0)
                log_level = G_LOG_LEVEL_DEBUG;
        else if (g_ascii_strcasecmp(level, "deb") == 0)
                log_level = G_LOG_LEVEL_DEBUG;
        else
                LOG_W("Unknown log level: '%s'", level);
}

void log_set_level(GLogLevelFlags level)
{
        log_level = level;
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
        if (log_level < message_level)
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
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
