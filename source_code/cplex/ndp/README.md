Introduction
------------

The directory "src/" includes all source files and header files for the code.
To successfully compile and run the code, it's necessary to have the CPLEX libraries and header files installed.
These files should be placed in the "lib/" and "include/" subdirectories within the "ThirdParty/cplex/" directory.

Building
--------

Use "Makefile" or "CMake" to compile and generate the executable file "cplex_callback_ndp" in "check/bin/" directory.

### 1. Building using CMake

```
mkdir build
cd build
cmake ..       [-DCMAKE_BUILD_TYPE=Debug\Release]
make
```

### 2. Building using Makefile

```
make           [OPT=Debug\Release]
```

How to run the solver
---------------------

### 1. local

The executable file can be run as
```
./bin/cplex_callback_ndp <data>.mps "<parameters list>"
```
or
```
make test SOLVER=cplex_callback_ndp SETTING=<setting file>
```

### 3. Pparameters list
1. TIME
2. MEM
3. SEED
4. MINGAP
5. MODE
6. CPX
7. OUTFILE
8. SETTING
