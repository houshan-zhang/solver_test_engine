#!/bin/bash

INSFILE=${1}
SOLFILE=${2}
TRAFILE=${3}
CMDFILE=${4}

if [[ -f ${INSFILE} ]]
then
   # post system information
   uname -a
   cat /proc/cpuinfo | grep 'model name' | uniq | sed 's/model name/CPU/g'

   echo "@01 INSTANCE: ${INSFILE}, SEED: ${SEED}"
   echo "@02 START TIME: $(date "+%Y-%m-%d %H:%M:%S")"
   echo ""
   ${CHECKPATH}/scripts/run_${SOLVER}.sh ${INSFILE} ${SOLFILE} ${TRAFILE} ${CMDFILE}
   retcode=$?
   if [[ ${retcode} != "0" ]]
   then
      echo ""
      echo "ERROR! run_${SOLVER}.sh exit code ${retcode}"
      echo ""
      exit ${retcode}
   fi
   echo ""
   echo "@03 END TIME: $(date "+%Y-%m-%d %H:%M:%S")"
   TIMEHOUR=$(echo "scale=1; ${TIMELIMIT}/3600" | bc)
   echo "@04 TIMELIMIT: ${TIMELIMIT} s [${TIMEHOUR} h]"
   MEMGB=$(echo "scale=1; ${MEMLIMIT}/1024" | bc)
   echo "@05 MEMLIMIT: ${MEMLIMIT} MB [${MEMGB} GB]"
   if [[ ${WRITE} == off ]]
   then
      if [[ ${MIPGAP} == "0" ]]
      then
         echo "@06 GAPLIMIT: ${MIPGAP} [0.0 %]"
      else
         GAPPERCENTAGE=$(echo "scale=1; ${MIPGAP}*100.0" | bc)
         echo "@06 GAPLIMIT: ${MIPGAP} [${GAPPERCENTAGE} %]"
      fi
      # if we have a (non-empty) solution and a checker: check the solution
      if [[ -e ${SOLFILE} ]]
      then
         if [[ -e ${CHECKPATH}/checker/bin/solchecker ]]
         then
            echo ""
            echo "Solution Check"
            ${CHECKPATH}/checker/bin/solchecker ${INSFILE} ${SOLFILE} ${LINTOL} ${INTTOL}
            retcode=$?
            if [[ ${retcode} != "0" ]]
            then
               echo ""
               echo "ERROR! solchecker exit code ${retcode}"
               echo ""
               exit ${retcode}
            fi
            echo ""
         else
            echo "warning! solution checker not found"
         fi
      else
         echo "solution file is empty"
      fi
      # delete solution file to save memory
      echo "remove solution file: ${SOLFILE}"
      rm -rf ./${SOLFILE}
      echo ""
   fi
   echo "= over ="
else
   echo "========================== ERROR: FILE NOT FOUND: ${INSFILE} =========================="
fi
