#include <rte_ether.h>

#include "utils.h"

/**
 * This function returns the index of next 'ch' in 'str' if it exists
 * and if the index is no more than 'max'. Otherwise, returns -1.
*/
int index_of_first_char(const char *str, char ch, int max)
{
    int i = 0;
    char curr_ch;
    while ((curr_ch = str[i]) != ch)
    {
        if (curr_ch == '\0' || i == max)
            return -1;

        i++;
    }

    return i;
}

/**
 * This function returns 1 if all characters are decimal numbers.
 * Otherwise, returns 0.
*/
bool are_all_char_decimal(const char *str)
{
    char ch;
    while ((ch = *str) != '\0')
    {
        if (!(ch >= '0' && ch <= '9'))
            return false;

        str++;
    }
    return true;
}

/**
 * This function returns 1 if all characters are hexadecimal numbers.
 * Otherwise, returns 0.
*/
bool are_all_char_hexadecimal(const char *str)
{
    char ch;
    while ((ch = *str) != '\0')
    {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')))
            return false;

        str++;
    }
    return true;
}

/**
 * This function returns the 32-bit value of each of the 4 groups
 * that exists in a valid ipv4 address.
*/
int32_t ipv4_group_val_from_group_str(const char *group_str, int group_idx)
{
    int32_t group_val;

    group_val = atoi(group_str);
    // Each group must have a value between 0 and 255 (1 byte).
    if (!(group_val >= IPV4_MIN_GROUP_VAL && group_val <= IPV4_MAX_GROUP_VAL))
        return -1;
    return group_val << ((IPV4_NUM_DOTS - group_idx) * 8);
}

/**
 * This function assumes 'addr_str' to be in the usual 4 group ipv4 format.
 * 
 * Example: "1.2.3.4"
*/
int ipv4_addr_from_str(char *addr_str, ipv4_addr *addr)
{
    int begin_idx = 0, end_idx, offset, group_idx;
    int32_t group_val;
    ipv4_addr res = 0;

    for (group_idx = 0; group_idx < IPV4_NUM_DOTS; group_idx++)
    {
        // Get the index of next '.' character.
        if ((offset = index_of_first_char(addr_str + begin_idx, '.', IPV4_GROUP_LEN)) == -1)
            return -1;
        end_idx = begin_idx + offset;

        // Convert the number specified before the '.'.
        addr_str[end_idx] = '\0';
        // Check if all characters are decimal.
        if (!are_all_char_decimal(addr_str + begin_idx))
            return -1;
        group_val = ipv4_group_val_from_group_str(addr_str + begin_idx, group_idx);
        addr_str[end_idx++] = '.';
        // Check the group value and add it to the result.
        if (group_val == -1)
            return -1;
        begin_idx = end_idx;
        res += group_val;
    }

    // Make sure the last group string is at most 3 characters long.
    if (strlen(addr_str + begin_idx) > IPV4_GROUP_LEN)
        return -1;
    // Check if all characters are decimal.
    if (!are_all_char_decimal(addr_str + begin_idx))
        return -1;
    // Handle the final group.
    if ((group_val = ipv4_group_val_from_group_str(addr_str + begin_idx, group_idx)) == -1)
        return -1;
    res += group_val;

    // Write the verified ipv4 address.
    *addr = res;
    return 0;
}

/**
 * This function assumes 'addr_str' to be in the usual 6 hexadecimal group format.
 * 
 * Example: "01:23:45:67:89:ab"
*/
int mac_addr_from_str(char *addr_str, ether_addr *mac)
{
    int begin_idx = 0, end_idx, offset, group_idx;
    int group_val;

    for (group_idx = 0; group_idx < MAC_NUM_COLONS; group_idx++)
    {
        // Get the index of next ':' character.
        if ((offset = index_of_first_char(addr_str + begin_idx, ':', MAC_GROUP_LEN)) == -1)
            return -1;
        end_idx = begin_idx + offset;

        // Convert the number specified before the ':'.
        addr_str[end_idx] = '\0';
        // Check if all characters are hexadecimal.
        if (!are_all_char_hexadecimal(addr_str + begin_idx))
            return -1;
        group_val = strtol(addr_str + begin_idx, NULL, 16);
        addr_str[end_idx++] = ':';
        // Each group must have a value between 0 and 255 (1 byte).
        if (!(group_val >= MAC_MIN_GROUP_VAL && group_val <= MAC_MAX_GROUP_VAL))
            return -1;
        begin_idx = end_idx;

        mac->addr_bytes[group_idx] = (uint8_t)group_val;
    }

    // Make sure the last group string is exactly 2 characters long.
    if (strlen(addr_str + begin_idx) != MAC_GROUP_LEN)
        return -1;
    // Check if all characters are hexadecimal.
    if (!are_all_char_hexadecimal(addr_str + begin_idx))
        return -1;
    // Handle the final group.
    group_val = strtol(addr_str + begin_idx, NULL, 16);
    // Each group must have a value between 0 and 255 (1 byte).
    if (!(group_val >= MAC_MIN_GROUP_VAL && group_val <= MAC_MAX_GROUP_VAL))
        return -1;

    mac->addr_bytes[group_idx] = (uint8_t)group_val;
    return 0;
}
