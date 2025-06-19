#include "input.h"
#include "log.h"
#include "menu.h"
#include "settings.h"
#include "queues.h"
#include "notification.h" /* Added - For notification_close / dbus.h include and history_remove was added */
#include "dbus.h" /* Added - For signal_history_removed */
#include <stddef.h>
#if defined(__linux__) || defined(__FreeBSD__)
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT	(0x110)
#define BTN_RIGHT	(0x111)
#define BTN_MIDDLE	(0x112)
#define BTN_TOUCH	(0x14a)
#endif

/* Calculate clickable height of a notification, including frame or separator */
int get_notification_clickable_height(struct notification *n, bool first, bool last) {
    int notification_size = n->displayed_height;
    if (settings.gap_size) {
        notification_size += settings.frame_width * 2;
    } else {
        double half_separator = settings.separator_height / 2.0;
        notification_size += settings.separator_height;
        if (first)
            notification_size += (settings.frame_width - half_separator);
        if (last)
            notification_size += (settings.frame_width - half_separator);
    }
    return notification_size;
}

/* Retrieve the notification at the given y-coordinate */
struct notification *get_notification_at(const int y) {
    int curr_y = 0;
    bool first = true;
    bool last;
    for (const GList *iter = queues_get_displayed(); iter; iter = iter->next) {
        struct notification *current = iter->data;
        struct notification *next = iter->next ? iter->next->data : NULL;

        last = !next;
        int notification_size = get_notification_clickable_height(current, first, last);

        if (y >= curr_y && y < curr_y + notification_size) {
            return current;
        }

        curr_y += notification_size;
        if (settings.gap_size)
            curr_y += settings.gap_size;

        first = false;
    }
    return NULL;
}

/* Handle mouse click events on notifications */
void input_handle_click(unsigned int button, bool button_down, int mouse_x, int mouse_y) {
    /* Force debug logging for this run */
    g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE, g_log_default_handler, NULL);
    LOG_I("Pointer handle button %u: %d", button, button_down);

    if (button_down) {
        return; /* Process only on release */
    }

    enum mouse_action *acts;

    /* Map button to action list */
    switch (button) {
        case BTN_LEFT:
            acts = settings.mouse_left_click;
            LOG_D("Processing left click with %d actions", (acts ? g_strv_length((char **)acts) : 0));
            break;
        case BTN_MIDDLE:
            acts = settings.mouse_middle_click;
            LOG_D("Processing middle click with %d actions", (acts ? g_strv_length((char **)acts) : 0));
            break;
        case BTN_RIGHT:
            acts = settings.mouse_right_click;
            LOG_D("Processing right click with %d actions", (acts ? g_strv_length((char **)acts) : 0));
            break;
        case BTN_TOUCH:
            acts = settings.mouse_left_click;
            LOG_D("Processing touch with %d actions", (acts ? g_strv_length((char **)acts) : 0));
            break;
        default:
            LOG_W("Unsupported mouse button: '%u'", button);
            return;
    }

    /* Process each action in the list */
    for (int i = 0; acts[i] != MOUSE_ACTION_END; i++) {
        enum mouse_action act = acts[i];
        struct notification *n = NULL;

        LOG_D("Processing action %d at index %d", act, i);

        switch (act) {
            case MOUSE_CLOSE_ALL:
                queues_history_push_all();
                LOG_I("Closed all notifications");
                break;
            case MOUSE_CONTEXT_ALL:
                context_menu();
                LOG_I("Opened context menu for all");
                break;
            case MOUSE_DO_ACTION:
                n = get_notification_at(mouse_y);
                if (n) {
                    notification_do_action(n);
                    LOG_I("Performed action on notification ID %u", n->id);
                }
                break;
            case MOUSE_CLOSE_CURRENT:
                n = get_notification_at(mouse_y);
                if (n) {
                    n->marked_for_closure = REASON_USER;
                    LOG_I("Marked notification ID %u for closure", n->id);
                }
                break;
            case MOUSE_CONTEXT:
                n = get_notification_at(mouse_y);
                if (n) {
                    notification_open_context_menu(n);
                    LOG_I("Opened context menu for notification ID %u", n->id);
                }
                break;
            case MOUSE_OPEN_URL:
                n = get_notification_at(mouse_y);
                if (n) {
                    notification_open_url(n);
                    LOG_I("Opened URL for notification ID %u", n->id);
                }
                break;
            case MOUSE_CLEAR_CURRENT:
                n = get_notification_at(mouse_y);
                if (n) {
                    LOG_I("Clearing notification ID %u", n->id);
                    notification_close(n, REASON_USER); /* Marks for closure */
                    struct dunst_status status = dunst_status_get();
		    status.pause_level = 0;
                    queues_update(status, g_get_monotonic_time()); /* Process closure */
                    queues_history_remove_by_id(n->id); /* Remove from history */
                    signal_history_removed(n->id); /* Notify Waybar */
                    LOG_I("Cleared notification ID %u from display and history", n->id);
                } else {
                    LOG_W("No notification found at y=%d", mouse_y);
                }
                break;
            default:
                LOG_W("Unknown mouse action: %d", act);
                break;
        }
    }

    wake_up(); /* Refresh display */
}
