#ifndef DUNST_TEST_HELPERS_H
#define DUNST_TEST_HELPERS_H

#include <glib.h>
#include "../src/draw.h"

GVariant *notification_setup_raw_image(const char *path);
struct notification *test_notification_uninitialized(const char *name);
struct notification *test_notification(const char *name, gint64 timeout);
struct notification *test_notification_with_icon(const char *name, gint64 timeout);
GSList *get_dummy_notifications(int count);
void free_dummy_notification(void *notification);

bool gradient_same(struct gradient *grad, struct gradient *grad2);
bool gradient_check_color(struct gradient *grad, struct color col);
struct gradient *gradient_from_color(struct color col);

#define ARRAY_SAME_LENGTH(a, b) { \
        ASSERT_EQm("Test is invalid. Input data has to be the same length",\
                        G_N_ELEMENTS(a), G_N_ELEMENTS(b));\
}

#endif
