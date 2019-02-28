.. contents::

Configuration
=============

An example configuration file is included (usually /usr/share/dunst/dunstrc).
To change the configuration, copy this file to ~/.config/dunst/dunstrc and edit
it accordingly.

The configuration is divided into sections in an ini-like format. The 'global'
section contains most general settings while the 'shortcuts' sections contains
all keyboard configuration and the 'experimental' section all the features that
have not yet been tested thoroughly.

Any section that is not one of the above is assumed to be a rule, see :ref:`rules` for
more details.

For backwards compatibility reasons the section name 'frame' is considered bound
and can't be used as a rule.

Overriding the configuration via the command line
-------------------------------------------------

Each configuration option in the global section can be overridden from the
command line by adding a single dash in front of it's name.
For example the font option can be overridden by running

    $ dunst -font "LiberationSans Mono 4"

Configuration options that take boolean values can only currently be set to
"true" through the command line via the same method. e.g.

    $ dunst -shrink

This is a known limitation of the way command line parameters are parsed and
will be changed in the future.

Available configuration options (global section)
------------------------------------------------

monitor
~~~~~~~
|  Type: number
|  Default: 0

Specifies on which monitor the notifications should be displayed in, count
starts at 0. See the follow_ setting.

follow
~~~~~~
| Type: multi
| Default: none
| Values: [none/mouse/keyboard]

Defines where the notifications should be placed in a multi-monitor setup. All
values except *none* override the monitor_ setting.

- none

  The notifications will be placed on the monitor specified by the monitor_
  setting.

- mouse

  The notifications will be placed on the monitor that the mouse is currently in.

- keyboard

  The notifications will be placed on the monitor that contains the window with
  keyboard focus.

geometry
~~~~~~~~
| Type: string
| Default: "0x0+0-0"
| Format: [{width}][x{height}][+/-{x}[+/-{y}]]

The geometry of the window the notifications will be displayed in.

- width

  The width of the notification window in pixels. A negative value sets the width
  to the screen width **minus the absolute value of the width**. If the width is
  omitted then the window expands to cover the whole screen. If it's 0 the window
  expands to the width of the longest message being displayed.

- height

  The number of notifications that can appear at one time. When this
  limit is reached any additional notifications will be queued and displayed when
  the currently displayed ones either time out or are manually dismissed. If
  indicate_hidden_ is true, then the specified limit is reduced by 1 and the
  last notification is a message informing how many hidden notifications are
  waiting to be displayed. See the indicate_hidden_ entry for more information.

  The physical(pixel) height of the notifications vary depending on the number of
  lines that need to be displayed.

  See notification_height_ for changing the physical height.

- x/y

  Respectively the horizontal and vertical offset in pixels from the corner
  of the screen that the notification should be drawn at. For the horizontal(x)
  offset, a positive value is measured from the left of the screen while a
  negative one from the right. For the vertical(y) offset, a positive value is
  measured from the top while a negative from the bottom.

  It's important to note that the positive and negative sign **DOES** affect the
  position even if the offset is 0. For example, a horizontal offset of +0 puts
  the notification on the left border of the screen while a horizontal offset of
  -0 at the right border. The same goes for the vertical offset.

indicate_hidden
~~~~~~~~~~~~~~~
| Type: boolean
| Default: true

If this is set to true, a notification indicating how many notifications are
not being displayed due to the notification limit (see geometry_) will be
shown **in place of the last notification slot**.

Meaning that if this is enabled the number of visible notifications will be 1
less than what is specified in geometry, the last slot will be taken by the
hidden count.

shrink
~~~~~~
| Type: boolean
| Default: false

Shrink window if it's smaller than the width. Will be ignored if width is 0.

This is used mainly in order to have the shrinking benefit of dynamic width (see
geometry_) while also having an upper bound on how long a notification can get
before wrapping.

transparency
~~~~~~~~~~~~
| Type: number
| Default: 0

A 0-100 range on how transparent the notification window should be, with 0
being fully opaque and 100 invisible.

This setting will only work if a compositor is running.

notification_height
~~~~~~~~~~~~~~~~~~~
| Type: number
| Default: 0

