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
   echo "add method presol_extended_sparsify" > ${SCIP_PATH}/scip/src/scip/patch.txt
   ln -s -f ${CODE_PATH}/presol_extended_sparsify.c ${SCIP_PATH}/scip/src/scip/presol_extended_sparsify.c
   ln -s -f ${CODE_PATH}/presol_extended_sparsify.h ${SCIP_PATH}/scip/src/scip/presol_extended_sparsify.h
   if [[ ! -d ${CODE_PATH}/.cache ]]
   then
      mkdir ${CODE_PATH}/.cache
      cp ${SCIP_PATH}/scip/src/scip/scipdefplugins.c ${CODE_PATH}/.cache/scipdefplugins.c
      cp ${SCIP_PATH}/scip/src/scip/scipdefplugins.h ${CODE_PATH}/.cache/scipdefplugins.h
      cp ${SCIP_PATH}/scip/Makefile ${CODE_PATH}/.cache/Makefile
      cp ${SCIP_PATH}/scip/src/CMakeLists.txt ${CODE_PATH}/.cache/CMakeLists.txt
      cp ${SCIP_PATH}/scip/src/scip/paramset.c ${CODE_PATH}/.cache/paramset.c
   fi
   cp ${CODE_PATH}/scipdefplugins.c ${SCIP_PATH}/scip/src/scip/scipdefplugins.c
   cp ${CODE_PATH}/scipdefplugins.h ${SCIP_PATH}/scip/src/scip/scipdefplugins.h
   cp ${CODE_PATH}/Makefile ${SCIP_PATH}/scip/Makefile
   cp ${CODE_PATH}/CMakeLists.txt ${SCIP_PATH}/scip/src/CMakeLists.txt
   cp ${CODE_PATH}/paramset.c ${SCIP_PATH}/scip/src/scip/paramset.c
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
      rm -f ${SCIP_PATH}/scip/src/scip/presol_extended_sparsify.c
      rm -f ${SCIP_PATH}/scip/src/scip/presol_extended_sparsify.h
      cp ${CODE_PATH}/.cache/scipdefplugins.c ${SCIP_PATH}/scip/src/scip/scipdefplugins.c
      cp ${CODE_PATH}/.cache/scipdefplugins.h ${SCIP_PATH}/scip/src/scip/scipdefplugins.h
      cp ${CODE_PATH}/.cache/Makefile ${SCIP_PATH}/scip/Makefile
      cp ${CODE_PATH}/.cache/CMakeLists.txt ${SCIP_PATH}/scip/src/CMakeLists.txt
      cp ${CODE_PATH}/.cache/paramset.c ${SCIP_PATH}/scip/src/scip/paramset.c
   fi
fi
