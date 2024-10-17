#include "blockmemshell/memory.h"
#include "scip/cons_linear.h"
#include "scip/debug.h"
#include "scip/presol_extended_sparsify.h"
#include "scip/pub_cons.h"
#include "scip/pub_matrix.h"
#include "scip/pub_message.h"
#include "scip/pub_misc.h"
#include "scip/pub_misc_sort.h"
#include "scip/pub_presol.h"
#include "scip/pub_var.h"
#include "scip/scip_cons.h"
#include "scip/scip_general.h"
#include "scip/scip_mem.h"
#include "scip/scip_message.h"
#include "scip/scip_nlp.h"
#include "scip/scip_numerics.h"
#include "scip/scip_param.h"
#include "scip/scip_presol.h"
#include "scip/scip_pricer.h"
#include "scip/scip_prob.h"
#include "scip/scip_probing.h"
#include "scip/scip_solvingstats.h"
#include "scip/scip_timing.h"
#include "scip/scip_var.h"
#include <string.h>

#include "scip/struct_var.h"
#include "scip/struct_scip.h"
#include "scip/struct_set.h"
#include <time.h>

#define PRESOL_NAME                 "extended_sparsify"
#define PRESOL_DESC                 "perform extended sparsify to simplify model"
#define PRESOL_PRIORITY             -230000     /**< priority of the presolver (>= 0: before, < 0: after constraint handlers) */
#define PRESOL_MAXROUNDS            -1          /**< maximal number of presolving rounds the presolver participates in (-1: no limit) */
#define PRESOL_ISACCURATE           TRUE        /**< if the number of nonzeros is accurately computed */
#define PRESOL_ENABLECOPY           TRUE        /**< should presolver be copied to sub-SCIPs? */
#define PRESOL_TIMING               SCIP_PRESOLTIMING_EXHAUSTIVE  /* timing of the presolver (fast, medium, or exhaustive) */

/* just for debug */
#define DEBUG_ISPRINT               FALSE       /**< whether print the presolve process */
/* this substitution process may lead to some issues, so we use the corressponding restriction */
/* 1) increase nonzeros of the costraints matrix */
#define INEQUALITY                  TRUE        /**< do inequ? */
#define CANCEL_ROW                  5           /**< minimal eleminated nonzeros per reduction */
#define CANCEL_TOT                  50          /**< minimal eleminated nonzeros if add a variable */
#define PENALTY                     0           /**< penalty of changing coefficient */
#define DEFAULT_SCALING_RATIO       1000
/* in order to limit the time consuming, we use the corressponding restriction */
#define DEFAULT_MAX_COLNONZEROS     20          /**< maximal number of considered nonzeros within one column (-1: no limit) */
#define DEFAULT_MAX_ROWNONZEROS     20          /**< maximal number of considered nonzeros within one row (-1: no limit) */
#define DEFAULT_WAITINGFAC          2.0         /**< number of calls to wait until next execution as a multiple of the number of useless calls */
#define MARKOWITZ                   0.01        /**< max abs coefficients ratio */


/*
 * Data structures
 */

/** presolver data */
struct SCIP_PresolData
{
   /* statistical data */
   int                   nvar;               /**< number of all variables */
   int                   ncancels;           /**< total number of canceled nonzeros (net value, i.e., removed minus added nonzeros) */
   int                   nexec;              /**< number of executions */
   int                   naddvar;              /**< number of deleted variables */
   SCIP_Real             time;

   /* compare with traditional variable aggregation method */

   int                   nfailures;          /**< number of calls to presolver without success */
   int                   nwaitingcalls;      /**< number of presolver calls until next real execution */

   /* presolve setting */
   SCIP_Bool             isprint;
   SCIP_Bool             isaccurate;
   SCIP_Bool             inequality;
   int                   cancel_row;
   int                   cancel_tot;
   int                   penalty;
   int                   scaling_ratio;
   int                   maxcolnonzeros;     /**< maximal number of considered nonzeros within one column (-1: no limit) */
   int                   maxrownonzeros;     /**< maximal number of considered nonzeros within one row (-1: no limit) */
   SCIP_Bool             enablecopy;         /**< should presolver be copied to sub-SCIPs? */
   SCIP_Real             waitingfac;         /**< number of calls to wait until next execution as a multiple of the number of useless calls */

   SCIP_Bool             isinit;
};

