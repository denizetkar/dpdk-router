#ifndef DPDK_INIT_H__
#define DPDK_INIT_H__

#include <stdint.h>
#include <unistd.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

void init_dpdk();
void configure_device(uint8_t port_id, uint16_t num_tx_queues);

static inline uint32_t recv_from_device(uint8_t port_id, uint16_t num_rx_queues, struct rte_mbuf* bufs[], uint32_t num_bufs) {
	uint32_t rx = 0;
	for (uint16_t i = 0; i < num_rx_queues && rx < num_bufs; ++i) {
		rx += rte_eth_rx_burst(port_id, i, bufs + rx, num_bufs - rx);
	}
	if (!rx) usleep(100);
	return rx;
}


#endif
