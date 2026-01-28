#include "../src/rules.c"

#include "greatest.h"
#include <regex.h>

extern const char *base;

// test filtering rules matching
TEST test_pattern_match(void) {
        // NULL should match everything
        ASSERT(rule_field_matches_string("anything", NULL));

        // Literal matches
        ASSERT(rule_field_matches_string("asdf", "asdf"));
        ASSERT(rule_field_matches_string("test123", "test123"));

        ASSERT(rule_field_matches_string("!", "!"));
        ASSERT(rule_field_matches_string("!asd", "!asd"));
        ASSERT(rule_field_matches_string("/as/d", "/as/d"));
        ASSERT(rule_field_matches_string("/as/d", "/as/d"));

        // ranges
        ASSERT(rule_field_matches_string("ac", "[a-z][a-z]"));

        // Non-matches
        ASSERT_FALSE(rule_field_matches_string("asd", "!asd"));
        ASSERT_FALSE(rule_field_matches_string("ffff", "*asd"));
        ASSERT_FALSE(rule_field_matches_string("ffff", "?"));
        ASSERT_FALSE(rule_field_matches_string("Ac", "[a-z][a-z]"));

        // This is checked before fnmatch/regex kicks in
        ASSERT(rule_field_matches_string("asd", ""));

        // Things that differ between fnmatch(3) and regex(3)

        if (settings.enable_pcre || settings.enable_regex) {
                // Single character matching
                ASSERT(rule_field_matches_string("a", "."));

                // Wildcard matching
                ASSERT(rule_field_matches_string("anything", ".*"));
                ASSERT(rule_field_matches_string("*", ".*"));
                ASSERT(rule_field_matches_string("", ".*"));
                ASSERT(rule_field_matches_string("ffffasd", ".*asd"));

                // Substring matching
                ASSERT(rule_field_matches_string("asd", "sd"));
                ASSERT(rule_field_matches_string("asd", "a"));
                ASSERT(rule_field_matches_string("asd", "d"));
                ASSERT(rule_field_matches_string("asd", "asd"));

                // Match multiple strings
                ASSERT(rule_field_matches_string("ghj", "asd|dfg|ghj"));
                ASSERT(rule_field_matches_string("asd", "asd|dfg|ghj"));
                ASSERT(rule_field_matches_string("dfg", "asd|dfg|ghj"));
                ASSERT_FALSE(rule_field_matches_string("azd", "asd|dfg|ghj"));

                // Special characters
                // Notice the disagreement between PCRE and ERE!
                ASSERT_EQ(rule_field_matches_string("{", "{"), settings.enable_pcre);
                ASSERT(rule_field_matches_string("{", "\\{"));
                ASSERT(rule_field_matches_string("a", "(a)"));
        } else {
                // Single character matching
                ASSERT(rule_field_matches_string("a", "?"));

                // Wildcard matching
                ASSERT(rule_field_matches_string("anything", "*"));
                ASSERT(rule_field_matches_string("*", "*"));
                ASSERT(rule_field_matches_string("", "*"));
                ASSERT(rule_field_matches_string("ffffasd", "*asd"));

                // Substring matching
                ASSERT_FALSE(rule_field_matches_string("asd", "sd"));
                ASSERT_FALSE(rule_field_matches_string("asd", "a"));
                ASSERT_FALSE(rule_field_matches_string("asd", "d"));
                ASSERT(rule_field_matches_string("asd", "asd"));
        }
        PASS();
}

SUITE(suite_rules) {
        bool store = settings.enable_regex;

        /*
         * Test fnmatch
         */
        settings.enable_regex = false;
        RUN_TEST(test_pattern_match);

        /*
         * Test Posix regex
         */
        settings.enable_regex = true;
        RUN_TEST(test_pattern_match);

        settings.enable_regex = store;

        /*
         * Test PCRE regex
         */
        store = settings.enable_pcre;

        settings.enable_pcre = true;
        RUN_TEST(test_pattern_match);

        settings.enable_pcre = store;
}
