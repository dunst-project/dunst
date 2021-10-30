#include "input.h"
#include "log.h"
#include "menu.h"
#include "settings.h"
#include "queues.h"
#include <stddef.h>
#include <linux/input-event-codes.h>

struct notification *get_notification_at(const int y) {
        int curr_y = settings.frame_width;
        for (const GList *iter = queues_get_displayed(); iter;
                        iter = iter->next) {
                struct notification *current = iter->data;
                if (y > curr_y && y < curr_y + current->displayed_height) {
                        return current;
                }

                curr_y += current->displayed_height + settings.separator_height;
        }
        // no matching notification was found
        return NULL;
}

void input_handle_click(unsigned int button, bool button_down, int mouse_x, int mouse_y){
        LOG_I("Pointer handle button %i: %i", button, button_down);

        if (button_down) {
                // make sure it only reacts on button release
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
