#!/bin/bash

bjobs > job
awk '{print "bkill "$1}' job > killjob
chmod +x killjob
./killjob
rm killjob
rm job
