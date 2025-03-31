#include "../src/icon-lookup.c"
#include "../src/notification.h"
#include "../src/settings_data.h"
#include "greatest.h"
#include "helpers.h"

extern const char *base;
#define ICONPREFIX "data", "icons"

int setup_test_theme(void)
{
    char *theme_path = g_build_filename(base, ICONPREFIX, NULL);
    int theme_index = load_icon_theme_from_dir(theme_path, "theme");
    add_default_theme(theme_index);
    g_free(theme_path);
    return theme_index;
}

#define find_icon_test(iconname, size, ...)                                    \
    {                                                                          \
        char *icon = find_icon_path(iconname, size);                           \
        char *expected =                                                       \
            g_build_filename(base, ICONPREFIX, "theme", __VA_ARGS__, NULL);    \
        ASSERTm("Could not find icon", icon);                                  \
        ASSERT_STR_EQ(expected, icon);                                         \
        g_free(expected);                                                      \
        g_free(icon);                                                          \
    }

TEST test_load_theme_from_dir(void)
{
    setup_test_theme();
    free_all_themes();
    PASS();
}

TEST test_find_icon(void)
{
    setup_test_theme();
    find_icon_test("edit", 8, "16x16", "actions", "edit.png");
    find_icon_test("edit", 16, "16x16", "actions", "edit.png");
    find_icon_test("edit", 32, "16x16", "actions", "edit.png");
    find_icon_test("edit", 48, "16x16", "actions", "edit.png");
    find_icon_test("edit", 49, "32x32", "actions", "edit.png");
    find_icon_test("edit", 64, "32x32", "actions", "edit.png");
    find_icon_test("preferences", 16, "16x16", "apps", "preferences.png");
    find_icon_test("preferences", 32, "16x16", "apps", "preferences.png");
    free_all_themes();
    PASS();
}

TEST test_new_icon_overrides_raw_icon(void)
{
    setup_test_theme();

    struct notification *n = test_notification_with_icon("new_icon", 10);
    struct rule *rule = g_malloc(sizeof(struct rule));
    *rule = empty_rule;
    rule->summary = g_strdup("new_icon");
    rule->new_icon = g_strdup("edit");

    ASSERT(n->icon);
    int old_width = cairo_image_surface_get_width(n->icon);
    rule_apply(rule, n, true);

    ASSERT(n->icon);
    ASSERT(old_width != cairo_image_surface_get_width(n->icon));

    cairo_surface_destroy(n->icon);
    n->icon = NULL;

    notification_unref(n);
    g_free(rule->summary);
    g_free(rule->new_icon);
    g_free(rule);
    free_all_themes();
    PASS();
}

// TODO move this out of the test suite, since this isn't a real test
TEST test_bench_search(void)
{
    printf("Starting benchmark\n");
    // At the time of committing, I get numbers like 0.25 second for 1000 icon
    // lookups
    int index = load_icon_theme("Papirus");
    add_default_theme(index);
    printf("Benchmarking icons\n");
    clock_t start_time = clock();
    for (int i = 0; i < 1000; i++) {
        // icon is part of papirus, right at the end of index.theme
        char *icon = find_icon_path("weather-windy-symbolic", 512);
        ASSERT(icon);
        g_free(icon);
    }
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Done in %f seconds\n", elapsed_time);
    free_all_themes();
    PASS();
}

TEST test_bench_multiple_search(void)
{
    printf("Starting benchmark\n");
    // At the time of committing, I get numbers like 2 second for 1000 icon
    // lookups
    int index = load_icon_theme("Adwaita");
    add_default_theme(index);
    index = load_icon_theme("Papirus");
    add_default_theme(index);
    printf("Benchmarking icons\n");
    clock_t start_time = clock();
    for (int i = 0; i < 1000; i++) {
        // icon is part of papirus, right at the end of index.theme
        char *icon = find_icon_path("view-wrapped-rtl-symbolic", 512);
        /* printf("%s\n", icon); */
        ASSERT(icon);
        g_free(icon);
    }
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Done in %f seconds\n", elapsed_time);
    free_all_themes();
    PASS();
}

TEST test_bench_doesnt_exist(void)
{
    printf("Starting benchmark\n");
    // At the time of committing, I get numbers like 9 seconds for 1000 icon
    // lookups
    int index = load_icon_theme("Adwaita");
    add_default_theme(index);
    index = load_icon_theme("Papirus");
    add_default_theme(index);
    printf("Benchmarking icons\n");
    clock_t start_time = clock();
    for (int i = 0; i < 1000; i++) {
        // Icon size is chosen as some common icon size.
        char *icon = find_icon_path("doesn't exist", 48);
        /* printf("%s\n", icon); */
        ASSERT_FALSE(icon);
        g_free(icon);
    }
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Done in %f seconds\n", elapsed_time);
    free_all_themes();
    PASS();
}

SUITE(suite_icon_lookup)
{
    RUN_TEST(test_load_theme_from_dir);
    RUN_TEST(test_find_icon);
    RUN_TEST(test_new_icon_overrides_raw_icon);
    bool bench = false;
    if (bench) {
        RUN_TEST(test_bench_search);
        RUN_TEST(test_bench_multiple_search);
        RUN_TEST(test_bench_doesnt_exist);
    }
}
