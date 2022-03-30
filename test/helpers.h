#ifndef DUNST_TEST_HELPERS_H
#define DUNST_TEST_HELPERS_H

#include <glib.h>

GVariant *notification_setup_raw_image(const char *path);
struct notification *test_notification_uninitialized(const char *name);
struct notification *test_notification(const char *name, gint64 timeout);
struct notification *test_notification_with_icon(const char *name, gint64 timeout);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
