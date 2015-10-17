if [[ "$1" == "" ]];then 
    echo "not found commit msg , must add !"
    exit 0
fi
git status | grep "modified" | awk '{print "git add",$3}' | sh
git commit -m \""$1"\"
git push origin master
