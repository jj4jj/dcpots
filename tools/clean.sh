
echo "clean shm list:"
echo "----------------------------------------------"
ipcs -m | awk '{if($6 == 0){print $0}}'
ipcs -m | awk '{if($6 == 0){print "ipcrm -m",$2}}' | sh
echo "----------------------------------------------"




if [ $1 == 'q' ];then
echo "clean msgq list:"
echo "----------------------------------------------"
ipcs -q | awk '{if($6 == 0){print $0}}'
ipcs -q | awk '{if($6 == 0){print "ipcrm -q",$2}}' | sh
echo "----------------------------------------------"
fi

