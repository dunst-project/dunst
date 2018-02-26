#!/bin/bash

while IFS=: read type conv; do
	sed -i "s%OPTION(${conv}%OPTION((${type})%" settings.c
	sed -i "s%DEPREC(${conv}%DEPREC((${type})%" settings.c
	sed -i "s%RULE(${conv}%RULE((${type})%" settings.c
done <<-END
T_INT:string_parse_int
T_BOOL:string_parse_bool
T_TIME:string_to_time
T_PATH:string_to_path
T_ALIGN:parse_alignment
T_ICONPOS:parse_icon_position
T_STRING:g_strdup
T_LOGLEVEL:string_parse_loglevel
T_ELLIPSIZE:parse_ellipsize
T_FOLLOW:parse_follow_mode
T_MARKUP:parse_markup_mode
T_SEPCOLOR:parse_sepcolor
T_URGENCY:parse_urgency
T_FULLSCREEN:parse_enum_fullscreen
END
