/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#pragma once

#include <glib.h>
#include <stdbool.h>

#include "x.h"
#include "notification.h"

#define ERR(msg) printf("%s : %d\n", (msg), __LINE__)
#define PERR(msg, errnum) printf("(%d) %s : %s\n", __LINE__, (msg), (strerror(errnum)))
#define LENGTH(X)               (sizeof X / sizeof X[0])

#define ColLast 2
#define ColFG 1
#define ColBG 0

enum alignment { left, center, right };
enum icon_position_t { icons_left, icons_right, icons_off };
enum separator_color { FOREGROUND, AUTO, FRAME, CUSTOM };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };

extern int verbosity;
extern GQueue *queue;
extern GQueue *displayed;
extern GQueue *history;
extern GSList *rules;
extern bool pause_display;
extern const char *color_strings[2][3];

/* return id of notification */
gboolean run(void *data);
void wake_up(void);

void check_timeouts(void);
void history_pop(void);
void history_push(notification *n);
void usage(int exit_status);
void move_all_to_history(void);
void print_version(void);
char *extract_urls(const char *str);
void context_menu(void);
void wake_up(void);
void pause_signal_handler(int sig);
/* vim: set ts=8 sw=8 tw=0: */
