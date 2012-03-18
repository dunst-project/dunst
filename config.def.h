#ifndef CONFIG_H
#define CONFIG_H

#include "dunst.h"


/* appearance */
const char *font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
/*
 * Background and forground colors for messages with
 * low normal and critical urgency.
 */
char *normbgcolor = "#1793D1";
char *normfgcolor = "#DDDDDD";
char *critbgcolor = "#ffaaaa";
char *critfgcolor = "#000000";
char *lowbgcolor =  "#aaaaff";
char *lowfgcolor = "#000000";
char *format = "%s %b"; /* default format */
int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */
char *geom = "0x3-30+20"; /* geometry */
int sort = True; /* sort messages by urgency */
int indicate_hidden = True; /* show count of hidden messages */
/* const char *geom = "x1"; */


char *key_string = NULL; /* set to NULL for no keybinging */
/* char *key_string = "space"; */
KeySym mask = 0;
/* KeySym mask = ControlMask; */
/* KeySym mask = ControlMask || Mod4Mask */

/* 0 -> print nothing
 * 1 -> print messages to stdout
 * 2 -> print everything to stdout (Useful for finding rules
 * 3 -> print everything above + debug info
 */
int verbosity = 0;

#endif
