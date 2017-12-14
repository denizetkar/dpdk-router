#include "routing_table.h"

#include <rte_config.h>
#include <rte_ip.h>

// do nothing :)
void build_routing_table() {
}

static struct routing_table_entry hop_info1 = {
    .dst_mac = {.addr_bytes = {0x5a, 0xd4, 0xea, 0x8a, 0xd2, 0x60}},
    .dst_port = 0
};
static struct routing_table_entry hop_info2 = {
    .dst_mac = {.addr_bytes = {0xd2, 0x09, 0xea, 0x8a, 0xd2, 0x60}},
    .dst_port = 1
};

struct routing_table_entry* get_next_hop(uint32_t ip) {
	if (ip == rte_cpu_to_be_32(IPv4(10,0,0,2))) {
		return &hop_info1;
	} else if (ip == rte_cpu_to_be_32(IPv4(192,168,0,2))) {
		return &hop_info2;
	} else {
		return NULL;
	}
}

void add_route(uint32_t ip_addr, uint8_t prefix, struct ether_addr* mac_addr, uint8_t port) {
	return; // do nothing
}

