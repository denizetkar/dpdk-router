#include <limits.h>
#include <gtest/gtest.h>
extern "C"
{
#include "../router.h"
#include "../routing_table.h"
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct ether_addr port_id_to_mac[256];

void check_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, int next_hop)
{
	int ip = IPv4(a, b, c, d);
	struct routing_table_entry *info = get_next_hop(ip);
	ASSERT_TRUE(info != NULL) << "entry for " << (int)a << "." << (int)b << "." << (int)c << "." << (int)d << " is null";
	EXPECT_EQ(next_hop, info->dst_port) << ip << " failed";
	EXPECT_EQ(0, memcmp(&info->dst_mac, &port_id_to_mac[next_hop], sizeof(struct ether_addr))) << ip << " failed";
	//print_routing_table_entry(info);
}

TEST(VERY_SIMPLE_TEST, SIMPLE_ADDRESSES)
{
	// create dummy mac addresses
	for (int i = 0; i < 256; ++i)
	{
		for (int a = 0; a < 6; ++a)
		{
			port_id_to_mac[i].addr_bytes[a] = (uint8_t)i;
		}
	}

	// init routing table stuff
	printf("Try to add routes.\n");
	add_route(IPv4(10, 0, 10, 0), 24, &port_id_to_mac[0], 0);
	add_route(IPv4(10, 0, 10, 10), 32, &port_id_to_mac[1], 1);
	printf("Routes added.\n");

	// call once before test
	build_routing_table();

	// test cases
	check_address(10, 0, 10, 10, 1);

	EXPECT_EQ(NULL, get_next_hop(IPv4(10, 0, 9, 255)));
	for (int i = 0; i < 10; ++i)
		check_address(10, 0, 10, i % 256, 0);
	check_address(10, 0, 10, 10, 1);
	for (int i = 11; i < 256; ++i)
		check_address(10, 0, 10, i % 256, 0);
	EXPECT_EQ(NULL, get_next_hop(IPv4(10, 0, 11, 0)));
}

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
