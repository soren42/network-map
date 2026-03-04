#include "net/lldp.h"
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

/* Test that nm_lldp_available returns 0 or 1 */
static void test_lldp_available(void)
{
    int r = nm_lldp_available();
    TEST_ASSERT(r == 0 || r == 1, "lldp_available returns bool");
}

/* Test that nm_lldp_discover handles NULL graph */
static void test_lldp_discover_null(void)
{
    int r = nm_lldp_discover(NULL);
    TEST_ASSERT(r == -1 || r == 0, "lldp_discover NULL returns -1 or 0");
}

/* Test creating L2 hosts and edges that mirror LLDP discovery output */
static void test_lldp_graph_integration(void)
{
    nm_graph_t *g = nm_graph_create();
    TEST_ASSERT(g != NULL, "graph created");

    /* Create local host (what LLDP would find as the local interface) */
    nm_host_t h;
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.100");
    nm_strlcpy(h.iface_name, "eth0", sizeof(h.iface_name));
    h.type = NM_HOST_LOCAL;
    unsigned char local_mac[] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
    nm_host_set_mac(&h, local_mac);
    int local_id = nm_graph_add_host(g, &h);
    TEST_ASSERT_EQ(local_id, 0, "local host added at 0");

    /* Simulate what LLDP discovery creates: a switch neighbor */
    nm_host_init(&h);
    unsigned char sw_mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    nm_host_set_mac(&h, sw_mac);
    nm_strlcpy(h.hostname, "USW-Pro-Max-16", sizeof(h.hostname));
    h.type = NM_HOST_SWITCH;
    h.connection_medium = NM_MEDIUM_WIRED;
    h.vlan_id = 100;
    nm_host_compute_display_name(&h);
    int sw_id = nm_graph_add_host(g, &h);
    TEST_ASSERT_EQ(sw_id, 1, "switch host added at 1");

    /* Verify switch properties */
    TEST_ASSERT_EQ(g->hosts[sw_id].type, NM_HOST_SWITCH, "host is switch type");
    TEST_ASSERT_EQ(g->hosts[sw_id].connection_medium, NM_MEDIUM_WIRED, "medium is wired");
    TEST_ASSERT_EQ(g->hosts[sw_id].vlan_id, 100, "vlan_id is 100");

    /* Verify MAC lookup works */
    int found = nm_graph_find_by_mac(g, sw_mac);
    TEST_ASSERT_EQ(found, sw_id, "find by mac returns switch");

    /* Create L2 edge (as LLDP would) */
    int has = nm_graph_has_edge(g, local_id, sw_id);
    TEST_ASSERT_EQ(has, 0, "no edge yet");

    int eidx = nm_graph_add_edge(g, local_id, sw_id, 0.5, NM_EDGE_L2);
    TEST_ASSERT(eidx >= 0, "L2 edge created");

    nm_edge_t *e = &g->edges[eidx];
    e->medium = NM_MEDIUM_WIRED;
    nm_strlcpy(e->dst_port_name, "Port 5", sizeof(e->dst_port_name));
    nm_strlcpy(e->src_port_name, "eth0", sizeof(e->src_port_name));
    e->dst_port_num = 5;

    /* Verify edge properties */
    TEST_ASSERT_EQ(e->type, NM_EDGE_L2, "edge type is L2");
    TEST_ASSERT_EQ(e->medium, NM_MEDIUM_WIRED, "edge medium is wired");
    TEST_ASSERT_EQ(e->dst_port_num, 5, "dst port num is 5");
    TEST_ASSERT(strcmp(e->dst_port_name, "Port 5") == 0, "dst port name");
    TEST_ASSERT(strcmp(e->src_port_name, "eth0") == 0, "src port name");

    /* Verify has_edge now returns true */
    has = nm_graph_has_edge(g, local_id, sw_id);
    TEST_ASSERT(has != 0, "edge exists now");

    /* Add an AP neighbor (WiFi link) */
    nm_host_init(&h);
    unsigned char ap_mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x66};
    nm_host_set_mac(&h, ap_mac);
    nm_strlcpy(h.hostname, "U7-Pro", sizeof(h.hostname));
    h.type = NM_HOST_AP;
    h.connection_medium = NM_MEDIUM_WIRED;
    nm_host_compute_display_name(&h);
    int ap_id = nm_graph_add_host(g, &h);
    TEST_ASSERT_EQ(ap_id, 2, "AP host added at 2");
    TEST_ASSERT_EQ(g->hosts[ap_id].type, NM_HOST_AP, "host is AP type");

    /* L2 edge from switch to AP */
    int eidx2 = nm_graph_add_edge(g, sw_id, ap_id, 0.5, NM_EDGE_L2);
    TEST_ASSERT(eidx2 >= 0, "switch-AP L2 edge created");

    /* Verify MST works with L2 edges */
    int mst = nm_graph_kruskal_mst(g);
    TEST_ASSERT(mst >= 0, "MST computed with L2 edges");

    nm_graph_destroy(g);
}

