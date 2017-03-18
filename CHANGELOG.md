# Dunst changelog

## Unreleased

### Added
- `always_run_script` option to run script even if a notification is suppressed
- Support for svg icon files
- Support for raw icons
- `hide_duplicate_count` option to hide the number of duplicate notifications
- Support for per-urgency frame colours
- `markup` setting for more fine-grained control over how markup is handled
- `history_ignore` rule action to exclude a notification from being added to the history

### Changed
- Text and icons are now centred vertically
- Notifications aren't considered duplicate if urgency or icons differ

### Deprecated
- `allow_markup` will be removed in later versions. It is being replaced by `markup`

### Fixed
- Infinite loop if there are 2 configuration file sections with the same name
- URLs with dashes and underscores in them are now parsed properly
- Many memory leaks
- Category based rules were applied without actually matching
- dmenu command not parsing quoted arguments correctly
- Icon alignment with dynamic width
- Loading each line of the configuration file no longer relies on a static size buffer
- `ignore_newline` now works regardless of the markup setting
- dmenu process being left as a zombie if no option was selected

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
