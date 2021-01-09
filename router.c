#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_arp.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_launch.h>

#include <arpa/inet.h>

#include "router.h"
#include "dpdk_init.h"
#include "routing_table.h"

// An arbitrary maximum decimal digit length for those options that specify a number.
#define MAX_DEC_DIGIT_LEN 10
// Should the master lcore do work?
#define MASTER_LCORE_DOES_WORK
#ifndef MASTER_LCORE_DOES_WORK
#define DPDK_MIN_WORKER_ID 1
#else
#define DPDK_MIN_WORKER_ID 0
#endif
// DPDK value of minimum slave id.
#define DPDK_MIN_SLAVE_ID 1
// Burst size will determine the frame capture buffer length.
#define MAX_BURST_SIZE 32
// Maximum number of trials for transmitting a packet.
#define MAX_TRANSMIT_TRIAL 10

typedef struct interface_config
{
    dpdk_interface int_id;
    ipv4_addr addr;
    ether_addr mac;
} interface_config, *interface_config_ptr;

typedef struct thread_config
{
    pointer_list int_confs;
    uint16_t worker_count;
    dpdk_queue q_id;
} thread_config, *thread_config_ptr;

static pointer_list int_confs;
static pointer_list thr_confs;
static volatile bool force_quit;

//---------'interface_config' FUNCTIONS------------------
static void interface_config_print(const generic_ptr ptr)
{
    if (ptr == NULL)
        return;
    const interface_config_ptr int_conf = (const interface_config_ptr)ptr;
    char mac_str[ETHER_ADDR_FMT_SIZE], msg_str[MAX_STR_LEN];
    ether_format_addr(mac_str, ETHER_ADDR_FMT_SIZE, &int_conf->mac);
    snprintf(msg_str, MAX_STR_LEN,
             "-p argument: interface id %d, ipv4 addr 0x%08x, MAC %s\n",
             int_conf->int_id, int_conf->addr, mac_str);
    printf(msg_str);
}

//---------'thread_config' FUNCTIONS-----------------------
static void thread_config_init(thread_config_ptr self, uint16_t worker_count, dpdk_queue q_id)
{
    pointer_list_init(&self->int_confs);
    self->worker_count = worker_count;
    self->q_id = q_id;
}

/**
 * Signal handler is usually called upon SIGINT or SIGTERM. It
 * signals the router to quit and finalize.
*/
static void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("\n\nSignal %d received, preparing to exit...\n",
               signum);
        force_quit = true;
    }
}

/**
 * self function parses the option '-p' into DPDK interface id and the
 * corresponding ipv4 address.
*/
static interface_config_ptr parse_option_p(char *arg)
{
    int i, int_id, status;
    ipv4_addr addr;

    // Get the index of next ',' character.
    if ((i = index_of_first_char(arg, ',', MAX_DEC_DIGIT_LEN)) == -1)
        return NULL;

    // Convert the number specified before the ','.
    arg[i] = '\0';
    // Check if all characters are decimal.
    if (!are_all_char_decimal(arg))
        return NULL;
    int_id = atoi(arg);
    arg[i++] = ',';
    // Interface value must be a 1-byte unsigned integer.
    if (!(int_id >= DPDK_MIN_INTERFACE_VAL && int_id <= DPDK_MAX_INTERFACE_VAL))
        return NULL;

    // Get the ipv4 address.
    status = ipv4_addr_from_str(arg + i, &addr);
    if (status == -1)
        return NULL;

    // Create a router config and fill its values.
    interface_config_ptr res = (interface_config_ptr)malloc(sizeof(interface_config));
    res->int_id = (dpdk_interface)int_id;
    res->addr = addr;
    rte_eth_macaddr_get(int_id, &res->mac);

    return res;
}

