## Introduction

The main function of the scip_test_engine program is to test the open-source mixed-integer optimization solver SCIP.
This includes:

1. Installing SCIP (using Docker)
2. Testing SCIP
3. Collecting test results.

In addition, this testing program can also perform some simple tests on the commercial solvers Gurobi and Cplex.
The tests are performed using the Makefile system.
To get the main usage of this program, type `make help`.

## Installing SCIP using Docker

To install SCIP using Docker, please ensure that:

1. Docker is installed on your computer.
2. The source code for SCIP and Bliss is downloaded and placed in the `scip_test_engine/docker` directory.

You can then use the Dockerfile to automatically build a Docker image with SCIP installed and create a container. To do so, run `make docker-build`.

## Replace SCIP files

If you want to replace SCIP files, you should place the files that need to be modified in the `./source_code` directory.

For example, if you want to test the multi-aggregation presolver, you should creat subdirectory `multi_aggregation`
in `./source_code`.
Putting in the source code of multi-aggregation presolver and shell script `change_scip.sh` to replace and recover the
original SCIP source code.

#### Multi-aggregation
(1) Change into the `multi_aggregation` directory

    cd ./source_code/multi_aggregation

(2) Execute the script to replace the code

    ./change_scip.sh <root_path_of_scip>

(3) restore SCIP to its initial state

    ./change_scip.sh <root_path_of_scip> 1

## Test results

The test results are stored in the `scip_test_engine/check/results` folder.
The `compare.awk` file is used to compare different test results and can directly generate a LATEX table.
The `run_res.sh` file is used to generate the `.res` file for a large scale submission.

It is recommended to set up a separate folder to store each test result.
The test results should be stored in `scip_test_engine/check/results/<output>`.
The directory structure for each test result is as follows

#### Folder
1. The `cmd` folder is used to store the history of commands for a single test instance.
2. The `error` folder is used to store error files.
3. The `solutions` folder is used to store `.sol` files.
4. The `transformed` folder is used to store presolved model files.

#### File
1. The `.out` file is used to store the log file for each test instance
2. The `.history` file contains the history of commands.
3. The `.file` file stores all test case names (including seeds).
4. The `.sh` file is used to run the statistics script.
5. The `.res` file contains the statistical results.

## Compare presolved model

Use SCIP or Cplex solver to preprocess the problem and output the compressed and preprocessed file `.mps.gz`.
This can be used to compare the differences between preprocessing of different solvers.