The minimum height of the notification window in pixels. If the text and
padding cannot fit in within the height specified by this value, the height
will be increased as needed.

separator_height
~~~~~~~~~~~~~~~~
| Type: number
| Default: 2

The height in pixels of the separator between notifications, if set to 0 there
will be no separating line between notifications.

padding
~~~~~~~
| Type: number
| Default: 0

The distance in pixels from the content to the separator/border of the window
in the vertical axis

horizontal_padding
~~~~~~~~~~~~~~~~~~
| Type: number
| Default: 0

The distance in pixels from the content to the border of the window
in the horizontal axis

frame_width
~~~~~~~~~~~
| Type: number
| Default: 0

Defines width in pixels of frame around the notification window. Set to 0 to
disable.

frame_color
~~~~~~~~~~~
| Type: color
| Default: "#888888"

Defines color of the frame around the notification window. See COLORS.

separator_color
~~~~~~~~~~~~~~~
| Type: multi or color
| Default: "auto"
| Values: [auto/foreground/frame/#RRGGBB]

Sets the color of the separator line between two notifications.

- auto

  Dunst tries to find a color that fits the rest of the notification color
  scheme automatically.

- foreground

  The color will be set to the same as the foreground color of the topmost
  notification that's being separated.

- frame

  The color will be set to the frame color of the notification with the highest
  urgency between the 2 notifications that are being separated.

- Anything else

  Any other value is interpreted as a color, see colors
  .. TODO link to colors section

sort
~~~~
| Type: boolean
| Default: true

If set to true, display notifications with higher urgency above the others.

idle_threshold
~~~~~~~~~~~~~~
| Type: number
| Default: 0

Don't timeout notifications if user is idle longer than this time.
See TIME FORMAT for valid times.

Set to 0 to disable.

Transient notifications will ignore this setting and timeout anyway.
Use a rule overwriting with 'set_transient = no' to disable this behavior.

font
~~~~
| Type: string
| Default: "Monospace 8"

Defines the font or font set used. Optionally set the size as a decimal number
after the font name and space.
Multiple font options can be separated with commas.

This options is parsed as a Pango font description.

line_height
~~~~~~~~~~~
| Type: number
| Default: 0

The amount of extra spacing between text lines in pixels. Set to 0 to
disable.

markup
~~~~~~
| Type: multi
| Default: no
| Values: [full/strip/no]

Defines how markup in notifications is handled.

It's important to note that markup in the format option will be parsed
regardless of what this is set to.

Possible values:

- full

Allow a small subset of html markup in notifications

    <b>bold</b>
    <i>italic</i>
    <s>strikethrough</s>
    <u>underline</u>

For a complete reference see
<http://developer.gnome.org/pango/stable/PangoMarkupFormat.html>

- strip

  This setting is provided for compatibility with some broken
  clients that send markup even though it's not enabled on the
  server.

  Dunst will try to strip the markup but the parsing is simplistic so using this
  option outside of matching rules for specific applications **IS GREATLY
  DISCOURAGED**.

  See RULES

- no

  Disable markup parsing, incoming notifications will be treated as
  plain text. Dunst will not advertise that it can parse markup if this is set as
  a global setting.

format
~~~~~~
| Type: string
| Default: "%s %b"

Specifies how the various attributes of the notification should be formatted on
the notification window.

Regardless of the status of the markup_ setting, any markup tags that are
present in the format will be parsed. Note that because of that, if a literal
ampersand (&) is needed it needs to be escaped as '&amp;'

If '\n' is present anywhere in the format, it will be replaced with
a literal newline.

If any of the following strings are present, they will be replaced with the
equivalent notification attribute.

- %a  appname

- %s  summary

- %b  body

- %i  iconname (including its path)

- %I  iconname (without its path)

- %p  progress value ([  0%] to [100%])

- %n  progress value without any extra characters

- %%  Literal %

If any of these exists in the format but hasn't been specified in the
notification (e.g. no icon has been set), the placeholders will simply be
removed from the format.

alignment
~~~~~~~~~
| Type: multi
| Values: [left/center/right]
| Default: left

Defines how the text should be aligned within the notification.

show_age_threshold
~~~~~~~~~~~~~~~~~~
| Type: number
| Default: -1

Show age of message if message is older than this time.
See TIME FORMAT for valid times.

Set to -1 to disable.

word_wrap
~~~~~~~~~
| Type: boolean
| Default: false

Specifies how very long lines should be handled

If it's set to false, long lines will be truncated an ellipsised.

If it's set to true, long lines will be broken into multiple lines expanding
the notification window height as necessary for them to fit.

ellipsize
~~~~~~~~~
| Type: multi
| Values: [start/middle/end]
| Default: middle

If word_wrap is set to false, specifies where truncated lines should be
ellipsized.

ignore_newline
~~~~~~~~~~~~~~
| Type: boolean
| Default: false

If set to true, replace newline characters in notifications with whitespace.

stack_duplicates
~~~~~~~~~~~~~~~~
| Type: boolean
| Default: true

If set to true, duplicate notifications will be stacked together instead of
being displayed separately.

Two notifications are considered duplicate if the name of the program that sent
it, summary, body, icon and urgency are all identical.

hide_duplicates_count
~~~~~~~~~~~~~~~~~~~~~
| Type: boolean
| Default: false

Hide the count of stacked duplicate notifications.

show_indicators
~~~~~~~~~~~~~~~
| Type: boolean
| Default: true

Show an indicator if a notification contains actions and/or open-able URLs. See
ACTIONS below for further details.

icon_position
~~~~~~~~~~~~~
| Type: multi
| Values: [left/right/off]
| Default: off

Defines the position of the icon in the notification window. Setting it to off
disables icons.

max_icon_size
~~~~~~~~~~~~~
| Type: number
| Default: 0

Defines the maximum size in pixels for the icons.
If the icon is smaller than the specified value it won't be affected.
If it's larger then it will be scaled down so that the larger axis is equivalent
to the specified size.

Set to 0 to disable icon scaling. (default)

If icon_position_ is set to off, this setting is ignored.

icon_path
~~~~~~~~~
| Type: string
| Default: "/usr/share/icons/gnome/16x16/status/:/usr/share/icons/gnome/16x16/devices/"

Can be set to a colon-separated list of paths to search for icons to use with
notifications.

Dunst doesn't currently do any type of icon lookup outside of these
directories.

sticky_history
~~~~~~~~~~~~~~
| Type: boolean
| Default: true

If set to true, notifications that have been recalled from history will not
time out automatically.

history_length
~~~~~~~~~~~~~~
| Type: number
| Default: 20

Maximum number of notifications that will be kept in history. After that limit
is reached, older notifications will be deleted once a new one arrives. See
HISTORY.

dmenu
~~~~~
| Type: string
| Default: "/usr/bin/dmenu"

The command that will be run when opening the context menu. Should be either
a dmenu command or a dmenu-compatible menu.

browser
~~~~~~~
| Type: string
| Default: "/usr/bin/firefox"

The command that will be run when opening a URL. The URL to be opened will be
appended to the end of the value of this setting.

always_run_script
~~~~~~~~~~~~~~~~~
| Type: boolean
| Default: true

Always run rule-defined scripts, even if the notification is suppressed with
format = "". See SCRIPTING.

title
~~~~~
| Type: string
| Default: "Dunst"

Defines the title of notification windows spawned by dunst. (_NET_WM_NAME
property). There should be no need to modify this setting for regular use.

class
~~~~~
| Type: string
| Default: "Dunst"

Defines the class of notification windows spawned by dunst. (First part of
WM_CLASS). There should be no need to modify this setting for regular use.

startup_notification
~~~~~~~~~~~~~~~~~~~~
| Type: boolean
| Default: false

Display a notification on startup. This is usually used for debugging and there
shouldn't be any need to use this option.

force_xinerama
~~~~~~~~~~~~~~
| Type: boolean
| Default: false

Use the Xinerama extension instead of RandR for multi-monitor support. This
setting is provided for compatibility with older nVidia drivers that do not
support RandR and using it on systems that support RandR is highly discouraged.

By enabling this setting dunst will not be able to detect when a monitor is
connected or disconnected which might break follow mode if the screen layout
changes.

.. TODO Add colors section and more information about setting types and valid values
