#!/bin/bash

# script parameter
SOLVER=${1}             # (string) name of the binary in the 'bin' dir
TSTNAME=${2}            # (string) name of the testset to execute
TIMELIMIT=${3}          # (int > 0) in seconds
MEMLIMIT=${4}           # (int > 0) in MB
THREADS=${5}            # (int > 0) number of threads to be used by the solver
MIPGAP=${6}             # the MIP gap is not uniqely defined through all solvers
OUTDIR=results/${7}     # output dir
SETTING=${8}            # setting file
SEED=${9}               # random seed
SEEDFILE=${10}          # random seedfile
CLUSTER=${11}           # run on cluster
EXCLUSIVE=${12}         # exclusive cluster run
MPI=${13}               # mpi parallel execute commands
QUEUE=${14}             # cluster queue name
WRITE=${15}             # write presolved problem
TC=${16}                # total number of cores can used
HC=${17}                # number of cores per host allocated
JC=${18}                # number of cores per job used

# additional parameter
LINTOL=1e-4       # absolut tolerance for checking linear constraints and objective value
INTTOL=1e-4       # absolut tolerance for checking integrality constraints

# import some useful functions that make this script less cluttered
. $(dirname "${BASH_SOURCE[0]}")/run_functions.sh

# construct paths
CHECKPATH=$(pwd)

# write presolved problem can not with seedfile
if [[ ${WRITE} == on  && -n ${SEEDFILE} ]]
then
   echo "NOTE: write presolved problem with seedfile ${SEEDFILE}"
   exit -1
fi

# check if the setting file exists
if [[ -n ${SETTING} && ! -f ${CHECKPATH}/settings/${SETTING}.set ]]
then
   echo "ERROR: setting file '${SETTING}.set' does not exist in 'settings' folder"
   exit -1
fi

# check if the solver link (binary) exists
if [[ ! -e "${CHECKPATH}/bin/${SOLVER}" ]]
then
   echo "ERROR: solver link '${SOLVER}' does not exist in 'bin' folder"
   exit -1
fi

# check if the test seedfile exists
if [[ -n ${SEEDFILE} && ! -f ${CHECKPATH}/seeds/${SEEDFILE}.seed ]]
then
   echo "ERROR: seedfile file/link '${SEEDFILE}.seed' does not exist in 'seeds' folder"
   exit -1
fi

# check if the test set file/link exists
if [[ ! -e "${CHECKPATH}/testsets/${TSTNAME}.test" ]]
then
   echo "ERROR: test set file/link '${TSTNAME}.test' does not exist in 'testsets' folder"
   exit -1
fi

# check if a solution file exists
if [[ ! -e ${CHECKPATH}/testsets/${TSTNAME}.solu ]]
then
   echo "Warning: solution file/link '${TSTNAME}.solu' does not exist in 'testsets' folder; therefore, no consistency check"
   SOLUFILE=""
else
   SOLUFILE="\${path}/../../testsets/${TSTNAME}.solu"
fi

# check if the result folder exist. if not create the result folder
if [[ ! -e ${CHECKPATH}/results ]]
then
   mkdir ${CHECKPATH}/results
fi

# check if the output folder exist. if not create the solution folder
if [[ ! -e ${CHECKPATH}/$OUTDIR ]]
then
   mkdir -p ${CHECKPATH}/${OUTDIR}
fi

# check if the solution folder exist. if not create the solution folder
if [[ ! -e ${CHECKPATH}/${OUTDIR}/solutions ]]
then
   mkdir ${CHECKPATH}/${OUTDIR}/solutions
fi

# check if the error folder exist. if not create the error folder
if [[ ! -e ${CHECKPATH}/${OUTDIR}/error ]]
then
   mkdir ${CHECKPATH}/${OUTDIR}/error
fi

# check if the command folder exist. if not create the command folder
if [[ ! -e ${CHECKPATH}/${OUTDIR}/cmd ]]
then
   mkdir ${CHECKPATH}/${OUTDIR}/cmd
fi

# check if the transformed problem folder exist. if not create the transformed problem folder
if [[ ${WRITE} == on && ! -e ${CHECKPATH}/${OUTDIR}/transformed ]]
then
   mkdir ${CHECKPATH}/${OUTDIR}/transformed
fi

rm -f ${OUTDIR}/${TSTNAME}.file
touch ${OUTDIR}/${TSTNAME}.file

# touch history file
rm -f ${OUTDIR}/${TSTNAME}.history
touch ${OUTDIR}/${TSTNAME}.history

