Getting Started
===============

.. note::

    There's no single true path to install dunst. Just execute the following commands and in case of an error, lookup the error message in the linked troubleshooting section.

Ensure a DBus session
---------------------

DBus has to be running in your session::

    dbus-send --session --dest=org.freedesktop.DBus / org.freedesktop.DBus.Peer.Ping && echo "DBus works!"

It has to print out ``DBus works!``, otherwise please consult :ref:`trouble-dbus-unsuccessful`.

Install dunst
-------------

Mostly, just install the ``dunst`` package with your package manager. For more detailed information, please consider `the installation page <./installation.html>`_.

Alternatively you can `compile dunst from source <./compiling.html>`_.

Run dunst
---------

Open your terminal and start the dunst **server** for validation::

    dunst -startup_notification

A small window should pop up saying ``dunst is up and running`` for at least a few seconds.

Relogin to your session
-----------------------

Now, after you've installed and made sure that dunst is able to start, please log out and log in again. Dunst ist now running and receives notifications.

Configuring dunst
-----------------

Dunst works and is now running. Its appearance may not match your liking. For this, copy the :download:`example dunstrc <../dunstrc>` to :file:`~/.config/dunst/dunstrc`. Change the settings to your liking.

You can apply the changes simply by killing the service (``killall dunst``). It will get restarted automatically by DBus.

Enjoy your new desktop notifications!
