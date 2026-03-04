#include "net/unifi.h"
#include "core/graph.h"
#include "core/host.h"
#include "core/edge.h"
#include "util/strutil.h"
#include "cJSON.h"
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

/* Test that nm_unifi_available returns 0 with empty config */
static void test_unifi_available_no_config(void)
{
    nm_config_t cfg;
    nm_cli_defaults(&cfg);
    int r = nm_unifi_available(&cfg);
    TEST_ASSERT_EQ(r, 0, "unifi_available returns 0 without config");
}

/* Test that nm_unifi_discover handles NULL graph */
static void test_unifi_discover_null(void)
{
    nm_config_t cfg;
    nm_cli_defaults(&cfg);
    int r = nm_unifi_discover(NULL, &cfg);
    TEST_ASSERT(r == -1 || r == 0, "unifi_discover NULL graph");

    r = nm_unifi_discover(NULL, NULL);
    TEST_ASSERT(r == -1 || r == 0, "unifi_discover NULL both");
}

/* Test parsing canned UniFi device JSON structure */
static void test_unifi_device_json_parse(void)
{
    const char *json_str =
        "{"
        "  \"data\": ["
        "    {"
        "      \"mac\": \"00:11:22:33:44:55\","
        "      \"name\": \"USW Pro Max 16\","
        "      \"type\": \"usw\","
        "      \"model\": \"USW-Pro-Max-16\","
        "      \"ip\": \"192.168.1.2\","
        "      \"port_table\": ["
        "        {"
        "          \"port_idx\": 1,"
        "          \"speed\": 1000,"
        "          \"lldp_table\": ["
        "            {"
        "              \"lldp_chassis_id\": \"aa:bb:cc:dd:ee:01\","
        "              \"lldp_port_id\": \"Port 1\""
        "            }"
        "          ]"
        "        },"
        "        {"
        "          \"port_idx\": 5,"
        "          \"speed\": 2500"
        "        }"
        "      ]"
        "    },"
        "    {"
        "      \"mac\": \"00:11:22:33:44:66\","
        "      \"name\": \"U7 Pro\","
        "      \"type\": \"uap\","
        "      \"model\": \"U7-Pro\","
        "      \"ip\": \"192.168.1.4\""
        "    }"
        "  ]"
        "}";

    cJSON *root = cJSON_Parse(json_str);
    TEST_ASSERT(root != NULL, "device JSON parses");

    cJSON *data = cJSON_GetObjectItem(root, "data");
    TEST_ASSERT(data && cJSON_IsArray(data), "data is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(data), 2, "two devices");

    /* First device: switch */
    cJSON *dev0 = cJSON_GetArrayItem(data, 0);
    cJSON *jtype = cJSON_GetObjectItem(dev0, "type");
    TEST_ASSERT(cJSON_IsString(jtype), "type is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(jtype), "usw") == 0,
                "first device is usw");

    cJSON *port_table = cJSON_GetObjectItem(dev0, "port_table");
    TEST_ASSERT(port_table && cJSON_IsArray(port_table), "port_table array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(port_table), 2, "two ports");

    /* Port 1 has LLDP neighbor */
    cJSON *port0 = cJSON_GetArrayItem(port_table, 0);
    cJSON *lldp_table = cJSON_GetObjectItem(port0, "lldp_table");
    TEST_ASSERT(lldp_table && cJSON_IsArray(lldp_table), "lldp_table array");

    cJSON *lldp0 = cJSON_GetArrayItem(lldp_table, 0);
    cJSON *lchassis = cJSON_GetObjectItem(lldp0, "lldp_chassis_id");
    TEST_ASSERT(cJSON_IsString(lchassis), "lldp chassis is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(lchassis), "aa:bb:cc:dd:ee:01") == 0,
                "lldp chassis MAC");

    /* Second device: AP */
    cJSON *dev1 = cJSON_GetArrayItem(data, 1);
    cJSON *jtype1 = cJSON_GetObjectItem(dev1, "type");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(jtype1), "uap") == 0,
                "second device is uap");

    cJSON_Delete(root);
}

