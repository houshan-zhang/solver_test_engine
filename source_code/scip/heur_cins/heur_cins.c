/**@file   heur_cins.c
 * @ingroup DEFPLUGINS_HEUR
 * @brief  LNS heuristic that combines the incumbent with the LP optimum
 */

#include "blockmemshell/memory.h"
#include "scip/heuristics.h"
#include "scip/heur_cins.h"
#include "scip/pub_event.h"
#include "scip/pub_heur.h"
#include "scip/pub_message.h"
#include "scip/pub_misc.h"
#include "scip/pub_misc_sort.h"
#include "scip/pub_sol.h"
#include "scip/pub_var.h"
#include "scip/scip_branch.h"
#include "scip/scip_cons.h"
#include "scip/scip_copy.h"
#include "scip/scip_event.h"
#include "scip/scip_general.h"
#include "scip/scip_heur.h"
#include "scip/scip_lp.h"
#include "scip/scip_mem.h"
#include "scip/scip_message.h"
#include "scip/scip_nodesel.h"
#include "scip/scip_numerics.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_sol.h"
#include "scip/scip_solve.h"
#include "scip/scip_solvingstats.h"
#include <string.h>

#include "scip/pub_lp.h"

#define HEUR_NAME             "cins"
#define HEUR_DESC             "clique induced neighborhood search"
#define HEUR_DISPCHAR         SCIP_HEURDISPCHAR_LNS
//#define HEUR_PRIORITY         -1000000
#define HEUR_PRIORITY         -1000
#define HEUR_FREQ             5
#define HEUR_FREQOFS          0
#define HEUR_MAXDEPTH         -1
//#define HEUR_TIMING           SCIP_HEURTIMING_AFTERLPNODE
#define HEUR_TIMING           SCIP_HEURTIMING_DURINGLPLOOP
//#define HEUR_TIMING           SCIP_HEURTIMING_AFTERLPPLUNGE
#define HEUR_USESSUBSCIP      TRUE      /**< does the heuristic use a secondary SCIP instance? */

#define DEFAULT_NODESOFS      500       /* number of nodes added to the contingent of the total nodes          */
#define DEFAULT_MAXNODES      5000      /* maximum number of nodes to regard in the subproblem                 */
#define DEFAULT_MINNODES      50        /* minimum number of nodes to regard in the subproblem                 */
#define DEFAULT_MINIMPROVE    0.01      /* factor by which CINS should at least improve the incumbent          */
#define DEFAULT_MINFIXINGRATE 0.01      /* minimum percentage of all variables that have to be fixed       */
#define DEFAULT_FIXTYPE       0         /* */
#define DEFAULT_NODESQUOT     0.3       /* subproblem nodes in relation to nodes of the original problem       */
#define DEFAULT_LPLIMFAC      2.0       /* factor by which the limit on the number of LP depends on the node limit  */
#define DEFAULT_NWAITINGNODES 200       /* number of nodes without incumbent change that heuristic should wait */
#define DEFAULT_USELPROWS     FALSE     /* should subproblem be created out of the rows in the LP rows,
                                         * otherwise, the copy constructors of the constraints handlers are used */
#define DEFAULT_COPYCUTS      TRUE      /* if DEFAULT_USELPROWS is FALSE, then should all active cuts from the cutpool
                                         * of the original scip be copied to constraints of the subscip
                                         */
#define DEFAULT_USEUCT        FALSE     /* should uct node selection be used at the beginning of the search?     */

/* event handler properties */
#define EVENTHDLR_NAME         "Cins"
#define EVENTHDLR_DESC         "LP event handler for " HEUR_NAME " heuristic"

/*
 * Data structures
 */

/** primal heuristic data */
struct SCIP_HeurData
{
   int                   nodesofs;           /**< number of nodes added to the contingent of the total nodes          */
   int                   maxnodes;           /**< maximum number of nodes to regard in the subproblem                 */
   int                   minnodes;           /**< minimum number of nodes to regard in the subproblem                 */
   SCIP_Real             minfixingrate;      /**< minimum percentage of integer variables that have to be fixed       */
   int                   fixtype;
   int                   nwaitingnodes;      /**< number of nodes without incumbent change that heuristic should wait */
   SCIP_Real             minimprove;         /**< factor by which CINS should at least improve the incumbent          */
   SCIP_Real             nodelimit;          /**< the nodelimit employed in the current sub-SCIP, for the event handler*/
   SCIP_Real             lplimfac;           /**< factor by which the limit on the number of LP depends on the node limit */
   SCIP_Longint          usednodes;          /**< nodes already used by CINS in earlier calls                         */
   SCIP_Real             nodesquot;          /**< subproblem nodes in relation to nodes of the original problem       */
   SCIP_Bool             uselprows;          /**< should subproblem be created out of the rows in the LP rows?        */
   SCIP_Bool             copycuts;           /**< if uselprows == FALSE, should all active cuts from cutpool be copied
                                              *   to constraints in subproblem?
                                              */
   SCIP_Bool             useuct;             /**< should uct node selection be used at the beginning of the search?  */
};

