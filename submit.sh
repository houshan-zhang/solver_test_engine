#!/bin/bash

SETTING_DIR=${1}
TESTNAME=4M

if [[ ! -n ${SETTING_DIR} ]]
then
   echo "usage: ./submit.sh <setting_dir>";
   exit;
fi

if [[ ! -e ./check/settings/${SETTING_DIR} ]]
then
   echo "error: directory ${SETTING_DIR} does not exist";
   exit;
fi

if [ ! "$(ls -A ./check/settings/${SETTING_DIR})" ]
then
   echo "error: directory ${SETTING_DIR} is empty";
   exit;
fi

NUM=$(ls -l -d ./check/settings/${SETTING_DIR}/*.set | grep ^- | wc -l)
echo "Total ${NUM} settings"

NOW=0
for temp in ./check/settings/${SETTING_DIR}/*.set
do
   setting=${temp##*/}
   setting=${setting%.set*}
   NOW=$((${NOW}+1))
   echo "|== ${NOW}/${NUM} == | ${setting}"
   make test TEST=${TESTNAME} CLUSTER=on MPI=on SEEDFILE=5 SETTING=${SETTING_DIR}/${setting} \
   OUTFILE=${TESTNAME}_allmpi_2_$(date '+%m_%d')_${setting} > /dev/null
done
