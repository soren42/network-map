#include "core/json_out.h"
#include "core/types.h"
#include "util/strutil.h"
#include "mock_net.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
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

static void test_json_serialize(void)
{
    nm_graph_t *g = mock_build_sample_graph();
    cJSON *json = nm_json_serialize(g);
    TEST_ASSERT(json != NULL, "serialize returns non-null");

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    TEST_ASSERT(hosts != NULL, "has hosts array");
    TEST_ASSERT(cJSON_IsArray(hosts), "hosts is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(hosts), 8, "8 hosts in array");

    cJSON *edges = cJSON_GetObjectItem(json, "edges");
    TEST_ASSERT(edges != NULL, "has edges array");
    TEST_ASSERT(cJSON_IsArray(edges), "edges is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(edges), 7, "7 edges in array");

    /* Check first host */
    cJSON *h0 = cJSON_GetArrayItem(hosts, 0);
    cJSON *name = cJSON_GetObjectItem(h0, "display_name");
    TEST_ASSERT(name != NULL, "host 0 has display_name");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(name), "my-machine") == 0,
                "host 0 display name");

    cJSON *type = cJSON_GetObjectItem(h0, "type");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(type), "local") == 0,
                "host 0 type is local");

    /* Check server host has os_name */
    cJSON *h4 = cJSON_GetArrayItem(hosts, 4);
    cJSON *os = cJSON_GetObjectItem(h4, "os_name");
    TEST_ASSERT(os != NULL, "server host has os_name");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(os), "Ubuntu 22.04") == 0,
                "server os_name value");

    /* Check server host has services */
    cJSON *svcs = cJSON_GetObjectItem(h4, "services");
    TEST_ASSERT(svcs != NULL, "server host has services");
    TEST_ASSERT(cJSON_IsArray(svcs), "services is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(svcs), 2, "server has 2 services");

    /* Check IoT host has manufacturer */
    cJSON *h6 = cJSON_GetArrayItem(hosts, 6);
    cJSON *mfr = cJSON_GetObjectItem(h6, "manufacturer");
    TEST_ASSERT(mfr != NULL, "IoT host has manufacturer");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(mfr), "Espressif") == 0,
                "IoT manufacturer value");

    /* Check boundary host has is_boundary */
    cJSON *h7 = cJSON_GetArrayItem(hosts, 7);
    cJSON *bnd = cJSON_GetObjectItem(h7, "is_boundary");
    TEST_ASSERT(bnd != NULL, "boundary host has is_boundary");
    TEST_ASSERT(cJSON_IsTrue(bnd), "is_boundary is true");

    cJSON_Delete(json);
    nm_graph_destroy(g);
}

static void test_json_roundtrip(void)
{
    nm_graph_t *g = mock_build_sample_graph();
    cJSON *json = nm_json_serialize(g);
    char *str = cJSON_Print(json);
    TEST_ASSERT(str != NULL, "print produces string");
    TEST_ASSERT(strlen(str) > 100, "json string is non-trivial");

    /* Parse it back */
    cJSON *parsed = cJSON_Parse(str);
    TEST_ASSERT(parsed != NULL, "can parse back");

    cJSON *hc = cJSON_GetObjectItem(parsed, "host_count");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(hc), 8, "host_count round-trip");

    cJSON *ec = cJSON_GetObjectItem(parsed, "edge_count");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(ec), 7, "edge_count round-trip");

    cJSON_Delete(parsed);
    free(str);
    cJSON_Delete(json);
    nm_graph_destroy(g);
}

static void test_json_deserialize(void)
{
    /* Serialize a sample graph, then deserialize, and compare */
    nm_graph_t *g1 = mock_build_sample_graph();
    cJSON *json = nm_json_serialize(g1);

    nm_graph_t *g2 = nm_json_deserialize(json);
    TEST_ASSERT(g2 != NULL, "deserialize returns non-null");
    TEST_ASSERT_EQ(g2->host_count, g1->host_count,
                   "deserialized host count matches");
    TEST_ASSERT_EQ(g2->edge_count, g1->edge_count,
                   "deserialized edge count matches");

    /* Spot-check host 0 */
    TEST_ASSERT(strcmp(g2->hosts[0].display_name,
                       g1->hosts[0].display_name) == 0,
                "host 0 display_name preserved");
    TEST_ASSERT_EQ(g2->hosts[0].type, g1->hosts[0].type,
                   "host 0 type preserved");
    TEST_ASSERT_EQ(g2->hosts[0].has_ipv4, g1->hosts[0].has_ipv4,
                   "host 0 has_ipv4 preserved");

    /* Spot-check server host (index 4) services */
    TEST_ASSERT_EQ(g2->hosts[4].service_count, g1->hosts[4].service_count,
                   "server service_count preserved");
    TEST_ASSERT(strcmp(g2->hosts[4].os_name, g1->hosts[4].os_name) == 0,
                "server os_name preserved");

    /* Spot-check edge 0 */
    TEST_ASSERT_EQ(g2->edges[0].src_id, g1->edges[0].src_id,
                   "edge 0 src_id preserved");
    TEST_ASSERT_EQ(g2->edges[0].dst_id, g1->edges[0].dst_id,
                   "edge 0 dst_id preserved");
    TEST_ASSERT_EQ(g2->edges[0].type, g1->edges[0].type,
                   "edge 0 type preserved");

    /* Check boundary host preserved */
    TEST_ASSERT_EQ(g2->hosts[7].is_boundary, 1,
                   "boundary host is_boundary preserved");
    TEST_ASSERT_EQ(g2->hosts[7].type, NM_HOST_BOUNDARY,
                   "boundary host type preserved");

    cJSON_Delete(json);
    nm_graph_destroy(g1);
    nm_graph_destroy(g2);
}

