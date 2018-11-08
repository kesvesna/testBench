sudo sysctl -w vm.nr_hugepages=1000
echo "Количество страниц"
sudo cat /proc/sys/vm/nr_hugepages
./atomicOperations



