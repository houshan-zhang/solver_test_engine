#ifndef __PARAMSET__
#define __PARAMSET__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* all settings */
class Set
{
   public:
      Set();                                 /* constructor to set the default value */
      ~Set();
      void ReadParam(char* input);           /* read parameters from a string */
      void ReadFile(char* paramfile);        /* read parameters from a file */
      void PrintParam();                     /* print parameter values */
      /* parameters */
      char* soladress = (char*)malloc(2000 * sizeof(char));
      bool check_benchmark_file(char* path, double* ref_obj);
      int timelimit;
      int memorylimit;
      int randseed;
      double mipgap;
      int heurmode;
      int cpxmode;
};

#endif
