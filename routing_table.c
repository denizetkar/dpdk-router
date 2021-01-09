#include "routing_table.h"
#include "utils/utils.h"

#include <stdio.h>

// Must not be bigger than 32 !
#define TBL_PREFIX_LEN 24

#define TBL24_TABLE_SIZE (1 << TBL_PREFIX_LEN)
#define TBLLONG_ENTRY_SIZE (1 << (32 - TBL_PREFIX_LEN))
// Must not be bigger than ((1 << 15) - 1) !
#define TBLLONG_TABLE_SIZE 255
// Must not be bigger than (1 << 15) !
#define NH_ID_TO_INFO_SIZE (1 << 8)
#define INVALID_NH_ID (NH_ID_TO_INFO_SIZE - 1)

// 2-byte entry to be used in tbl24 table.
typedef union
{
    struct
    {
        uint16_t next_id : 15;
        uint16_t is_long : 1;
    };
    uint16_t val;
} tbl24_entry, *tbl24_entry_ptr;
// Each tbllong entry is itself a table (array).
typedef uint16_t tbllong_entry[TBLLONG_ENTRY_SIZE];
typedef tbllong_entry *tbllong_entry_ptr;
// The struct to hold information about a pair of (destination prefix, next hop).
// So, multiple next_hop_info might point to the same next hop (mac_addr, port_id).
typedef struct
{
    uint32_t ip_addr;
    uint8_t prefix;
    struct routing_table_entry next_hop;
    bool in_use;
} next_hop_info, *next_hop_info_ptr;

static tbl24_entry tbl24_table[TBL24_TABLE_SIZE];
static tbllong_entry tbllong_table[TBLLONG_TABLE_SIZE];
static uint16_t tbllong_table_idx;
static next_hop_info nh_id_to_info[NH_ID_TO_INFO_SIZE] = {0};
static uint16_t nh_id_to_info_idx = 0;

void add_route(uint32_t ip_addr, uint8_t prefix, struct ether_addr *mac_addr, uint8_t port)
{
    if (nh_id_to_info_idx >= INVALID_NH_ID)
    {
        printf("ERROR: cannot add any more routes!\n");
        exit(EXIT_FAILURE);
    }
    next_hop_info_ptr nh_info = &nh_id_to_info[nh_id_to_info_idx++];
    nh_info->ip_addr = ip_addr;
    nh_info->prefix = (prefix <= 32) ? prefix : 32;
    ether_addr_copy(mac_addr, &nh_info->next_hop.dst_mac);
    nh_info->next_hop.dst_port = port;
    nh_info->in_use = true;
}

static void _build_route_lte_24_long_idx(uint16_t nh_id, uint16_t long_idx)
{
    tbllong_entry_ptr tbllong_ent = &tbllong_table[long_idx];
    uint64_t i;
    // Iterate over all tbllong entry ports and replace the port id if possible.
    for (i = 0; i < TBLLONG_ENTRY_SIZE; i++)
    {
        uint16_t curr_nh_id = (*tbllong_ent)[i];
        // If the current port is not used, then replace it.
        if (curr_nh_id == INVALID_NH_ID)
        {
            (*tbllong_ent)[i] = nh_id;
            continue;
        }
        // If the current port id belongs to a lesser or equal destination prefix, then replace it.
        if (nh_id_to_info[curr_nh_id].prefix <= nh_id_to_info[nh_id].prefix)
        {
            (*tbllong_ent)[i] = nh_id;
        }
    }
}

static void _build_route_lte_24_idx(uint16_t nh_id, uint32_t idx)
{
    // If the tbl24 entry is not used, then start using it.
    if (tbl24_table[idx].val == INVALID_NH_ID)
    {
        tbl24_table[idx].next_id = nh_id;
        tbl24_table[idx].is_long = 0;
        return;
    }

    // If the tbl24 entry is used by a destination prefix <= 24.
    if (tbl24_table[idx].is_long == 0)
    {
        // If the entry is used by a lesser or equal destination prefix, then replace it.
        if (nh_id_to_info[tbl24_table[idx].next_id].prefix <= nh_id_to_info[nh_id].prefix)
        {
            tbl24_table[idx].next_id = nh_id;
        }
        return;
    }

    // If the entry is used by a destination prefix > 24.
    _build_route_lte_24_long_idx(nh_id, tbl24_table[idx].next_id);
}

static void _build_route_lte_24(uint16_t nh_id)
{
    next_hop_info_ptr nh_info = &nh_id_to_info[nh_id];
    uint32_t min_index, max_index;
    min_index = (nh_info->ip_addr >> (32 - nh_info->prefix)) << (TBL_PREFIX_LEN - nh_info->prefix);
    max_index = min_index | ((1 << (TBL_PREFIX_LEN - nh_info->prefix)) - 1);

    uint64_t idx;
    for (idx = min_index; idx <= max_index; idx++)
    {
        _build_route_lte_24_idx(nh_id, (uint32_t)idx);
    }
}