/* Test LLDP capability-to-type mapping via host type strings */
static void test_lldp_host_types(void)
{
    /* Verify switch and AP type strings round-trip */
    const char *sw_str = nm_host_type_str(NM_HOST_SWITCH);
    TEST_ASSERT(strcmp(sw_str, "switch") == 0, "switch type string");

    const char *ap_str = nm_host_type_str(NM_HOST_AP);
    TEST_ASSERT(strcmp(ap_str, "ap") == 0, "ap type string");

    nm_host_type_t sw_type = nm_host_type_from_str("switch");
    TEST_ASSERT_EQ(sw_type, NM_HOST_SWITCH, "switch from string");

    nm_host_type_t ap_type = nm_host_type_from_str("ap");
    TEST_ASSERT_EQ(ap_type, NM_HOST_AP, "ap from string");
}

/* Test that L2 edges get MST weight preference */
static void test_lldp_mst_preference(void)
{
    nm_graph_t *g = nm_graph_create();

    /* Three hosts: local, switch, workstation */
    nm_host_t h;
    nm_host_init(&h);
    h.type = NM_HOST_LOCAL;
    nm_host_set_ipv4(&h, "192.168.1.100");
    nm_graph_add_host(g, &h);

    nm_host_init(&h);
    h.type = NM_HOST_SWITCH;
    nm_host_set_ipv4(&h, "192.168.1.2");
    nm_graph_add_host(g, &h);

    nm_host_init(&h);
    h.type = NM_HOST_WORKSTATION;
    nm_host_set_ipv4(&h, "192.168.1.50");
    nm_graph_add_host(g, &h);

    /* LAN edge 0->2 with weight 0.5 */
    nm_graph_add_edge(g, 0, 2, 0.5, NM_EDGE_LAN);

    /* L2 edge 1->2 with weight 0.5 (effective: 0.05 due to L2 bias) */
    nm_graph_add_edge(g, 1, 2, 0.5, NM_EDGE_L2);

    /* L2 edge 0->1 with weight 0.5 */
    nm_graph_add_edge(g, 0, 1, 0.5, NM_EDGE_L2);

    /* MST should prefer L2 edges */
    int mst = nm_graph_kruskal_mst(g);
    TEST_ASSERT(mst >= 0, "MST computed");

    /* Count how many L2 edges are in MST */
    int l2_in_mst = 0;
    int lan_in_mst = 0;
    for (int i = 0; i < g->edge_count; i++) {
        if (g->edges[i].in_mst) {
            if (g->edges[i].type == NM_EDGE_L2) l2_in_mst++;
            if (g->edges[i].type == NM_EDGE_LAN) lan_in_mst++;
        }
    }
    /* Both L2 edges should be preferred for MST (3 nodes need 2 edges) */
    TEST_ASSERT_EQ(l2_in_mst, 2, "L2 edges preferred in MST");
    TEST_ASSERT_EQ(lan_in_mst, 0, "LAN edge not in MST when L2 available");

    nm_graph_destroy(g);
}