/* Test parsing canned UniFi client JSON structure */
static void test_unifi_client_json_parse(void)
{
    const char *json_str =
        "{"
        "  \"data\": ["
        "    {"
        "      \"mac\": \"aa:bb:cc:00:00:10\","
        "      \"hostname\": \"macbook\","
        "      \"ip\": \"192.168.1.50\","
        "      \"is_wired\": true,"
        "      \"sw_mac\": \"00:11:22:33:44:55\","
        "      \"sw_port\": 3,"
        "      \"vlan\": 100"
        "    },"
        "    {"
        "      \"mac\": \"aa:bb:cc:00:00:20\","
        "      \"name\": \"iPhone\","
        "      \"ip\": \"192.168.1.60\","
        "      \"is_wired\": false,"
        "      \"ap_mac\": \"00:11:22:33:44:66\","
        "      \"essid\": \"MyNetwork\","
        "      \"rssi\": 45,"
        "      \"signal\": -62"
        "    }"
        "  ]"
        "}";

    cJSON *root = cJSON_Parse(json_str);
    TEST_ASSERT(root != NULL, "client JSON parses");

    cJSON *data = cJSON_GetObjectItem(root, "data");
    TEST_ASSERT(data && cJSON_IsArray(data), "data is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(data), 2, "two clients");

    /* Wired client */
    cJSON *sta0 = cJSON_GetArrayItem(data, 0);
    cJSON *jis_wired = cJSON_GetObjectItem(sta0, "is_wired");
    TEST_ASSERT(cJSON_IsTrue(jis_wired), "first client is wired");

    cJSON *jsw_mac = cJSON_GetObjectItem(sta0, "sw_mac");
    TEST_ASSERT(cJSON_IsString(jsw_mac), "sw_mac is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(jsw_mac), "00:11:22:33:44:55") == 0,
                "sw_mac matches switch");

    cJSON *jsw_port = cJSON_GetObjectItem(sta0, "sw_port");
    TEST_ASSERT(cJSON_IsNumber(jsw_port), "sw_port is number");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(jsw_port), 3, "sw_port is 3");

    /* WiFi client */
    cJSON *sta1 = cJSON_GetArrayItem(data, 1);
    cJSON *jis_wired1 = cJSON_GetObjectItem(sta1, "is_wired");
    TEST_ASSERT(!cJSON_IsTrue(jis_wired1), "second client is wifi");

    cJSON *jessid = cJSON_GetObjectItem(sta1, "essid");
    TEST_ASSERT(cJSON_IsString(jessid), "essid is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(jessid), "MyNetwork") == 0,
                "essid matches");

    cJSON *jsignal = cJSON_GetObjectItem(sta1, "signal");
    TEST_ASSERT(cJSON_IsNumber(jsignal), "signal is number");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(jsignal), -62, "signal is -62");

    cJSON_Delete(root);
}