/**
 * self function parses the option '-r' into ipv4 CIDR destination address,
 * next hop MAC address and DPDK interface.
*/
bool parse_option_r(char *arg)
{
    ipv4_addr addr;
    ether_addr mac;
    int begin_idx = 0, end_idx, offset, cidr, int_id, status;

    // Get the index of next '/' character.
    if ((offset = index_of_first_char(arg + begin_idx, '/', IPV4_MAX_ADDR_LEN)) == -1)
        return false;
    end_idx = begin_idx + offset;

    // Convert the ipv4 address specified before the '/'.
    arg[end_idx] = '\0';
    status = ipv4_addr_from_str(arg + begin_idx, &addr);
    arg[end_idx++] = '/';
    // Check validity of the addr.
    if (status == -1)
        return false;
    begin_idx = end_idx;

    // Get the index of next ',' character.
    if ((offset = index_of_first_char(arg + begin_idx, ',', MAX_DEC_DIGIT_LEN)) == -1)
        return false;
    end_idx = begin_idx + offset;

    // Convert the number specified before the ','.
    arg[end_idx] = '\0';
    cidr = atoi(arg + begin_idx);
    arg[end_idx++] = ',';
    // CIDR value must be between 0 and 32.
    if (!(cidr >= IPV4_MIN_CIDR_VAL && cidr <= IPV4_MAX_CIDR_VAL))
        return false;
    begin_idx = end_idx;

    // Get the index of next ',' character.
    if ((offset = index_of_first_char(arg + begin_idx, ',', MAC_ADDR_LEN)) == -1)
        return false;
    end_idx = begin_idx + offset;

    // Convert the mac address specified before the ','.
    arg[end_idx] = '\0';
    status = mac_addr_from_str(arg + begin_idx, &mac);
    arg[end_idx++] = ',';
    // Check validity of the addr.
    if (status == -1)
        return false;
    begin_idx = end_idx;

    // Make sure the interface id string is not too long.
    if (strlen(arg + begin_idx) >= MAX_DEC_DIGIT_LEN)
        return false;
    // Check if all characters are decimal.
    if (!are_all_char_decimal(arg + begin_idx))
        return false;
    // Get the interface id value.
    int_id = atoi(arg + begin_idx);
    // Interface value must be a 1-byte unsigned integer.
    if (!(int_id >= DPDK_MIN_INTERFACE_VAL && int_id <= DPDK_MAX_INTERFACE_VAL))
        return false;

    // Create a router config and fill its values.
    add_route(addr, (uint8_t)cidr, &mac, int_id);
    return true;
}

/** 
 * Usage of applicaiton
 */
static void usage()
{
    printf(
        "-p for specifying a DPDK interface and the corresponding IP address to attach self router program (comma separated).\n"
        "-r for specifying a routing entry which will be used for forwarding IP packets on attached interfaces (comma separated).\n");
}

/**
 * Performs ipv4 header validity checks that have not been already done,
 * according to https://tools.ietf.org/html/rfc1812#section-5.2.2 .
*/
static bool is_ipv4_hdr_valid(struct ipv4_hdr *hdr, uint32_t payload_size)
{
    // Check if the checksum is correct.
    uint16_t checksum = hdr->hdr_checksum;
    hdr->hdr_checksum = 0;
    uint16_t calc_chksm = rte_ipv4_cksum(hdr);
    if (calc_chksm != checksum)
        return false;
    // Check if the ip version is 4.
    uint8_t version = (hdr->version_ihl & IPV4_HDR_VER_MASK) >> IPV4_HDR_VER_BIT_OFFSET;
    if (version != 4)
        return false;
    // Check if the header length is at least 20 bytes.
    uint8_t ihl = (hdr->version_ihl & IPV4_HDR_IHL_MASK);
    if (ihl < 5)
        return false;
    // Check if the datagram length is enough to hold the header.
    uint16_t tot_len = rte_be_to_cpu_16(hdr->total_length);
    if (tot_len < IPV4_IHL_MULTIPLIER * ihl)
        return false;

    // Additionally check if the total length reported by the ipv4 header is no more than the frame payload size.
    if ((uint32_t)tot_len > payload_size)
        return false;
    // Check if the TTL is zero (it must not be).
    if (hdr->time_to_live == 0)
        return false;
    return true;
}

