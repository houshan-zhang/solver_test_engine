#!/bin/bash
#TODO check

INSFILE=${1}
SOLFILE=${2}
TRAFILE=${3}
CMDFILE=${4}

# disable log file
echo set logfile '*'                                     >> ${CMDFILE}
# set threads to given value
echo set threads ${THREADS}                              >> ${CMDFILE}
# set random seed
echo set randomseed ${SEED}                              >> ${CMDFILE}
# set mipgap to given value
echo set mip tolerances mipgap ${MIPGAP}                 >> ${CMDFILE}
# set timing to wall-clock time
echo set clocktype 2                                     >> ${CMDFILE}
# set time limit
echo set timelimit ${TIMELIMIT}                          >> ${CMDFILE}
# set memory limit in MB
echo set mip limits treememory ${MEMLIMIT}               >> ${CMDFILE}
# use deterministic mode (warning if not possible)
echo set parallel 1                                      >> ${CMDFILE}
# read problem
echo read ${INSFILE}                                     >> ${CMDFILE}
# to identify obj sense
echo change sense 0 min                                  >> ${CMDFILE}
# display original problem
echo display problem stats                               >> ${CMDFILE}
if [[ ${WRITE} == on ]]
then
   # write binary format for presolved problem
   echo write ${SOLFILE}.pre                             >> ${CMDFILE}
   # read binary format for presolved problem
   echo read ${SOLFILE}.pre                              >> ${CMDFILE}
   # write presolved problem
   echo write ${TRAFILE}                                 >> ${CMDFILE}
else
   # solve problem
   echo optimize                                         >> ${CMDFILE}
   #TODO if time limit, cplex can not write sol
   # write solution
   echo write ${SOLFILE}                                 >> ${CMDFILE}
fi
# exit interactive interface
echo quit                                                >> ${CMDFILE}

${CHECKPATH}/bin/${SOLVER} < ${CMDFILE}
retcode=$?
if [[ ${retcode} != "0" ]]
then
   exit ${retcode};
fi

if [[ ${WRITE} == on ]]
then
   # remove binary format for presolved problem
   rm -rf ${SOLFILE}.pre
   # compress presolved problem
   gzip ${TRAFILE}
else
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
fi

# remove CPLEX log
rm -f cplex.log
