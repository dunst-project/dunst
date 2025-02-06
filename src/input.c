#include "input.h"
#include "log.h"
#include "menu.h"
#include "settings.h"
#include "queues.h"
#include <stddef.h>
#if defined(__linux__) || defined(__FreeBSD__)
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT	(0x110)
#define BTN_RIGHT	(0x111)
#define BTN_MIDDLE	(0x112)
#define BTN_TOUCH	(0x14a)
#endif

int get_notification_clickable_height(struct notification *n, bool first, bool last)
{
        int notification_size = n->displayed_height;
        if (settings.gap_size) {
                notification_size += settings.frame_width * 2;
        } else {
                double half_separator = settings.separator_height / 2.0;
                notification_size += settings.separator_height;
                if(first)
                    notification_size += (settings.frame_width - half_separator);
                if(last)
                    notification_size += (settings.frame_width - half_separator);
        }
        return notification_size;
}

struct notification *get_notification_at(const int y) {
        int curr_y = 0;
        bool first = true;
        bool last;
        for (const GList *iter = queues_get_displayed(); iter;
                        iter = iter->next) {
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
        // no matching notification was found
        return NULL;
}

bool handle_builtin_menu_click(int x, int y) {
        if (!settings.built_in_menu) {
                return false;
        }

        struct notification *n = get_notification_at(y);
        if (n) {
                if (menu_get_count(n) > 0) {
                        struct menu *m = menu_get_at(n, x, y);
                        if (m) {
                                signal_action_invoked(n, m->key);
                                return true;
                        }
                }
        }
        return false;
}

void input_handle_click(unsigned int button, bool button_down, int mouse_x, int mouse_y){
        LOG_I("Pointer handle button %i: %i", button, button_down);

        if (button_down) {
                // make sure it only reacts on button release
                return;
        }

        if (settings.built_in_menu){
                if (handle_builtin_menu_click( mouse_x, mouse_y))
                        return;
        }

        enum mouse_action *acts;

        switch (button) {
                case BTN_LEFT:
                        acts = settings.mouse_left_click;
                        break;
                case BTN_MIDDLE:
                        acts = settings.mouse_middle_click;
                        break;
                case BTN_RIGHT:
                        acts = settings.mouse_right_click;
                        break;
                case BTN_TOUCH:
                        // TODO Add separate action for touch
                        acts = settings.mouse_left_click;
                        break;
                default:
                        LOG_W("Unsupported mouse button: '%d'", button);
                        return;
        }

        // if other list types are added, make sure they have the same end value
        for (int i = 0; acts[i] != MOUSE_ACTION_END; i++) {
                enum mouse_action act = acts[i];
                if (act == MOUSE_CLOSE_ALL) {
                        queues_history_push_all();
                        continue;
                }

                if (act == MOUSE_CONTEXT_ALL) {
                        context_menu();
                        continue;
                }

                if (act == MOUSE_DO_ACTION || act == MOUSE_CLOSE_CURRENT || act == MOUSE_CONTEXT || act == MOUSE_OPEN_URL) {
                        struct notification *n = get_notification_at(mouse_y);

                        if (n) {
                                if (act == MOUSE_CLOSE_CURRENT) {
                                        n->marked_for_closure = REASON_USER;
                                } else if (act == MOUSE_DO_ACTION) {
                                        notification_do_action(n);
                                } else if (act == MOUSE_OPEN_URL) {
                                        notification_open_url(n);
                                } else {
                                        notification_open_context_menu(n);
                                }
                        }
                }
        }

        wake_up();
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
