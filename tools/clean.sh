
echo "clean shm list:"
ipcs -m | awk '{if($6 == 0){print $0}}'
ipcs -m | awk '{if($6 == 0){print "ipcrm -m",$2}}'

echo "----------------------------------------------"
echo "clean msgq list:"
ipcs -q | awk '{if($6 == 0){print $0}}'
ipcs -q | awk '{if($6 == 0){print "ipcrm -q",$2}}' | sh

echo "----------------------------------------------"

