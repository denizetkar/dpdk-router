modprobe uio
insmod dpdk/build/kmod/igb_uio.ko

dpdk/usertools/dpdk-devbind.py --bind=igb_uio 0000:03:00.0 0000:04:00.0 0000:05:00.0

mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge
echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages


# dpdk/usertools/dpdk-devbind.py --status
# export RTE_SDK=/root/framework/dpdk
# export RTE_TARGET=x86_64-native-linuxapp-gcc
# -c1 --lcores="(0-7)@0" -n1
