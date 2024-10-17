/**
 * @file main.cpp
 * @brief Solution Checker main file
 *
 */

#include "model.h"
#include "mpsinput.h"
#include "gmputils.h"

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>

int main (int argc, char const *argv[])
{
   if( argc < 3 || argc > 5 )
   {
      printf("Usage: solchecker filename.mps[.gz] solution.sol [linear_tol int_tol]\n");
      return 0;
   }

   /* read model */
   Model* model = new Model;
   MpsInput* mpsi = new MpsInput;
   bool success = mpsi->readMps(argv[1], model);
   printf("Read MPS: %d\n", success);
   if( !success )
      return 0;
   printf("MIP has %d vars and %d constraints\n", model->numVars(), model->numConss());

   /* read solution */
   success = model->readSol(argv[2]);
   printf("Read SOL: %d\n", success);
   if( !success )
      return 0;
   if( !model->hasObjectiveValue )
      printf("No objective value given\n");
   printf("\n");

   /* default tolerances */
   Rational linearTolerance(1, 10000);
   Rational intTolerance(linearTolerance);

   /* read tolerances */
   if( argc > 3 )
   {
      linearTolerance.fromString(argv[3]);
      intTolerance.fromString(argv[3]);
   }
   if( argc > 4 )
      intTolerance.fromString(argv[4]);

   printf("Integrality tolerance:   %s\n", intTolerance.toString().c_str());
   printf("Linear tolerance:        %s\n", linearTolerance.toString().c_str());
   printf("Objective tolerance:     %s\n", linearTolerance.toString().c_str());
   printf("\n");

   /* check feasibility of solution and correctness of objective value */
   bool intFeas;
   bool linFeas;
   bool obj;
   model->check(intTolerance, linearTolerance, intFeas, linFeas, obj);

   printf("Check SOL: Integrality %d Constraints %d Objective %d\n", intFeas, linFeas, obj);

   /* calculate maximum violations */
   Rational intViol;
   Rational linearViol;
   Rational objViol;
   model->maxViolations(intViol, linearViol, objViol);

   printf("Maximum violations: Integrality %f Constraints %f Objective %f\n", intViol.toDouble(), linearViol.toDouble(), objViol.toDouble());

   delete mpsi;
   delete model;
   return 0;
}