/**
 * self function sends the ipv4 packet to the given next hop, given it is valid.
*/
static void thread_send_ipv4_packet(
    thread_config_ptr thr_conf, interface_config_ptr int_conf,
    struct rte_mbuf *buf, struct routing_table_entry *next_hop)
{
    // Make sure the next hop is valid.
    if (next_hop == NULL)
    {
        rte_pktmbuf_free(buf);
        return;
    }
    // Decrement the TTL and check if it is 0.
    struct ipv4_hdr *hdr = rte_pktmbuf_mtod_offset(
        buf, struct ipv4_hdr *,
        sizeof(struct ether_hdr));
    hdr->time_to_live--;
    if (hdr->time_to_live == 0)
    {
        // TODO: send a 'TTL exceeded' icmp error back to the sender.
        rte_pktmbuf_free(buf);
        return;
    }
    // Calculate the header checksum.
    hdr->hdr_checksum = 0;
    hdr->hdr_checksum = rte_ipv4_cksum(hdr);
    // Set the destination and source MAC addresses.
    struct ether_hdr *eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    rte_eth_macaddr_get(next_hop->dst_port, &eth->s_addr);
    ether_addr_copy(&next_hop->dst_mac, &eth->d_addr);
    // Send the packet.
    unsigned int i;
    for (i = 0; i < MAX_TRANSMIT_TRIAL; i++)
        if (rte_eth_tx_burst(next_hop->dst_port, thr_conf->q_id, &buf, 1))
            break;
    if (i >= MAX_TRANSMIT_TRIAL)
        rte_pktmbuf_free(buf);
}

/**
 * self function handles ipv4 packet inside an ethernet frame.
*/
static void thread_handle_ether_ipv4(thread_config_ptr thr_conf, interface_config_ptr int_conf, struct rte_mbuf *buf)
{
    struct ipv4_hdr *hdr = rte_pktmbuf_mtod_offset(
        buf, struct ipv4_hdr *,
        sizeof(struct ether_hdr));
    // Check if the ipv4 header is valid.
    if (!is_ipv4_hdr_valid(hdr, buf->pkt_len - sizeof(struct ether_hdr)))
    {
        rte_pktmbuf_free(buf);
        return;
    }
    // Get the next hop route entry.
    struct routing_table_entry *next_hop = get_next_hop(rte_be_to_cpu_32(hdr->dst_addr));
    thread_send_ipv4_packet(thr_conf, int_conf, buf, next_hop);
}

/**
 * Performs validity check on the fields of an ARP message. Currently
 * only mapping between MAC and IPv4 addresses are supported.
*/
static bool is_arp_msg_valid(struct arp_hdr *hdr, thread_config_ptr thr_conf)
{
    // Check ARP hardware type (HTYPE).
    if (rte_be_to_cpu_16(hdr->arp_hrd) != ARP_HRD_ETHER || hdr->arp_hln != 6)
    {
        return false;
    }
    // Check ARP protocol type (PTYPE).
    if (rte_be_to_cpu_16(hdr->arp_pro) != ETHER_TYPE_IPv4 || hdr->arp_pln != 4)
    {
        return false;
    }
    // Check if the target address belongs to any of our interfaces.
    unsigned int i, sz = pointer_list_len(&int_confs);
    for (i = 0; i < sz; i++)
    {
        interface_config_ptr int_conf = (interface_config_ptr)pointer_list_get(&int_confs, i);
        if (int_conf->addr == rte_be_to_cpu_32(hdr->arp_data.arp_tip))
        {
            return true;
        }
    }
    return false;
}