/** calculate minimal activity of one row */
static SCIP_Bool getMinActivitySingleRow(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   row,                /**< row index */
      SCIP_Real*            value               /**< min activity */
      )
{
   SCIP_Real* valptr;
   int* rowptr;
   int* rowend;
   SCIP_Real minactivity;
   SCIP_Real val;
   SCIP_Real lb;
   SCIP_Real ub;
   int c;

   assert(scip != NULL);
   assert(matrix != NULL);
   assert(value != NULL);

   minactivity = 0;

   rowptr = SCIPmatrixGetRowIdxPtr(matrix, row);
   rowend = rowptr + SCIPmatrixGetRowNNonzs(matrix, row);
   valptr = SCIPmatrixGetRowValPtr(matrix, row);

   for( ; (rowptr < rowend); rowptr++, valptr++ )
   {
      c = *rowptr;
      val = *valptr;

      if( val > 0.0 )
      {
         lb = SCIPmatrixGetColLb(matrix, c);
         if( SCIPisInfinity(scip, -lb) )
            return FALSE;
         minactivity += val * lb;
      }
      else if( val < 0.0 )
      {
         ub = SCIPmatrixGetColUb(matrix, c);
         if( SCIPisInfinity(scip, ub) )
            return FALSE;
         minactivity += val * ub;
      }
   }
   *value = minactivity;
   return TRUE;
}

/** calculate maximal activity of one row */
static SCIP_Bool getMaxActivitySingleRow(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   row,                /**< row index */
      SCIP_Real*            value               /**< max activity */
      )
{
   SCIP_Real* valptr;
   int* rowptr;
   int* rowend;
   SCIP_Real maxactivity;
   SCIP_Real val;
   SCIP_Real lb;
   SCIP_Real ub;
   int c;

   assert(scip != NULL);
   assert(matrix != NULL);
   assert(value != NULL);

   maxactivity = 0;

   rowptr = SCIPmatrixGetRowIdxPtr(matrix, row);
   rowend = rowptr + SCIPmatrixGetRowNNonzs(matrix, row);
   valptr = SCIPmatrixGetRowValPtr(matrix, row);

   for( ; (rowptr < rowend); rowptr++, valptr++ )
   {
      c = *rowptr;
      val = *valptr;

      if( val > 0.0 )
      {
         ub = SCIPmatrixGetColUb(matrix, c);
         if( SCIPisInfinity(scip, ub) )
            return FALSE;
         maxactivity += val * ub;
      }
      else if( val < 0.0 )
      {
         lb = SCIPmatrixGetColLb(matrix, c);
         if( SCIPisInfinity(scip, -lb) )
            return FALSE;
         maxactivity += val * lb;
      }
   }
   *value = maxactivity;
   return TRUE;
}

