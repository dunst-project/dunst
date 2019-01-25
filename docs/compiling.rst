Compiling from source
---------------------

Please consult the current docs for this and build the current master.

.. TODO: Do not add this to 1.3 and only add it in current master.

Dependencies
~~~~~~~~~~~~

Dunst has a number of build dependencies that must be present before attempting configuration. The names are different depending on your distribution:

- dbus
- libxinerama
- libxrandr
- libxss
- glib
- pango/cairo
- gdk-pixbuf


.. Keep these package lists in sync with the docker containers.

For ArchLinux::

    pacman -S --needed gdk-pixbuf2 libnotify librsvg libxinerama libxrandr libxss pango

For Debian and Ubuntu based systems::

    apt-get install dbus libdbus-1-dev libglib2.0-dev libnotify-dev libpango1.0-dev librsvg2-common libx11-dev libxinerama-dev libxrandr-dev libxss-dev

For Fedora (and other RPM based systems)::

    dnf install dbus-devel glib2-devel libnotify-devel librsvg2 libX11-devel libXinerama libXinerama-devel libXrandr-devel libXScrnSaver-devel pango-devel

Building
~~~~~~~~

::

    git clone https://github.com/dunst-project/dunst.git
    cd dunst
    make
    sudo make install

Make parameters
~~~~~~~~~~~~~~~

**Make sure to run all make calls with the same parameter set.** So when building with ``make PREFIX=/usr``, you have to install it with ``make PREFIX=/usr install``, too.

============================= =========================== ===============================================================================
Parameter                     Default Value               Purpose
============================= =========================== ===============================================================================
:makevar:`PREFIX`             :file:`/usr/local`          The general prefix of the dunst installation.
:makevar:`MANPREFIX`          :file:`${PREFIX}/share/man` The place, where the manpages should get installed.
:makevar:`SYSTEMD=(0|1)`      detected via ``pkg-config`` Enable the systemd unit.
:makevar:`SERVICEDIR_SYSTEMD` detected via ``pkg-config`` The path to put the systemd user service file. Unused, if :makevar:`SYSTEMD=0`.
:makevar:`SERVICEDIR_DBUS`    detected via ``pkg-config`` The path to put the dbus service file.
============================= =========================== ===============================================================================
