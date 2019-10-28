Setting up DPDK
===============

Compile DPDK
    cd dpdk
    make config T=x86_64-native-linuxapp-gcc
    make T=x86_64-native-linuxapp-gcc

Load the `igb_uio` driver and its dependency `uio`
	modprobe uio
	insmod build/kmod/igb_uio.ko

Use the tool `dpdk-devbind.py` to bind the VirtIO NICs to the `igb_uio` driver.
(Find out how to do this, the tool is pretty self-explanatory.)
	usertools/dpdk-devbind.py --h

Setup hugetlbfs huge pages for DPDK
	mkdir /mnt/huge
	mount -t hugetlbfs nodev /mnt/huge
Statically allocate 256 2MB huge pages
	echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

Compiling your App
==================

This example project comes with a CMakeFile and a simple wrapper that initializes DPDK for you.
Run the following steps to build the router app
    cmake .
    make

That's all!

Compiling gtest
===============

Compiling the gtest library is required to execute table-test

Fetching gtest
    apt-get install libgtest-dev

Compiling gtest
    cd /usr/src/googletest/googletest
    cmake .
    make
    make install

Remark on ACN-VM
================

add recv_from_device() utility function

Apparently our virtual switch works different from last year and sets rx = tx queues with automatic load balancing
(even when not configured) so this is a simple work-around.


