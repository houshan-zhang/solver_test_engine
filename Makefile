OUTFILE       	= default
SETTING       	= ''
SEED          	= 0
SEEDFILE      	= ''
TEST          	= testeasy
SOLVER        	= scip
TIME          	= 7200
MEM           	= 32768
THREADS       	= 1
MINGAP        	= 0.01
CLUSTER       	= off
EXCLUSIVE     	= off
MPI           	= off
QUEUE         	= batch
WRITE         	= off
TC            	= 504
HC            	= 36
JC            	= 1

.PHONY: help
help:
	@echo "TARGETS:"
	@echo "** test               -> start automatic test runs local (default) or on cluster"
	@echo "** scripts            -> compile the scripts in the 'check/' directory"
	@echo "** docker-build       -> build a docker image with scip"
	@echo "** docker-update      -> update the scip source code in docker container"
	@echo "** clean              -> clean up the executable files for 'mpi' and 'solchecker'."
	@echo
	@echo "PARAMETERS:"
	@echo "** OUTFILE            -> directory of the output files [default]"
	@echo "** SETTING            -> setting file []"
	@echo "** SEED               -> random seed [0]"
	@echo "** SEEDFILE           -> random seedfile []"
	@echo "** TEST               -> test set [testeasy]"
	@echo "** SOLVER             -> solver [scip]"
	@echo "** TIME               -> time limit per instance in seconds [7200]"
	@echo "** MEM                -> maximum memory to use MB [819200]"
	@echo "** THREADS            -> number of threads (0: automatic) [1] (useless for scip)"
	@echo "** MINGAP             -> relative gap limit [0]"
	@echo "** CLUSTER            -> run on cluster [off]"
	@echo "** EXCLUSIVE          -> exclusive cluster run [off]"
	@echo "** MPI                -> mpi parallel execute commands [off]"
	@echo "** QUEUE              -> cluster queur [batch]"
	@echo "** WRITE              -> write presolved problem [off] (just for scip)"
	@echo "** TC                 -> total number of cores can used [216]"
	@echo "** HC                 -> number of cores per host allocated [36]"
	@echo "** JC                 -> number of cores per job used [1]"

.PHONY:test
test:
	cd check; \
		./scripts/run.sh $(SOLVER) $(TEST) $(TIME) $(MEM) $(THREADS) ${MINGAP} ${OUTFILE} ${SETTING} ${SEED} ${SEEDFILE} ${CLUSTER} ${EXCLUSIVE} ${MPI} ${QUEUE} ${WRITE} ${TC} ${HC} ${JC}

.PHONY:cplex
cplex:
	cd source_code/cplex/ndp; \
		make

.PHONY:scripts
scripts:
	cd check/checker; \
		make; \
		cd ../scripts/mpi; \
		make

.PHONY:docker-build
docker-build:
	cd docker; \
		docker build -t iscip .; \
		docker run -it --read-only=false --name dscip \
		-v $(shell pwd)/check/instances:/root/instances \
		-v $(shell pwd)/check/results:/root/results \
		-v $(shell pwd)/source_code:/root/source_code \
		iscip; \
		mkdir -p ../check/bin; \
		cd ../check/bin; \
		rm -f scip; \
		printf '#!/bin/bash\n\ncat $$@ | docker exec -i dscip scip' > scip; \
		chmod +x scip

.PHONY:docker-update
docker-update:
	docker start dscip; \
		docker exec -it dscip ./scip_update.sh\

.PHONY:clean
clean:
	cd check/checker; \
		make clean; \
		cd ../scripts/mpi; \
		make clean