/*
 * Local methods
 */




/** determines variable fixings for CINS
 *
 *  CINS fixes variables with matching solution values in the current LP and the
 *  incumbent solution
 */
   static
SCIP_RETCODE determineFixings(
      SCIP*                 scip,               /**< original SCIP data structure  */
      SCIP_VAR**            fixedvars,          /**< array to store source SCIP variables that should be fixed in the copy  */
      SCIP_Real*            fixedvals,          /**< array to store fixing values for variables that should be fixed in the copy */
      int*                  nfixedvars,         /**< pointer to store the number of variables that CINS can fix */
      SCIP_Real             minfixingrate,      /**< percentage of integer variables that have to be fixed */
      int                   fixtype,
      SCIP_Bool*            success             /**< pointer to store whether sufficiently many variable fixings were found */
      )
{
   SCIP_SOL* bestsol;                        /* incumbent solution of the original problem */
   SCIP_VAR** vars;                          /* original scip variables                    */
   SCIP_ROW** colrows;
   SCIP_COL* col;
   SCIP_ROW* row;
   SCIP_Real fixingrate;

   int nvars;
   int nbinvars;
   int nintvars;
   int nimplvars;
   int ncontvars;
   int i;
   int j;
   int idx;
   int fixingcounter;
   SCIP_Bool hasintfeas = TRUE;

   int* numrows;
   int* numdegr;
   SCIP_Real* weight;
   int* varfixseq;

   assert(fixedvals != NULL);
   assert(fixedvars != NULL);
   assert(nfixedvars != NULL);

   /* get required data of the original problem */
   SCIP_CALL( SCIPgetVarsData(scip, &vars, &nvars, &nbinvars, &nintvars, &nimplvars, &ncontvars) );
   if( SCIPgetNSols(scip) <= 0  )
      hasintfeas = FALSE;
   if( hasintfeas )
      bestsol = SCIPgetBestSol(scip);

   /* get score */
   SCIP_CALL( SCIPallocBufferArray(scip, &numrows, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &numdegr, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &weight, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &varfixseq, nvars) );
   for( i = 0; i < nvars; i++ )
   {
      numrows[i] = 0;
      numdegr[i] = 0;
      weight[i] = 0;
      varfixseq[i] = i;
      col = SCIPvarGetCol(vars[i]);
      numrows[i] = SCIPcolGetNLPNonz(col);
      colrows = SCIPcolGetRows(col);
      for( j = 0; j < numrows[i]; j++ )
      {
         row = colrows[j];
         numdegr[i] += SCIProwGetNLPNonz(row) - 1;
      }
      //int nlocksdown = SCIPvarGetNLocksDownType(cand, SCIP_LOCKTYPE_MODEL);
      //int nlocksup = SCIPvarGetNLocksUpType(cand, SCIP_LOCKTYPE_MODEL);
      //SCIP_CALL( SCIPvariableGraphCreate(scip, &vargraph, FALSE, 1.0, NULL) );
      if( fixtype == 0 )
      {
         weight[i] = numrows[i];
      }
      if( fixtype == 1 )
         weight[i] = numdegr[i];
      if( fixtype == 2 )
         weight[i] = numrows[i] + numdegr[i];
      if( fixtype == 3 )
         weight[i] = (double)(rand()%nvars);
   }
   //weight can not use again
   //SCIPsortRealInt(weight, varfixseq, nvars);

   fixingcounter = 0;
   /* determine variables to fix in the subproblem */
   for( i = 0; i < nvars; i++ )
   {
      SCIP_Real lpsolval;
      SCIP_Real solval;
      //idx = varfixseq[nvars-i-1];
      idx = i;
      if( SCIPvarGetType(vars[idx]) <= SCIP_VARTYPE_IMPLINT )
         continue;
      //printf("name %s\n", vars[idx]->name);
      /* get the current LP solution and the incumbent solution for each variable */
      lpsolval = SCIPvarGetLPSol(vars[idx]);
      if( hasintfeas )
      {
         solval = SCIPgetSolVal(scip, bestsol, vars[idx]);
         /* iff both solutions are equal, variable is stored to be fixed */
         if( SCIPisFeasEQ(scip, lpsolval, solval) )
         {
            /* store the fixing and increase the number of fixed variables */
            fixedvars[fixingcounter] = vars[idx];
            fixedvals[fixingcounter] = lpsolval;
            fixingcounter++;
         }
      }
      else
      {
         fixedvars[fixingcounter] = vars[idx];
         fixedvals[fixingcounter] = lpsolval;
         fixingcounter++;
      }

      //TODO
      /* can not use SCIPcolGetNLPNonz */
      //printf("weight %d name %s\n", SCIPcolGetNLPNonz(SCIPvarGetCol(vars[idx])), vars[idx]->name);
      //printf("weight %d\n", SCIPcolGetNLPNonz(SCIPvarGetCol(vars[idx])));
      //if( (SCIP_Real)fixingcounter / (SCIP_Real)(nvars) >= 0.6 || weight[nvars-i-1] <= 1 )
         //break;
   }

   SCIPfreeBufferArray(scip, &numrows);
   SCIPfreeBufferArray(scip, &numdegr);
   SCIPfreeBufferArray(scip, &weight);
   SCIPfreeBufferArray(scip, &varfixseq);

   /* store the number of fixings */
   *nfixedvars = fixingcounter;

   /* abort, if no variable should be fixed */
   if( fixingcounter == 0 )
   {
      *success = FALSE;
      return SCIP_OKAY;
   }
   else
      fixingrate = (SCIP_Real)fixingcounter / (SCIP_Real)(nvars);

   /* abort, if the amount of fixed variables is insufficient */
   if( fixingrate < minfixingrate )
   {
      *success = FALSE;
      return SCIP_OKAY;
   }

   *success = TRUE;
   return SCIP_OKAY;
}

