#include "../src/settings_data.h"
#include "greatest.h"

extern const char *base;

// TODO check enums on NULL-termination

TEST test_names_valid(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Setting name is null (setting description is \"%s\")", allowed_settings[i].description);
                gchar *error2 = g_strdup_printf("Setting name is empty (setting description is \"%s\")", allowed_settings[i].description);
                ASSERTm(error1, allowed_settings[i].name);
                ASSERTm(error2, strlen(allowed_settings[i].name));
                g_free(error1);
                g_free(error2);
        }
        PASS();
}

TEST test_description_valid(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Description of setting %s is null", allowed_settings[i].name);
                gchar *error2 = g_strdup_printf("Description of setting %s is empty", allowed_settings[i].name);
                ASSERTm(error1, allowed_settings[i].description);
                ASSERTm(error2, strlen(allowed_settings[i].description));
                g_free(error1);
                g_free(error2);
        }
        PASS();
}

#define BETWEEN(arg, low, high) (((arg) > (low) ) && ((arg) < (high)))

TEST test_type_valid(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Type of setting %s is not valid: %i", allowed_settings[i].name, allowed_settings[i].type);
                ASSERTm(error1, BETWEEN(allowed_settings[i].type, TYPE_MIN, TYPE_MAX));
                g_free(error1);
        }
        PASS();
}

TEST test_section_valid(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Section of setting %s is null", allowed_settings[i].name);
                gchar *error2 = g_strdup_printf("Section of setting %s is empty", allowed_settings[i].name);
                ASSERTm(error1, allowed_settings[i].section);
                ASSERTm(error2, strlen(allowed_settings[i].section));
                g_free(error1);
                g_free(error2);
        }
        PASS();
}

TEST test_default_value_valid(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Default_value of setting %s is null", allowed_settings[i].name);
                gchar *error2 = g_strdup_printf("Default_value of setting %s is empty", allowed_settings[i].name);
                ASSERTm(error1, allowed_settings[i].default_value);
                if (allowed_settings[i].type != TYPE_STRING)
                        ASSERTm(error2, strlen(allowed_settings[i].default_value));
                g_free(error1);
                g_free(error2);
        }
        PASS();
}

TEST test_value_non_null(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                gchar *error1 = g_strdup_printf("Error in settting %s. A setting must have a 'value' or a 'rule_offset', or both.",
                                allowed_settings[i].name);
                ASSERTm(error1, allowed_settings[i].value ||
                                allowed_settings[i].rule_offset);
                g_free(error1);
        }
        PASS();
}

TEST test_valid_parser_and_data_per_type(void)
{
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                struct setting curr = allowed_settings[i];
                switch (curr.type) {
                        case TYPE_STRING:
                        case TYPE_TIME:
                        case TYPE_DOUBLE:
                        case TYPE_LENGTH:
                        case TYPE_INT: ; // no parser and no parser data needed
                                gchar *error1 = g_strdup_printf("Parser of setting %s should be NULL. It's not needed for this type", curr.name);
                                gchar *error2 = g_strdup_printf("Parser data of setting %s should be NULL. It's not needed for this type", curr.name);
                                ASSERTm(error1, !curr.parser);
                                ASSERTm(error2, !curr.parser_data);
                                g_free(error1);
                                g_free(error2);
                                break;
                        case TYPE_CUSTOM: ; // both parser data and parser are needed
                                gchar *error3 = g_strdup_printf("Parser of setting %s should not be NULL. It's needed for this type", curr.name);
                                gchar *error4 = g_strdup_printf("Parser data of setting %s should not be NULL. It's needed for this type", curr.name);
                                ASSERTm(error3, curr.parser);
                                ASSERTm(error4, curr.parser_data);
                                g_free(error3);
                                g_free(error4);
                                break;
                        case TYPE_LIST: ; // only parser data is needed
                                gchar *error5 = g_strdup_printf("Parser of setting %s should be NULL. It's needed not for this type", curr.name);
                                gchar *error6 = g_strdup_printf("Parser data of setting %s should not be NULL. It's needed for this type", curr.name);
                                ASSERTm(error5, !curr.parser);
                                ASSERTm(error6, curr.parser_data);
                                g_free(error5);
                                g_free(error6);
                                break;
                        case TYPE_PATH: ; // only parser data is neede, but when it's a rule none is needed.
                                gchar *error7 = g_strdup_printf("Parser of setting %s should be NULL. It's needed not for this type", curr.name);
                                gchar *error8 = g_strdup_printf("Parser data of setting %s should not be NULL. It's needed for this type", curr.name);
                                bool is_rule = !curr.value; // if it doesn't have a 'value' it's a rule
                                ASSERTm(error7, !curr.parser);
                                ASSERTm(error8, is_rule || curr.parser_data);
                                g_free(error7);
                                g_free(error8);
                                break;
                        default: ;
                                gchar *error20 = g_strdup_printf("You should make a test for type %i", curr.type);
                                FAILm(error20);
                                break;
                }
        }
        PASS();
}

SUITE(suite_settings_data)
{
        RUN_TEST(test_names_valid);
        RUN_TEST(test_description_valid);
        RUN_TEST(test_type_valid);
        RUN_TEST(test_section_valid);
        RUN_TEST(test_default_value_valid);
        RUN_TEST(test_value_non_null);
        RUN_TEST(test_valid_parser_and_data_per_type);
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
