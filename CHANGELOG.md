# Dunst changelog

## 1.9.0 -- 2022-06-27

### Added
- `override_dbus_timeout` setting to override the notification timeout set via
  dbus. (#1035)
- Support notification gaps via the `gap_size` setting. Note that since the
  notifications are not separate windows, you cannot click in between the
  notifications. (#1053)
- Make `min_icon_size` and `max_icon_size` a rule for even more flexibility
  (#1069)

### Changed
- The window offset is now scaled according to `scale` as well. This way
  notification stay visually in the same place on higher DPI screens. (#1039)
- For the recursive icon lookup, revert to using `min_icon_size` and
  `max_icon_size` instead of `icon_size`. `min_icon_size` is used as the size to
  look for in icon themes. This way of defining icon size is more flexible and
  compatible with the old icon lookup. The new icon lookup should now be
  superior for all use cases. (#1069)
- Recursive icon lookup is no longer experimental.
- Recursive icon lookup is enabled in the default dunstrc. This does not change
  your settings when you have a custom dunstrc.

### Fixed
- Added back the `action_name` setting that was accidentally dropped. (#1051)
- Broken `dunstctl history`. (#1060)
- Merged a few wayland fixes from mako (https://github.com/emersion/mako)
  (#1067)
- `follow=keyboard`: Fix regression where we don't fall back to mouse (#1062)
- Raw icons not being scaled according to icon size (#1043)
- Notifications not disappearing. For some people notifications would sometimes
  stay on screen until a new notification appeared. This should not happen
  anymore (#1073).

## 1.8.1 -- 2022-03-02

### Fixed
- Dunst sometimes not using the right config file, sometimes falling back to the
  internal defaults.

## 1.8.0 -- 2022-02-24

### Added
- Implemented `progress_bar_min_width`. Before it was an unused setting. (#1006)
- `progress_bar_horizontal_alignment` for changing the alignment of the progress
  bar. (#1021)
- Support for config drop-ins. You can add as many configuration files as you
  want in `dunstrc.d`. See the man page dunst(1) for more information. This was
  done with help from @WhitePeter. (#997)
- Thanks to @m-barlett you can place your icons at the center of your
  notifications with `icon_position = top`.
- `icon_position` is now a rule (also by @m-barlett).
- `hide_text` for hiding all text of a notification. This also removes all
  padding that would be present for a notification without text. (also by
  @m-barlett) (#985)
- The previously removed keyboard shortcuts have been added again, but now they
  are in the `[global]` section of the config. Not everything that was possible
  with the keyboard shortcuts was possible with dunstctl on X11. Mainly
  activating a keyboard shortcut only when notifications are on screen. Thanks
  to @wgmayer0 for testing. (#1033).

### Changed
- Improved the man page regarding transitioning from the old geometry.
- The default alignment of the progress bar is now center instead of left.
- Better regex matching for rules. When you set `enable_posix_regex`. Take a
  look at
  https://en.m.wikibooks.org/wiki/Regular_Expressions/POSIX-Extended_Regular_Expressions
  for how the new regex syntax works. Note that you cannot do inverse matching
  yet, I'm working on that in #1040. (#1017)
- Thanks to @kurogetsusai you can once again use negative offsets to put a
  notification window slightly off-screen if you so like. (#1027)
- As mentioned above, the keyboard shortcuts have been moved to the `[global]`
  section. Please move your settings there.


### Fixed
- Crash when `open_url` was used without URL's. (#1000)
- Icons sometimes being incorrectly sized with the new icon lookup. (#1003)
- Incorrect defaults mentioned in the documentation. (#1004, #1029 and more)
- Crash when icon could not be read by glib. (#1023)
- Not being able to override anymore raw icons with `new_icon` (#1009)
- High cpu usage when selecting an action in dmenu or similar. This was caused
  by dunst not going to sleep when waiting for a response. (#898)
- Updated default values documentation (with help from @profpatch) (#1004 and
  more)


## 1.7.3 -- 2021-12-08

### Added
### Changed
- `follow` is now `none` again by default. This was the case before v1.7.0 as well. (#990).

### Fixed
- `dunstctl action` is now working again.
- Segfault in experimental icon lookup when an inherited theme doesn't exist.
- `icon_position = off` not being respected (#996).

## 1.7.2 -- 2021-11-30

### Added
- Experimental recursive icon lookup. This is not enabled by default and can be
  enabled by setting `enable_recursive_icon_lookup=true`. Setting icon sizes
  still doesn't work entirely as it's supposed to and will be improved in future
  releases. (#965)
- You can now enable or disable rules on the fly with `dunstctl rule $name$
  enable/disable`. (#981)
- `dunstctl history` lists your notification history in JSON format for
  processing by scripts. (#970)
- You can now pop specific notifications from history by passing a notification
  ID to `dunstctl history-pop`. (#970)
- `default_icon` setting for setting the icon when no icons are given (#984)
- Implemented display size detection in Wayland. (#973)
### Changed
### Fixed
- Text being cut off on X11 when using fractional scaling. (#975)
- Incorrect hitbox for notification on X11 with scaling. (#980)
- Improved warning messages for deprecated sections. (#974)
- `icon` being interpreted as a filter and not being allowed in the special
  urgency sections. This is a compatibility fix, but it's recommended to replace
  all usages of `icon` in these sections with `default_icon` to prevent
  confusion with the `icon` rule in other sections. (#984)
- `new_icon` being used in the default dunstrc where `default_icon` is the
  intended settings. This was commented by default, so it doesn't affect any
  default behaviour. (#984)
- Notifications bleeding to other screens when the width was big enough. Now the
  notification's width is lowered when it would otherwise leave the display.


## 1.7.1 -- 2021-11-01

### Added
- Script environment variable `DUNST_DESKTOP_ENTRY`. (#874)
- Rule `set_category` for change a notifications category with rules. (1b72b2a)

### Fixed
- Dunst not building with WAYLAND=0. (#938)
- Wrong icon being shown in chromium-based browsers. (#939)
- `set_stack_tag` not working anymore. (#942)
- Outdated documentation. (#943, #944 and more)
- Empty strings not being allowed in settings. (#946)
- Dunst crashing when compositor doesn't support `zwlr_foreign_toplevel_v1`. (#948)
- Xmore notifications showing a progress bar and icon. (#915)
- Markup is now a rule again. Before this was undocumented behaviour. (#955)
- Double free when setting `XDG_CONFIG_DIR`. (#957)
- Dunst crashing on some compositors. (#948)
- Dunst not exiting when wayland compositor quits. (#961)
- Now the separators are not responsive to mouse clicks anymore. (#960)
- Mouse action stopping the rest of the actions. (bf58928)

## 1.7.0 -- 2021-10-19:

### Added
- `context` and `context_all` mouse actions for opening the context menu (#848)
- `open_url` mouse action for opening url's in a notification (#848)
- `action_name` rule for setting a default action to perform when using
  `do_action` (#848)
- HiDPI support for both Wayland and X11. On wayland the scale can be set from
  your compositor's settings and is automatically picked up by dunst. On X11
  dunst will guess the scale based on the DPI of the screen. If that isn't good,
  you can set the `scale` variable in the settings. (#854 and #890)
- `highlight` can now also be set through dbus hints with the key `hlcolor`
  (#862)
- Your dunstrc is now being checked by dunst. Dunst will print a warning when
  coming across an non-existing/invalid setting. (#803)
- Wayland fullscreen detection (#814)
- Wayland touch support (#814)
- Cursor is now being changed to `left_ptr` when hovering over dunst (Wayland)
  (#903)

### Changed
- `startup_notification` and `verbosity` are now only available as a command
  line arguments. (#803)
- Rule settings can now also be used in the `[global]` section. They will then
  apply to all the notifications. (#803)
- `fullscreen`, `ellpsize` and `word_wrap` are now rules. They can still be used
  in the `[global]` section as well (see above). (#937 and #803)
- The appid's now also need to match when stacking notifications. (#886)
- `xdg-open` is now being used by default for opening URL's. (#889)
- `geometry` and `notification_height` have been replaced by `origin`, `width`,
  `height`, `offset` and `notification_limit`. This allows for more flexible
  geometry settings. (#855)
- There were a bunch of changes in the installation and default locations. See
  the release notes for more information.
- Upon seeing invalid markup, dunst is a bit smarter in stripping the markup.

### Fixed
- Lots of debug messages when `idle_timeout=0` (#814)
- `follow=none` not working on Wayland (#814)
- Incorrect sorting when `sort` is false
- NULL pointer dereference on Wayland
- Dunst not redrawing after `close_all` action.
- Dunst not announcing icon-static capability over dbus (#867)
- Dunst not falling back to X11 output when it can't initialize the Wayland
  output. (#834)
- Improve stability on Wayland. (#930 and more)

### Removed
- The `[shortcuts]` section with all it's settings. Use your WM/DE's shortcut
  manager and `dunstctl` to replace it. (#803)
- Setting settings via command line arguments. (#803)
- Setting settings via `config.h`. (#803)

## 1.6.1 - 2021-02-21:

### Fixed
- Incorrect version in Makefile

## 1.6.0 - 2021-02-21:

### Added
- Wayland support. Dunst now runs natively on wayland. This fixes several bugs
  with dunst on wayland and allows idle detection. (#781)
- A progress bar, useful for showing volume or brightness in notifications (#775)
- A script in contrib for using the progress bar (#791)
- `dunstctl count` for showing the number of notifications (#793)
- Expose environment variables info about the notification to scripts (#802)
- `text_icon_padding` for adding padding between the notification icon and text
  (#810)

### Changed
- Dunst now installs a system-wide config in `/etc/dunst/dunstrc` (#798)
- Move part of the man page to dunst(5) (#799)

### Fixed
- `history_ignore` flag broken when using multiple rules (#747)
- Divide by zero in radius calculation (#750)
- Monitor setting overriding `follow_mode` (#755)
- Incorrect monitor usage when using multiple X11 screens (#762)
- Emit signal when `paused` property changes (#766)
- `dunstify` can pass empty appname to libnotify (#768)
- Incorrect handling of 'do_action, close' mouse action (#778)

# Removed

- `DUNST_COMMAND_{PAUSE,RESUME,TOGGLE}` (#830)

## 1.5.0 - 2020-07-23

### Added
- `min_icon_size` option to automatically scale up icons to a desired value (#674)
- `vertical_alignment` option to control the text/icon alignment within the notification (#684)
- Ability to configure multiple actions for each mouse event (#705)
- `dunstctl` command line control client (#651)
- RGBA support for all color strings (#717)
- Ability to run multiple scripts for each notification
- `ignore_dbusclose` setting (#732)

### Changed
- `dunstify` notification client is now installed by default (#701)
- Keyboard follow mode falls back to the monitor with the mouse if no window has keyboard focus (#708)

### Fixed
- Overflow when setting a >=40 minute timeout (#646)
- Unset configuration options not falling back to default values (#649)
- Crash when `$HOME` environment variable is unset (#693)
- Lack of antialiasing with round corners enabled (#713)

## 1.4.1 - 2019-07-03

### Fixed

- `max_icon_size` not working with dynamic width (#614)
- Failure to parse color strings with trailing comments in the config (#626)
- Negative width in geometry being ignored (#628)
- Incorrect handling of the argument terminator `--` in dunstify
- Crash when changing DPI while no notifications are displayed (#630)
- Fullscreen status change not being detected in some cases (#613)

## 1.4.0 - 2019-03-30

### Added

- Add support to override `frame_color` via rules (#498)
- Support for round corners (#420)
- Ability to reference `$HOME` in icon paths with `~/` (#520)
- Support to customize the mouse bindings (#530)
- Command to toggle pause status (#535)
- Ability to automatically replace similar notifications (like volume changes)
  via `stack_tag` (#552)
- Comparison of raw icons for duplicate notifications (#571)
- Introduce new desktop-entry filter (#470)
- `fullscreen` rule to hide notifications when a fullscreen window is active (#472)
- Added `skip_display` rule option to skip initial notification display, and
  include the notification in the history. (#590)

### Fixed

- Notification age not counting the time while the computer was suspended (#492)
- Dunst losing always-on-top status on a window manager restart (#160)
- Xpm icons not being recognized
- When new notifications arrive, but display is full, important notifications don't
  have to wait for a timeout in a displayed notification (#541)
- Dunst hanging while the context menu is open (#456)
- Having & inside a notification breaking markup (#546)
- `<I> more` notifications don't occupy space anymore, if there is only a single
  notification waiting to get displayed. The notification gets displayed directly (#467)
- Segfault when comparing icon name with a notification with a raw icon (#536)
- Icon size can no longer be larger than the notification when a fixed width is specified (#540)

### Changed

- Transient notifications no longer skip history by default (#508)
- The notification summary no longer accepts markup (#497)

### Removed

- Dependency on libxdg-basedir (#550)

## 1.3.2 - 2018-05-06

### Fixed

- Crash when trying to load an invalid or corrupt icon (#512)

## 1.3.1 - 2018-01-30

### Fixed

- Race condition resulting in the service files being empty (#488)

## 1.3.0 - 2018-01-05

### Added
- `ellipsize` option to control how long lines should be ellipsized when `word_wrap` is set to `false` (#374)
- A beginning tilde of a path is now expanded to the home of the current user (#351)
- The image-path hint is now respected, as GApplications send their icon only via this link (#447)
- The (legacy) image\_data hint is now respected (#353)
- If dunst can't acquire the DBus name, dunst prints the PID of the process holding the name (#458 #460)
- Increased accuracy of timeouts by using microseconds internally (#379 #291)
- Support for specifying timeout values in milliseconds, minutes, hours, or days. (#379)
- Support for HTML img tags (via context menu) (#428)

### Fixed
- `new_icon` rule being ignored on notifications that had a raw icon (#423)
- Format strings being replaced recursively in some cases (#322 #365)
- DBus related memory leaks (#397)
- Crash on X11 servers with RandR support less than 1.5. (#413 #364)
- Silently reading the default config file, if `-conf` did not specify a valid file (#452)
- Notification window flickering when a notification is replaced (#320 #415)
- Inaccurate timeout in some cases (#291 #379)

### Changed
- Transient hints are now handled (#343 #310)
  An additional rule option (`match_transient` and `set_transient`) is added
  to optionally reset the transient setting
- HTML links are now referred to by their text in the context menu rather than numbers (#428)
- `icon_folders` setting renamed to `icon_path` (#170)
- `config.def.h` and `config.h` got merged (#371)
- The dependency on GTK3+ has been removed. Instead of GTK3+, dunst now
  requires gdk-pixbuf which had been a transient dependency before. (#334
  #376)
- The `_GNU_SOURCE` macros had been removed to make dunst portable to nonGNU systems (#403)
- Internal refactorings of the notification queue handling. (#411)
- Dunst does now install the systemd and dbus service files into their proper location given
  by pkg-config. Use `SERVICEDIR_(DBUS|SYSTEMD)` params to overwrite them. (#463)

## 1.2.0 - 2017-07-12

### Added
- `always_run_script` option to run script even if a notification is suppressed
- Support for more icon file types
- Support for raw icons
- `hide_duplicate_count` option to hide the number of duplicate notifications
- Support for per-urgency frame colours
- `markup` setting for more fine-grained control over how markup is handled
- `history_ignore` rule action to exclude a notification from being added to the history
- Support for setting the dpi value dunst will use for font rendering via the `Xft.dpi` X resource
- Experimental support for per-monitor dpi calculation
- `max_icon_size` option to scale down icons if they exceed a certain size
- Middle click on notifications can be used to trigger actions
- Systemd service file, installed by default
- `%n` format flag for getting progress value without any extra characters

### Changed
- Text and icons are now centred vertically
- Notifications aren't considered duplicate if urgency or icons differ
- The maximum length of a notification is limited to 5000 characters
- The frame width and color settings were moved to the global section as `frame_width` and `frame_color` respectively
- Dropped Xinerama in favour of RandR, Xinerama can be enabled with the `-force_xinerama` option if needed

### Deprecated
- `allow_markup` is deprecated with `markup` as its replacement
- The urgency specific command line flags have been deprecated with no replacement, respond to issue #328 on the bug tracker if you depend on them

### Fixed
- Infinite loop if there are 2 configuration file sections with the same name
- URLs with dashes and underscores in them are now parsed properly
- Many memory leaks
- Category based rules were applied without actually matching
- dmenu command not parsing quoted arguments correctly
- Icon alignment with dynamic width
- Issue when loading configuration files with very long lines
- '\n' is no longer expanded to a newline inside notification text
- Notification window wasn't redrawn if obscured on systems without a compositor
- `ignore_newline` now works regardless of the markup setting
- dmenu process being left as a zombie if no option was selected
- Crash when opening urls parsed from `<a href="">` tags

## 1.1.0 - 2014-07-29
- fix nasty memory leak
- icon support (still work in progress)
- fix issue where keybindings aren't working when numlock is activated

## 1.0.0 - 2013-04-15
- use pango/cairo as drawing backend
- make use of pangos ability to parse markup
- support for actions via context menu
- indicator for actions/urls found
- use blocking I/O. No more waking up the CPU multiple times per second to check for new dbus messages

## 0.5.0 - 2013-01-26
- new default dunstrc
- frames for window
- trigger scripts on matching notifications
- context menu for urls (using dmenu)
- pause and resume function
- use own code for ini parsing (this removes inih)
- progress hints

## 0.4.0 - 2012-09-27
- separator between notifications
- word wrap long lines
- real transparance
- bouncing text (alternative to word_wrap)
- new option for line height
- better multihead support
- don't die when keybindings can't be grabbed
- bugfix: forgetting geometry
- (optional) static configuration

## 0.3.1 - 2012-08-02
- fix -mon option

## 0.3.0 - 2012-07-30
- full support for Desktop Notification Specification (mandatory parts)
- option to select monitor on which notifications are shown
- follow focus
- oneline mode
- text alignment
- show age of notifications
- sticky history
- filter duplicate messages
- keybinding to close all notifications
- new way to specify keybindings
- cleanup / bugfixes etc.
- added dunst.service

## 0.2.0 - 2012-06-26
- introduction of dunstrc
- removed static configuration via config.h
- don't timeout when user is idle
- xft-support
- history (a.k.a. redisplay old notifications)
