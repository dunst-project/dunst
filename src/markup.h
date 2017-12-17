/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MARKUP_H
#define DUNST_MARKUP_H

#include "settings.h"

char *markup_strip(char *str);

void markup_strip_a(char **str, char **urls);
void markup_strip_img(char **str, char **urls);

char *markup_transform(char *str, enum markup_mode markup_mode);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */
