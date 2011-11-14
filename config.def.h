#ifndef CONFIG_H
#define CONFIG_H

#include "dunst.h"


/* appearance */
const char *font = NULL;
const char *normbgcolor = "#1793D1";
const char *normfgcolor = "#DDDDDD";
const char *critbgcolor = "#ffaaaa";
const char *critfgcolor = "#000000";
const char *lowbgcolor =  "#aaaaff";
const char *lowfgcolor = "#000000";
const char *format = "%a-->%s %b"; /* default format */
int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */
const char *geom = "0x4-10+10"; /* geometry */


int verbose = True; /* print info to stdout? */


const rule_t rules[] = {
    /* appname,       summary,         body,  icon,  timeout,  urgency,  fg,    bg */
    { "notify-send",  NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL },
    { "Pidgin",       NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL },
    { "Pidgin",       "*signed on*",   NULL,  NULL,  -1,       LOW,      NULL,  NULL },
    { "Pidgin",       "*signed off*",  NULL,  NULL,  -1,       LOW,      NULL,  NULL },
    { "Pidgin",       "*says*",        NULL,  NULL,  -1,       CRIT,     NULL,  NULL },
    { "Pidgin",       "twitter.com*",  NULL,  NULL,  -1,       NORM,     NULL,  NULL },
};
 


#endif