/** aggregate variables */
static SCIP_Bool reduction(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,         /**< presolver data */
      SCIP_Real*            ratios,
      int                   colidx,
      int                   rowidx,             /**< the current row */
      SCIP_Bool*            blockedrows,
      int*                  residue_length
      )
{
   /* aggregation variable */
   SCIP_VAR** vars;
   /* aggregation variable coefficient */
   SCIP_Real* coefs;
   /* newvar's information */
   char newvarname[SCIP_MAXSTRLEN];
   char newconname[SCIP_MAXSTRLEN];
   SCIP_CONS* newcons;
   SCIP_VAR* newvar;
   SCIP_VARTYPE newvartype;
   SCIP_Real newlb;
   SCIP_Real newub;
   SCIP_Real lhs;
   SCIP_Real rhs;
   SCIP_Bool isequ;
   SCIP_Real ratio;
   int i;
   int index;
   int* colptr;
   SCIP_Real* coefptr;
   int indexsub;
   int* colsubptr;
   SCIP_Real* coefsubptr;
   int* colend;
   int* colsubend;
   int colsize = SCIPmatrixGetColNNonzs(matrix, colidx);
   int nvars;
   lhs = SCIPmatrixGetRowLhs(matrix, rowidx);
   rhs = SCIPmatrixGetRowRhs(matrix, rowidx);
   isequ = SCIPisEQ(scip, lhs, rhs) ? TRUE : FALSE;


   if( !isequ )
   {
      /* covert "lhs <= \sum x <= rhs" to "\sum x - y = 0, lhs <= y <= rhs" */
      /* creat a new variable */
      if( !SCIPisInfinity(scip, -lhs) )
         newlb = lhs;
      else
      {
         SCIP_Real minactivity;
         if( getMinActivitySingleRow(scip, matrix, rowidx, &minactivity) )
            newlb = minactivity;
         else
            newlb = -SCIPinfinity(scip);
      }
      if( !SCIPisInfinity(scip, rhs) )
         newub = rhs;
      else
      {
         SCIP_Real maxactivity;
         if( getMaxActivitySingleRow(scip, matrix, rowidx, &maxactivity) )
            newub = maxactivity;
         else
            newub = SCIPinfinity(scip);
      }
      newvartype = SCIP_VARTYPE_CONTINUOUS;
      (void) SCIPsnprintf(newvarname, SCIP_MAXSTRLEN, "slack_%d", presoldata->naddvar);
      presoldata->naddvar += 1;

      SCIP_CALL( SCIPcreateVar(scip, &newvar, newvarname, newlb, newub, 0.0, newvartype,
               FALSE, FALSE, NULL, NULL, NULL, NULL, NULL) );
      SCIP_CALL( SCIPaddVar(scip, newvar) );

      /* creat a new constraint */
      nvars = SCIPmatrixGetRowNNonzs(matrix, rowidx);
      SCIP_CALL( SCIPallocBufferArray(scip, &coefs, nvars+1) );
      SCIP_CALL( SCIPallocBufferArray(scip, &vars, nvars+1) );
      colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
      colend = colptr + nvars;
      coefptr = SCIPmatrixGetRowValPtr(matrix, rowidx);
      for( i = 0 ; colptr < colend ; i++, colptr++, coefptr++ )
      {
         vars[i] = SCIPmatrixGetVar(matrix, *colptr);
         coefs[i] = *coefptr;
      }
      vars[i] = newvar;
      coefs[i] = -1;
      (void) SCIPsnprintf(newconname, SCIP_MAXSTRLEN, "%s_equ", SCIPmatrixGetRowName(matrix, rowidx));
      SCIP_CALL( SCIPcreateConsLinear(scip, &newcons, newconname, nvars+1, vars, coefs,
               0, 0, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) );
      SCIPaddCons(scip, newcons);
      SCIPreleaseCons(scip, &newcons);
      SCIPdelCons(scip, SCIPmatrixGetCons(matrix, rowidx));
      blockedrows[rowidx] = TRUE;
   }
   /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

   /* the removed or changed row would be counted correctly */
   int* rowsubptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   int* rowend = rowsubptr + colsize;
   for( i = 0 ; rowsubptr < rowend; i++, rowsubptr++ )
   {
      if( residue_length[i] == -1 )
         continue;
      blockedrows[*rowsubptr] = TRUE;
      int seq = 0;
      ratio = ratios[i];
      SCIP_Real* cancelrowcoefs;
      SCIP_VAR** cancelrowvars;
      SCIPallocBufferArray(scip, &cancelrowcoefs, residue_length[i]);
      SCIPallocBufferArray(scip, &cancelrowvars, residue_length[i]);
      /* change rowsubptr to reduce nonzeros */
      /*----------*/
      colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
      colend = colptr + SCIPmatrixGetRowNNonzs(matrix, rowidx);
      coefptr = SCIPmatrixGetRowValPtr(matrix, rowidx);
      colsubptr = SCIPmatrixGetRowIdxPtr(matrix, *rowsubptr);
      colsubend = colsubptr + SCIPmatrixGetRowNNonzs(matrix, *rowsubptr);
      coefsubptr = SCIPmatrixGetRowValPtr(matrix, *rowsubptr);
      while( colptr < colend && colsubptr < colsubend )
      {
         if( *colptr == colidx && *colsubptr == colidx )
         {
            colptr += 1;
            coefptr += 1;
            colsubptr += 1;
            coefsubptr += 1;
            continue;
         }
         if( *colptr == colidx )
         {
            colptr += 1;
            coefptr += 1;
            continue;
         }
         if( *colsubptr == colidx )
         {
            colsubptr += 1;
            coefsubptr += 1;
            continue;
         }
         /* compare variable except colidx */
         index = SCIPmatrixGetVar(matrix, *colptr)->index;
         indexsub = SCIPmatrixGetVar(matrix, *colsubptr)->index;
         //printf("IDXCOL= %s\n", SCIPmatrixGetVar(matrix, colidx)->name);
         //printf("Var %s Idx %d Var %s Idx %d\n", SCIPmatrixGetVar(matrix, *colptr)->name, index, SCIPmatrixGetVar(matrix, *colsubptr)->name, indexsub);
         if( index == indexsub )
         {
            if( !SCIPisEQ(scip, (*coefptr) * ratio, *coefsubptr) )
            {
               cancelrowvars[seq] = SCIPmatrixGetVar(matrix, *colptr);
               cancelrowcoefs[seq] = *coefsubptr - (*coefptr) * ratio;
               seq += 1;
               assert(seq <= residue_length[i]);
            }
            colptr += 1;
            coefptr += 1;
            colsubptr += 1;
            coefsubptr += 1;
         }
         else if( index < indexsub )
         {
            cancelrowvars[seq] = SCIPmatrixGetVar(matrix, *colptr);
            cancelrowcoefs[seq] = -(*coefptr) * ratio;
            seq += 1;
            assert(seq <= residue_length[i]);
            colptr += 1;
            coefptr += 1;
         }
         else
         {
            cancelrowvars[seq] = SCIPmatrixGetVar(matrix, *colsubptr);
            cancelrowcoefs[seq] = *coefsubptr;
            seq += 1;
            assert(seq <= residue_length[i]);
            colsubptr += 1;
            coefsubptr += 1;
         }
      }
      if( colptr < colend )
      {
         while( colptr < colend )
         {
            if( *colptr == colidx )
            {
               colptr += 1;
               coefptr += 1;
               continue;
            }
            cancelrowvars[seq] = SCIPmatrixGetVar(matrix, *colptr);
            cancelrowcoefs[seq] = -(*coefptr) * ratio;
            seq += 1;
            assert(seq <= residue_length[i]);
            colptr++;
            coefptr++;
         }
      }
      if( colsubptr < colsubend )
      {
         while( colsubptr < colsubend )
         {
            if( *colsubptr == colidx )
            {
               colsubptr += 1;
               coefsubptr += 1;
               continue;
            }
            cancelrowvars[seq] = SCIPmatrixGetVar(matrix, *colsubptr);
            cancelrowcoefs[seq] = *coefsubptr;
            seq += 1;
            assert(seq <= residue_length[i]);
            colsubptr++;
            coefsubptr++;
         }
      }
      if( !isequ )
      {
         cancelrowvars[seq] = newvar;
         cancelrowcoefs[seq] = ratio;
         seq += 1;
      }
      if( presoldata->isprint )
      {
         printf("--------------------\n");
         for( int z = 0; z < seq; z++ )
            printf("remain row = var: %s, coef: %.2f\n", cancelrowvars[z]->name, cancelrowcoefs[z]);
      }
      assert(seq == residue_length[i]);
      /*----------*/
      char cancelconsname[SCIP_MAXSTRLEN];
      SCIP_CONS* cancelcons;
      (void) SCIPsnprintf(cancelconsname, SCIP_MAXSTRLEN, "%s_rem", SCIPmatrixGetRowName(matrix, *rowsubptr));
      if( isequ )
      {
         SCIP_CALL( SCIPcreateConsLinear(scip, &cancelcons, cancelconsname, residue_length[i], cancelrowvars, cancelrowcoefs,
                  SCIPmatrixGetRowLhs(matrix, *rowsubptr) - lhs*ratio, SCIPmatrixGetRowRhs(matrix, *rowsubptr) - rhs*ratio,
                  TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) );
      }
      else
      {
         SCIP_CALL( SCIPcreateConsLinear(scip, &cancelcons, cancelconsname, residue_length[i], cancelrowvars, cancelrowcoefs,
                  SCIPmatrixGetRowLhs(matrix, *rowsubptr), SCIPmatrixGetRowRhs(matrix, *rowsubptr),
                  TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) );
      }
      SCIPaddCons(scip, cancelcons);
      SCIPreleaseCons(scip, &cancelcons);
      SCIPdelCons(scip, SCIPmatrixGetCons(matrix, *rowsubptr));
      SCIPfreeBufferArray(scip, &cancelrowvars);
      SCIPfreeBufferArray(scip, &cancelrowcoefs);
      presoldata->nexec += 1;
   }
   if( !isequ )
   {
      SCIPreleaseVar(scip, &newvar);
      SCIPfreeBufferArray(scip, &vars);
      SCIPfreeBufferArray(scip, &coefs);
   }

   return TRUE;
}

