#ifndef ROUTER_H__
#define ROUTER_H__

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

int router_thread(void* arg);
void parse_route(char *route);
int parse_args(int argc, char **argv);
void start_thread(uint8_t port);

#endif

