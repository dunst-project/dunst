/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/**
 *  @file src/log.c
 *  @brief logging wrapper to use GLib's logging capabilities
 */

#include "log.h"

#include <glib.h>

#include "utils.h"

static GLogLevelFlags log_level = G_LOG_LEVEL_WARNING;

/* see log.h */
static const char *log_level_to_string(GLogLevelFlags level)
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
void log_set_level_from_string(const char *level)
{
        ASSERT_OR_RET(level,);

        if (STR_CASEQ(level, "critical"))
                log_level = G_LOG_LEVEL_CRITICAL;
        else if (STR_CASEQ(level, "crit"))
                log_level = G_LOG_LEVEL_CRITICAL;
        else if (STR_CASEQ(level, "warning"))
                log_level = G_LOG_LEVEL_WARNING;
        else if (STR_CASEQ(level, "warn"))
                log_level = G_LOG_LEVEL_WARNING;
        else if (STR_CASEQ(level, "message"))
                log_level = G_LOG_LEVEL_MESSAGE;
        else if (STR_CASEQ(level, "mesg"))
                log_level = G_LOG_LEVEL_MESSAGE;
        else if (STR_CASEQ(level, "info"))
                log_level = G_LOG_LEVEL_INFO;
        else if (STR_CASEQ(level, "debug"))
                log_level = G_LOG_LEVEL_DEBUG;
        else if (STR_CASEQ(level, "deb"))
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
                log_level = G_LOG_LEVEL_ERROR;

        GLogLevelFlags message_level_masked = message_level & G_LOG_LEVEL_MASK;

/* if you want to have a debug build, you want to log anything,
 * unconditionally, without specifying debug log level again */
#ifndef DEBUG_BUILD
        if (log_level < message_level_masked)
                return;
#endif
        const char *log_level_str =
                log_level_to_string(message_level_masked);

        /* Use stderr for warnings and higher */
        if (message_level_masked <= G_LOG_LEVEL_WARNING)
                g_printerr("%s: %s\n", log_level_str, message);
        else
                g_print("%s: %s\n", log_level_str, message);
}

/* see log.h */
void dunst_log_init(bool testing)
{
        g_log_set_default_handler(dunst_log_handler, (void*)testing);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
