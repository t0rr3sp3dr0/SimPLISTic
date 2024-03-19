#!/bin/bash
for i in `find tests -name "*.plist"`; do  
#	echo testing $i
	if ./jlutil -x `file $i | grep binary | cut -d: -f1 ` | plutil - | grep OK > /dev/null; then
		true ; # echo $i PASS
	else 
		echo $i FAIL
	fi
done


