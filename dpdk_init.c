/**
 * Basic init tasks (memory pools, devices, dpdk framework) for DPDK applications.
 */
#include "dpdk_init.h"

#include <stdlib.h>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>


static void check_dpdk_error(uint32_t rc, const char* operation) {
	if (rc) {
		printf("could not %s: %s\n", operation, rte_strerror(-rc));
		exit(1);
	}
}

static const uint32_t RX_DESCS = 256;
static const uint32_t TX_DESCS = 256;
static const uint32_t MEMPOOL_CACHE_SIZE = 256;
static const uint32_t MEMPOOL_SIZE = 2047;
static const uint32_t MBUF_SIZE = 1600;

static struct rte_mempool* create_mempool() {
	static volatile int pool_id = 0;
	char pool_name[32];
	sprintf(pool_name, "pool%d", __sync_fetch_and_add(&pool_id, 1));
	struct rte_mempool* pool = rte_pktmbuf_pool_create(pool_name, MEMPOOL_SIZE, MEMPOOL_CACHE_SIZE,
			0, MBUF_SIZE + RTE_PKTMBUF_HEADROOM,
			rte_socket_id()
			);
	if (!pool) {
		printf("could not allocate mempool\n");
		exit(1);
	}
	return pool;
}

/**
 * Initialize a device by configuring hardware queues.
 *
 * Number of allocated queues for device with port_id:
 * - 1 RX queue
 * - num_tx_queues TX queues
 */
void configure_device(uint8_t port_id, uint16_t num_tx_queues) {
	struct rte_eth_conf port_conf = { 0 };
	check_dpdk_error(rte_eth_dev_configure(port_id, 1, num_tx_queues, &port_conf), "configure device");
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id, &dev_info);
	check_dpdk_error(rte_eth_rx_queue_setup(port_id, 0, RX_DESCS, rte_socket_id(), &dev_info.default_rxconf, create_mempool()), "configure rx queue");
	for (uint16_t queue = 0; queue < num_tx_queues; ++queue) {
		check_dpdk_error(rte_eth_tx_queue_setup(port_id, queue, TX_DESCS, rte_socket_id(), &dev_info.default_txconf), "configure tx queue");
	}
	check_dpdk_error(rte_eth_dev_start(port_id), "starting device");
}


void init_dpdk() {
	uint32_t argc = 3;
	char* argv[argc];
	argv[0] = "-c1";
	argv[1] = "--lcores=(0-7)@0";
	argv[2] = "-n1";
	rte_eal_init(argc, argv);
}

