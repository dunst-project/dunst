/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef _DUNST_DBUS_H
#define _DUNST_DBUS_H

#include <dbus/dbus.h>

void initdbus(void);
void dbus_poll(int timeout);
void notify(DBusMessage * msg);
void getCapabilities(DBusMessage * dmsg);
void closeNotification(DBusMessage * dmsg);
void getServerInformation(DBusMessage * dmsg);
void notificationClosed(notification * n, int reason);

#endif

/* vim: set ts=8 sw=8 tw=0: */
