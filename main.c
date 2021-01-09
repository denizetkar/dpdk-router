#include "router.h"

/**
 * Main function of the router.
 */
int main(int argc, char *argv[])
{
    // 'init_dpdk' MIGHT BE PROBLEMATIC, see https://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html#lcore-related-options
    init_dpdk();
    router_init();
    parse_args(argc, argv);
    start_router(0);

    return 0;
}
