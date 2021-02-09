/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTING_DATA_H
#define DUNST_SETTING_DATA_H

// TODO move all enum definitions here and remove include
#include "settings.h"
#include "option_parser.h"


struct string_to_enum_def {
        const char* string;
        const int enum_value;
};

struct setting {
      /**
       * A string with the setting key as found in the config file.
       */
      char *name;

      /**
       * A string with the ini section where the variable is allowed. This
       * section should be part of the special_sections array.
       *
       * Example:
       *        .section = "global",
       */
      char *section;

      /**
       * A string with a short description of the config variable. This is
       * currently not used, but it may be used to generate help messages.
       */
      char *description;

      // IDEA: Add long description to generate man page from this. This could
      // also be useful for an extended help text.

      /**
       * Enum of the setting type. Every setting type is parsed differently in
       * option_parser.c.
       */
      enum setting_type type;

      /**
       * A string with the default value of the setting. This should be the
       * same as what it would be in the config file, as this is parsed by the
       * same parser.
       *
       * Example:
       *        .default_value = "10s", // 10 seconds of time
       */
      char *default_value;

      /**
       * A pointer to the corresponding setting in the setting struct. Make
       * sure to always take the address, even if it's already a pointer in the
       * settings struct.
       *
       * Example:
       *        .value = &settings.font,
       */
      void *value;


      /**
       * (nullable)
       * Function pointer for the parser - to be used in case of enums or other
       * special settings. If the parse requires extra data, it should be given
       * with parser_data. This allows for one generic parser for, for example,
       * enums, instead of a parser for every enum.
       *
       * @param data The required data for parsing the value. See parser_data.
       * @param cfg_value The string representing the value of the config
       * variable
       * @param ret A pointer to the return value. This casted by the parser to
       * the right type.
       */
      int (*parser)(void* data, const char *cfg_value, void* ret);

      /**
       * (nullable)
       * A pointer to the data required for the parser to parse this setting.
       */
      void* parser_data; // This is passed to the parser function
};

static struct setting allowed_settings[] = {
        {
                .name = "frame_color",
                .section = "global",
                .description = "Color of the frame around the window",
                .type = TYPE_STRING,
                .default_value = "#888888",
                .value = &settings.frame_color,
                .parser = NULL,
                .parser_data = NULL,
        },
};

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
