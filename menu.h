#include "dunst.h"
#include <regex.h>

char *extract_urls(const char *to_match);
void open_browser(const char *url);
void invoke_action(const char *action);