/**
 * self function handles arp request message inside an ethernet frame.
 * Sends back an arp reply with the hardware address of the protocol
 * address.
*/
static void thread_handle_ether_arp_request(thread_config_ptr thr_conf, interface_config_ptr int_conf, struct rte_mbuf *buf)
{
    struct arp_hdr *hdr = rte_pktmbuf_mtod_offset(
        buf, struct arp_hdr *,
        sizeof(struct ether_hdr));
    // Swap destination and source MAC addresses.
    struct ether_hdr *eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    ether_addr_copy(&eth->s_addr, &eth->d_addr);
    ether_addr_copy(&int_conf->mac, &eth->s_addr);
    // Change the op code to ARP reply.
    hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
    // Save sender mac and ip addresses.
    ether_addr sender_mac;
    ether_addr_copy(&hdr->arp_data.arp_sha, &sender_mac);
    ipv4_addr sender_ip = hdr->arp_data.arp_sip;
    // Set sender mac and ip addresses to our addresses.
    ether_addr_copy(&int_conf->mac, &hdr->arp_data.arp_sha);
    hdr->arp_data.arp_sip = hdr->arp_data.arp_tip;
    // Set the target addresses from the previous sender addresses.
    ether_addr_copy(&sender_mac, &hdr->arp_data.arp_tha);
    hdr->arp_data.arp_tip = sender_ip;
    // Send the ARP message.
    unsigned int i;
    for (i = 0; i < MAX_TRANSMIT_TRIAL; i++)
        if (rte_eth_tx_burst(int_conf->int_id, thr_conf->q_id, &buf, 1))
            break;
    if (i >= MAX_TRANSMIT_TRIAL)
        rte_pktmbuf_free(buf);
}

/**
 * self function handles arp message inside an ethernet frame. Currently
 * only supports MAC and IPv4 address resolution.
*/
static void thread_handle_ether_arp(thread_config_ptr thr_conf, interface_config_ptr int_conf, struct rte_mbuf *buf)
{
    struct arp_hdr *hdr = rte_pktmbuf_mtod_offset(
        buf, struct arp_hdr *,
        sizeof(struct ether_hdr));
    // Check if the arp message is valid.
    if (!is_arp_msg_valid(hdr, thr_conf))
    {
        rte_pktmbuf_free(buf);
        return;
    }
    // Check and handle the operation type, if possible.
    switch (rte_be_to_cpu_16(hdr->arp_op))
    {
    case ARP_OP_REQUEST:
        thread_handle_ether_arp_request(thr_conf, int_conf, buf);
        break;
    default:
        rte_pktmbuf_free(buf);
        return;
    }
}

/**
 * Checks if the frame can be further processed without worrying about the length
 * and the destination MAC address.
*/
static bool is_frame_valid(struct rte_mbuf *buf, interface_config_ptr int_conf)
{
    // Check if the frame is too short.
    if (buf->pkt_len < ETHER_HDR_LEN)
        return false;
    // Check if the frame was destined to us.
    struct ether_hdr *eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    if (!is_broadcast_ether_addr(&eth->d_addr) && !is_same_ether_addr(&eth->d_addr, &int_conf->mac))
        return false;
    // Check if the frame has a recognized payload type.
    uint16_t ether_type = rte_be_to_cpu_16(rte_pktmbuf_mtod(buf, struct ether_hdr *)->ether_type);
    uint32_t min_payload_len = 0;
    switch (ether_type)
    {
    case ETHER_TYPE_IPv4:
        min_payload_len = sizeof(struct ipv4_hdr);
        break;
    case ETHER_TYPE_IPv6:
        min_payload_len = sizeof(struct ipv6_hdr);
        break;
    case ETHER_TYPE_ARP:
        min_payload_len = sizeof(struct arp_hdr);
        break;
    default:
        return false;
    }
    // Check if the frame is too short given the payload type.
    if (buf->pkt_len < ETHER_HDR_LEN + min_payload_len)
        return false;
    // Check if the frame is too long.
    if (buf->pkt_len > ETHER_MAX_LEN - ETHER_CRC_LEN)
        return false;
    // Everything seems fine.
    return true;
}