/* Test building a UniFi-like L2 graph with switches, APs, wired & WiFi clients */
static void test_unifi_graph_integration(void)
{
    nm_graph_t *g = nm_graph_create();

    /* Gateway (UDR) */
    nm_host_t h;
    nm_host_init(&h);
    unsigned char gw_mac[] = {0x00, 0x11, 0x22, 0x00, 0x00, 0x01};
    nm_host_set_mac(&h, gw_mac);
    nm_host_set_ipv4(&h, "192.168.1.1");
    nm_strlcpy(h.hostname, "UDR7", sizeof(h.hostname));
    h.type = NM_HOST_GATEWAY;
    h.connection_medium = NM_MEDIUM_WIRED;
    nm_strlcpy(h.unifi_device_type, "UDR", sizeof(h.unifi_device_type));
    nm_host_compute_display_name(&h);
    int gw_id = nm_graph_add_host(g, &h);

    /* Switch */
    nm_host_init(&h);
    unsigned char sw_mac[] = {0x00, 0x11, 0x22, 0x00, 0x00, 0x02};
    nm_host_set_mac(&h, sw_mac);
    nm_host_set_ipv4(&h, "192.168.1.2");
    nm_strlcpy(h.hostname, "USW-Pro-Max-16", sizeof(h.hostname));
    h.type = NM_HOST_SWITCH;
    h.connection_medium = NM_MEDIUM_WIRED;
    nm_strlcpy(h.unifi_device_type, "USW-Pro-Max-16",
               sizeof(h.unifi_device_type));
    nm_host_compute_display_name(&h);
    int sw_id = nm_graph_add_host(g, &h);

    /* AP */
    nm_host_init(&h);
    unsigned char ap_mac[] = {0x00, 0x11, 0x22, 0x00, 0x00, 0x03};
    nm_host_set_mac(&h, ap_mac);
    nm_host_set_ipv4(&h, "192.168.1.4");
    nm_strlcpy(h.hostname, "U7-Pro", sizeof(h.hostname));
    h.type = NM_HOST_AP;
    h.connection_medium = NM_MEDIUM_WIRED;
    nm_strlcpy(h.unifi_device_type, "U7-Pro", sizeof(h.unifi_device_type));
    nm_host_compute_display_name(&h);
    int ap_id = nm_graph_add_host(g, &h);

    /* Wired client */
    nm_host_init(&h);
    unsigned char c1_mac[] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x10};
    nm_host_set_mac(&h, c1_mac);
    nm_host_set_ipv4(&h, "192.168.1.50");
    nm_strlcpy(h.hostname, "Apple-TV", sizeof(h.hostname));
    h.type = NM_HOST_WORKSTATION;
    h.connection_medium = NM_MEDIUM_WIRED;
    memcpy(h.switch_mac, sw_mac, 6);
    h.switch_port = 3;
    h.has_switch_info = 1;
    nm_host_compute_display_name(&h);
    int c1_id = nm_graph_add_host(g, &h);

    /* WiFi client */
    nm_host_init(&h);
    unsigned char c2_mac[] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x20};
    nm_host_set_mac(&h, c2_mac);
    nm_host_set_ipv4(&h, "192.168.1.60");
    nm_strlcpy(h.hostname, "iPhone", sizeof(h.hostname));
    h.type = NM_HOST_WORKSTATION;
    h.connection_medium = NM_MEDIUM_WIFI;
    nm_strlcpy(h.wifi_ssid, "MyNetwork", sizeof(h.wifi_ssid));
    h.wifi_signal = -62;
    nm_host_compute_display_name(&h);
    int c2_id = nm_graph_add_host(g, &h);

    /* L2 edges: gw->sw, sw->ap, sw->wired_client */
    int e1 = nm_graph_add_edge(g, gw_id, sw_id, 0.5, NM_EDGE_L2);
    TEST_ASSERT(e1 >= 0, "gw-sw edge");
    g->edges[e1].medium = NM_MEDIUM_WIRED;
    g->edges[e1].src_port_num = 1;

    int e2 = nm_graph_add_edge(g, sw_id, ap_id, 0.5, NM_EDGE_L2);
    TEST_ASSERT(e2 >= 0, "sw-ap edge");
    g->edges[e2].medium = NM_MEDIUM_WIRED;
    g->edges[e2].src_port_num = 5;

    int e3 = nm_graph_add_edge(g, sw_id, c1_id, 0.5, NM_EDGE_L2);
    TEST_ASSERT(e3 >= 0, "sw-wired_client edge");
    g->edges[e3].medium = NM_MEDIUM_WIRED;
    g->edges[e3].src_port_num = 3;

    /* WiFi edge: ap->wifi_client */
    int e4 = nm_graph_add_edge(g, ap_id, c2_id, 0.5, NM_EDGE_WIFI);
    TEST_ASSERT(e4 >= 0, "ap-wifi_client edge");
    g->edges[e4].medium = NM_MEDIUM_WIFI;

    /* Verify graph structure */
    TEST_ASSERT_EQ(g->host_count, 5, "5 hosts");
    TEST_ASSERT_EQ(g->edge_count, 4, "4 edges");

    /* Verify types */
    TEST_ASSERT_EQ(g->hosts[gw_id].type, NM_HOST_GATEWAY, "gateway type");
    TEST_ASSERT_EQ(g->hosts[sw_id].type, NM_HOST_SWITCH, "switch type");
    TEST_ASSERT_EQ(g->hosts[ap_id].type, NM_HOST_AP, "ap type");

    /* Verify WiFi client properties */
    TEST_ASSERT_EQ(g->hosts[c2_id].connection_medium, NM_MEDIUM_WIFI,
                   "wifi medium");
    TEST_ASSERT(strcmp(g->hosts[c2_id].wifi_ssid, "MyNetwork") == 0,
                "wifi ssid");
    TEST_ASSERT_EQ(g->hosts[c2_id].wifi_signal, -62, "wifi signal");

    /* Verify wired client switch info */
    TEST_ASSERT_EQ(g->hosts[c1_id].has_switch_info, 1, "has switch info");
    TEST_ASSERT_EQ(g->hosts[c1_id].switch_port, 3, "switch port 3");

    /* MST should work */
    int mst = nm_graph_kruskal_mst(g);
    TEST_ASSERT(mst >= 0, "MST computed");

    /* All edges in MST since it's a tree already */
    int mst_count = 0;
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].in_mst) mst_count++;
    }
    TEST_ASSERT_EQ(mst_count, 4, "all 4 edges in MST");

    nm_graph_destroy(g);
}

void test_unifi_suite(void)
{
    test_unifi_available_no_config();
    test_unifi_discover_null();
    test_unifi_device_json_parse();
    test_unifi_client_json_parse();
    test_unifi_graph_integration();
}
