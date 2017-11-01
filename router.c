#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_arp.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_launch.h>

#include <arpa/inet.h>

#include "dpdk_init.h"
#include "routing_table.h"
#include "router.h"


int router_thread(void* arg) {
    return 1;
}

void parse_route(char *route) {
}

int parse_args(int argc, char **argv) {
    return 1;
}

void start_thread(uint8_t port) {
}