/**
 * self function checks each layer 2 frame for its type and if it knows
 * how to handle the frame, it is further processed. Otherwise, the frame
 * is discarded and freed.
*/
static void thread_handle_frames(
    thread_config_ptr thr_conf, interface_config_ptr int_conf,
    struct rte_mbuf *bufs[MAX_BURST_SIZE], uint16_t rx)
{
    uint16_t i;
    for (i = 0; i < rx; i++)
    {
        // Check if the frame is valid first.
        if (!is_frame_valid(bufs[i], int_conf))
        {
            rte_pktmbuf_free(bufs[i]);
            continue;
        }
        // Check if we know how to process the payload type.
        uint16_t ether_type = rte_be_to_cpu_16(rte_pktmbuf_mtod(bufs[i], struct ether_hdr *)->ether_type);
        switch (ether_type)
        {
        case ETHER_TYPE_IPv4:
            thread_handle_ether_ipv4(thr_conf, int_conf, bufs[i]);
            break;
        case ETHER_TYPE_ARP:
            thread_handle_ether_arp(thr_conf, int_conf, bufs[i]);
            break;
        default:
            rte_pktmbuf_free(bufs[i]);
            break;
        }
    }
}

/**
 * Main function of the thread performing the packet processing.
 */
static int router_thread(void *arg)
{
    thread_config_ptr thr_conf = (thread_config_ptr)arg;
    pointer_list_ptr thr_int_confs = &thr_conf->int_confs;
    unsigned int i, nb_ports = pointer_list_len(thr_int_confs);
    uint16_t worker_count = thr_conf->worker_count;
    bool received_frames;

    while (!force_quit)
    {
        received_frames = false;
        for (i = 0; i < nb_ports; i++)
        {
            interface_config_ptr int_conf = (interface_config_ptr)pointer_list_get(thr_int_confs, i);
            struct rte_mbuf *bufs[MAX_BURST_SIZE];
            uint16_t rx = recv_from_device(int_conf->int_id, worker_count, bufs, MAX_BURST_SIZE);

            if (rx == 0)
                continue;
            received_frames = true;
            thread_handle_frames(thr_conf, int_conf, bufs, rx);
        }
        // If we did not receive any frames from any interface, then sleep a bit.
        if (!received_frames)
        {
            usleep(100);
        }
    }

    return 1;
}

/**
 * Initialize a thread handling the packet processing.
 */
static void start_thread(unsigned int thr_idx)
{
    thread_config_ptr thr_conf = (thread_config_ptr)pointer_list_get(&thr_confs, thr_idx);
    rte_eal_remote_launch(router_thread, thr_conf, thr_idx + DPDK_MIN_WORKER_ID);
}