static void printSubstitutionProcess(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_Real*            ratios,
      int                   colidx,
      int                   rowidx,
      int*                  residue_length
      )
{
   int ncancel;
   int ncancel_oneround = 0;
   int rowsize;
   SCIP_Real ratio;
   printf("sub row: ");
   SCIPmatrixPrintRow(scip, matrix, rowidx);
   /* print other row */
   int* printrowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   int* printrowend = printrowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
   for( int i = 0; printrowptr < printrowend ; i++, printrowptr++ )
   {
      if ( residue_length[i] == -1 )
         continue;
      ratio =ratios[i];
      rowsize = SCIPmatrixGetRowNNonzs(matrix, *printrowptr);
      ncancel = rowsize - residue_length[i];
      ncancel_oneround += ncancel;
      printf("|===| ratio: %f; cancel: %d; remain: %d. |---|", ratio, ncancel, residue_length[i]);
      SCIPmatrixPrintRow(scip, matrix, *printrowptr);
   }
   ncancel_oneround -= 1;
   printf("total cancel nonzeros: %d\n", ncancel_oneround);
}

static int* reductioncheck(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,
      int                   colidx,             /**< the current variable */
      SCIP_Bool*            blockedrows,
      SCIP_Bool*            success
      )
{
   /* decare variables */
   int ncancel;
   int nchange;
   int ncancel_oneround;
   SCIP_Bool isequ;
   int i;
   int* colptr;
   int* colend;
   int rowsize;
   int* colsubptr;
   int* colsubend;
   int rowsubsize;
   SCIP_Real  ratio;
   SCIP_Real*  ratios;
   SCIP_Real* valptr;
   SCIP_Real* valsubptr;
   SCIP_Real* coefptr;
   SCIP_Real* coefsubptr;
   int index;
   int indexsub;
   int* rowptr;
   int* rowsubptr;
   int* rowend;
   int colsize;
   int* residue_length;

   colsize = SCIPmatrixGetColNNonzs(matrix, colidx);
   SCIPallocBufferArray(scip, &residue_length, colsize);
   SCIPallocBufferArray(scip, &ratios, colsize);

   /* start first loop */
   rowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   rowsubptr = rowptr;
   rowend = rowptr + colsize;
   valptr = SCIPmatrixGetColValPtr(matrix, colidx);
   /* sort variables by var->index */
   /* loop through rowptr, counting the number of non-zeros reduced */
   for( ; rowptr < rowend; rowptr++, valptr++ )
   {
      rowsize = SCIPmatrixGetRowNNonzs(matrix, *rowptr);
      if( rowsize > presoldata->maxrownonzeros && presoldata->maxrownonzeros >= 0 )
         continue;
      if( blockedrows[*rowptr] )
         continue;
      if( rowsize <= 1 )
         continue;
      if( SCIPisInfinity(scip, SCIPmatrixGetRowRhs(matrix, *rowptr)) && SCIPisInfinity(scip, -SCIPmatrixGetRowLhs(matrix, *rowptr)) )
         continue;
      isequ = SCIPisEQ(scip, SCIPmatrixGetRowLhs(matrix, *rowptr), SCIPmatrixGetRowRhs(matrix, *rowptr)) ? TRUE : FALSE;
      if( !isequ && !presoldata->inequality )
         continue;

      /* init */
      for( i = 0; i < colsize; i++ )
      {
         residue_length[i] = -1;
         ratios[i] = 0.0;
      }
      ncancel_oneround = 0;

      /*
       * the var->index of colptr has been sorted
       * */
      rowsubptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
      valsubptr = SCIPmatrixGetColValPtr(matrix, colidx);
      for( i = 0 ; rowsubptr < rowend; i++, rowsubptr++, valsubptr++ )
      {
         rowsubsize = SCIPmatrixGetRowNNonzs(matrix, *rowsubptr);
         if( *rowsubptr == *rowptr )
            continue;
         if( blockedrows[*rowsubptr] )
            continue;
         if( rowsubsize < rowsize )
            continue;
         /* forbid change (1)setppc (2)logicor constraints */
         if( SCIPconsGetHdlr(SCIPmatrixGetCons(matrix, *rowsubptr)) == SCIPfindConshdlr(scip, "setppc") ||
               SCIPconsGetHdlr(SCIPmatrixGetCons(matrix, *rowsubptr)) == SCIPfindConshdlr(scip, "logicor") )
            continue;
         /* numerical stability check */
         ratio = *valsubptr / *valptr;
         if( ratio > presoldata->scaling_ratio || ratio < 1/presoldata->scaling_ratio )
            continue;

         /* At this step, both row(sub)ptr and val(sub)ptr are known, and now we need to compare them col by col */
         /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

         /* init */
         if( isequ )
            ncancel = 1;
         else
            ncancel = 0;
         nchange = 0;
         colptr = SCIPmatrixGetRowIdxPtr(matrix, *rowptr);
         colend = colptr + rowsize;
         coefptr = SCIPmatrixGetRowValPtr(matrix, *rowptr);
         colsubptr = SCIPmatrixGetRowIdxPtr(matrix, *rowsubptr);
         colsubend = colsubptr + rowsubsize;
         coefsubptr = SCIPmatrixGetRowValPtr(matrix, *rowsubptr);
         while( colptr < colend && colsubptr < colsubend )
         {
            if( *colptr == colidx && *colsubptr == colidx )
            {
               colptr += 1;
               coefptr += 1;
               colsubptr += 1;
               coefsubptr += 1;
               continue;
            }
            if( *colptr == colidx )
            {
               colptr += 1;
               coefptr += 1;
               continue;
            }
            if( *colsubptr == colidx )
            {
               colsubptr += 1;
               coefsubptr += 1;
               continue;
            }
            /* compare variable except colidx */
            index = SCIPmatrixGetVar(matrix, *colptr)->index;
            indexsub = SCIPmatrixGetVar(matrix, *colsubptr)->index;
            if( index == indexsub )
            {
               if( SCIPisEQ(scip, (*coefptr) * ratio, *coefsubptr) )
               {
                  ncancel += 1;
               }
               else
               {
                  nchange += 1;
               }
               colptr += 1;
               coefptr += 1;
               colsubptr += 1;
               coefsubptr += 1;
            }
            else if( index < indexsub )
            {
               colptr += 1;
               coefptr += 1;
               ncancel -= 1;
            }
            else
            {
               colsubptr += 1;
               coefsubptr += 1;
            }
         }
         if( colptr < colend )
         {
            while( colptr < colend )
            {
               if( *colptr == colidx )
               {
                  colptr += 1;
                  coefptr += 1;
                  continue;
               }
               ncancel -= 1;
               colptr += 1;
               coefptr += 1;
            }
         }
         if( ncancel > presoldata->cancel_row )
         {
            residue_length[i] = rowsubsize - ncancel;
            ratios[i] = ratio;
            ncancel_oneround += ncancel;
         }
      }
      ncancel_oneround -= 1;
      /* heuristically set some criterions */
      if( (isequ && ncancel_oneround > 0) || (!isequ && ncancel_oneround >= presoldata->cancel_tot) )
      {
         if( presoldata->isprint )
         {
            printSubstitutionProcess(scip, matrix, ratios, colidx, *rowptr, residue_length);
         }
         /* two constraints reduction */
         reduction(scip, matrix, presoldata, ratios, colidx, *rowptr, blockedrows, residue_length);
         presoldata->ncancels += ncancel_oneround;
         *success = TRUE;
      }
   }
   /* free the memory */
   SCIPfreeBufferArray(scip, &residue_length);
   SCIPfreeBufferArray(scip, &ratios);
}

