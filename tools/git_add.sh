if [[ "$1" == "" ]];then 
    echo "not found commit msg , must add !"
    exit 0
fi
uname -v| grep "Ubuntu"
if [[ "$?" == "0" ]];then
git status | grep "modified" | awk '{print "git add",$3}' | sh
else
git status | grep "modified" | awk '{print "git add",$2}' | sh
fi
git commit -m \""$1"\"
git push origin master
