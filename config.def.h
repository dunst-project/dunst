#ifndef CONFIG_H
#define CONFIG_H

#include "dunst.h"


/* appearance */
const char *font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
/*
 * Background and forground colors for messages with
 * low normal and critical urgency.
 */
const char *normbgcolor = "#1793D1";
const char *normfgcolor = "#DDDDDD";
const char *critbgcolor = "#ffaaaa";
const char *critfgcolor = "#000000";
const char *lowbgcolor =  "#aaaaff";
const char *lowfgcolor = "#000000";
const char *format = "%a-->%s %b"; /* default format */
int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */
const char *geom = "0x4-10+10"; /* geometry */


char *key_string = "space"; /* set to NULL for no keybinging */
KeySym mask = ControlMask;
/* KeySym mask = ControlMask || Mod4Mask */

/* 0 -> print nothing
 * 1 -> print messages to stdout
 * 2 -> print everything to stdout (Useful for finding rules
 * 3 -> print everything above + debug info
 */
int verbosity = 0;

/* You can use shell-like wildcards to match <appname> <summary> <body> and <icon>. */
const rule_t rules[] = {
    /* appname,       summary,         body,  icon,  timeout,  urgency,  fg,    bg, format */
    { "notify-send",  NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL, "%s %b" },
    { "Pidgin",       NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL, "%s %b" },
    { "Pidgin",       "*signed on*",   NULL,  NULL,  -1,       LOW,      NULL,  NULL, "%s %b" },
    { "Pidgin",       "*signed off*",  NULL,  NULL,  -1,       LOW,      NULL,  NULL, "%s %b" },
    { "Pidgin",       "*says*",        NULL,  NULL,  -1,       CRIT,     NULL,  NULL, "%s %b" },
    { "Pidgin",       "twitter.com*",  NULL,  NULL,  -1,       NORM,     NULL,  NULL, "%s %b" },
};



#endif
