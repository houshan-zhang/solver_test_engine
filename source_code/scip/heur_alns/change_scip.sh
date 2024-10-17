#!/bin/bash

SCIP_PATH=${1}
RECOVER=${2}
CODE_PATH=$(cd $(dirname $0);pwd)

if [ -z ${SCIP_PATH} ] || [ ${SCIP_PATH:0:1} != "/" ] || [ ! -f "${SCIP_PATH}/scip/src/CMakeLists.txt" ]
then
   echo "Pleaese Enter the Absolute Path of SCIP."
   exit -1
fi

if [ -z ${RECOVER} ]
then
   if [[ -e ${SCIP_PATH}/scip/src/scip/patch.txt ]]
   then
      echo "A Patch File Has Already Exists，Please Recover SCIP First."
      exit -1
   fi
   echo "change method heur_alns" > ${SCIP_PATH}/scip/src/scip/patch.txt
   if [[ ! -d ${CODE_PATH}/.cache ]]
   then
      mkdir ${CODE_PATH}/.cache
      cp ${SCIP_PATH}/scip/src/scip/heur_alns.c ${CODE_PATH}/.cache/heur_alns.c
   fi
   rm -f ${SCIP_PATH}/scip/src/scip/heur_alns.c
   ln -s -f ${CODE_PATH}/heur_alns.c ${SCIP_PATH}/scip/src/scip/heur_alns.c
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
      rm -f ${SCIP_PATH}/scip/src/scip/patch.txt
      rm -f ${SCIP_PATH}/scip/src/scip/heur_alns.c
      cp ${CODE_PATH}/.cache/heur_alns.c ${SCIP_PATH}/scip/src/scip/heur_alns.c
   fi
fi
