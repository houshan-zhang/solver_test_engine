#!/bin/bash

# compress all files in folder FILEDIR
FILEDIR=${1}

cd ${FILEDIR}
for i in $(ls -I "*.gz")
do
   INSNAME=${i}
   echo "gzip ${INSNAME}"
   gzip ${INSNAME}
done