# loop over all instance names which are listed in the test set file name
echo "==========================" ${TSTNAME} start "=========================="
for i in $(cat ${CHECKPATH}/testsets/${TSTNAME}.test)
do
   # check if the current instance exists
   if [[ -f ${i} ]]
   then
      export INSFILE=${i}
      INSFILENAME=$(stripPathAndInstanceExtension ${INSFILE})
      export OUTDIR
      export MEMLIMIT
      export TIMELIMIT
      export CHECKPATH
      export SOLVER
      export THREADS
      export MIPGAP
      export LINTOL
      export INTTOL
      export TSTNAME
      export SETTING
      export CLUSTER
      export EXCLUSIVE
      export MPI
      export QUEUE
      export WRITE
      if [[ -z ${SEEDFILE} ]]
      then
         export SEED
         echo "${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out" >> ${OUTDIR}/${TSTNAME}.file
         if [[ ${MPI} == on ]]
         then
            echo -e "export SEED=${SEED} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export OUTDIR=${OUTDIR} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export MEMLIMIT=${MEMLIMIT} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export TIMELIMIT=${TIMELIMIT} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export CHECKPATH=${CHECKPATH} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export INSFILE=${INSFILE} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export SOLVER=${SOLVER} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export THREADS=${THREADS} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export MIPGAP=${MIPGAP} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export LINTOL=${LINTOL} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export INTTOL=${INTTOL} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export TSTNAME=${TSTNAME} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export SETTING=${SETTING} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export CLUSTER=${CLUSTER} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export EXCLUSIVE=${EXCLUSIVE} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export MPI=${MPI} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "export WRITE=${WRITE} && \c" >> ${OUTDIR}/${TSTNAME}.history
            echo -e "${CHECKPATH}/scripts/runjob.sh" >> ${OUTDIR}/${TSTNAME}.history
         else
            scripts/runjob.sh
         fi
      else
         for SEED in $(cat ${CHECKPATH}/seeds/${SEEDFILE}.seed)
         do
            export SEED
            echo "${TSTNAME}.${INSFILENAME}.${SEED}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out" >> ${OUTDIR}/${TSTNAME}.file
            if [[ ${MPI} == on ]]
            then
               echo -e "export SEED=${SEED} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export OUTDIR=${OUTDIR} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export MEMLIMIT=${MEMLIMIT} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export TIMELIMIT=${TIMELIMIT} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export CHECKPATH=${CHECKPATH} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export INSFILE=${INSFILE} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export SOLVER=${SOLVER} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export THREADS=${THREADS} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export MIPGAP=${MIPGAP} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export LINTOL=${LINTOL} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export INTTOL=${INTTOL} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export TSTNAME=${TSTNAME} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export SETTING=${SETTING} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export CLUSTER=${CLUSTER} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export EXCLUSIVE=${EXCLUSIVE} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export MPI=${MPI} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "export WRITE=${WRITE} && \c" >> ${OUTDIR}/${TSTNAME}.history
               echo -e "${CHECKPATH}/scripts/runjob.sh" >> ${OUTDIR}/${TSTNAME}.history
            else
               scripts/runjob.sh
            fi
         done
      fi
   elif [[ ${i} == \#* ]]
   then
      continue
   else
      echo "Instance ${i} not found, skipping it."
   fi
done
echo "=========================="\ \ ${TSTNAME} end\ \ "=========================="

# write statistic shell script
rm -f ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "#!/bin/bash" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "path=\$(cd \"\$(dirname \"\$0\")\"; pwd)" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "rm -f \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "rm -f \${path}/${TSTNAME}.${SOLVER}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.res" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "for i in \$(cat \${path}/${TSTNAME}.file)" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "do" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "   outfile=\${path}/\${i}" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "   if [[ -e \${outfile} ]]" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "   then" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "      gzip -f \${outfile}" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "   fi" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "   gunzip -c \${outfile}.gz >> \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "done" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "results_index=\$(echo \"\${path}\" | awk -F '/results/' '{print length(\$1)}')" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
echo "check_path=\${path:0:\${results_index}}" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh

if [[ -n ${SEEDFILE} && ${SOLVER} == "scip" ]]
then
   echo "awk -v CMPSEED=1 -f \${check_path}/scripts/parse.awk -f \${check_path}/scripts/parse_${SOLVER}.awk -v "LINTOL=${LINTOL}" -v "MIPGAP=${MIPGAP}" ${SOLUFILE} \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out | tee \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.res" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
else
   echo "awk -v CMPSEED=0 -f \${check_path}/scripts/parse.awk -f \${check_path}/scripts/parse_${SOLVER}.awk -v "LINTOL=${LINTOL}" -v "MIPGAP=${MIPGAP}" ${SOLUFILE} \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out | tee \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.res" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
fi

echo "rm -f \${path}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.out" >> ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
chmod +x ${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh

if [[ ${MPI} == on ]]
then
   if [[ ${CLUSTER} == on ]]
   then
      bsub -J ${TSTNAME} -q ${QUEUE} -R "span[ptile=${HC}]" -n ${TC} -e %J.err -o %J.out "mpirun ./scripts/mpi/mpiexecline ./${OUTDIR}/${TSTNAME}.history ${HC} ${JC}"
   else
      # count computer cores
      mpirun -n $(cat /proc/cpuinfo|grep "cpu cores"|uniq | awk '{print $4}') ./scripts/mpi/mpiexecline ./${OUTDIR}/${TSTNAME}.history ${HC} ${JC}
   fi
fi

if [[ ${CLUSTER} == off && ${WRITE} == off ]]
then
   ${CHECKPATH}/${OUTDIR}/${TSTNAME}.${SOLVER}.${THREADS}threads.${TIMELIMIT}s.sh
fi