/** updates failure counter after one execution */
static void updateFailureStatistic(
      SCIP_PRESOLDATA*      presoldata,         /**< presolver data */
      SCIP_Bool             success             /**< was this execution successful? */
      )
{
   assert(presoldata != NULL);

   if( success )
   {
      presoldata->nfailures = 0;
      presoldata->nwaitingcalls = 0;
   }
   else
   {
      presoldata->nfailures++;
      presoldata->nwaitingcalls = (int)(presoldata->waitingfac*(SCIP_Real)presoldata->nfailures);
   }
}

/*
 * Callback methods of presolver
 */

/** copy method for constraint handler plugins (called when SCIP copies plugins) */
static SCIP_DECL_PRESOLCOPY(presolCopyExtendedSparsify)
{
   SCIP_PRESOLDATA* presoldata;

   assert(scip != NULL);
   assert(presol != NULL);
   assert(strcmp(SCIPpresolGetName(presol), PRESOL_NAME) == 0);

   /* call inclusion method of presolver if copying is enabled */
   presoldata = SCIPpresolGetData(presol);
   assert(presoldata != NULL);
   if( presoldata->enablecopy )
   {
      SCIP_CALL( SCIPincludePresolExtendedSparsify(scip) );
   }

   return SCIP_OKAY;
}

