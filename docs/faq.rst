==========================
Frequently asked questions
==========================

Why are my notifications prefixed with an ``(A)``?
==================================================

The notification has got an attached action. You can activate the notification. For example in chat applications, this activates the chat window of the corresponding chat.

If you want to get rid of it, set the ``show_indicators`` option to ``no``.

Why are my notifications prefixed with an ``(U)``?
==================================================

The notification text contains an URL. You can activate the notification and the browser will get opened with the URL.

If you want to get rid of it, set the ``show_indicators`` option to ``no``.

How do I restart dunst?
=======================

Simply restart dunst by using ``killall dunst``. DBus automatically starts dunst again with the next incoming notification.

How do I reload dunst?
======================

A configuration reload is currently not supported in dunst. Use the restart method to reload your configuration.

Where is the configuration file located?
========================================

.. TODO: use context variable in link: {{ github_version }}
.. TODO:  it may not be available yet https://docs.readthedocs.io/en/latest/design/theme-context.html#read-the-docs-data-passed-to-sphinx-build-context

The file has to be in :file:`~/.config/dunst/dunstrc`. See the `dunst configuration file <https://github.com/dunst-project/dunst/blob/master/dunstrc>`_.
