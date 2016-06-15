#! / bin /bash
if [ "$#" -ne 1 ]
then
echo "Usage : ./startTests.sh <path-to-kernel-and-fault-library-config-files >"
exit 1
fi
DATA_COLLECTOR_PATH="data_collector.txt"
CONFIG_FILE=$1
counter=1
while read line
do
KERNEL_PATH=$(echo -e "$line\n" | cut -f1 -d,)
FAULT_LIBRARY_PATH=$ ( echo -e "$line\n" | cut -f2 -d ,)
FAULT_COUNTER_ADDRESS=$ ( readelf $KERNELPATH -s | grep fault_counter)
FAULT_COUNTER_ADDRESS=$ ( echo $FAULT_COUNTER_ADDRESS | cut -f2 -d :)
FAULT_COUNTER_ADDRESS=$ ( echo $FAULT_COUNTER_ADDRESS | cut -f1 -d ' ' )
echo "FAULT_COUNTER_ADDRESS: $FAULT_COUNTER_ADDRESS"
SBST_CYCLE_COUNT=$ ( readelf $KERNEL_PATH -s | grep sbst_cycle_count )
SBST_CYCLE_COUNT=$ ( echo $SBST_CYCLE_COUNT | cut -f2 -d : )
SBST_CYCLE_COUNT=$ ( echo $SBST_CYCLE_COUNT | cut -f1 -d ' ' )
echo "SBST_CYCLE_COUNT: $SBST_CYCLE_COUNT"
qemu-system-arm -M imx28evk -m 128 -kernel $KERNEL_PATH -fi \
$FAULT_COUNTER_ADDRESS,$FAULT_LIBRARY_PATH,$SBST_CYCLE_COUNT
DATA_COLLECTOR_OUT="data_collector_$counter.txt"
cat $DATA_COLLECTOR_PATH > $DATA_COLLECTOR_OUT
counter=$((counter+1))
done <$1
echo "Fault injection experiment finished"


