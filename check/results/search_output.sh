#!/bin/bash

# This script is used to statist the output we generate ourselves.
# Firstly, compare.awk counts the affected instances and then outputs the specified lines
# in the log file for those instances (containing our output information).
# Secondly, statistic_multiagg.awk is used to generate statistical tables.

#  Inut example: ./search_output.sh "awk -v LEVEL=4 -f compare.awk 4M_allmpi_v4_03_20_open/4M.scip.1threads.7200s.res 4M_allmpi_v4_close/4M.scip.1threads.7200s.res"
#* Inut example: ./search_output.sh awk -v LEVEL=4 -f compare.awk 4M_allmpi_v4_03_20_open/4M.scip.1threads.7200s.res 4M_allmpi_v4_close/4M.scip.1threads.7200s.res

#AWKFILE=$($1)          # parameters should be wrapped in ""
AWKFILE=$("$@")
PREFIX="Compare Aggregation Type"

rm -rf .temp
touch .temp

while read -r line
do
   if [[ ${line} != -Affected-* ]]
   then
      continue;
   fi
   file=$(echo "${line}" | cut -d' ' -f2-)
   if [[ -f "${file}" ]]
   then
      printf "%s, %s" "$(echo "$line" | awk -F'/' '{print $NF}')" >> .temp
      zgrep "^${PREFIX}" "${file}" >> .temp
   else
      echo "File ${file} does not exist or is not a regular file."
      exit -1;
   fi
done <<< ${AWKFILE}

awk -f statistic_multiagg.awk .temp
rm -rf .temp
