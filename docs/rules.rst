=====
Rules
=====

Purpose of rules
================

Rules are available to modify single incoming notifications, which is dunst's main feature.

You can change almost everything of a notification by a rule.



Adding rules
============

Rules are defined in their own section in the configuration file. It's possible to have multiple matching rules per notification. All of the matching rules get applied to a notification.

Rules must not get named ``global``, ``frame``, ``experimental``, ``shortcuts``, ``urgency_low``, ``urgency_normal`` or ``urgency_critical`` as these are the sections for normal configuration options.


Rules possibilites
==================

.. TODO: add missing options like desktop_entry

Filters
~~~~~~~

appname
--------------------

summary
--------------------

body
--------------------

icon
--------------------

category
--------------------

match_transient
--------------------

msg_urgency
--------------------

Modifiers
~~~~~~~~~

background
--------------------

foreground
--------------------

format
--------------------

history_ignore
--------------------

markup
--------------------

new_icon
--------------------

script
--------------------

set_transient
--------------------

timeout
--------------------

urgency
--------------------