static
SCIP_DECL_EVENTEXEC(eventExecCins);

/** wrapper for the part of heuristic that runs a subscip. Wrapper is needed to avoid possible ressource leaks */
   static
SCIP_RETCODE wrapperCins(
      SCIP*                 scip,               /**< original SCIP data structure                        */
      SCIP*                 subscip,            /**< SCIP structure of the subproblem                    */
      SCIP_HEUR*            heur,               /**< Heuristic pointer                                   */
      SCIP_HEURDATA*        heurdata,           /**< Heuristic's data                                    */
      SCIP_VAR**            vars,               /**< original problem's variables                        */
      SCIP_VAR**            fixedvars,          /**< Fixed variables of original SCIP                    */
      SCIP_Real*            fixedvals,          /**< Fixed values of original SCIP                       */
      SCIP_RESULT*          result,             /**< Result pointer                                      */
      int                   nvars,              /**< Number of variables                                 */
      int                   nfixedvars,         /**< Number of fixed variables                           */
      SCIP_Longint          nnodes              /**< Number of nodes in the b&b tree                     */
      )
{
   SCIP_VAR** subvars;                       /* variables of the subscip */
   SCIP_HASHMAP*  varmapfw;                  /* hashmap for mapping between vars of scip and subscip */
   SCIP_EVENTHDLR* eventhdlr;                /* event handler for LP events  */
   SCIP_Real upperbound;                     /* upperbound of the original SCIP */
   SCIP_Real cutoff;                         /* objective cutoff for the subproblem */

   SCIP_Bool success;

   int i;

   /* create the variable mapping hash map */
   SCIP_CALL( SCIPhashmapCreate(&varmapfw, SCIPblkmem(subscip), nvars) );

   /* create a problem copy as sub SCIP */
   SCIP_CALL( SCIPcopyLargeNeighborhoodSearch(scip, subscip, varmapfw, "cins", fixedvars, fixedvals, nfixedvars,
            heurdata->uselprows, heurdata->copycuts, &success, NULL) );

   eventhdlr = NULL;
   /* create event handler for LP events */
   SCIP_CALL( SCIPincludeEventhdlrBasic(subscip, &eventhdlr, EVENTHDLR_NAME, EVENTHDLR_DESC, eventExecCins, NULL) );
   if( eventhdlr == NULL )
   {
      SCIPerrorMessage("event handler for " HEUR_NAME " heuristic not found.\n");
      return SCIP_PLUGINNOTFOUND;
   }

   /* copy subproblem variables from map to obtain the same order */
   SCIP_CALL( SCIPallocBufferArray(scip, &subvars, nvars) );
   for( i = 0; i < nvars; i++ )
      subvars[i] = (SCIP_VAR*) SCIPhashmapGetImage(varmapfw, vars[i]);

   /* free hash map */
   SCIPhashmapFree(&varmapfw);

   /* do not abort subproblem on CTRL-C */
   SCIP_CALL( SCIPsetBoolParam(subscip, "misc/catchctrlc", FALSE) );

#ifdef SCIP_DEBUG
   /* for debugging, enable full output */
   SCIP_CALL( SCIPsetIntParam(subscip, "display/verblevel", SCIP_VERBLEVEL_FULL) );
   SCIP_CALL( SCIPsetIntParam(subscip, "display/freq", 100000000) );
#else
   /* disable statistic timing inside sub SCIP and output to console */
   SCIP_CALL( SCIPsetIntParam(subscip, "display/verblevel", (int) SCIP_VERBLEVEL_NONE) );
   SCIP_CALL( SCIPsetBoolParam(subscip, "timing/statistictiming", FALSE) );
#endif

   /* set limits for the subproblem */
   SCIP_CALL( SCIPcopyLimits(scip, subscip) );
   heurdata->nodelimit = nnodes;
   SCIP_CALL( SCIPsetLongintParam(subscip, "limits/nodes", nnodes) );
   SCIP_CALL( SCIPsetLongintParam(subscip, "limits/stallnodes", MAX(10, nnodes/10)) );
   SCIP_CALL( SCIPsetIntParam(subscip, "limits/bestsol", 3) );

   /* forbid recursive call of heuristics and separators solving subMIPs */
   SCIP_CALL( SCIPsetSubscipsOff(subscip, TRUE) );

   /* disable cutting plane separation */
   SCIP_CALL( SCIPsetSeparating(subscip, SCIP_PARAMSETTING_OFF, TRUE) );

   /* disable expensive presolving */
   SCIP_CALL( SCIPsetPresolving(subscip, SCIP_PARAMSETTING_FAST, TRUE) );

   /* use best estimate node selection */
   if( SCIPfindNodesel(subscip, "estimate") != NULL && !SCIPisParamFixed(subscip, "nodeselection/estimate/stdpriority") )
   {
      SCIP_CALL( SCIPsetIntParam(subscip, "nodeselection/estimate/stdpriority", INT_MAX/4) );
   }

   /* activate uct node selection at the top of the tree */
   if( heurdata->useuct && SCIPfindNodesel(subscip, "uct") != NULL && !SCIPisParamFixed(subscip, "nodeselection/uct/stdpriority") )
   {
      SCIP_CALL( SCIPsetIntParam(subscip, "nodeselection/uct/stdpriority", INT_MAX/2) );
   }

   /* use inference branching */
   if( SCIPfindBranchrule(subscip, "inference") != NULL && !SCIPisParamFixed(subscip, "branching/inference/priority") )
   {
      SCIP_CALL( SCIPsetIntParam(subscip, "branching/inference/priority", INT_MAX/4) );
   }

   /* enable conflict analysis, disable analysis of boundexceeding LPs, and restrict conflict pool */
   if( !SCIPisParamFixed(subscip, "conflict/enable") )
   {
      SCIP_CALL( SCIPsetBoolParam(subscip, "conflict/enable", TRUE) );
   }
   if( !SCIPisParamFixed(subscip, "conflict/useboundlp") )
   {
      SCIP_CALL( SCIPsetCharParam(subscip, "conflict/useboundlp", 'o') );
   }
   if( !SCIPisParamFixed(subscip, "conflict/maxstoresize") )
   {
      SCIP_CALL( SCIPsetIntParam(subscip, "conflict/maxstoresize", 100) );
   }

   /* speed up sub-SCIP by not checking dual LP feasibility */
   SCIP_CALL( SCIPsetBoolParam(subscip, "lp/checkdualfeas", FALSE) );

   /* add an objective cutoff */
   if( SCIPgetNSols(scip) >= 1 )
   {
      upperbound = SCIPgetUpperbound(scip) - SCIPsumepsilon(scip);
      if( !SCIPisInfinity(scip, -1.0 * SCIPgetLowerbound(scip)) )
      {
         cutoff = (1 - heurdata->minimprove) * SCIPgetUpperbound(scip) + heurdata->minimprove * SCIPgetLowerbound(scip);
      }
      else
      {
         if( SCIPgetUpperbound(scip) >= 0 )
            cutoff = (1 - heurdata->minimprove) * SCIPgetUpperbound(scip);
         else
            cutoff = (1 + heurdata->minimprove) * SCIPgetUpperbound(scip);
      }
      cutoff = MIN(upperbound, cutoff);
      SCIP_CALL( SCIPsetObjlimit(subscip, cutoff) );
   }

   /* catch LP events of sub-SCIP */
   SCIP_CALL( SCIPtransformProb(subscip) );
   SCIP_CALL( SCIPcatchEvent(subscip, SCIP_EVENTTYPE_LPSOLVED, eventhdlr, (SCIP_EVENTDATA*) heurdata, NULL) );

   /* Errors in solving the subproblem should not kill the overall solving process
    * Hence, the return code is caught and a warning is printed, only in debug mode, SCIP will stop.
    */
   /* solve the subproblem */
   SCIP_CALL_ABORT( SCIPsolve(subscip) );

   /* drop LP events of sub-SCIP */
   SCIP_CALL( SCIPdropEvent(subscip, SCIP_EVENTTYPE_LPSOLVED, eventhdlr, (SCIP_EVENTDATA*) heurdata, -1) );

   /* we try to merge variable statistics with those of our main SCIP */
   SCIP_CALL( SCIPmergeVariableStatistics(subscip, scip, subvars, vars, nvars) );

   /* print solving statistics of subproblem if we are in SCIP's debug mode */
   SCIPdebug( SCIP_CALL( SCIPprintStatistics(subscip, NULL) ) );

   heurdata->usednodes += SCIPgetNNodes(subscip);

   SCIP_CALL( SCIPtranslateSubSols(scip, subscip, heur, subvars, &success, NULL) );
   if( success )
      *result = SCIP_FOUNDSOL;

   /* free subproblem */
   SCIPfreeBufferArray(scip, &subvars);

   return SCIP_OKAY;
}

