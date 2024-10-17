#!/bin/bash

INSFILE=${1}
SOLFILE=${2}
TRAFILE=${3}
CMDFILE=${4}


# set threads to given value
# nothing to be done for SCIP

if [[ -n ${SETTING} ]]
then
   echo set load ${CHECKPATH}/settings/${SETTING}.set       >> ${CMDFILE}
fi
# set mipgap to given value
echo set limits gap ${MIPGAP}                               >> ${CMDFILE}
# set random seed
echo set randomization randomseedshift ${SEED}              >> ${CMDFILE}
# set time limit
echo set limits time ${TIMELIMIT}                           >> ${CMDFILE}
# set memory limit
echo set limits memory ${MEMLIMIT}                          >> ${CMDFILE}
# set display options
echo set display freq -1                                    >> ${CMDFILE}
# set verbosity level of output
echo set display verblevel 4                                >> ${CMDFILE}
# read problem
echo read ${INSFILE}                                        >> ${CMDFILE}
if [[ ${WRITE} == on ]]
then
   # presolve
   echo presolve                                            >> ${CMDFILE}
   # write transformed problem
   echo write transproblem ${TRAFILE}                       >> ${CMDFILE}
else
   # solve problem
   echo optimize                                            >> ${CMDFILE}
   # write solution
   echo write solution ${SOLFILE}                           >> ${CMDFILE}
fi
# display statistics
echo display statistics                                     >> ${CMDFILE}
# exit interactive interface
echo quit                                                   >> ${CMDFILE}

${CHECKPATH}/bin/${SOLVER} < ${CMDFILE}
retcode=$?
if [[ ${retcode} != "0" ]]
then
   exit ${retcode};
fi

if [[ ${WRITE} == on  ]]
then
   # compress presolved problem
   gzip ${TRAFILE}
else
   if test -e ${SOLFILE}
   then
      # translate SCIP solution format into format for solution checker. The
      # SOLFILE format is a very simple format where in each line we have a
      # <variable, value> pair, separated by spaces.  A variable name of
      # =obj= is used to store the objective value of the solution, as
      # computed by the solver. A variable name of =infeas= can be used to
      # indicate that an instance is infeasible.
      sed ' /solution status:/d;
      s/objective value:/=obj=/g;
      s/no solution available//g' ${SOLFILE} > ${CMDFILE}tempsol
      mv -f ${CMDFILE}tempsol ${SOLFILE}
   fi
fi
