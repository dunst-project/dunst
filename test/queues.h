#include "greatest.h"

#ifndef DUNST_TEST_QUEUES_H
#define DUNST_TEST_QUEUES_H

#include <stdbool.h>
#include <glib.h>

#include "../src/notification.h"
#include "../src/queues.h"

#define STATUS_NORMAL ((struct dunst_status) {.fullscreen=false, .running=true,  .idle=false})
#define STATUS_IDLE   ((struct dunst_status) {.fullscreen=false, .running=true,  .idle=true})
#define STATUS_FSIDLE ((struct dunst_status) {.fullscreen=true,  .running=true,  .idle=true})
#define STATUS_FS     ((struct dunst_status) {.fullscreen=true,  .running=true,  .idle=false})
#define STATUS_PAUSE  ((struct dunst_status) {.fullscreen=false, .running=false, .idle=false})

#define QUEUE_WAIT waiting
#define QUEUE_DISP displayed
#define QUEUE_HIST history
#define QUEUE(q) QUEUE_##q

#define QUEUE_LEN_ALL(wait, disp, hist) do { \
        if (wait >= 0) ASSERTm("Waiting is not "   #wait, wait == g_queue_get_length(QUEUE(WAIT))); \
        if (disp >= 0) ASSERTm("Displayed is not " #disp, disp == g_queue_get_length(QUEUE(DISP))); \
        if (disp >= 0) ASSERTm("History is not "   #hist, hist == g_queue_get_length(QUEUE(HIST))); \
        } while (0)

#define QUEUE_CONTAINS(q, n) QUEUE_CONTAINSm("QUEUE_CONTAINS(" #q "," #n ")", q, n)
#define QUEUE_CONTAINSm(msg, q, n) ASSERTm(msg, g_queue_find(QUEUE(q), n))

#define NOT_LAST(n) do {ASSERT_EQm("Notification " #n " should have been deleted.", 1, notification_refcount_get(n)); g_clear_pointer(&n, notification_unref); } while(0)

/* Retrieve a notification by its id. Solely for debugging purposes */
struct notification *queues_debug_find_notification_by_id(int id);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