/** execution method of presolver */
static SCIP_DECL_PRESOLEXEC(presolExecExtendedSparsify)
{
   /* start the presolve method */
   clock_t TimeStart=clock();

   /* declarate the used data */
   SCIP_MATRIX* matrix;
   int colidx;
   SCIP_Bool* blockedrows;
   SCIP_PRESOLDATA* presoldata;

   /* initinize  */
   SCIP_Bool success = FALSE;
   presoldata = SCIPpresolGetData(presol);

   /* basic conditions for method execution */
   assert(result != NULL);
   *result = SCIP_DIDNOTRUN;

   if( SCIPdoNotAggr(scip) )
      return SCIP_OKAY;

   /* If restart is performed, some cuts will be tranformed into linear constraints.
    * However, SCIPmatrixCreate() only collects the original constraints (not the constraints transformed from cuts)
    * For this reason, we only perform this method in the first run of branch-and-cut.
    * */
   if( SCIPgetNRuns(scip) > 1 )
      return SCIP_OKAY;
   if( presoldata->nwaitingcalls > 0 )
   {
      presoldata->nwaitingcalls--;
      SCIPdebugMsg(scip, "skipping exrended sparsify: nfailures=%d, nwaitingcalls=%d\n", presoldata->nfailures,
            presoldata->nwaitingcalls);
      return SCIP_OKAY;
   }
   SCIPdebugMsg(scip, "starting extended sparsify. . .\n");
   *result = SCIP_DIDNOTFIND;

   /* make matrix start */
   matrix = NULL;
   SCIP_Bool initialized;
   SCIP_Bool complete;
   SCIP_Bool infeasible;
   SCIP_CALL( SCIPmatrixCreate(scip, &matrix, TRUE, &initialized, &complete, &infeasible,
            naddconss, ndelconss, nchgcoefs, nchgbds, nfixedvars) );

   /* if infeasibility was detected during matrix creation, return here */
   if( infeasible )
   {
      if( initialized )
         SCIPmatrixFree(scip, &matrix);

      *result = SCIP_CUTOFF;
      return SCIP_OKAY;
   }
   if( !initialized )
      return SCIP_OKAY;
   /* make matrix end */

   if( presoldata->isprint )
      printf("\nStart Extended Sparsify Presolving\n\n");

   int ncols = SCIPmatrixGetNColumns(matrix);
   int nrows = SCIPmatrixGetNRows(matrix);
   assert(SCIPgetNVars(scip) == ncols);
   presoldata->nvar = ncols;

   SCIP_CALL( SCIPallocBufferArray(scip, &blockedrows, nrows) );
   for( int rowidx = 0; rowidx < nrows; rowidx++ )
      blockedrows[rowidx] = FALSE;

   /* loop through all variables, including binary variables */
   for( colidx = 0; colidx < ncols; colidx++ )
   {
      SCIP_VAR* var = SCIPmatrixGetVar(matrix, colidx);
      if( presoldata->maxcolnonzeros < SCIPmatrixGetColNNonzs(matrix, colidx) && presoldata->maxcolnonzeros >= 0 )
         continue;
      /* multi-row nonzero cancellation */
      reductioncheck(scip, matrix, presoldata, colidx, blockedrows, &success);
      if( success )
         *result = SCIP_SUCCESS;
   }
   updateFailureStatistic(presoldata, success);

   SCIPfreeBufferArray(scip, &blockedrows);
   SCIPmatrixFree(scip, &matrix);

   if( presoldata->isprint )
      printf("\nEnd Extended Sparsify Presolving\n\n");

   clock_t TimeEnd=clock();
   presoldata->time += (SCIP_Real)(TimeEnd-TimeStart)/(double)CLOCKS_PER_SEC;

   return SCIP_OKAY;
}