static void _build_route_gt_24_long_idx(uint16_t nh_id, uint16_t long_idx)
{
    next_hop_info_ptr nh_info = &nh_id_to_info[nh_id];
    uint32_t min_i, max_i;
    min_i = (nh_info->ip_addr & ((1 << (32 - TBL_PREFIX_LEN)) - 1)) & (~((1 << (32 - nh_info->prefix)) - 1));
    max_i = min_i | ((1 << (32 - nh_info->prefix)) - 1);

    tbllong_entry_ptr tbllong_ent = &tbllong_table[long_idx];
    uint64_t i;
    for (i = min_i; i <= max_i; i++)
    {
        uint16_t curr_nh_id = (*tbllong_ent)[i];
        // If the current port is not used, then replace it.
        if (curr_nh_id == INVALID_NH_ID)
        {
            (*tbllong_ent)[i] = nh_id;
            continue;
        }
        // If the current port id belongs to a lesser or equal destination prefix, then replace it.
        if (nh_id_to_info[curr_nh_id].prefix <= nh_id_to_info[nh_id].prefix)
        {
            (*tbllong_ent)[i] = nh_id;
        }
    }
}

static void _build_route_gt_24_idx(uint16_t nh_id, uint32_t idx)
{
    if (tbllong_table_idx >= TBLLONG_TABLE_SIZE)
    {
        printf("ERROR: tbllong table size is exceeded!\n");
        exit(EXIT_FAILURE);
    }

    uint16_t long_idx = tbl24_table[idx].next_id = tbllong_table_idx++;
    tbl24_table[idx].is_long = 1;

    next_hop_info_ptr nh_info = &nh_id_to_info[nh_id];
    uint32_t min_i, max_i;
    min_i = (nh_info->ip_addr & ((1 << (32 - TBL_PREFIX_LEN)) - 1)) & (~((1 << (32 - nh_info->prefix)) - 1));
    max_i = min_i | ((1 << (32 - nh_info->prefix)) - 1);

    tbllong_entry_ptr tbllong_ent = &tbllong_table[long_idx];
    uint64_t i;
    for (i = min_i; i <= max_i; i++)
    {
        (*tbllong_ent)[i] = nh_id;
    }
}

static void _build_route_gt_24(uint16_t nh_id)
{
    uint32_t idx = nh_id_to_info[nh_id].ip_addr >> (32 - TBL_PREFIX_LEN);
    // If the tbl24 entry is not used, then start using it.
    if (tbl24_table[idx].val == INVALID_NH_ID)
    {
        _build_route_gt_24_idx(nh_id, idx);
        return;
    }

    // If the tbl24 entry is used by a destination prefix <= 24.
    if (tbl24_table[idx].is_long == 0)
    {
        uint16_t cur_nh_id = tbl24_table[idx].next_id;
        _build_route_gt_24_idx(nh_id, idx);
        _build_route_lte_24_long_idx(cur_nh_id, tbl24_table[idx].next_id);
        return;
    }

    // If the entry is used by a destination prefix > 24.
    _build_route_gt_24_long_idx(nh_id, tbl24_table[idx].next_id);
}

void build_routing_table()
{
    tbllong_table_idx = 0;

    uint64_t i, j;
    for (i = 0; i < TBL24_TABLE_SIZE; i++)
    {
        tbl24_table[i].val = INVALID_NH_ID;
    }
    for (i = 0; i < TBLLONG_TABLE_SIZE; i++)
        for (j = 0; j < TBLLONG_ENTRY_SIZE; j++)
            tbllong_table[i][j] = INVALID_NH_ID;

    uint16_t nh_id;
    for (nh_id = 0; nh_id < nh_id_to_info_idx; nh_id++)
    {
        next_hop_info_ptr nh_info = &nh_id_to_info[nh_id];
        if (!nh_info->in_use)
            continue;

        if (nh_info->prefix <= TBL_PREFIX_LEN)
            _build_route_lte_24(nh_id);
        else
            _build_route_gt_24(nh_id);
    }
    nh_id_to_info_idx = 0;
}

void print_routes()
{
    uint16_t nh_id;
    for (nh_id = 0; nh_id_to_info[nh_id].in_use; nh_id++)
    {
        next_hop_info_ptr nh_info = &nh_id_to_info[nh_id];
        char mac_str[ETHER_ADDR_FMT_SIZE], msg_str[MAX_STR_LEN];
        ether_format_addr(mac_str, ETHER_ADDR_FMT_SIZE, &nh_info->next_hop.dst_mac);
        snprintf(msg_str, MAX_STR_LEN,
                 "-r argument: ipv4 addr 0x%08x, cidr %d, MAC %s, interface id %d\n",
                 nh_info->ip_addr, nh_info->prefix, mac_str, nh_info->next_hop.dst_port);
        printf(msg_str);
    }
}
void print_port_id_to_mac() {}
void print_next_hop_tab() {}
void print_routing_table_entry(struct routing_table_entry *info) {}

struct routing_table_entry *get_next_hop(uint32_t ip)
{
    uint32_t idx = ip >> (32 - TBL_PREFIX_LEN);

    if (tbl24_table[idx].val == INVALID_NH_ID)
        return NULL;

    if (tbl24_table[idx].is_long == 0)
        return &nh_id_to_info[tbl24_table[idx].next_id].next_hop;

    tbllong_entry_ptr tbllong_ent = &tbllong_table[tbl24_table[idx].next_id];
    uint32_t ent_idx = ip & ((1 << (32 - TBL_PREFIX_LEN)) - 1);
    uint16_t nh_id = (*tbllong_ent)[ent_idx];
    if (nh_id == INVALID_NH_ID)
        return NULL;
    return &nh_id_to_info[nh_id].next_hop;
}
