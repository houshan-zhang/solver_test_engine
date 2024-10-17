#!/bin/bash

INSFILE=${1}
SOLFILE=${2}
TRAFILE=${3}
CMDFILE=${4}

# record the parameter file first, and then set the other parameters separately
echo "${CHECKPATH}/bin/cplex_callback_ndp ${INSFILE} \
   \"SETTING=${CHECKPATH}/settings/${SETTING}.set TIME=${TIMELIMIT} SEED=${SEED} MINGAP=${MIPGAP} MEM=${MEMLIMIT} SOL=${SOLFILE}\"" \
   > ${CMDFILE}

${CHECKPATH}/bin/cplex_callback_ndp ${INSFILE} \
   "SETTING=${CHECKPATH}/settings/${SETTING}.set TIME=${TIMELIMIT} SEED=${SEED} MINGAP=${MIPGAP} MEM=${MEMLIMIT} SOL=${SOLFILE}"

retcode=$?
if [[ ${retcode} != "0" ]]
then
   exit ${retcode};
fi

if test -e ${SOLFILE}
then
   # translate CPLEX solution format into format for solution checker.
   # The SOLFILE format is a very simple format where in each line
   # we have a <variable, value> pair, separated by spaces.
   # A variable name of =obj= is used to store the objective value
   # of the solution, as computed by the solver. A variable name of
   # =infeas= can be used to indicate that an instance is infeasible.
   $(dirname "${BASH_SOURCE[0]}")/cpx2solchecker.py ${SOLFILE} > ${CMDFILE}tempsol
   mv ${CMDFILE}tempsol ${SOLFILE}
fi
