
#max msgq size 1000mb , init
echo 1024000000 > /proc/sys/kernel/msgmax
echo 10485760 > /proc/sys/kernel/msgmnb
echo 102400 > /proc/sys/kernel/msgmni

#max size 64G
echo 68719476736 > /proc/sys/kernel/shmmax
echo 102400 > /proc/sys/kernel/shmmni

#default tcp rbuffer 2MB
echo 2097152 > /proc/sys/net/core/rmem_max
echo 2097152 > /proc/sys/net/core/wmem_max