/*
 * presolver specific interface methods
 */

/** destructor of presolver to free user data (called when SCIP is exiting) */
static SCIP_DECL_PRESOLFREE(presolFreeExtendedSparsify)
{
   SCIP_PRESOLDATA* presoldata;

   /* free presolver data */
   presoldata = SCIPpresolGetData(presol);
   assert(presoldata != NULL);
   if( scip->set->disp_verblevel != 0 && presoldata->isinit == TRUE )
   {
      printf("Extended Sparsify: nvar= %d nexec= %d naddvar= %d nnonz= %d time= %.2f\n", presoldata->nvar, presoldata->nexec, presoldata->naddvar, presoldata->ncancels, presoldata->time);
   }
   SCIPfreeBlockMemory(scip, &presoldata);
   SCIPpresolSetData(presol, NULL);

   return SCIP_OKAY;
}

/** initialization method of presolver (called after problem was transformed) */
static SCIP_DECL_PRESOLINIT(presolInitExtendedSparsify)
{
   SCIP_PRESOLDATA* presoldata;
   presoldata = SCIPpresolGetData(presol);

   presoldata->nvar = 0;
   presoldata->nexec = 0;
   presoldata->naddvar = 0;
   presoldata->ncancels = 0;
   presoldata->time = 0.0;
   presoldata->nfailures = 0;
   presoldata->nwaitingcalls = 0;
   presoldata->isinit = TRUE;

   return SCIP_OKAY;
}

