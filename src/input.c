#include "input.h"
#include "log.h"
#include "settings.h"
#include "queues.h"
#include <stddef.h>
#include <linux/input-event-codes.h>

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

        for (int i = 0; acts[i]; i++) {
                enum mouse_action act = acts[i];
                if (act == MOUSE_CLOSE_ALL) {
                        queues_history_push_all();
                        return;
                }

                if (act == MOUSE_DO_ACTION || act == MOUSE_CLOSE_CURRENT) {
                        int y = settings.separator_height;
                        struct notification *n = NULL;
                        int first = true;
                        for (const GList *iter = queues_get_displayed(); iter;
                             iter = iter->next) {
                                n = iter->data;
                                if (mouse_y > y && mouse_y < y + n->displayed_height)
                                        break;

                                y += n->displayed_height + settings.separator_height;
                                if (first)
                                        y += settings.frame_width;
                        }

                        if (n) {
                                if (act == MOUSE_CLOSE_CURRENT) {
                                        n->marked_for_closure = REASON_USER;
                                } else {
                                        notification_do_action(n);
                                }
                        }
                }
        }

        wake_up();
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
