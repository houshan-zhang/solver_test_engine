#!/bin/bash

# This file is used to manage your own scip plugin.
# It is divided into two parts, one is to add new plugin to scip,
# and the other is to restore the default scip.


# parameters
#--------------------
# scip root path
SCIP_PATH=${1}
# is recover
RECOVER=${2}
#--------------------

# the absolute path to the current file
CODE_PATH=$(cd $(dirname $0);pwd)

# determine if the scip path is correct
if [ -z ${SCIP_PATH} ] || [ ${SCIP_PATH:0:1} != "/" ] || [ ! -f "${SCIP_PATH}/scip/src/CMakeLists.txt" ]
then
   echo "Pleaese Enter the Absolute Path of SCIP."
   exit -1
fi

# if we're going to add plugin to scip
if [ -z ${RECOVER} ]
then
   if [[ -e ${SCIP_PATH}/scip/src/scip/patch.txt ]]
   then
      echo "A Patch File Has Already Exists，Please Recover SCIP First."
      exit -1
   fi
   # generate a prompt file
   echo "add method presol_multiagg" > ${SCIP_PATH}/scip/src/scip/patch.txt
   # now we will "add a new file" to scip by adding a soft link
   ln -s -f ${CODE_PATH}/presol_multiagg.c ${SCIP_PATH}/scip/src/scip/presol_multiagg.c
   ln -s -f ${CODE_PATH}/presol_multiagg.h ${SCIP_PATH}/scip/src/scip/presol_multiagg.h
   # now we will "replace the default file" to scip by making a copy of the default file and adding a soft link
   if [[ ! -d ${CODE_PATH}/.cache ]]
   then
      mkdir ${CODE_PATH}/.cache
      cp ${SCIP_PATH}/scip/src/scip/scipdefplugins.c ${CODE_PATH}/.cache/scipdefplugins.c
      cp ${SCIP_PATH}/scip/src/scip/scipdefplugins.h ${CODE_PATH}/.cache/scipdefplugins.h
      cp ${SCIP_PATH}/scip/Makefile ${CODE_PATH}/.cache/Makefile
      cp ${SCIP_PATH}/scip/src/CMakeLists.txt ${CODE_PATH}/.cache/CMakeLists.txt
      cp ${SCIP_PATH}/scip/src/scip/paramset.c ${CODE_PATH}/.cache/paramset.c
   fi
   # CMakeLists.txt can NOT be soft link
   cp ${CODE_PATH}/scipdefplugins.c ${SCIP_PATH}/scip/src/scip/scipdefplugins.c
   cp ${CODE_PATH}/scipdefplugins.h ${SCIP_PATH}/scip/src/scip/scipdefplugins.h
   cp ${CODE_PATH}/Makefile ${SCIP_PATH}/scip/Makefile
   cp ${CODE_PATH}/CMakeLists.txt ${SCIP_PATH}/scip/src/CMakeLists.txt
   cp ${CODE_PATH}/paramset.c ${SCIP_PATH}/scip/src/scip/paramset.c
# if we're going to restore scip
else
   if [[ ! -d ${CODE_PATH}/.cache ]]
   then
      echo "Recover SCIP Need \".cache\" Directory Containing Default Source Code."
      exit -1
   elif [[ ! -e ${SCIP_PATH}/scip/src/scip/patch.txt ]]
   then
      echo "Can Not Find Patch File，Please Check Source Code."
      exit -1
   else
      # remove the file we added
      rm -f ${SCIP_PATH}/scip/src/scip/patch.txt
      rm -f ${SCIP_PATH}/scip/src/scip/presol_multiagg.c
      rm -f ${SCIP_PATH}/scip/src/scip/presol_multiagg.h
      # restore default file of scip
      cp ${CODE_PATH}/.cache/scipdefplugins.c ${SCIP_PATH}/scip/src/scip/scipdefplugins.c
      cp ${CODE_PATH}/.cache/scipdefplugins.h ${SCIP_PATH}/scip/src/scip/scipdefplugins.h
      cp ${CODE_PATH}/.cache/Makefile ${SCIP_PATH}/scip/Makefile
      cp ${CODE_PATH}/.cache/CMakeLists.txt ${SCIP_PATH}/scip/src/CMakeLists.txt
      cp ${CODE_PATH}/.cache/paramset.c ${SCIP_PATH}/scip/src/scip/paramset.c
   fi
fi
