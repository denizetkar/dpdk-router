#ifndef DPDK_INIT_H__
#define DPDK_INIT_H__

#include <stdint.h>

void init_dpdk();
void configure_device(uint8_t port_id, uint16_t num_tx_queues);

#endif
