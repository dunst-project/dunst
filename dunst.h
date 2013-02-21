/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#pragma once

#include <glib.h>
#include <stdbool.h>

#include "x.h"

#define ERR(msg) printf("%s : %d\n", (msg), __LINE__)
#define PERR(msg, errnum) printf("(%d) %s : %s\n", __LINE__, (msg), (strerror(errnum)))
#define LENGTH(X)               (sizeof X / sizeof X[0])


#define ColLast 2
#define ColFG 1
#define ColBG 0

enum alignment { left, center, right };
enum separator_color { FOREGROUND, AUTO, FRAME, CUSTOM };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };


extern int verbosity;
extern GQueue *queue;
extern GQueue *displayed;
extern GQueue *history;
extern GSList *rules;
extern bool pause_display;
extern const char *color_strings[2][3];
extern DC *dc;



/* return id of notification */
gboolean run(void *data);
void wake_up(void);

/* vim: set ts=8 sw=8 tw=0: */
