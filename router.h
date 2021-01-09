#ifndef ROUTER_H__
#define ROUTER_H__

#include <stdint.h>
#include <stdlib.h>
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

#include "utils/utils.h"
#include "utils/pointer_list.h"
#include "dpdk_init.h"

void router_init();
void router_finalize();
int parse_args(int argc, char **argv);
void start_router(unsigned int);

#endif
