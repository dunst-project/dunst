==========================
Frequently asked questions
==========================

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
