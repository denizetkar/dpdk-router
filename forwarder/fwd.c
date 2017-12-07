#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_launch.h>

#include "../dpdk_init.h"

uint8_t src_interface = 0;
uint8_t dst_interface = 0;

uint32_t sent_pkts = 0;
uint32_t recv_pkts = 0;

uint64_t timestamp = 0;

/**
 * Thread-specific configuration
 */
struct thread_config {
	uint8_t src_interface;
	uint16_t src_queue;
	uint8_t dst_interface;
	uint16_t dst_queue;
};

/**
 * Send a packet buffer from a hardware queue of an interface.
 */
void transmit_packet(uint8_t interface, uint16_t dst_queue, struct rte_mbuf* buf) {
	while (!rte_eth_tx_burst(interface, dst_queue, &buf, 1));
	++sent_pkts;
}

/**
 * Perform packet forwarding.
 */
void handle_packet(struct thread_config* config, struct rte_mbuf* buf) {

	/* map packet buffer to ethernet struct */
	struct ether_hdr* eth = rte_pktmbuf_mtod(buf, struct ether_hdr*);

	/* dummy packet processing */
	eth->s_addr.addr_bytes[5]++;

	/* send packet */
	transmit_packet(config->dst_interface, config->dst_queue, buf);

}

/**
 * Main function of the thread performing the packet processing.
 */
int run_thread(void* arg) {
	struct thread_config* config = (struct thread_config*) arg;
	struct rte_mbuf* bufs[64];
	while (1) {

		/* Receive a burst of packets */
		uint16_t rx = rte_eth_rx_burst(config->src_interface, config->src_queue, bufs, 64);
		if (!rx) usleep(100);
		for (uint16_t i = 0; i < rx; ++i) handle_packet(config, bufs[i]);
		recv_pkts += rx;

		/* statistics */
		if (rte_get_tsc_cycles() - timestamp > rte_get_timer_hz()) {
			timestamp = rte_get_tsc_cycles();
			printf("Received pkts: %i, sent pkts: %i\n", recv_pkts, sent_pkts);
		}	
	}
	return 0;
}

/**
 * Initialize a thread handling the packet processing.
 */
void start_thread() {
	struct thread_config* config = (struct thread_config*) malloc(sizeof(struct thread_config));
	config->src_interface = src_interface;
	config->src_queue = 0;
	config->dst_interface = dst_interface;
	config->dst_queue = 0;
	rte_eal_remote_launch(run_thread, config, 1);
}

/** 
 * Usage of applicaiton
 */
void usage() {
	printf("-s source interface [0-2] -d destination interface [0-2]\n");
}

/**
 * Parse the arguments for the application.
 *
 * This forwarder offers two arguments a source and a destination interface.
 * The traffic is forwarded from source to destination
 */
int parse_args(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "s:d:")) != EOF) {
		switch (opt) {
			/* source interface */
			case 's':
				src_interface = atoi(optarg);
				if (!(src_interface < 3)) {
					src_interface = 0;
					usage();
				}
				break;
				/* destination interface */
			case 'd':
				dst_interface = atoi(optarg);
				if (!(dst_interface < 3)) {
					dst_interface = 0;
					usage();
				}
				break;
			case 0:
			default:
				usage();
				return -1;
		}
	}
	return 1;
}

/**
 * Main function of the forwarding example.
 */
int main(int argc, char* argv[]) {

	init_dpdk();
	parse_args(argc, argv);

	/* open hardware queues */
	if (src_interface == dst_interface) {
		/* open 1x RX and 1xTX for src */
		configure_device(src_interface, 1);
		printf("same interface\n");
	} else {
		/* open 1x RX and 1xTX for src/dst */
		configure_device(dst_interface, 1);
		configure_device(src_interface, 1);
	}
	printf("Forwarding from interface %i to interface %i\n", src_interface, dst_interface);

	start_thread();
	rte_eal_mp_wait_lcore();

}

