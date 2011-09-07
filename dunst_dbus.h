#include <dbus/dbus.h>

void initdbus(void);
void dbus_poll(void);
void notify(DBusMessage *msg);
void getCapabilities(DBusMessage *dmsg);
void closeNotification(DBusMessage *dmsg);
void getServerInformation(DBusMessage *dmsg);

#include "dunst_dbus.c"