/** creates the presolver and includes it in SCIP */
SCIP_RETCODE SCIPincludePresolExtendedSparsify(
      SCIP*                 scip                /**< SCIP data structure */
      )
{
   SCIP_PRESOLDATA* presoldata;
   SCIP_PRESOL* presol;

   /* create presolver data */
   SCIP_CALL( SCIPallocBlockMemory(scip, &presoldata) );

   presoldata->isinit = FALSE;

   /* include presolver */
   SCIP_CALL( SCIPincludePresolBasic(scip, &presol, PRESOL_NAME, PRESOL_DESC, PRESOL_PRIORITY, PRESOL_MAXROUNDS,
            PRESOL_TIMING, presolExecExtendedSparsify, presoldata) );

   SCIP_CALL( SCIPsetPresolCopy(scip, presol, presolCopyExtendedSparsify) );
   SCIP_CALL( SCIPsetPresolFree(scip, presol, presolFreeExtendedSparsify) );
   SCIP_CALL( SCIPsetPresolInit(scip, presol, presolInitExtendedSparsify) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/extended_sparsify/enablecopy",
            "should extended sparsify presolver be copied to sub-SCIPs?",
            &presoldata->enablecopy, TRUE, PRESOL_ENABLECOPY, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/scaling_ratio",
            "scaling ratio",
            &presoldata->scaling_ratio, TRUE, DEFAULT_SCALING_RATIO, -1, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/maxcolnonzeros",
            "maximal number of considered nonzeros within one column (-1: no limit)",
            &presoldata->maxcolnonzeros, TRUE, DEFAULT_MAX_COLNONZEROS, -1, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/maxrownonzeros",
            "maximal number of considered nonzeros within one row (-1: no limit)",
            &presoldata->maxrownonzeros, TRUE, DEFAULT_MAX_ROWNONZEROS, -1, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/penalty",
            "penalty of changing coefficient",
            &presoldata->penalty, FALSE, PENALTY, -INT_MAX, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/cancel_row",
            "minimal eliminated nonzeros per row",
            &presoldata->cancel_row, FALSE, CANCEL_ROW, -INT_MAX, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/extended_sparsify/cancel_tot",
            "minimal eliminated nonzeros total",
            &presoldata->cancel_tot, FALSE, CANCEL_TOT, -INT_MAX, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip,
            "presolving/extended_sparsify/waitingfac",
            "number of calls to wait until next execution as a multiple of the number of useless calls",
            &presoldata->waitingfac, TRUE, DEFAULT_WAITINGFAC, 0.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/extended_sparsify/isprint",
            "should we print the aggregation process?",
            &presoldata->isprint, TRUE, DEBUG_ISPRINT, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/extended_sparsify/isaccurate",
            "whether the number of nonzeros is accurately computed?",
            &presoldata->isaccurate, TRUE, PRESOL_ISACCURATE, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/extended_sparsify/inequality",
            "whether do inequality?",
            &presoldata->inequality, TRUE, INEQUALITY, NULL, NULL) );

   return SCIP_OKAY;
}