static void test_json_deserialize_null(void)
{
    nm_graph_t *g = nm_json_deserialize(NULL);
    TEST_ASSERT(g == NULL, "deserialize NULL returns NULL");
}

static void test_json_l2_roundtrip(void)
{
    /* Build a graph with L2 fields and verify round-trip */
    nm_graph_t *g = nm_graph_create();
    nm_host_t h;

    /* Switch host */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.2");
    h.type = NM_HOST_SWITCH;
    nm_strlcpy(h.unifi_device_type, "USW-Pro-Max-16",
               sizeof(h.unifi_device_type));
    h.connection_medium = NM_MEDIUM_WIRED;
    nm_strlcpy(h.display_name, "Switch", sizeof(h.display_name));
    nm_graph_add_host(g, &h);

    /* WiFi client */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.50");
    h.type = NM_HOST_WORKSTATION;
    unsigned char smac[] = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33};
    memcpy(h.switch_mac, smac, 6);
    h.switch_port = 5;
    h.has_switch_info = 1;
    h.vlan_id = 10;
    nm_strlcpy(h.wifi_ssid, "MyNetwork", sizeof(h.wifi_ssid));
    h.wifi_signal = -45;
    h.connection_medium = NM_MEDIUM_WIFI;
    nm_strlcpy(h.display_name, "MacBook", sizeof(h.display_name));
    nm_graph_add_host(g, &h);

    /* L2 edge with port info */
    int eidx = nm_graph_add_edge(g, 0, 1, 0.5, NM_EDGE_L2);
    TEST_ASSERT(eidx >= 0, "L2 edge added");
    g->edges[eidx].src_port_num = 5;
    g->edges[eidx].dst_port_num = 0;
    nm_strlcpy(g->edges[eidx].src_port_name, "Port 5",
               sizeof(g->edges[eidx].src_port_name));
    g->edges[eidx].speed_mbps = 1000;
    g->edges[eidx].medium = NM_MEDIUM_WIRED;

    /* Serialize and deserialize */
    cJSON *json = nm_json_serialize(g);
    TEST_ASSERT(json != NULL, "L2 serialize non-null");

    nm_graph_t *g2 = nm_json_deserialize(json);
    TEST_ASSERT(g2 != NULL, "L2 deserialize non-null");
    TEST_ASSERT_EQ(g2->host_count, 2, "L2 host count");
    TEST_ASSERT_EQ(g2->edge_count, 1, "L2 edge count");

    /* Verify host L2 fields */
    TEST_ASSERT_EQ((int)g2->hosts[0].type, (int)NM_HOST_SWITCH,
                   "switch type preserved");
    TEST_ASSERT(strcmp(g2->hosts[0].unifi_device_type, "USW-Pro-Max-16") == 0,
                "unifi_device_type preserved");
    TEST_ASSERT_EQ(g2->hosts[1].has_switch_info, 1,
                   "has_switch_info preserved");
    TEST_ASSERT_EQ(g2->hosts[1].switch_port, 5, "switch_port preserved");
    TEST_ASSERT_EQ(g2->hosts[1].vlan_id, 10, "vlan_id preserved");
    TEST_ASSERT(strcmp(g2->hosts[1].wifi_ssid, "MyNetwork") == 0,
                "wifi_ssid preserved");
    TEST_ASSERT_EQ(g2->hosts[1].wifi_signal, -45, "wifi_signal preserved");
    TEST_ASSERT_EQ((int)g2->hosts[1].connection_medium, (int)NM_MEDIUM_WIFI,
                   "connection_medium preserved");

    /* Verify edge L2 fields */
    TEST_ASSERT_EQ((int)g2->edges[0].type, (int)NM_EDGE_L2,
                   "L2 edge type preserved");
    TEST_ASSERT_EQ(g2->edges[0].src_port_num, 5, "src_port_num preserved");
    TEST_ASSERT(strcmp(g2->edges[0].src_port_name, "Port 5") == 0,
                "src_port_name preserved");
    TEST_ASSERT_EQ(g2->edges[0].speed_mbps, 1000, "speed_mbps preserved");
    TEST_ASSERT_EQ((int)g2->edges[0].medium, (int)NM_MEDIUM_WIRED,
                   "edge medium preserved");

    cJSON_Delete(json);
    nm_graph_destroy(g);
    nm_graph_destroy(g2);
}

void test_json_suite(void)
{
    test_json_serialize();
    test_json_roundtrip();
    test_json_deserialize();
    test_json_deserialize_null();
    test_json_l2_roundtrip();
}
