[![Build Status](https://travis-ci.org/dunst-project/dunst.svg?branch=master)](https://travis-ci.org/dunst-project/dunst)

## Dunst


## Description

Dunst is a highly configurable and lightweight notification daemon.


## Compiling

Dunst has a number of build dependencies that must be present before attempting configuration. The names are different depending on distribution:

- dbus
- libxinerama
- libxft
- libxss
- libxdg-basedir
- glib
- pango/cairo
- libgtk2.0

On Debian and Debian-based distros you'll probably also need the *-dev packages.

To build it:
```bash
cd dunst
make
sudo make PREFIX=/usr install
```


## Bug reports

Please use the [issue tracker][issue-tracker] provided by GitHub to send us bug reports or feature requests. You can also join us at [#dunst](irc://irc.freenode.net/#dunst) IRC channel on Freenode.

## Author

written by Sascha Kruse <knopwob@googlemail.com>

[issue-tracker]:  https://github.com/dunst-project/dunst/issues
## Copyright

copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information)

If you feel that copyrights are violated, please send me an email.
