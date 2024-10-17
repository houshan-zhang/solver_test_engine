#!/bin/bash

INSFILE=${1}
SOLFILE=${2}
TRAFILE=${3}
CMDFILE=${4}

# gurobi_cl version
${CHECKPATH}/bin/${SOLVER} TimeLimit=${TIMELIMIT} MemLimit=${MEMLIMIT} Seed=${SEED} Logfile="" ResultFile=${SOLFILE} MIPGap=${MIPGAP} Threads=${THREADS} ${INSFILE}
retcode=$?
if [[ ${retcode} != "0" ]]
then
   exit ${retcode};
fi

if test -e ${SOLFILE}
then
   # translate GUROBI solution format into format for solution checker.
   # The SOLFILE format is a very simple format where in each line
   # we have a <variable, value> pair, separated by spaces.
   # A variable name of =obj= is used to store the objective value
   # of the solution, as computed by the solver. A variable name of
   # =infeas= can be used to indicate that an instance is infeasible.
   if test ! -s $SOLFILE
   then
      # empty file, i.e., no solution given
      echo "=infeas=" > $SOLFILE
   else
      # grep objective out off the Gurobi log file
      grep "Best objective " gurobi.log | sed 's/.*objective \([0-9\.eE+-]*\), .*/=obj= \1/g' > ${CMDFILE}tempsol
      sed '  /# /d;
      /Solution for/d' ${SOLFILE} | grep -v "MPS_Rg" >> ${CMDFILE}tempsol
      mv ${CMDFILE}tempsol ${SOLFILE}
   fi
fi
