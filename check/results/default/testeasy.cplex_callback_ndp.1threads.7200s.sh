#!/bin/bash

path=$(cd "$(dirname "$0")"; pwd)
rm -f ${path}/testeasy.cplex_callback_ndp.1threads.7200s.out
rm -f ${path}/testeasy.cplex_callback_ndp.cplex_callback_ndp.1threads.7200s.res
for i in $(cat ${path}/testeasy.file)
do
   outfile=${path}/${i}
   if [[ -e ${outfile} ]]
   then
      gzip -f ${outfile}
   fi
   gunzip -c ${outfile}.gz >> ${path}/testeasy.cplex_callback_ndp.1threads.7200s.out
done
results_index=$(echo "${path}" | awk -F '/results/' '{print length($1)}')
check_path=${path:0:${results_index}}
awk -v CMPSEED=0 -f ${check_path}/scripts/parse.awk -f ${check_path}/scripts/parse_cplex_callback_ndp.awk -v LINTOL=1e-4 -v MIPGAP=0 ${path}/../../testsets/testeasy.solu ${path}/testeasy.cplex_callback_ndp.1threads.7200s.out | tee ${path}/testeasy.cplex_callback_ndp.1threads.7200s.res
rm -f ${path}/testeasy.cplex_callback_ndp.1threads.7200s.out
