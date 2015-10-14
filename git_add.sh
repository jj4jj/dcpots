git status | grep "modified" | awk '{print $3}' | awk '{print "git add",$1}' | sh
git commit -m 'git auto add modified'