/* ---------------- Callback methods of event handler ---------------- */

/* exec the event handler
 *
 * we interrupt the solution process
 */
   static
SCIP_DECL_EVENTEXEC(eventExecCins)
{
   SCIP_HEURDATA* heurdata;

   assert(eventhdlr != NULL);
   assert(eventdata != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);
   assert(event != NULL);
   assert(SCIPeventGetType(event) & SCIP_EVENTTYPE_LPSOLVED);

   heurdata = (SCIP_HEURDATA*)eventdata;
   assert(heurdata != NULL);

   /* interrupt solution process of sub-SCIP */
   if( SCIPgetNLPs(scip) > heurdata->lplimfac * heurdata->nodelimit )
   {
      SCIPdebugMsg(scip, "interrupt after  %" SCIP_LONGINT_FORMAT " LPs\n",SCIPgetNLPs(scip));
      SCIP_CALL( SCIPinterruptSolve(scip) );
   }

   return SCIP_OKAY;
}


/*
 * Callback methods of primal heuristic
 */

/** copy method for primal heuristic plugins (called when SCIP copies plugins) */
   static
SCIP_DECL_HEURCOPY(heurCopyCins)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(heur != NULL);
   assert(strcmp(SCIPheurGetName(heur), HEUR_NAME) == 0);

   /* call inclusion method of primal heuristic */
   SCIP_CALL( SCIPincludeHeurCins(scip) );

   return SCIP_OKAY;
}

