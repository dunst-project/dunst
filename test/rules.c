#include "../src/rules.c"

#include "greatest.h"

extern const char *base;

// test filtering rules matching
TEST test_pattern_match(void) {
        // NULL should match everything
        ASSERT(rule_field_matches_string("anything", NULL));

        // Wildcard matching
        ASSERT(rule_field_matches_string("anything", "*"));
        ASSERT(rule_field_matches_string("*", "*"));
        ASSERT(rule_field_matches_string("", "*"));
        ASSERT(rule_field_matches_string("ffffasd", "*asd"));

        // Single character matching
        ASSERT(rule_field_matches_string("a", "?"));

        // special characters that are not special to fnmatch
        ASSERT(rule_field_matches_string("!", "!"));
        ASSERT(rule_field_matches_string("!asd", "!asd"));
        ASSERT(rule_field_matches_string("/as/d", "/as/d"));
        ASSERT(rule_field_matches_string("/as/d", "/as/d"));

        // Match substrings
        ASSERT(rule_field_matches_string("asd", "asd"));

        // ranges
        ASSERT(rule_field_matches_string("ac", "[a-z][a-z]"));

        // Non-matches
        ASSERT_FALSE(rule_field_matches_string("asd", "!asd"));
        ASSERT_FALSE(rule_field_matches_string("ffff", "*asd"));
        ASSERT_FALSE(rule_field_matches_string("ffff", "?"));
        ASSERT_FALSE(rule_field_matches_string("Ac", "[a-z][a-z]"));
        PASS();
}
SUITE(suite_rules) {
        RUN_TEST(test_pattern_match);
}