/* Test parsing canned lldpcli JSON structure with cJSON */
static void test_lldp_json_parse(void)
{
    /* Minimal lldpcli JSON output structure */
    const char *json_str =
        "{"
        "  \"lldp\": {"
        "    \"interface\": ["
        "      {"
        "        \"eth0\": {"
        "          \"chassis\": {"
        "            \"USW-Pro\": {"
        "              \"id\": { \"type\": \"mac\", \"value\": \"00:11:22:33:44:55\" },"
        "              \"capability\": ["
        "                { \"type\": \"Bridge\", \"enabled\": true },"
        "                { \"type\": \"Router\", \"enabled\": false }"
        "              ]"
        "            }"
        "          },"
        "          \"port\": {"
        "            \"id\": { \"type\": \"mac\", \"value\": \"00:11:22:33:44:55\" },"
        "            \"descr\": \"Port 5\""
        "          },"
        "          \"vlan\": { \"vlan-id\": 100 }"
        "        }"
        "      }"
        "    ]"
        "  }"
        "}";

    cJSON *root = cJSON_Parse(json_str);
    TEST_ASSERT(root != NULL, "JSON parses successfully");

    /* Navigate the structure as lldp.c does */
    cJSON *lldp = cJSON_GetObjectItem(root, "lldp");
    TEST_ASSERT(lldp != NULL, "lldp object found");

    cJSON *ifaces = cJSON_GetObjectItem(lldp, "interface");
    TEST_ASSERT(ifaces != NULL, "interface array found");
    TEST_ASSERT(cJSON_IsArray(ifaces), "interface is array");

    cJSON *iface_entry = cJSON_GetArrayItem(ifaces, 0);
    TEST_ASSERT(iface_entry != NULL, "first interface entry");

    cJSON *inner = iface_entry->child;
    TEST_ASSERT(inner != NULL, "inner object exists");
    TEST_ASSERT(strcmp(inner->string, "eth0") == 0, "interface name is eth0");

    /* Parse chassis */
    cJSON *chassis = cJSON_GetObjectItem(inner, "chassis");
    TEST_ASSERT(chassis != NULL, "chassis found");

    cJSON *chassis_entry = chassis->child;
    TEST_ASSERT(chassis_entry != NULL, "chassis entry found");
    TEST_ASSERT(strcmp(chassis_entry->string, "USW-Pro") == 0, "system name");

    cJSON *id_obj = cJSON_GetObjectItem(chassis_entry, "id");
    TEST_ASSERT(id_obj != NULL, "id object found");

    cJSON *val = cJSON_GetObjectItem(id_obj, "value");
    TEST_ASSERT(val != NULL && cJSON_IsString(val), "id value is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(val), "00:11:22:33:44:55") == 0,
                "chassis MAC correct");

    /* Parse capabilities */
    cJSON *cap_arr = cJSON_GetObjectItem(chassis_entry, "capability");
    TEST_ASSERT(cap_arr != NULL && cJSON_IsArray(cap_arr), "capability array");

    int bridge_enabled = 0;
    cJSON *c;
    cJSON_ArrayForEach(c, cap_arr) {
        cJSON *enabled = cJSON_GetObjectItem(c, "enabled");
        cJSON *ctype = cJSON_GetObjectItem(c, "type");
        if (enabled && cJSON_IsTrue(enabled) &&
            ctype && cJSON_IsString(ctype)) {
            if (strcmp(cJSON_GetStringValue(ctype), "Bridge") == 0)
                bridge_enabled = 1;
        }
    }
    TEST_ASSERT(bridge_enabled, "Bridge capability enabled");

    /* Parse port */
    cJSON *port_obj = cJSON_GetObjectItem(inner, "port");
    TEST_ASSERT(port_obj != NULL, "port object found");

    cJSON *pdesc = cJSON_GetObjectItem(port_obj, "descr");
    TEST_ASSERT(pdesc && cJSON_IsString(pdesc), "port descr is string");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(pdesc), "Port 5") == 0,
                "port description correct");

    /* Parse VLAN */
    cJSON *vlan = cJSON_GetObjectItem(inner, "vlan");
    TEST_ASSERT(vlan != NULL, "vlan found");

    cJSON *vid = cJSON_GetObjectItem(vlan, "vlan-id");
    TEST_ASSERT(vid && cJSON_IsNumber(vid), "vlan-id is number");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(vid), 100, "vlan-id is 100");

    cJSON_Delete(root);
}

void test_lldp_suite(void)
{
    test_lldp_available();
    test_lldp_discover_null();
    test_lldp_graph_integration();
    test_lldp_host_types();
    test_lldp_mst_preference();
    test_lldp_json_parse();
}
