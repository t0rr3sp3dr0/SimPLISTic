#!/bin/bash
for k in `for i in /System/Library/LaunchDaemons/*.plist; do  file $i | grep binary | cut -d: -f1; done`; do ./jlutil -x $k | plutil -; done
echo now testing XML:
#for k in `for i in /System/Library/LaunchDaemons/*.plist; do  file $i | grep -v binary | cut -d: -f1; done`; do echo $k ; ./jlutil -x $k | plutil -; done