/** destructor of primal heuristic to free user data (called when SCIP is exiting) */
   static
SCIP_DECL_HEURFREE(heurFreeCins)
{  /*lint --e{715}*/
   SCIP_HEURDATA* heurdata;

   assert( heur != NULL );
   assert( scip != NULL );

   /* get heuristic data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* free heuristic data */
   SCIPfreeBlockMemory(scip, &heurdata);
   SCIPheurSetData(heur, NULL);

   return SCIP_OKAY;
}


/** initialization method of primal heuristic (called after problem was transformed) */
   static
SCIP_DECL_HEURINIT(heurInitCins)
{  /*lint --e{715}*/
   SCIP_HEURDATA* heurdata;

   assert( heur != NULL );
   assert( scip != NULL );

   /* get heuristic's data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* initialize data */
   heurdata->usednodes = 0;

   return SCIP_OKAY;
}


/** execution method of primal heuristic */
   static
SCIP_DECL_HEUREXEC(heurExecCins)
{  /*lint --e{715}*/
   SCIP_Longint nnodes;

   SCIP_HEURDATA* heurdata;                  /* heuristic's data                    */
   SCIP* subscip;                            /* the subproblem created by CINS      */
   SCIP_VAR** vars;                          /* original problem's variables        */
   SCIP_VAR** fixedvars;
   SCIP_Real* fixedvals;

   SCIP_RETCODE retcode;                     /* retcode needed for wrapper method  */

   int nvars;
   int nbinvars;
   int nintvars;
   int nimplvars;
   int ncontvars;
   int nfixedvars;

   SCIP_Bool success;

   assert( heur != NULL );
   assert( scip != NULL );
   assert( result != NULL );
   assert( SCIPhasCurrentNodeLP(scip) );

   *result = SCIP_DELAYED;

   /* do not call heuristic of node was already detected to be infeasible */
   if( nodeinfeasible )
      return SCIP_OKAY;

   /* get heuristic's data */
   heurdata = SCIPheurGetData(heur);
   assert( heurdata != NULL );

   /* only call heuristic, if an optimal LP solution or a feasible solution are at hand */
   //if( SCIPgetLPSolstat(scip) != SCIP_LPSOLSTAT_OPTIMAL || SCIPgetNSols(scip) <= 0  )
   if( SCIPgetLPSolstat(scip) != SCIP_LPSOLSTAT_OPTIMAL )
      return SCIP_OKAY;

   /* only call heuristic, if the LP objective value is smaller than the cutoff bound */
   if( SCIPisGE(scip, SCIPgetLPObjval(scip), SCIPgetCutoffbound(scip)) )
      return SCIP_OKAY;

   *result = SCIP_DIDNOTRUN;

   /* calculate the maximal number of branching nodes until heuristic is aborted */
   nnodes = (SCIP_Longint)(heurdata->nodesquot * SCIPgetNNodes(scip));

   /* reward CINS if it succeeded often */
   nnodes = (SCIP_Longint)(nnodes * (SCIPheurGetNBestSolsFound(heur)+1.0)/(SCIPheurGetNCalls(heur) + 1.0));
   nnodes -= (SCIP_Longint)(100.0 * SCIPheurGetNCalls(heur));  /* count the setup costs for the sub-MIP as 100 nodes */
   nnodes += heurdata->nodesofs;

   /* determine the node limit for the current process */
   nnodes -= heurdata->usednodes;
   nnodes = MIN(nnodes, heurdata->maxnodes);

   /* check whether we have enough nodes left to call subproblem solving */
   if( nnodes < heurdata->minnodes )
      return SCIP_OKAY;

   SCIP_CALL( SCIPgetVarsData(scip, &vars, &nvars, &nbinvars, &nintvars, &nimplvars, &ncontvars) );

   if( SCIPisStopped(scip) )
      return SCIP_OKAY;

   /* allocate buffer storage to hold the CINS fixings */
   SCIP_CALL( SCIPallocBufferArray(scip, &fixedvars, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &fixedvals, nvars) );

   success = FALSE;

   nfixedvars = 0;
   /* determine possible fixings for CINS: variables with same value in bestsol and LP relaxation */
   SCIP_CALL( determineFixings(scip, fixedvars, fixedvals, &nfixedvars, heurdata->minfixingrate, heurdata->fixtype, &success) );

   /* too few variables could be fixed by the CINS scheme */
   if( !success )
      goto TERMINATE;

   /* check whether there is enough time and memory left */
   SCIP_CALL( SCIPcheckCopyLimits(scip, &success) );

   /* abort if no time is left or not enough memory to create a copy of SCIP */
   if( !success )
      goto TERMINATE;

   assert(nfixedvars > 0 && nfixedvars < nvars);

   *result = SCIP_DIDNOTFIND;

   SCIPdebugMsg(scip, "CINS heuristic fixes %d out of %d variables\n", nfixedvars, nvars);
   SCIP_CALL( SCIPcreate(&subscip) );

   retcode = wrapperCins(scip, subscip, heur, heurdata, vars, fixedvars, fixedvals, result, nvars, nfixedvars, nnodes);

   SCIP_CALL( SCIPfree(&subscip) );

   SCIP_CALL( retcode );

TERMINATE:
   SCIPfreeBufferArray(scip, &fixedvals);
   SCIPfreeBufferArray(scip, &fixedvars);

   return SCIP_OKAY;
}

