[![Build Status](https://travis-ci.org/dunst-project/dunst.svg?branch=master)](https://travis-ci.org/dunst-project/dunst) [![Coverage Status](https://coveralls.io/repos/github/dunst-project/dunst/badge.svg?branch=coveralls)](https://coveralls.io/github/dunst-project/dunst?branch=master)

## Dunst

* [Wiki][wiki]
* [Description](#description)
* [Compiling](#compiling)
* [Copyright](#copyright)

## Description

Dunst is a highly configurable and lightweight notification daemon.


## Installation

### Dependencies

Dunst has a number of build dependencies that must be present before attempting configuration. The names are different depending on [distribution](https://github.com/dunst-project/dunst/wiki/Dependencies):

- dbus
- libxinerama
- libxrandr
- libxss
- libxdg-basedir
- glib
- pango/cairo
- libgtk-3-dev

### Building

```
git clone https://github.com/dunst-project/dunst.git
cd dunst
make
sudo make install
```

### Make parameters

- `PREFIX=<PATH>`: Set the prefix of the installation. (Default: `/usr/local`)
- `MANPREFIX=<PATH>`: Set the prefix of the manpage. (Default: `${PREFIX}/share/man`)
- `SYSTEMD=(0|1)`: Enable/Disable the systemd unit. (Default: detected via `pkg-config`)
- `SERVICEDIR_SYSTEMD=<PATH>`: The path to put the systemd user service file. Unused, if `SYSTEMD=0`. (Default: detected via `pkg-config`)
- `SERVICEDIR_DBUS=<PATH>`: The path to put the dbus service file. (Default: detected via `pkg-config`)

**Make sure to run all make calls with the same parameter set. So when building with `make PREFIX=/usr`, you have to install it with `make PREFIX=/usr install`, too.**

Checkout the [wiki][wiki] for more information.

## Bug reports

Please use the [issue tracker][issue-tracker] provided by GitHub to send us bug reports or feature requests. You can also join us on the IRC channel `#dunst` on Freenode.

## Maintainers

Nikos Tsipinakis <nikos@tsipinakis.com>

Jonathan Lusso <jonilusso@gmail.com>

## Author

written by Sascha Kruse <dunst@knopwob.de>

## Copyright

copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information)

If you feel that copyrights are violated, please send me an email.

[issue-tracker]:  https://github.com/dunst-project/dunst/issues
[wiki]: https://github.com/dunst-project/dunst/wiki
