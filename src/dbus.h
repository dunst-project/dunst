/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_DBUS_H
#define DUNST_DBUS_H

#include "notification.h"

int initdbus(void);
void dbus_tear_down(int id);
/* void dbus_poll(int timeout); */
void notification_closed(notification * n, int reason);
void action_invoked(notification * n, const char *identifier);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
