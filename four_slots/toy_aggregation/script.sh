run_dir="/home/jcheung2/ofs_fourslot/oneapi-asp/n6001/hardware/ofs_n6001_iopipes/build/scripts/"

# timestamp=$(date +"%Y%m%d_%H%M%S")
# echo "Current timestamp: $timestamp"
# echo "starting slot0"
# cat $run_dir"run1.txt" > $run_dir"run.sh"
# make s0

# timestamp=$(date +"%Y%m%d_%H%M%S")
# echo "Current timestamp: $timestamp"
# echo "starting slot1"
# cat $run_dir"run2.txt" > $run_dir"run.sh"
# make s1

# timestamp=$(date +"%Y%m%d_%H%M%S")
# echo "Current timestamp: $timestamp"
# echo "starting slot2"
# cat $run_dir"run3.txt" > $run_dir"run.sh"
# make s2

timestamp=$(date +"%Y%m%d_%H%M%S")
echo "Current timestamp: $timestamp"
echo "starting slot3"
cat $run_dir"run4.txt" > $run_dir"run.sh"
make s3