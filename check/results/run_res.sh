#!/bin/bash

PREFIX=${1}
DEFAULT=${2}
REGENERATE="off"

# find 'regenerate' parameter
if [[ $# -eq 3 && "${3}" == "on" ]]
then
   REGENERATE="on"
fi


if [[ ! -n ${PREFIX} ]]
then
   echo "usage:  ./run_res.sh <prefix>";
   exit;
fi

NUM=$(ls -l -d ./${PREFIX}* | grep ^d | wc -l)
echo "Total ${NUM} results"

NOW=0
for dir in ${PREFIX}*
do
   # if not exist .res file
   if [[ -z "$(find ${dir} -maxdepth 1 -name '*.res' -print -quit)" || ${REGENERATE} == on ]]
   then
      ${dir}/*.sh > /dev/null
   fi
   NOW=$((${NOW}+1))
   echo "|== ${NOW}/${NUM} == | ${dir}"
   if [[ -n ${DEFAULT} ]]
   then
      awk -f compare.awk ${dir}/*res ${DEFAULT}/*res
   fi
done
