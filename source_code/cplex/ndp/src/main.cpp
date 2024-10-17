#include "functions.h"
#include "paramset.h"

int main(int argc, char** argv)
{
   if( argc > 3 )
   {
      PrintErr();
      return -1;
   }

   /* setting default parameters */
   Set* set = new Set();
   if( argc == 3 )
      set->ReadParam(argv[2]);
   set->PrintParam();

   /* set up to use MIP callback */
   HeurData* heurdata;
   heurdata = new HeurData();
   heurdata->Init(argv, set);

   /* get init time */
   GetTime(&heurdata->init_time);

   int origstatus = 0;
   bool success = false;
   /* declare environment */
   CPXENVptr origenv = NULL;
   /* declare mode */
   CPXLPptr origmodel = NULL;
   /* create environment */
   origenv = CPXopenCPLEX(&origstatus);
   /* create the problem */
   origmodel = CPXcreateprob(origenv, &origstatus, "orig");
   /* read the file, and copy the data into the created problem */
   origstatus = CPXreadcopyprob(origenv, origmodel, argv[1], NULL);
   if( origstatus ) { PrintErr(); goto TERMINATE; }

   /* get reference solution */
   success = set->check_benchmark_file(argv[1], &heurdata->refval);
   if( success )
   {
      origstatus = CPXsetbranchcallbackfunc(origenv, CheckCutOff, heurdata);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      /* simplex setting is useless */
      origstatus = CPXsetdblparam(origenv, CPXPARAM_Simplex_Limits_UpperObj, heurdata->refval);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }

   /* settings */
   if( set->heurmode == -1 )
   {
      origstatus = CPXsetintparam(origenv, CPXPARAM_Emphasis_MIP, CPX_MIPEMPHASIS_FEASIBILITY);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }
   else if( set->heurmode == -2 )
   {
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Strategy_RINSHeur, 20);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }
   else if( set->heurmode == -3 )
   {
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Strategy_LBHeur, CPX_ON);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }
   /* turn on/off output to the screen */
   origstatus = CPXsetintparam(origenv, CPXPARAM_ScreenOutput, CPX_ON);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* assure linear mappings between the presolved and original models */
   origstatus = CPXsetintparam(origenv, CPXPARAM_Preprocessing_Linear, 0);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   origstatus = CPXsetintparam(origenv, CPXPARAM_Preprocessing_Presolve, 0);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* let MIP callbacks work on the original model */
   origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Strategy_CallbackReducedLP, CPX_OFF);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* set mipgap */
   origstatus = CPXsetdblparam(origenv, CPXPARAM_MIP_Tolerances_MIPGap, set->mipgap);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* set time limit */
   origstatus = CPXsetdblparam(origenv, CPXPARAM_TimeLimit, set->timelimit);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* set memory limit */
   origstatus = CPXsetdblparam(origenv, CPXPARAM_MIP_Limits_TreeMemory, set->memorylimit);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* set random seed during the branch-and-bound process in cplex */
   origstatus = CPXsetintparam(origenv, CPX_PARAM_RANDOMSEED, set->randseed);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* turn on traditional search for use with control callbacks */
   origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Strategy_Search, CPX_MIPSEARCH_TRADITIONAL);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* set the maximum number of threads to 1. This instruction is redundant, if CPX control callbacks */
   /* Note! set threads may bring other unknowen changes of CPX */
   origstatus = CPXsetintparam(origenv, CPXPARAM_Threads, 1);
   if( origstatus ) { PrintErr(); goto TERMINATE; }

   if( set->heurmode <= 10 )
   {
      /* the routine to be called when an integer solution has been found but before */
      origstatus = CPXsetincumbentcallbackfunc(origenv, GetIncumbent, heurdata);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      /* the routine to be called after the subproblem has been solved to optimality */
      origstatus = CPXsetheuristiccallbackfunc(origenv, Heuristic, heurdata);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }
   else
   {
      /* LP-Based Heuristic, i.e. it is independent of branch-and-bound */
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Limits_Nodes, 1);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      /* disabled all cutting planes */
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Limits_CutPasses, -1);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      /* disabled all heuristics */
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Strategy_HeuristicFreq, -1);
      if( origstatus ) { PrintErr(); goto TERMINATE; }

      if( set->heurmode >= 11 && set->heurmode < 20 )
      {
         /* independent implementation of my GINS method + my Method 1 */
         origstatus = CPXsetheuristiccallbackfunc(origenv, Alone, heurdata);
         if( origstatus ) { PrintErr(); goto TERMINATE; }
      }
      else if( set->heurmode == 21 )
      {
         /* independent implementation of slope scaling method */
         origstatus = CPXsetheuristiccallbackfunc(origenv, LPBH1, heurdata);
         if( origstatus ) { PrintErr(); goto TERMINATE; }
      }
      else if( set->heurmode == 22 )
      {
         /* independent implementation of slope scaling + my Method 1 */
         origstatus = CPXsetheuristiccallbackfunc(origenv, LPBH2, heurdata);
         if( origstatus ) { PrintErr(); goto TERMINATE; }
      }
      else if( set->heurmode == 23 )
      {
         /* independent implementation of slope scaling + MIP solver */
         origstatus = CPXsetheuristiccallbackfunc(origenv, LPBH3, heurdata);
         if( origstatus ) { PrintErr(); goto TERMINATE; }
      }
   }

   if( set->cpxmode == 1 )
   {
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Cuts_FlowCovers, 2);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Cuts_MCFCut, 2);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
      origstatus = CPXsetintparam(origenv, CPXPARAM_MIP_Cuts_MIRCut, 2);
      if( origstatus ) { PrintErr(); goto TERMINATE; }
   }

   /* optimize the problem and obtain solution */
   origstatus = CPXmipopt(origenv, origmodel);
   if( origstatus ) { PrintErr(); goto TERMINATE; }
   /* write solution */
   //TODO check if no sol
   if( strcmp(set->soladress, " ") )
      CPXsolwrite(origenv, origmodel, set->soladress);

TERMINATE:

   /* output */
   double pb = 0.0;
   double db = 0.0;
   int node = CPXgetnodecnt(origenv, origmodel);
   CPXgetobjval(origenv, origmodel, &pb);
   if( success )
   {
      pb = Max(pb, heurdata->refval);
      db = heurdata->refval;
   }
   else
      CPXgetbestobjval(origenv, origmodel, &db);
   int ncol = CPXgetnumcols(origenv, origmodel);
   int nrow = CPXgetnumrows(origenv, origmodel);

   /* print */
   printf("\nCPX Callback NDP: %s\n\n", CPXversion(origenv));
   printf("CPX Return Code: %d\n", origstatus);
   printf("CPX Callback Results: pb %f db %f node %d\n", pb, db, node);
   printf("Problem Size: nrow %d ncol %d\n", nrow, ncol);
   printf("\n");
   printf("Time Stat - Total: %.2f \t Method1: %.2f \t Method2: %.2f\n", heurdata->time, heurdata->time1, heurdata->time2 );
   printf("Num Exec - Method1: %d \t Method2: %d\n", heurdata->nexec1, heurdata->nexec2 );
   printf("Num Succ - Method1: %d \t Method2: %d\n", heurdata->nsucc1, heurdata->nsucc2 );

   /* free all object, if necessary */
   if( origmodel != NULL )
      CPXfreeprob(origenv, &origmodel);
   if( origenv != NULL )
      CPXcloseCPLEX(&origenv);
   delete set;
   delete heurdata;

   return origstatus;
}
