===============
Troubleshooting
===============

If you get any error message during the installation, please consult this document.

.. _trouble-dbus-unsuccessful:

dbus-send unsuccessful
----------------------

Do you have DBus running? Test it by checking your :envvar:`DBUS_SESSION_BUS_ADDRESS` environment variable.

If it's not available, it's a strong indication, that you haven't configured DBus correctly.

Please consult your distribution vendor's wiki.

For ``startx`` sessions, please prepend your session-command with ``dbus-launch``.

.. _trouble-x11-badaccess:

Error message: ``BadAccess (attempt to access private resource denied)``
------------------------------------------------------------------------

.. TODO: Rename this to corresponding error message on master

This error message is directly related to `Unable to grab key`.  See there for more info.

.. _trouble-x11-keygrab:

Error message: ``Unable to grab key "mod4+<XY>"``
-------------------------------------------------

.. TODO: Rename this to corresponding error message on master

dunst can't grab your key.  This is most likely the case, that there's another instance of dunst already running.  If not, you either have to change the key mapping in the dunst config or change it in the other program.

.. _trouble-x11-displayvar:

Error message: ``cannot open display``
--------------------------------------

.. TODO: Rename this to "Cannot open X11 display" in master

If you're using an :file:`~/.xinitrc` for your session instantiation, please add import the :envvar:`DISPLAY` variable into your session:

.. code-block:: bash

    systemctl --user import-environment DISPLAY

.. _trouble-dbus-fdn-acquired:

Error message: ``Cannot acquire 'org.freedesktop.Notifications': Name is acquired by PID 'XY'.``
------------------------------------------------------------------------------------------------

.. TODO: Rename this to corresponding error message on master

There's already a notification daemon running.  Either it's dunst itself or another notification daemon.  Check it by executing ``readlink -f /proc/<PID>/exe``.
