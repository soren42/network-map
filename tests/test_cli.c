#include "cli.h"
#include <stdio.h>
#include <string.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;
#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        g_tests_failed++; \
    } else { \
        g_tests_passed++; \
    } \
} while(0)
#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_STR(a, b, msg) TEST_ASSERT(strcmp((a),(b)) == 0, msg)

static void test_cli_defaults(void)
{
    nm_config_t cfg;
    nm_cli_defaults(&cfg);
    TEST_ASSERT_EQ(cfg.output_flags, (unsigned)NM_OUT_TEXT, "default output is TEXT");
    TEST_ASSERT_STR(cfg.file_base, "intranet", "default file base");
    TEST_ASSERT_EQ(cfg.verbosity, 0, "default verbosity is 0");
    TEST_ASSERT(!cfg.ipv4_only, "not ipv4 only");
    TEST_ASSERT(!cfg.ipv6_only, "not ipv6 only");
    TEST_ASSERT(!cfg.no_mdns, "mdns enabled");
    TEST_ASSERT(!cfg.no_arp, "arp enabled");
    TEST_ASSERT(!cfg.no_nmap, "nmap enabled");
    TEST_ASSERT(!cfg.fast_mode, "fast mode off");
    TEST_ASSERT(!cfg.has_boundary, "no boundary by default");
}

static void test_cli_output_formats(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-o", "text,json,html"};
    int rc = nm_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse output ok");
    TEST_ASSERT(cfg.output_flags & NM_OUT_TEXT, "text flag");
    TEST_ASSERT(cfg.output_flags & NM_OUT_JSON, "json flag");
    TEST_ASSERT(cfg.output_flags & NM_OUT_HTML, "html flag");
    TEST_ASSERT(!(cfg.output_flags & NM_OUT_CURSES), "no curses flag");
    TEST_ASSERT(!(cfg.output_flags & NM_OUT_PNG), "no png flag");
}

static void test_cli_verbosity(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-vvv"};
    int rc = nm_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse -vvv ok");
    TEST_ASSERT_EQ(cfg.verbosity, 3, "verbosity 3");
}

static void test_cli_file(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-f", "output"};
    int rc = nm_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse file ok");
    TEST_ASSERT_STR(cfg.file_base, "output", "file base");
}

static void test_cli_ipv4_only(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-4"};
    int rc = nm_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse -4 ok");
    TEST_ASSERT(cfg.ipv4_only, "ipv4 only set");
    TEST_ASSERT(!cfg.ipv6_only, "ipv6 only not set");
}

static void test_cli_nameserver(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-n", "10.0.0.1"};
    int rc = nm_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse nameserver ok");
    TEST_ASSERT_STR(cfg.nameserver, "10.0.0.1", "nameserver value");
}

static void test_cli_no_nmap(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "--no-nmap"};
    int rc = nm_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse --no-nmap ok");
    TEST_ASSERT(cfg.no_nmap, "no_nmap set");
}

static void test_cli_fast(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "--fast"};
    int rc = nm_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse --fast ok");
    TEST_ASSERT(cfg.fast_mode, "fast_mode set");
}

static void test_cli_combined(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "-vv", "-o", "json,html", "-f", "test",
                    "-4", "--no-nmap", "--fast", "10.0.0.1"};
    int rc = nm_cli_parse(&cfg, 10, argv);
    TEST_ASSERT_EQ(rc, 0, "combined parse ok");
    TEST_ASSERT_EQ(cfg.verbosity, 2, "verbosity 2");
    TEST_ASSERT(cfg.output_flags & NM_OUT_JSON, "json output");
    TEST_ASSERT(cfg.output_flags & NM_OUT_HTML, "html output");
    TEST_ASSERT(!(cfg.output_flags & NM_OUT_TEXT), "text not set when explicit");
    TEST_ASSERT_STR(cfg.file_base, "test", "file base");
    TEST_ASSERT(cfg.ipv4_only, "ipv4 only");
    TEST_ASSERT(cfg.no_nmap, "no_nmap");
    TEST_ASSERT(cfg.fast_mode, "fast_mode");
    TEST_ASSERT(cfg.has_boundary, "has boundary");
    TEST_ASSERT_STR(cfg.boundary_host, "10.0.0.1", "boundary host");
}

static void test_cli_from_json(void)
{
    nm_config_t cfg;
    char *argv[] = {"prog", "--from-json", "scan.json", "-o", "png"};
    int rc = nm_cli_parse(&cfg, 5, argv);
    TEST_ASSERT_EQ(rc, 0, "parse --from-json ok");
    TEST_ASSERT(cfg.load_from_json, "load_from_json set");
    TEST_ASSERT_STR(cfg.json_input_path, "scan.json", "json_input_path");
    TEST_ASSERT(cfg.output_flags & NM_OUT_PNG, "png output");
}

static void test_cli_from_json_default(void)
{
    nm_config_t cfg;
    nm_cli_defaults(&cfg);
    TEST_ASSERT(!cfg.load_from_json, "load_from_json default is 0");
    TEST_ASSERT_EQ(cfg.json_input_path[0], '\0', "json_input_path default empty");
}

void test_cli_suite(void)
{
    test_cli_defaults();
    test_cli_output_formats();
    test_cli_verbosity();
    test_cli_file();
    test_cli_ipv4_only();
    test_cli_nameserver();
    test_cli_no_nmap();
    test_cli_fast();
    test_cli_combined();
    test_cli_from_json();
    test_cli_from_json_default();
}
