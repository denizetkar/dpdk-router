#include "dummy_routing_table.h"

#include <rte_config.h>
#include <rte_ip.h>

#include "utils/pointer_list.h"

static pointer_list rt_ents;

void routing_table_init_dummy()
{
	// Allocate memory for the route entry list.
	pointer_list_init(&rt_ents);
}

void routing_table_finalize_dummy()
{
	// Clean up all of the route entries.
	pointer_list_deep_clear(&rt_ents);
}

void add_route_dummy(ipv4_addr addr, uint8_t cidr, ether_addr *mac, dpdk_interface port)
{
	unsigned int i, sz = pointer_list_len(&rt_ents);
	for (i = 0; i < sz; i++)
	{
		route_entry_ptr rt_ent = pointer_list_get(&rt_ents, i);
		if (rt_ent->dest.cidr < cidr)
			break;
	}
	route_entry_ptr res = (route_entry_ptr)malloc(sizeof(route_entry));
	res->dest.addr = addr;
	res->dest.cidr = cidr;
	ether_addr_copy(mac, &res->dst_mac);
	res->dst_port = port;
	pointer_list_insert(&rt_ents, res, i);
}

route_entry_ptr get_next_hop_dummy(ipv4_addr addr)
{
	unsigned int i, sz = pointer_list_len(&rt_ents);
	for (i = 0; i < sz; i++)
	{
		route_entry_ptr rt_ent = pointer_list_get(&rt_ents, i);
		ipv4_cidr_addr prefix = rt_ent->dest;
		ipv4_addr subnet_mask = ((int32_t)(1 << 31)) >> (prefix.cidr - 1);

		if ((addr & subnet_mask) == prefix.addr)
			return rt_ent;
	}
	return NULL;
}

//---------'route_entry' FUNCTIONS-----------------------
void route_entry_print_dummy(const generic_ptr ptr)
{
	if (ptr == NULL)
		return;
	const route_entry_ptr rt_ent = ptr;
	char mac_str[ETHER_ADDR_FMT_SIZE], msg_str[MAX_STR_LEN];
	ether_format_addr(mac_str, ETHER_ADDR_FMT_SIZE, &rt_ent->dst_mac);
	snprintf(msg_str, MAX_STR_LEN,
			 "-r argument: ipv4 addr 0x%08x, cidr %d, MAC %s, interface id %d\n",
			 rt_ent->dest.addr, rt_ent->dest.cidr, mac_str, rt_ent->dst_port);
	printf(msg_str);
}

void routing_table_print_dummy()
{
	pointer_list_print(&rt_ents, route_entry_print_dummy);
}
