#include "greatest.h"
#include "../src/icon-lookup.c"

extern const char *base;
#define ICONPREFIX "data", "icons"

int setup_test_theme(){
        char *theme_path = g_build_filename(base, ICONPREFIX,  NULL);
        int theme_index = load_icon_theme_from_dir(theme_path, "theme");
        set_default_theme(theme_index);
        g_free(theme_path);
        return theme_index;
}

#define find_icon_test(iconname, size, ...) { \
        char *icon = find_icon_path(iconname, size); \
        char *expected = g_build_filename(base, ICONPREFIX, "theme", __VA_ARGS__, NULL); \
        ASSERTm("Could not find icon", icon); \
        ASSERT_STR_EQ(expected, icon); \
        g_free(expected); \
        g_free(icon); \
}

#define find_path_test(path, ...) { \
        char *icon = find_icon_path(path, 42); /* size doesn't matter */ \
        char *expected = g_build_filename(base, ICONPREFIX, "theme", __VA_ARGS__, NULL); \
        ASSERTm("Could not find icon", icon); \
        ASSERT_STR_EQ(expected, icon); \
        g_free(expected); \
        g_free(icon); \
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

TEST test_find_path(void)
{
        setup_test_theme();
        char *path = g_build_filename(base, ICONPREFIX, "theme", "16x16", "actions", "edit.png", NULL);
        find_path_test(path, "16x16", "actions", "edit.png");
        char *path2 = g_strconcat("file://", path, NULL);
        find_path_test(path2, "16x16", "actions", "edit.png");
        g_free(path2);
        g_free(path);
        free_all_themes();
        PASS();
}

// TODO move this out of the test suite, since this isn't a real test
TEST test_bench_search(void)
{
        // At the time of committing, I get numbers like 0.001189 seconds for looking up this icon for the first time and 0.000489 for subsequent times.
        int index = load_icon_theme("Papirus");
        set_default_theme(index);
        clock_t start_time = clock();
        printf("Benchmarking icons\n");
        char *icon = find_icon_path("gnome-fallback-compiz_badge-symbolic", 512);
        ASSERT(icon);
        printf("Icon found: %s\n", icon);
        double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("Done in %f seconds\n", elapsed_time);
        printf("Result :%s\n", icon);
        free_all_themes();
        g_free(icon);
        PASS();
}

SUITE (suite_icon_lookup)
{
        RUN_TEST(test_load_theme_from_dir);
        RUN_TEST(test_find_icon);
        RUN_TEST(test_find_path);
        /* RUN_TEST(test_bench_search); */
}