/**
 * self function initializes all static variables of 'router.c' and
 * enables the parsing and starting functions.
*/
void router_init()
{
    // Allocate memory for the interface configuration list.
    pointer_list_init(&int_confs);

    // Allocate memory for the thread configurations.
    pointer_list_init(&thr_confs);

    // Set quit status to false and register signal handlers.
    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

/**
 * self function frees memory used by all static variables of 'router.c'.
*/
void router_finalize()
{
    // Clean up all of the interface configurations.
    pointer_list_deep_clear(&int_confs);

    // Clean up all of the thread configurations.
    unsigned int i, len;
    thread_config_ptr thr_conf;
    len = pointer_list_len(&thr_confs);
    for (i = 0; i < len; i++)
    {
        thr_conf = (thread_config_ptr)pointer_list_get(&thr_confs, i);
        pointer_list_clear(&thr_conf->int_confs);
    }
    pointer_list_deep_clear(&thr_confs);
}

/**
 * Parse the arguments for the application.
 *
 * self router offers 2 arguments. '-p' for specifying a DPDK interface and the
 * corresponding IP address to attach self router program. '-r' for specifying a
 * routing entry which will be used for forwarding IP packets on attached interfaces.
 */
int parse_args(int argc, char **argv)
{
    interface_config_ptr int_conf;
    int opt;
    while ((opt = getopt(argc, argv, "p:r:")) != EOF)
    {
        switch (opt)
        {
        /* DPDK interface and the IP address */
        case 'p':
            if ((int_conf = parse_option_p(optarg)) == NULL)
            {
                usage();
                break;
            }
            if (int_conf->int_id >= rte_eth_dev_count())
            {
                printf("interface id %d is greater than the number of available interfaces (%d).\n",
                       int_conf->int_id, rte_eth_dev_count());
                free(int_conf);
                break;
            }
            pointer_list_append(&int_confs, (generic_ptr)int_conf);
            break;
            /* routing entry */
        case 'r':
            if (!parse_option_r(optarg))
            {
                usage();
                break;
            }
            break;
        case 0:
        default:
            usage();
            router_finalize();
            return -1;
        }
    }
    build_routing_table();
    return 1;
}

/**
 * self function will evenly distribute DPDK interfaces to all available
 * lcores and launch them. Then wait for all to finish.
 * 
 * Note: Depending on the whether 'MASTER_LCORE_DOES_WORK' macro is defined,
 * the master lcore (main thread) will also be doing work along with all the
 * slave lcores.
*/
void start_router(unsigned int lcore_count)
{
    // The default value for 'lcore_count' is 0,
    // in which case we find the count on our own.
    if (lcore_count == 0)
        lcore_count = rte_lcore_count();
    if (lcore_count <= DPDK_MIN_WORKER_ID)
    {
        router_finalize();
        printf("%d lcores are not enough (%d needed).\n", lcore_count, DPDK_MIN_WORKER_ID + 1);
        return;
    }
    // Let's print all of the interface configurations.
    pointer_list_print(&int_confs, interface_config_print);
    // Let's print all of the route entries.
    print_routes();

    unsigned int i, len, thr_idx, thr_count;
    interface_config_ptr int_conf;
    thread_config_ptr thr_conf;

    thr_count = lcore_count - DPDK_MIN_WORKER_ID;
    // Create a configuration for each worker lcore (1 thread per core).
    for (i = DPDK_MIN_WORKER_ID; i < lcore_count; i++)
    {
        // Allocate.
        thr_conf = (thread_config_ptr)malloc(sizeof(thread_config));
        // Initialize.
        thread_config_init(thr_conf, thr_count, pointer_list_len(&thr_confs));
        // Append to the list.
        pointer_list_append(&thr_confs, (generic_ptr)thr_conf);
    }

    // Configure each interface and evenly distribute them to threads.
    len = pointer_list_len(&int_confs);
    for (i = 0; i < len; i++)
    {
        // Get interface configuration.
        int_conf = (interface_config_ptr)pointer_list_get(&int_confs, i);
        // Do actual configuration.
        configure_device(int_conf->int_id, thr_count);
        // Assign to the next thread.
        thr_idx = i % thr_count;
        // Get the corresponding thread configuration.
        thr_conf = (thread_config_ptr)pointer_list_get(&thr_confs, thr_idx);
        // Append the interface configuration to the list.
        pointer_list_append(&thr_conf->int_confs, (generic_ptr)int_conf);
    }

    // Launch all of the slave worker threads.
    for (i = DPDK_MIN_SLAVE_ID - DPDK_MIN_WORKER_ID; i < thr_count; i++)
    {
        start_thread(i);
    }
#ifdef MASTER_LCORE_DOES_WORK
    router_thread(pointer_list_get(&thr_confs, 0));
#endif
    rte_eal_mp_wait_lcore();

    router_finalize();
}
