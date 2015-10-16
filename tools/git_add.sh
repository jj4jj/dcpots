git status | grep "modified" | awk '{print "git add",$3}' | sh
git commit -m 'auto add'
git push origin master
