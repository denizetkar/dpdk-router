#ifndef __UTILS_H
#define __UTILS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_STR_LEN 1024

// Maximum character length of each ipv4 address group.
#define IPV4_GROUP_LEN 3
// Number of groups in a valid ipv4 address.
#define IPV4_NUM_GROUPS 4
// Number of dots (group seperator) is 1 less that the number of groups.
#define IPV4_NUM_DOTS (IPV4_NUM_GROUPS - 1)
// Maximum character length of an ipv4 address.
#define IPV4_MAX_ADDR_LEN 15
// IPv4 header version mask for version_ihl byte
#define IPV4_HDR_VER_MASK 0xf0
// IPv4 header version bit offset from the least significant bit
#define IPV4_HDR_VER_BIT_OFFSET 4
// IPv4 header expected checksum addition result
#define IPV4_CHKSM_ADD_RES 0xffff

#define IPV4_MIN_GROUP_VAL 0x00
#define IPV4_MAX_GROUP_VAL 0xff
#define DPDK_MIN_INTERFACE_VAL 0x00
#define DPDK_MAX_INTERFACE_VAL 0xff
#define IPV4_MIN_CIDR_VAL 0
#define IPV4_MAX_CIDR_VAL 32

// Character length of each mac address group.
#define MAC_GROUP_LEN 2
// Number of groups in a valid mac address.
#define MAC_NUM_GROUPS 6
// Number of colons (group seperator) is 1 less that the number of groups.
#define MAC_NUM_COLONS (MAC_NUM_GROUPS - 1)
// Character length of a valid MAC address.
#define MAC_ADDR_LEN 17

#define MAC_MIN_GROUP_VAL 0x00
#define MAC_MAX_GROUP_VAL 0xff

typedef uint8_t dpdk_interface;
typedef uint16_t dpdk_queue;
typedef uint32_t ipv4_addr;
typedef struct ether_addr ether_addr;
typedef void *generic_ptr;
typedef void (*printer_fn)(const generic_ptr);

int index_of_first_char(const char *str, char ch, int max);
bool are_all_char_decimal(const char *str);
bool are_all_char_hexadecimal(const char *str);
int32_t ipv4_group_val_from_group_str(const char *group_str, int group_idx);
int ipv4_addr_from_str(char *addr_str, ipv4_addr *addr);
int mac_addr_from_str(char *addr_str, ether_addr *mac);

#endif