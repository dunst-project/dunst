# Dunst changelog

## 1.3.0 - 2017-12-26

### Added
- `ellipsize` option to control how long lines should be ellipsized when `word_wrap` is set to `false` (#374)
- A beginning tilde of a path is now expanded to the home of the current user (#351)
- The image-path hint is now respected, as GApplications send their icon only via this link (#447)
- If dunst can't acquire the DBus name, dunst prints the PID of the process holding the name (#458 #460)

### Fixed
- `new_icon` rule being ignored on notifications that had a raw icon (#423)
- Do not replace format strings, which are in notification content (#322 #365)
- DBus related memory leaks closed: (#397)
  On the DBus connection, all hints never got freed. For raw icons, dunst saved them three times.
- Dunst crashed on X11 servers with RandR support less than 1.5. (#413 #364)
- Dunst silently read the default config file, if -conf did not specify a valid file (#452)

## Changed
- transient hints are now handled (#343 #310)
  An additional rule option (`match_transient` and `set_transient`) is added
  to optionally reset the transient setting
- Timeouts are now calculated in microseconds with GLib's `g_get_monotonic_time()` (#379 #291)
- `icon_folders` is renamed to `icon_path` (#170)
- `config.def.h` and `config.h` got merged (#371)
- The dependency on GTK3+ had been removed. Instead of GTK3+, dunst requires now gdk-pixbuf,
  which had been a transient dependency before. (#334 #376)
- The `_GNU_SOURCE` macros had been removed to make dunst portable to nonGNU systems (#403)
- Replacing notifications does not require a full redraw and replaces flickerless (#415)
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
