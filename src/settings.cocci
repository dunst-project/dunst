//compile with 
// spatch --sp-file settings.cocci -o settings.patch settings.c --smpl-spacing
//
// and settings.patch contains the fully patched settings.c file

@ rule_string @
identifier r, feld, type;
expression sec, key, def;
@@
-                r->feld = type(ini_get_string(sec, key, def));
+                RULE(type, feld, key);

@ rule_converter @
identifier r, feld, type;
expression sec, key, def;
@@
-                r->feld = type(ini_get_string(sec, key, NULL), def);
+                RULE(type, feld, key);

@ settingsconverter_deprecated @
identifier settings, feld, type, LOG_M;
expression sec, key, cmd, def1, desc, def, message, test;
@@
-if(test){
-        settings.feld = type(option_get_string(sec, key, cmd, def1, desc), def);
-        LOG_M(message);
-}
+        DEPREC(type, feld, def,
+               sec, key,
+               cmd, desc,
+               message);

@ settingsconverter_deprecated_string @
identifier settings, feld, type, LOG_M;
expression sec, key, cmd, def, desc, message, test;
@@
-if(test){
-        settings.feld = type(option_get_string(sec, key, cmd, def, desc));
-        LOG_M(message);
-}
+        DEPREC(type, feld, def,
+               sec, key,
+               cmd, desc,
+               message);

@ settingsconverter @
identifier settings, feld, type;
expression sec, key, cmd, def1, desc, def;
@@
-        settings.feld = type(option_get_string(sec, key, cmd, def1, desc), def);
+        OPTION(type, feld, def,
+               sec, key,
+               cmd, desc);

@ settingsconverter_path_and_path @
identifier settings, feld, type;
expression sec, key, cmd, def, desc;
@@
-        settings.feld = type(option_get_string(sec, key, cmd, def, desc));
+        OPTION(type, feld, def,
+               sec, key,
+               cmd, desc);

@ settingsconverter_keystroke @
identifier settings, feld, feld2, type;
expression sec, key, cmd, def, desc;
@@
-        settings.feld.feld2 = type(option_get_string(sec, key, cmd, def, desc));
+        OPTION(type, feld.feld2, def,
+               sec, key,
+               cmd, desc);

@ settingsconverter_timeout @
identifier settings, timeout, urgency, type;
expression sec, key, cmd, def, desc, def2;
@@
-        settings.timeout[urgency] = type(option_get_string(sec, key, cmd, def, desc), def2);
+        OPTION(type, timeout[urgency], def2,
+               sec, key,
+               cmd, desc);

@ settingsconverter_urgency @
identifier settings, icons, urgency;
expression sec, key, cmd, def, desc;
@@
-        settings.icons[urgency] = g_strdup(option_get_string(sec, key, cmd, def, desc));
+        OPTION(g_strdup, icons[urgency], def,
+               sec, key,
+               cmd, desc);

@ settingsconverter_cmdline @
identifier settings, feld, type;
expression cmd, def, desc, def2;
@@
-        settings.feld = type(cmdline_get_string(cmd, def, desc), def2);
+        OPTION(type, feld, def2,
+               NULL, NULL,
+               cmd, desc);