/*
 * primal heuristic specific interface methods
 */

/** creates the CINS primal heuristic and includes it in SCIP */
SCIP_RETCODE SCIPincludeHeurCins(
      SCIP*                 scip                /**< SCIP data structure */
      )
{
   SCIP_HEURDATA* heurdata;
   SCIP_HEUR* heur;

   /* create Cins primal heuristic data */
   SCIP_CALL( SCIPallocBlockMemory(scip, &heurdata) );

   /* include primal heuristic */
   SCIP_CALL( SCIPincludeHeurBasic(scip, &heur,
            HEUR_NAME, HEUR_DESC, HEUR_DISPCHAR, HEUR_PRIORITY, HEUR_FREQ, HEUR_FREQOFS,
            HEUR_MAXDEPTH, HEUR_TIMING, HEUR_USESSUBSCIP, heurExecCins, heurdata) );

   assert(heur != NULL);

   /* set non-NULL pointers to callback methods */
   SCIP_CALL( SCIPsetHeurCopy(scip, heur, heurCopyCins) );
   SCIP_CALL( SCIPsetHeurFree(scip, heur, heurFreeCins) );
   SCIP_CALL( SCIPsetHeurInit(scip, heur, heurInitCins) );

   /* add CINS primal heuristic parameters */
   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/" HEUR_NAME "/nodesofs",
            "number of nodes added to the contingent of the total nodes",
            &heurdata->nodesofs, FALSE, DEFAULT_NODESOFS, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/" HEUR_NAME "/maxnodes",
            "maximum number of nodes to regard in the subproblem",
            &heurdata->maxnodes, TRUE, DEFAULT_MAXNODES, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/" HEUR_NAME "/minnodes",
            "minimum number of nodes required to start the subproblem",
            &heurdata->minnodes, TRUE, DEFAULT_MINNODES, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/" HEUR_NAME "/fixtype",
            "fix var type",
            &heurdata->fixtype, TRUE, DEFAULT_FIXTYPE, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip, "heuristics/" HEUR_NAME "/nodesquot",
            "contingent of sub problem nodes in relation to the number of nodes of the original problem",
            &heurdata->nodesquot, FALSE, DEFAULT_NODESQUOT, 0.0, 1.0, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip, "heuristics/" HEUR_NAME "/nwaitingnodes",
            "number of nodes without incumbent change that heuristic should wait",
            &heurdata->nwaitingnodes, TRUE, DEFAULT_NWAITINGNODES, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip, "heuristics/" HEUR_NAME "/minimprove",
            "factor by which " HEUR_NAME " should at least improve the incumbent",
            &heurdata->minimprove, TRUE, DEFAULT_MINIMPROVE, 0.0, 1.0, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip, "heuristics/" HEUR_NAME "/minfixingrate",
            "minimum percentage of integer variables that have to be fixed",
            &heurdata->minfixingrate, FALSE, DEFAULT_MINFIXINGRATE, 0.0, 1.0, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip, "heuristics/" HEUR_NAME "/lplimfac",
            "factor by which the limit on the number of LP depends on the node limit",
            &heurdata->lplimfac, TRUE, DEFAULT_LPLIMFAC, 1.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "heuristics/" HEUR_NAME "/uselprows",
            "should subproblem be created out of the rows in the LP rows?",
            &heurdata->uselprows, TRUE, DEFAULT_USELPROWS, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "heuristics/" HEUR_NAME "/copycuts",
            "if uselprows == FALSE, should all active cuts from cutpool be copied to constraints in subproblem?",
            &heurdata->copycuts, TRUE, DEFAULT_COPYCUTS, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip, "heuristics/" HEUR_NAME "/useuct",
            "should uct node selection be used at the beginning of the search?",
            &heurdata->useuct, TRUE, DEFAULT_USEUCT, NULL, NULL) );

   return SCIP_OKAY;
}
