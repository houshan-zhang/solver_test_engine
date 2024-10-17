#!/bin/bash

# import some useful functions that make this script less cluttered
. ${CHECKPATH}/scripts/run_functions.sh

# we add a small time overhead for the cluster and file system
HARDTIMELIMIT=$(( ${TIMELIMIT}/2 + ${TIMELIMIT} ))

# get the instance name
INSFILENAME=$(stripPathAndInstanceExtension ${INSFILE})

# construct paths of output, error, solution, and command file
OUTFILE="${OUTDIR}/${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out"
ERRFILE="${OUTDIR}/error/${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.err"
SOLFILE="${OUTDIR}/solutions/${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sol"
TRAFILE="${OUTDIR}/transformed/${INSFILENAME}.mps"
CMDFILE="${OUTDIR}/cmd/${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.cmd"

rm -f ./${OUTFILE}
rm -f ./${ERRFILE}
rm -f ./${SOLFILE}
rm -f ./${TRAFILE}
rm -f ./${CMDFILE}

if [[ ${MPI} == on ]]
then
   ${CHECKPATH}/scripts/runsubjob.sh ${INSFILE} ${SOLFILE} ${TRAFILE} ${CMDFILE} 1> ${OUTFILE} 2>${ERRFILE}
else
   if [[ ${CLUSTER} == on ]]
   then
      if [[ ${EXCLUSIVE} == on ]]
      then
         EXCLUSIVESIGNAL="-x"
      else
         EXCLUSIVESIGNAL=""
      fi
      bsub -J ${INSFILENAME} -q ${QUEUE} -W ${HARDTIMELIMIT} -R "span[ptile=1]" -n 1 -e ${ERRFILE} -o ${OUTFILE} ${EXCLUSIVESIGNAL} "
      ${CHECKPATH}/scripts/runsubjob.sh ${INSFILE} ${SOLFILE} ${TRAFILE} ${CMDFILE}"
   else
      ${CHECKPATH}/scripts/runsubjob.sh ${INSFILE} ${SOLFILE} ${TRAFILE} ${CMDFILE} | tee -a ${OUTFILE} 2>${ERRFILE}
   fi
fi
