#ifndef ROUTING_TABLE_H__
#define ROUTING_TABLE_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

#include "utils/utils.h"

typedef struct ipv4_cidr_addr
{
	ipv4_addr addr;
	uint8_t cidr;
} ipv4_cidr_addr, *ipv4_cidr_addr_ptr;

typedef struct route_entry
{
	ipv4_cidr_addr dest;
	ether_addr dst_mac;
	dpdk_interface dst_port;
} route_entry, *route_entry_ptr;

void routing_table_init_dummy();
void routing_table_finalize_dummy();

void add_route_dummy(ipv4_addr addr, uint8_t cidr, ether_addr *mac, dpdk_interface port);
route_entry_ptr get_next_hop_dummy(ipv4_addr addr);

void route_entry_print_dummy(const generic_ptr ptr);
void routing_table_print_dummy();

#endif
