/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_DUNST_H
#define DUNST_DUNST_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "notification.h"

#define PERR(msg, errnum) printf("(%d) %s : %s\n", __LINE__, (msg), (strerror(errnum)))

#define ColLast 3
#define ColFrame 2
#define ColFG 1
#define ColBG 0

extern GSList *rules;
extern const char *colors[3][3];

gboolean run(void *data);
void wake_up(void);

int dunst_main(int argc, char *argv[]);

void usage(int exit_status);
void print_version(void);
char *extract_urls(const char *str);
void context_menu(void);
void pause_signal_handler(int sig);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
