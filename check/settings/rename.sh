#/usr/bin/bash

sed -i "s/old/new/g" ./*old*
for file in *old*; do
   mv "$file" "${file/old/new}"
done
