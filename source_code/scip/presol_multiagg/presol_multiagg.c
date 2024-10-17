/**@file   presol_multiagg.c
 * @brief  conduct multi-aggregation to eliminate variables, constraints, or nonzeros
 * @author Houshan Zhang
 *
 * This presolver attempts to simplify model by removing variables, constraints or nonzeros.
 * In more detail, for a given equality
 *
 *                  A_{iS}x_{S} + a_{ij}x_{j} = b,
 *
 * where x_{j} is the aggregated variable. We can aggregate the variable x_{j} = (b-A_{iS}x_{S}) / a_{ij}. After aggregation,
 * the variable x_{j} can be removed. The equality can be substituted by inequality
 *
 *                  l_{j} <= (b-A_{iS}x_{S}) / a_{ij} <= u_{j},
 *
 * where l_{j} and u_{j} are lower and upper bounds of x_{j}.
 *
 * If x_{j} is an implied free variable, the aggregated equality is redundant and therefore can be removed from the problem.
 *
 * In addition, for a given inequality
 *
 *                  b_{l} <= A_{iS}x_{S} + a_{ij}x_{j} <= b_{r},
 *
 * Wlog, assuming that b_{r} != +infinity, then we introduce a slack variable x_{j}':= (b_{r}-A_{iS}x_{S}-a_{ij}x_{j}) / |a_{ij}|,
 * and 0 <= x_{j}' <= (b_{r} - b_{l}) / |a_{ij}|.
 *
 * The inequality can be transformed into
 *
 *                  A_{iS}x_{S} + a_{ij}x_{j} + |a_{ij}|x_{j}' = b_{r},
 *
 * and we can aggregate the variable x_{j} = (b_{r}-A_{iS}x_{S}-a_{ij}x_{j}') / a_{ij}.
 *
 * @todo add infrastructure to SCIP for handling aggregated binary variables
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "blockmemshell/memory.h"
#include "scip/cons_linear.h"
#include "scip/debug.h"
#include "scip/presol_multiagg.h"
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

#define PRESOL_NAME                 "multiagg"
#define PRESOL_DESC                 "perform multi-aggregation to simplify model"
#define PRESOL_PRIORITY             -230000     /**< priority of the presolver (>= 0: before, < 0: after constraint handlers) */
#define PRESOL_MAXROUNDS            -1          /**< maximal number of presolving rounds the presolver participates in (-1: no limit) */
#define PRESOL_ISACCURATE           TRUE        /**< if the number of nonzeros is accurately computed */
#define PRESOL_ENABLECOPY           TRUE        /**< should multiagg presolver be copied to sub-SCIPs? */
#define PRESOL_TIMING               SCIP_PRESOLTIMING_EXHAUSTIVE  /* timing of the presolver (fast, medium, or exhaustive) */

/* just for debug */
#define DEBUG_ISPRINT               FALSE       /**< whether print the presolve process */
/* this substitution process may lead to some issues, so we use the corressponding restriction */
/* 1) increase nonzeros of the costraints matrix */
#define MIN_ELIM                    10          /**< minimal eleminated nonzeros if we choose a inequality and a bounded variable to substitute */
/* 2) if destroy variable bound constraints */
#define DEFAULT_PRESERVE_VARBOUND   TRUE        /**< if preserve variable bound constraints containing binary variable */
/* in order to limit the time consuming, we use the corressponding restriction */
#define DEFAULT_MAX_COLNONZEROS     40          /**< maximal number of considered nonzeros within one column (-1: no limit) */
#define DEFAULT_MAX_ROWNONZEROS     40          /**< maximal number of considered nonzeros within one row (-1: no limit) */
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
   int                   nfree;              /**< number of implicit free variables identified by using one row bound tightening */
   int                   nsemi;              /**< number of semi-continuous variable */
   int                   nsemiexec;
   int                   ncancels;           /**< total number of canceled nonzeros (net value, i.e., removed minus added nonzeros) */
   int                   noldcancels;        /**< old three aggregation types ncancels */
   int                   nexec;              /**< number of executions */
   int                   ndvar;              /**< number of deleted variables */
   int                   ndcon;              /**< number of deleted constraints */
   SCIP_Real             time;

   /* compare with traditional variable aggregation method */
   int                   ntwo;               /**< only two variable in the aggregated equality */
   int                   nsgt;               /**< singleton variable in the aggregated equality */
   int                   nfvs;               /**< implied free variable substitution in the aggregated equality */
   int                   nnew;               /**< aggregated but do not belong to the above three type in the aggregated equality */

   int                   nfailures;          /**< number of calls to presolver without success */
   int                   nwaitingcalls;      /**< number of presolver calls until next real execution */

   /* presolve setting */
   SCIP_Bool             isprint;
   SCIP_Bool             isaccurate;
   int                   mineliminate;
   int                   maxcolnonzeros;     /**< maximal number of considered nonzeros within one column (-1: no limit) */
   int                   maxrownonzeros;     /**< maximal number of considered nonzeros within one row (-1: no limit) */
   SCIP_Bool             preservevarbound;   /**< whether destroy constraints that contain only two variables is allowed? */
   SCIP_Bool             enablecopy;         /**< should multiagg presolver be copied to sub-SCIPs? */
   SCIP_Real             waitingfac;         /**< number of calls to wait until next execution as a multiple of the number of useless calls */

   SCIP_Bool             isinit;
};

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

/** calculate maximal activity of one row without one specific column */
static SCIP_Real getMaxActivitySingleRowWithoutCol(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   row,                /**< row index */
      int                   col                 /**< column index */
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

   maxactivity = 0;

   rowptr = SCIPmatrixGetRowIdxPtr(matrix, row);
   rowend = rowptr + SCIPmatrixGetRowNNonzs(matrix, row);
   valptr = SCIPmatrixGetRowValPtr(matrix, row);

   for( ; (rowptr < rowend); rowptr++, valptr++ )
   {
      c = *rowptr;
      val = *valptr;

      if( c == col )
         continue;

      if( val > 0.0 )
      {
         ub = SCIPmatrixGetColUb(matrix, c);
         assert(!SCIPisInfinity(scip, ub));

         maxactivity += val * ub;
      }
      else if( val < 0.0 )
      {
         lb = SCIPmatrixGetColLb(matrix, c);
         assert(!SCIPisInfinity(scip, -lb));

         maxactivity += val * lb;
      }
   }

   return maxactivity;
}

/** calculate minimal activity of one row without one specific column */
static SCIP_Real getMinActivitySingleRowWithoutCol(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   row,                /**< row index */
      int                   col                 /**< column index */
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

   minactivity = 0;

   rowptr = SCIPmatrixGetRowIdxPtr(matrix, row);
   rowend = rowptr + SCIPmatrixGetRowNNonzs(matrix, row);
   valptr = SCIPmatrixGetRowValPtr(matrix, row);

   for( ; (rowptr < rowend); rowptr++, valptr++ )
   {
      c = *rowptr;
      val = *valptr;

      if( c == col )
         continue;

      if( val > 0.0 )
      {
         lb = SCIPmatrixGetColLb(matrix, c);
         assert(!SCIPisInfinity(scip, -lb));

         minactivity += val * lb;
      }
      else if( val < 0.0 )
      {
         ub = SCIPmatrixGetColUb(matrix, c);
         assert(!SCIPisInfinity(scip, ub));

         minactivity += val * ub;
      }
   }

   return minactivity;
}

/** get minimal and maximal residual activity without one specified column */
static void getMinMaxActivityResiduals(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   col,                /**< column index */
      int                   row,                /**< row index */
      SCIP_Real             val,                /**< coefficient of this variable in this row */
      SCIP_Real*            minresactivity,     /**< minimum residual activity of this row */
      SCIP_Real*            maxresactivity,     /**< maximum residual activity of this row */
      SCIP_Bool*            isminsettoinfinity, /**< flag indicating if minresactiviy is set to infinity */
      SCIP_Bool*            ismaxsettoinfinity  /**< flag indicating if maxresactiviy is set to infinity */
      )
{
   SCIP_Real lb;
   SCIP_Real ub;
   int nmaxactneginf;
   int nmaxactposinf;
   int nminactneginf;
   int nminactposinf;
   SCIP_Real maxactivity;
   SCIP_Real minactivity;

   assert(scip != NULL);
   assert(matrix != NULL);
   assert(minresactivity != NULL);
   assert(maxresactivity != NULL);
   assert(isminsettoinfinity != NULL);
   assert(ismaxsettoinfinity != NULL);

   lb = SCIPmatrixGetColLb(matrix, col);
   ub = SCIPmatrixGetColUb(matrix, col);

   *isminsettoinfinity = FALSE;
   *ismaxsettoinfinity = FALSE;

   nmaxactneginf = SCIPmatrixGetRowNMaxActNegInf(matrix, row);
   nmaxactposinf = SCIPmatrixGetRowNMaxActPosInf(matrix, row);
   nminactneginf = SCIPmatrixGetRowNMinActNegInf(matrix, row);
   nminactposinf = SCIPmatrixGetRowNMinActPosInf(matrix, row);

   maxactivity = SCIPmatrixGetRowMaxActivity(matrix, row);
   minactivity = SCIPmatrixGetRowMinActivity(matrix, row);

   if( val >= 0.0 )
   {
      if( SCIPisInfinity(scip, ub) )
      {
         assert(nmaxactposinf >= 1);
         if( nmaxactposinf == 1 && nmaxactneginf == 0 )
            *maxresactivity = getMaxActivitySingleRowWithoutCol(scip, matrix, row, col);
         else
         {
            *maxresactivity = SCIPinfinity(scip);
            *ismaxsettoinfinity = TRUE;
         }
      }
      else
      {
         if( (nmaxactneginf + nmaxactposinf) > 0 )
         {
            *maxresactivity = SCIPinfinity(scip);
            *ismaxsettoinfinity = TRUE;
         }
         else
            *maxresactivity = maxactivity - val * ub;
      }

      if( SCIPisInfinity(scip, -lb) )
      {
         assert(nminactneginf >= 1);
         if( nminactneginf == 1 && nminactposinf == 0 )
            *minresactivity = getMinActivitySingleRowWithoutCol(scip, matrix, row, col);
         else
         {
            *minresactivity = -SCIPinfinity(scip);
            *isminsettoinfinity = TRUE;
         }
      }
      else
      {
         if( (nminactneginf + nminactposinf) > 0 )
         {
            *minresactivity = -SCIPinfinity(scip);
            *isminsettoinfinity = TRUE;
         }
         else
            *minresactivity = minactivity - val * lb;
      }
   }
   else
   {
      if( SCIPisInfinity(scip, -lb) )
      {
         assert(nmaxactneginf >= 1);
         if( nmaxactneginf == 1 && nmaxactposinf == 0 )
            *maxresactivity = getMaxActivitySingleRowWithoutCol(scip, matrix, row, col);
         else
         {
            *maxresactivity = SCIPinfinity(scip);
            *ismaxsettoinfinity = TRUE;
         }
      }
      else
      {
         if( (nmaxactneginf + nmaxactposinf) > 0 )
         {
            *maxresactivity = SCIPinfinity(scip);
            *ismaxsettoinfinity = TRUE;
         }
         else
            *maxresactivity = maxactivity - val * lb;
      }

      if( SCIPisInfinity(scip, ub) )
      {
         assert(nminactposinf >= 1);
         if( nminactposinf == 1 && nminactneginf == 0 )
            *minresactivity = getMinActivitySingleRowWithoutCol(scip, matrix, row, col);
         else
         {
            *minresactivity = -SCIPinfinity(scip);
            *isminsettoinfinity = TRUE;
         }
      }
      else
      {
         if( (nminactneginf + nminactposinf) > 0 )
         {
            *minresactivity = -SCIPinfinity(scip);
            *isminsettoinfinity = TRUE;
         }
         else
            *minresactivity = minactivity - val * ub;
      }
   }
}

/** calculate the upper and lower bound of one variable from one row */
static void getVarBoundsOfRow(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      int                   col,                /**< column index of variable */
      int                   row,                /**< row index */
      SCIP_Real             val,                /**< coefficient of this column in this row */
      SCIP_Real*            rowub,              /**< upper bound of row */
      SCIP_Bool*            ubfound,            /**< flag indicating that an upper bound was calculated */
      SCIP_Real*            rowlb,              /**< lower bound of row */
      SCIP_Bool*            lbfound             /**< flag indicating that a lower bound was caluclated */
      )
{
   SCIP_Bool isminsettoinfinity;
   SCIP_Bool ismaxsettoinfinity;
   SCIP_Real minresactivity;
   SCIP_Real maxresactivity;
   SCIP_Real lhs;
   SCIP_Real rhs;

   assert(rowub != NULL);
   assert(ubfound != NULL);
   assert(rowlb != NULL);
   assert(lbfound != NULL);

   *rowub = SCIPinfinity(scip);
   *ubfound = FALSE;
   *rowlb = -SCIPinfinity(scip);
   *lbfound = FALSE;

   getMinMaxActivityResiduals(scip, matrix, col, row, val,
         &minresactivity, &maxresactivity,
         &isminsettoinfinity, &ismaxsettoinfinity);

   lhs = SCIPmatrixGetRowLhs(matrix, row);
   rhs = SCIPmatrixGetRowRhs(matrix, row);

   if( val > 0.0 )
   {
      if( !isminsettoinfinity && !SCIPisInfinity(scip, rhs) )
      {
         *rowub = (rhs - minresactivity) / val;
         *ubfound = TRUE;
      }

      if( !ismaxsettoinfinity && !SCIPisInfinity(scip, -lhs) )
      {
         *rowlb = (lhs - maxresactivity) / val;
         *lbfound = TRUE;
      }
   }
   else
   {
      if( !ismaxsettoinfinity && !SCIPisInfinity(scip, -lhs) )
      {
         *rowub = (lhs - maxresactivity) / val;
         *ubfound = TRUE;
      }

      if( !isminsettoinfinity && !SCIPisInfinity(scip, rhs) )
      {
         *rowlb = (rhs - minresactivity) / val;
         *lbfound = TRUE;
      }
   }
}

/** detect implied variable bounds */
static void getImpliedBounds(
      SCIP*                 scip,               /**< SCIP main data structure */
      SCIP_MATRIX*          matrix,             /**< matrix containing the constraints */
      SCIP_PRESOLDATA*      presoldata,         /**< presolver data */
      int                   colidx,             /**< column index for implied free test */
      SCIP_Bool*            ubimplied,          /**< flag indicating an implied upper bound */
      SCIP_Bool*            lbimplied,          /**< flag indicating an implied lower bound */
      SCIP_Bool*            issemi
      )
{
   SCIP_Real* valptr;
   int* rowptr;
   int* rowend;
   SCIP_Real impliedub;
   SCIP_Real impliedlb;
   SCIP_Real ub;
   SCIP_Real lb;

   /* ----- */
   SCIP_Bool semi_ub;
   SCIP_Bool semi_lb;
   semi_ub = FALSE;
   semi_lb = FALSE;
   int* othercolptr;
   int semiubbinidx;
   int semilbbinidx;
   /* ----- */

   assert(scip != NULL);
   assert(matrix != NULL);
   assert(ubimplied != NULL);
   assert(lbimplied != NULL);

   *ubimplied = FALSE;
   impliedub = SCIPinfinity(scip);

   *lbimplied = FALSE;
   impliedlb = -SCIPinfinity(scip);

   ub =  SCIPmatrixGetColUb(matrix, colidx);
   lb =  SCIPmatrixGetColLb(matrix, colidx);

   rowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   rowend = rowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
   valptr = SCIPmatrixGetColValPtr(matrix, colidx);
   for( ; (rowptr < rowend); rowptr++, valptr++ )
   {
      SCIP_Real rowub;
      SCIP_Bool ubfound;
      SCIP_Real rowlb;
      SCIP_Bool lbfound;

      getVarBoundsOfRow(scip, matrix, colidx, *rowptr, *valptr, &rowub, &ubfound, &rowlb, &lbfound);
      /* ----- */
      /* detect semi-continuous variable */
      if( !semi_ub && SCIPmatrixGetRowNNonzs(matrix, *rowptr)==2 && (SCIPisInfinity(scip, ub) || SCIPisLE(scip, rowub, ub)) )
      {
         othercolptr = SCIPmatrixGetRowIdxPtr(matrix, *rowptr);
         if( *othercolptr == colidx )
            othercolptr += 1;
         if( SCIPvarGetType(SCIPmatrixGetVar(matrix, *othercolptr)) == SCIP_VARTYPE_BINARY )
         {
            semi_ub = TRUE;
            semiubbinidx = SCIPmatrixGetVar(matrix, *othercolptr)->index;
         }
      }
      if( !semi_lb && SCIPmatrixGetRowNNonzs(matrix, *rowptr)==2 && (SCIPisInfinity(scip, -lb) || SCIPisGE(scip, rowlb, lb)) )
      {
         othercolptr = SCIPmatrixGetRowIdxPtr(matrix, *rowptr);
         if( *othercolptr == colidx )
            othercolptr += 1;
         if( SCIPvarGetType(SCIPmatrixGetVar(matrix, *othercolptr)) == SCIP_VARTYPE_BINARY )
         {
            semi_lb = TRUE;
            semilbbinidx = SCIPmatrixGetVar(matrix, *othercolptr)->index;
         }
      }
      /* ----- */
      if( ubfound && (rowub < impliedub) )
         impliedub = rowub;

      if( lbfound && (rowlb > impliedlb) )
         impliedlb = rowlb;
   }
   if( semi_lb && semi_ub && semiubbinidx == semilbbinidx )
      *issemi = TRUE;
   else
      *issemi = FALSE;

   if( SCIPvarGetType(SCIPmatrixGetVar(matrix, colidx)) <= SCIP_VARTYPE_INTEGER )
   {
      impliedlb = SCIPceil(scip, impliedlb);
      impliedub = SCIPfloor(scip, impliedub);
   }

   /* we consider +/-inf bounds as implied bounds */
   if( SCIPisInfinity(scip, ub) ||
         (!SCIPisInfinity(scip, ub) && SCIPisLE(scip, impliedub, ub)) )
      //(!SCIPisInfinity(scip, ub) && impliedub <= ub) )
      *ubimplied = TRUE;

   if( SCIPisInfinity(scip, -lb) ||
         (!SCIPisInfinity(scip, -lb) && SCIPisGE(scip, impliedlb, lb)) )
      //(!SCIPisInfinity(scip, -lb) && impliedlb >= lb) )
      *lbimplied = TRUE;
}

/** aggregate variables */
static SCIP_Bool aggregation(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,         /**< presolver data */
      int                   colidx,             /**< the current variable */
      int                   rowidx,             /**< the current row */
      SCIP_Bool             islhs,              /**< whether use the lhs */
      SCIP_Real             aggcoef,            /**< coef of aggregatedvar */
      SCIP_Bool             isfree,             /**< if it is an (implied) free variable */
      SCIP_Bool*            blockedcols,        /**< array to indicates whether it is blocked vars or not */
      SCIP_Bool*            blockedrows,        /**< array to indicates whether it is blocked rows or not */
      SCIP_Bool*            removedrows,        /**< array to indicates whether it is removed rows or not */
      int*                  ndelconss,
      int*                  naggrvars
      )
{
   SCIP_Bool infeasible;
   SCIP_Bool aggregated;
   SCIP_Bool isinteger;
   SCIP_VAR* aggregatedvar;
   /* aggregation variable */
   SCIP_VAR** vars;
   /* aggregation variable coefficient */
   SCIP_Real* coefs;
   /* newvar's information */
   char newvarname[SCIP_MAXSTRLEN];
   char newconname[SCIP_MAXSTRLEN];
   SCIP_VAR* newvar;
   SCIP_VARTYPE newvartype;
   SCIP_Real aggrconstant;
   SCIP_Real newlb;
   SCIP_Real newub;
   SCIP_Real lhs;
   SCIP_Real rhs;
   SCIP_Real aggcoefabs;
   SCIP_Real* valptr;
   int idx;
   int* colptr;
   int* colend;
   int nvars;
   if( presoldata->isprint )
   {
      printf("start aggregation\n");
      SCIPmatrixPrintRow(scip, matrix, rowidx);
   }
   /* get the number of variables of this row */
   isinteger = SCIPvarGetType(SCIPmatrixGetVar(matrix, colidx)) <= SCIP_VARTYPE_IMPLINT;
   aggregatedvar = SCIPmatrixGetVar(matrix, colidx);
   lhs = SCIPmatrixGetRowLhs(matrix, rowidx);
   rhs = SCIPmatrixGetRowRhs(matrix, rowidx);
   if( aggcoef >= 0 )
      aggcoefabs = aggcoef;
   else
      aggcoefabs = -aggcoef;
   /* presolve */
   if( isinteger )
   {
      lhs = aggcoefabs*(SCIPceil(scip, lhs/aggcoefabs));
      rhs = aggcoefabs*(SCIPfloor(scip, rhs/aggcoefabs));
   }
   if( SCIPisGT(scip, lhs, rhs) )
   {
      /* infeasiable */
      return FALSE;
   }

   assert(!SCIPdoNotMultaggrVar(scip, aggregatedvar));

   /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/
   /* creat a new variable */
   if( islhs )
   {
      aggrconstant = lhs/aggcoef;
      if( isinteger )
         (void) SCIPsnprintf(newvarname, SCIP_MAXSTRLEN, "%s_freeLI", aggregatedvar->name);
      else
         (void) SCIPsnprintf(newvarname, SCIP_MAXSTRLEN, "%s_freeLC", aggregatedvar->name);
      /* tightening newvar's ub */
      if( SCIPisInfinity(scip, rhs) )
      {
         SCIP_Real maxactivity;
         if( getMaxActivitySingleRow(scip, matrix, rowidx, &maxactivity) )
            newub = (maxactivity-lhs)/aggcoefabs;
         else
            newub = SCIPinfinity(scip);
      }
      else
         newub = (rhs-lhs)/aggcoefabs;
      newlb = 0.0;
   }
   else
   {
      aggrconstant = rhs/aggcoef;
      if( isinteger )
         (void) SCIPsnprintf(newvarname, SCIP_MAXSTRLEN, "%s_freeRI", aggregatedvar->name);
      else
         (void) SCIPsnprintf(newvarname, SCIP_MAXSTRLEN, "%s_freeRC", aggregatedvar->name);
      /* tightening newvar's ub */
      if( SCIPisInfinity(scip, -lhs) )
      {
         SCIP_Real minactivity;
         if( getMinActivitySingleRow(scip, matrix, rowidx, &minactivity) )
            newub = (rhs-minactivity)/aggcoefabs;
         else
            newub = SCIPinfinity(scip);
      }
      else
         newub = (rhs-lhs)/aggcoefabs;
      newlb = 0.0;
   }
   if( isinteger )
      newvartype = (SCIPvarGetType(aggregatedvar) == SCIP_VARTYPE_IMPLINT) ? SCIP_VARTYPE_IMPLINT : SCIP_VARTYPE_INTEGER;
   else
      newvartype = SCIP_VARTYPE_CONTINUOUS;

   SCIP_CALL( SCIPcreateVar(scip, &newvar, newvarname, newlb, newub, 0.0, newvartype,
            SCIPvarIsInitial(aggregatedvar), SCIPvarIsRemovable(aggregatedvar), NULL, NULL, NULL, NULL, NULL) );
   SCIP_CALL( SCIPaddVar(scip, newvar) );
   /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

   /* get multi-agg variable coefficient and vars */
   nvars = SCIPmatrixGetRowNNonzs(matrix, rowidx);
   SCIP_CALL( SCIPallocBufferArray(scip, &coefs, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &vars, nvars) );

   colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
   colend = colptr + nvars;
   valptr = SCIPmatrixGetRowValPtr(matrix, rowidx);
   idx = 0;
   for( ; colptr < colend ; idx++, colptr++, valptr++ )
   {
      if ( colidx == *colptr )
      {
         assert(SCIPisEQ(scip, aggcoef, *valptr));
         assert(!SCIPisEQ(scip, aggcoef, 0.0));
         idx -= 1;
         continue;
      }
      vars[idx] = SCIPmatrixGetVar(matrix, *colptr);
      coefs[idx] = -(*valptr) / aggcoef;
   }
   vars[idx] = newvar;
   if( (!islhs && aggcoef > 0) || (islhs && aggcoef < 0) )
      coefs[idx] = -1;
   else
      coefs[idx] = 1;

   SCIP_CALL( SCIPmultiaggregateVar(scip, aggregatedvar, nvars, vars, coefs, aggrconstant, &infeasible, &aggregated) );

   if( !aggregated || infeasible )
   {
      SCIPfreeBufferArray(scip, &vars);
      SCIPfreeBufferArray(scip, &coefs);
      if( presoldata->isprint )
      {
         printf("scip forbid multiagg\n");
      }
      return FALSE;
   }
   presoldata->nexec += 1;
   if( isfree )
   {
      /* if the variable is implied free, we make sure that the columns bounds are removed,
       * so that subsequent checks for implied bounds do not interfere with the exploitation
       * of this variables implied bounds
       */
      SCIPmatrixRemoveColumnBounds(scip, matrix, colidx);
      presoldata->ndcon += 1;
      *ndelconss += 1;
   }
   if( SCIPisEQ(scip, lhs, rhs) )
   {
      presoldata->ndvar += 1;
      *naggrvars += 1;
      if( SCIPmatrixGetRowNNonzs(matrix, rowidx) == 2 )
         presoldata->ntwo += 1;
      if( SCIPmatrixGetColNNonzs(matrix, colidx) == 1 )
         presoldata->nsgt += 1;
      if( isfree )
         presoldata->nfvs += 1;
      if( SCIPmatrixGetRowNNonzs(matrix, rowidx) != 2 && SCIPmatrixGetColNNonzs(matrix, colidx) != 1 && !isfree )
         presoldata->nnew += 1;
   }

   if( !isfree )
   {
      SCIP_CONS* cons;
      (void) SCIPsnprintf(newconname, SCIP_MAXSTRLEN, "%s_F", SCIPmatrixGetRowName(matrix, rowidx));
      SCIP_CALL( SCIPcreateConsLinear(scip, &cons, newconname, nvars, vars, coefs,
               SCIPmatrixGetColLb(matrix, colidx)-aggrconstant, SCIPmatrixGetColUb(matrix, colidx)-aggrconstant,
               TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) );
      SCIP_CALL( SCIPaddCons(scip, cons) );
      SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   }
   SCIP_CALL( SCIPdelCons(scip, SCIPmatrixGetCons(matrix, rowidx)) );
   removedrows[rowidx] = TRUE;

   /* the removed row would be counted correctly if we do not block this variable */
   if( presoldata->isaccurate )
   {
      /* condition 1 */
      /* row overlap */
      int* rowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
      int* rowend = rowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
      for( ; rowptr < rowend; rowptr++ )
      {
         int* colptr = SCIPmatrixGetRowIdxPtr(matrix, *rowptr);
         int* colend = colptr + SCIPmatrixGetRowNNonzs(matrix, *rowptr);
         for( ; colptr < colend ; colptr++ )
         {
            blockedcols[*colptr] = TRUE;
         }
      }
      /* condition 2 */
      /* coloverlap */
      int* colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
      int* colend = colptr + SCIPmatrixGetRowNNonzs(matrix, rowidx);
      for( ; colptr < colend ; colptr++ )
      {
         int* rowptr = SCIPmatrixGetColIdxPtr(matrix, *colptr);
         int* rowend = rowptr + SCIPmatrixGetColNNonzs(matrix, *colptr);
         for( ; rowptr < rowend ; rowptr++ )
         {
            blockedrows[*rowptr] = TRUE;
         }
      }
   }

   SCIPfreeBufferArray(scip, &vars);
   SCIPfreeBufferArray(scip, &coefs);

   SCIP_CALL( SCIPreleaseVar(scip, &newvar) );
   if( presoldata->isprint )
   {
      printf("aggregation end\n");
   }
   return TRUE;
}

static SCIP_Bool numericalStable(
      SCIP*             scip,
      SCIP_MATRIX*      matrix,
      SCIP_PRESOLDATA*  presoldata,
      int               rowidx,
      int               colidx,
      SCIP_Real         val,
      SCIP_Bool*        islhs
      )
{
   SCIP_Bool numericalstable;
   SCIP_Bool markowitz_criterion_row;
   SCIP_Bool markowitz_criterion_col;
   SCIP_Real aggr_lhs;
   SCIP_Real aggr_rhs;
   SCIP_Bool aggrside_lhs;
   SCIP_Bool aggrside_rhs;
   int* colptr;
   SCIP_Real* colval;
   int* colend;
   int* rowptr;
   SCIP_Real* rowval;
   int* rowend;
   SCIP_Real maxabsval;
   SCIP_Real absval;
   maxabsval = -1.0;
   colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
   colval = SCIPmatrixGetRowValPtr(matrix, rowidx);
   colend = colptr + SCIPmatrixGetRowNNonzs(matrix, rowidx);
   for( ; colptr < colend; colptr++, colval++ )
   {
      absval = REALABS(*colval);
      if( absval > maxabsval )
         maxabsval = absval;
   }

   if( REALABS(val) >= MARKOWITZ*maxabsval )
      markowitz_criterion_row = TRUE;
   else
      markowitz_criterion_row = FALSE;

   maxabsval = -1.0;
   rowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   rowval = SCIPmatrixGetColValPtr(matrix, colidx);
   rowend = rowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
   for( ; rowptr < rowend; rowptr++, rowval++ )
   {
      absval = REALABS(*rowval);
      if( absval > maxabsval )
         maxabsval = absval;
   }

   /* 1. criterion
    * markowitz criterion
    */
   if( REALABS(val) >= MARKOWITZ*maxabsval )
      markowitz_criterion_col = TRUE;
   else
      markowitz_criterion_col = FALSE;
   if( !markowitz_criterion_row && !markowitz_criterion_col )
   {
      numericalstable = FALSE;
      return numericalstable;
   }

   aggr_lhs = SCIPmatrixGetRowLhs(matrix, rowidx);
   aggr_rhs = SCIPmatrixGetRowRhs(matrix, rowidx);
   if( !SCIPisInfinity(scip, -aggr_lhs) )
   {
      aggrside_lhs = TRUE;
   }
   else
      aggrside_lhs = FALSE;
   if( !SCIPisInfinity(scip, aggr_rhs) )
   {
      aggrside_rhs = TRUE;
   }
   else
      aggrside_rhs = FALSE;

   if( aggrside_lhs )
      *islhs = TRUE;
   else if( aggrside_rhs )
      *islhs = FALSE;
   else
      return FALSE;
   return TRUE;
}

static void printSubstitutionProcess(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,
      int                   colidx,             /**< the current variable */
      int                   rowidx,
      SCIP_Bool             bestrowisequ,
      SCIP_Bool             isfree,
      int                   ncancel,
      SCIP_Bool             isdefaultaggr
      )
{
   printf("--------------- substitution reduce var %d, cons %d, nonz %d ===  isdefaul %d  === ---------------\n", bestrowisequ, isfree, ncancel, isdefaultaggr);
   printf("sub var: %s\n", SCIPmatrixGetVar(matrix, colidx)->name);
   printf("sub row: ");
   SCIPmatrixPrintRow(scip, matrix, rowidx);
   /* print other row */
   int* printrowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   int* printrowend = printrowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
   for( ; printrowptr < printrowend ; printrowptr++ )
   {
      if ( rowidx == *printrowptr )
         continue;
      SCIPmatrixPrintRow(scip, matrix, *printrowptr);
   }
}

/* if all variables in this row are integer and coefficients are divisible by coef */
static SCIP_Bool checkIntCondition(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,         /**< presolver data */
      int                   colidx,             /**< the current variable */
      SCIP_Real             coef,               /**< the current variable's coef */
      int                   rowidx              /**< the current constraint */
      )
{
   int* colptr;
   int* colend;
   SCIP_Real* valptr;
   colptr = SCIPmatrixGetRowIdxPtr(matrix, rowidx);
   colend = colptr + SCIPmatrixGetRowNNonzs(matrix, rowidx);
   valptr = SCIPmatrixGetRowValPtr(matrix, rowidx);
   /* if colidx is implied integer variable, not lose implied integer information */
   if( SCIPvarGetType(SCIPmatrixGetVar(matrix, colidx)) == SCIP_VARTYPE_IMPLINT )
   {
      for( ; colptr < colend; colptr++, valptr++ )
      {
         if( *colptr == colidx )
            continue;
         if( SCIPvarGetType(SCIPmatrixGetVar(matrix, *colptr)) == SCIP_VARTYPE_CONTINUOUS )
            return FALSE;
         if( !SCIPisIntegral(scip, *valptr / coef) )
            return FALSE;
      }
   }
   /* if colidx is integer variable, we need the aggregated row all integer variables */
   else
   {
      for( ; colptr < colend; colptr++, valptr++ )
      {
         if( *colptr == colidx )
            continue;
         if( SCIPvarGetType(SCIPmatrixGetVar(matrix, *colptr)) >= SCIP_VARTYPE_IMPLINT )
            return FALSE;
         if( !SCIPisIntegral(scip, *valptr / coef) )
            return FALSE;
      }
   }
   return TRUE;
}

static int* getAggregatedRow(
      SCIP*                 scip,               /**< SCIP datastructure */
      SCIP_MATRIX*          matrix,             /**< matrix datastructure */
      SCIP_PRESOLDATA*      presoldata,
      int                   colidx,             /**< the current variable */
      SCIP_Bool*            islhs,              /**< whether use the row's lhs */
      SCIP_Real*            aggcoef,
      SCIP_Bool             isfree,
      SCIP_Bool*            blockedcols,
      SCIP_Bool*            blockedrows,
      SCIP_Bool*            removedrows,
      SCIP_Bool             isequ
      )
{
   int* bestrowptr;
   SCIP_Bool isvarbound;
   int nnonz_after;
   int ncancel;
   int temncancel;
   int maxtemvalue;
   /* temcancel + extra rewerd */
   int temvalue;
   int ndamagevarbound;
   SCIP_Bool isdefaultaggr;
   SCIP_Real lhs;
   SCIP_Real rhs;
   int* colptr;
   int* colend;
   int* colsubptr;
   int* colsubend;
   SCIP_Real* valptr;
   SCIP_Real* valsubptr;
   SCIP_Real* coefptr;
   SCIP_Real* coefsubptr;
   int index;
   int indexsub;
   int* rowptr;
   int* rowsubptr;
   int* rowend;
   bestrowptr = NULL;
   /* sort variables by var->index */
   /* default done in SCIP */
   /* loop through equality, counting the number of nonzeros reduced */
   rowptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
   rowend = rowptr + SCIPmatrixGetColNNonzs(matrix, colidx);
   valptr = SCIPmatrixGetColValPtr(matrix, colidx);
   maxtemvalue = presoldata->mineliminate-1;
   isdefaultaggr = FALSE;
   for( ; rowptr < rowend; rowptr++, valptr++ )
   {
      lhs = SCIPmatrixGetRowLhs(matrix, *rowptr);
      rhs = SCIPmatrixGetRowRhs(matrix, *rowptr);
      if( SCIPisEQ(scip, lhs, rhs) ^ isequ )
         continue;
      /* avoid singleton constraint */
      if( SCIPmatrixGetRowNNonzs(matrix, *rowptr) <= 1 )
         continue;
      if( SCIPmatrixGetRowNNonzs(matrix, *rowptr) > presoldata->maxrownonzeros && presoldata->maxrownonzeros >= 0 )
         continue;
      if( removedrows[*rowptr] )
         continue;
      if( blockedcols[colidx] && blockedrows[*rowptr] )
         continue;
      /* if the aggregated variable is integer, the aggregated row must only conatin integer variable */
      if( SCIPvarGetType(SCIPmatrixGetVar(matrix, colidx)) <= SCIP_VARTYPE_IMPLINT && !checkIntCondition(scip, matrix, presoldata, colidx, *valptr, *rowptr) )
         continue;
      if( !numericalStable(scip, matrix, presoldata, *rowptr, colidx, *valptr, islhs) )
         continue;
      ndamagevarbound = 0;

      /* if free, the aggregated row will be removed */
      /* if equa, the aggregated var will be removed */
      if( isfree )
      {
         if( isequ )
            temncancel = SCIPmatrixGetRowNNonzs(matrix, *rowptr) + SCIPmatrixGetColNNonzs(matrix, colidx) - 1;
         else
            temncancel = SCIPmatrixGetRowNNonzs(matrix, *rowptr);
      }
      else
      {
         if( isequ )
            temncancel = SCIPmatrixGetColNNonzs(matrix, colidx);
         else
            temncancel = 0;
      }
      /*
       * the var->index of colptr has been sorted
       * */
      rowsubptr = SCIPmatrixGetColIdxPtr(matrix, colidx);
      valsubptr = SCIPmatrixGetColValPtr(matrix, colidx);
      for( ; rowsubptr < rowend; rowsubptr++, valsubptr++ )
      {
         if( *rowsubptr == *rowptr )
            continue;
         /* At this step, both row(sub)ptr and val(sub)ptr are known, and now we need to compare them col by col */

         /* We do not want the substitution process to break a class of variable bound constraints (available for probing, branching,...) */
         /* instances: sp150*300d, ran14*18-disj-8 */
         /* init */
         nnonz_after = SCIPmatrixGetRowNNonzs(matrix, *rowsubptr);
         if( isequ )
            nnonz_after -= 1;
         colptr = SCIPmatrixGetRowIdxPtr(matrix, *rowptr);
         colend = colptr + SCIPmatrixGetRowNNonzs(matrix, *rowptr);
         coefptr = SCIPmatrixGetRowValPtr(matrix, *rowptr);
         colsubptr = SCIPmatrixGetRowIdxPtr(matrix, *rowsubptr);
         colsubend = colsubptr + SCIPmatrixGetRowNNonzs(matrix, *rowsubptr);
         coefsubptr = SCIPmatrixGetRowValPtr(matrix, *rowsubptr);
         isvarbound = FALSE;
         if( SCIPmatrixGetRowNNonzs(matrix, *rowsubptr) == 2 )
         {
            /* check whether binary variables exist */
            if( SCIPvarGetType(SCIPmatrixGetVar(matrix, *colsubptr)) == SCIP_VARTYPE_BINARY || SCIPvarGetType(SCIPmatrixGetVar(matrix, *(colsubptr+1))) == SCIP_VARTYPE_BINARY )
            {
               isvarbound = TRUE;
            }
         }
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
               if( SCIPisEQ(scip, *coefptr/(*valptr), *coefsubptr/(*valsubptr)) )
               {
                  temncancel += 1;
                  nnonz_after -= 1;
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
               temncancel -= 1;
               nnonz_after += 1;
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
               temncancel -= 1;
               nnonz_after += 1;
               colptr += 1;
               coefptr += 1;
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
               colsubptr += 1;
               coefsubptr += 1;
            }
         }
         if( isvarbound && nnonz_after >= 3 && presoldata->preservevarbound )
         {
            ndamagevarbound += 1;
         }
      }

      temvalue = temncancel;

      if( isequ && isfree )
         temvalue += presoldata->mineliminate;
      else if( isequ && !isfree )
         temvalue += (presoldata->mineliminate-1);
      else if( !isequ && isfree )
         temvalue += (presoldata->mineliminate-1);

      if( temvalue > maxtemvalue && ndamagevarbound == 0 )
      {
         maxtemvalue = temvalue;
         ncancel = temncancel;
         bestrowptr = rowptr;
         *aggcoef = *valptr;
         //break;
      }
   }
   if( bestrowptr != NULL )
   {
      presoldata->ncancels += ncancel;
      if( isequ && (isfree || SCIPmatrixGetRowNNonzs(matrix, *bestrowptr) == 2 || SCIPmatrixGetColNNonzs(matrix, colidx) == 1) )
      {
         isdefaultaggr = TRUE;
         presoldata->noldcancels += ncancel;
      }
      if( presoldata->isprint )
      {
         printSubstitutionProcess(scip, matrix, presoldata, colidx, *bestrowptr, isequ, isfree, ncancel, isdefaultaggr);
      }
   }
   return bestrowptr;
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
      presoldata->nfailures += 1;
      presoldata->nwaitingcalls = (int)(presoldata->waitingfac*(SCIP_Real)presoldata->nfailures);
   }
}

/*
 * Callback methods of presolver
 */

/** copy method for constraint handler plugins (called when SCIP copies plugins) */
static SCIP_DECL_PRESOLCOPY(presolCopyMultiagg)
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
      SCIP_CALL( SCIPincludePresolMultiagg(scip) );
   }

   return SCIP_OKAY;
}

/** execution method of presolver */
static SCIP_DECL_PRESOLEXEC(presolExecMultiagg)
{
   /* start the presolve method */
   clock_t TimeStart=clock();

   /* declarate the used data */
   SCIP_MATRIX* matrix;
   int colidx;
   /* number of implied free variable */
   int nfreetemp;
   int nsemitemp;
   SCIP_Bool islhs;
   SCIP_Real aggcoef;
   SCIP_Bool lbimplied;
   SCIP_Bool ubimplied;
   SCIP_Bool isaggregated;
   SCIP_Bool isexecuted;
   SCIP_Bool* blockedcols;
   SCIP_Bool* blockedrows;
   SCIP_Bool* removedrows;
   SCIP_PRESOLDATA* presoldata;

   /* initinize  */
   nfreetemp = 0;
   nsemitemp = 0;
   int* rowptr = NULL;
   isaggregated = FALSE;
   isexecuted = FALSE;
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
      presoldata->nwaitingcalls -= 1;
      SCIPdebugMsg(scip, "skipping multiagg: nfailures=%d, nwaitingcalls=%d\n", presoldata->nfailures,
            presoldata->nwaitingcalls);
      return SCIP_OKAY;
   }
   SCIPdebugMsg(scip, "starting multiagg. . .\n");
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
      printf("\nStart Multi-Aggregation Presolving\n\n");

   int ncols = SCIPmatrixGetNColumns(matrix);
   int nrows = SCIPmatrixGetNRows(matrix);
   assert(SCIPgetNVars(scip) == ncols);
   presoldata->nvar = ncols;

   /* initialize blockedcols(rows) */
   SCIP_CALL( SCIPallocBufferArray(scip, &blockedcols, ncols) );
   SCIP_CALL( SCIPallocBufferArray(scip, &blockedrows, nrows) );
   SCIP_CALL( SCIPallocBufferArray(scip, &removedrows, nrows) );
   for( int colidx = 0; colidx < ncols; colidx++ )
      blockedcols[colidx] = FALSE;
   for( int rowidx = 0; rowidx < nrows; rowidx++ )
   {
      blockedrows[rowidx] = FALSE;
      removedrows[rowidx] = FALSE;
   }

   /* loop through all variables and find appropriate variable to do aggregation, then perform multi-aggregation */
   for( colidx = 0; colidx < ncols; colidx++ )
   {
      SCIP_VAR* var = SCIPmatrixGetVar(matrix, colidx);
#if 1
      /* if the locks do not match do not consider the column for substitution */
      if( SCIPmatrixDownlockConflict(matrix, colidx) || SCIPmatrixUplockConflict(matrix, colidx) )
         continue;
      /* skip if the variable is not allowed to be multi-aggregated */
      if( SCIPdoNotMultaggrVar(scip, var) )
         continue;
      /* aggregation of binary variables is not supported */
#endif
      if( SCIPvarGetType(var) == SCIP_VARTYPE_BINARY )
         continue;
      if( SCIPvarGetType(var) <= SCIP_VARTYPE_IMPLINT && SCIPisEQ(scip, SCIPmatrixGetColLb(matrix, colidx), 0.0) && SCIPisEQ(scip, SCIPmatrixGetColUb(matrix, colidx), 1.0) )
         continue;
      if( presoldata->maxcolnonzeros < SCIPmatrixGetColNNonzs(matrix, colidx) && presoldata->maxcolnonzeros >= 0 )
         continue;
      /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/
      /* detect implied free variableis */
      SCIP_Bool isfree;
      /* detect semi-continuous variable */
      SCIP_Bool issemi = FALSE;
      getImpliedBounds(scip, matrix, presoldata, colidx, &ubimplied, &lbimplied, &issemi);
      if( !lbimplied || !ubimplied )
         isfree = FALSE;
      else
      {
         isfree = TRUE;
         nfreetemp += 1;
      }
      if( issemi )
         nsemitemp += 1;

      /* find a row for aggregation */
      //check it
      rowptr = getAggregatedRow(scip, matrix, presoldata, colidx, &islhs, &aggcoef, isfree, blockedcols, blockedrows, removedrows, TRUE);
      if( rowptr == NULL )
         rowptr = getAggregatedRow(scip, matrix, presoldata, colidx, &islhs, &aggcoef, isfree, blockedcols, blockedrows, removedrows, FALSE);
      if( rowptr == NULL )
         continue;
      /* perform multi-aggregation */
      isaggregated = aggregation(scip, matrix, presoldata, colidx, *rowptr, islhs, aggcoef, isfree, blockedcols, blockedrows, removedrows, ndelconss, naggrvars);
      if( !isaggregated )
         continue;
      /*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/
      /* multi-aggregation was performed */
      if( issemi )
         presoldata->nsemiexec += 1;
      *result = SCIP_SUCCESS;
      isexecuted = TRUE;
   }
   /* count the number of implied free variables found */
   presoldata->nfree = nfreetemp;
   presoldata->nsemi = nsemitemp;
   updateFailureStatistic(presoldata, isexecuted);

   SCIPfreeBufferArray(scip, &blockedcols);
   SCIPfreeBufferArray(scip, &blockedrows);
   SCIPfreeBufferArray(scip, &removedrows);
   SCIPmatrixFree(scip, &matrix);

   if( presoldata->isprint )
      printf("\nEnd Multi-Aggregation Presolving\n\n");

   clock_t TimeEnd=clock();
   presoldata->time += (SCIP_Real)(TimeEnd-TimeStart)/(double)CLOCKS_PER_SEC;

   return SCIP_OKAY;
}

/*
 * presolver specific interface methods
 */

/** destructor of presolver to free user data (called when SCIP is exiting) */
static SCIP_DECL_PRESOLFREE(presolFreeMultiagg)
{
   SCIP_PRESOLDATA* presoldata;

   /* free presolver data */
   presoldata = SCIPpresolGetData(presol);
   assert(presoldata != NULL);
   if( scip->set->disp_verblevel != 0 && presoldata->isinit == TRUE )
   {
      //TODO nfree maybe meanless
      printf("MultiAggregation: nvar= %d nfree= %d nexec= %d ndvar= %d ndcon= %d nnonz= %d time= %.2f\n", presoldata->nvar, presoldata->nfree, presoldata->nexec, presoldata->ndvar, presoldata->ndcon, presoldata->ncancels, presoldata->time);
      printf("Compare Aggregation Type: nexec= %d nequa= %d ntwo= %d nsgt= %d nfvs= %d nnew= %d nsemiexec= %d nsemi= %d\n", presoldata->nexec, presoldata->ndvar, presoldata->ntwo, presoldata->nsgt, presoldata->nfvs, presoldata->nnew, presoldata->nsemiexec, presoldata->nsemi);
      printf("The Number of Nonzeros Cancel: nold= %d nnew= %d\n", presoldata->noldcancels, presoldata->ncancels);
      printf("My Presolving Output\n");
   }
   SCIPfreeBlockMemory(scip, &presoldata);
   SCIPpresolSetData(presol, NULL);

   return SCIP_OKAY;
}

/** initialization method of presolver (called after problem was transformed) */
static SCIP_DECL_PRESOLINIT(presolInitMultiagg)
{
   SCIP_PRESOLDATA* presoldata;
   presoldata = SCIPpresolGetData(presol);

   presoldata->nvar = 0;
   presoldata->nfree = 0;
   presoldata->nsemi = 0;
   presoldata->nexec = 0;
   presoldata->ntwo = 0;
   presoldata->nsgt = 0;
   presoldata->nfvs = 0;
   presoldata->nnew = 0;
   presoldata->ndvar = 0;
   presoldata->ndcon = 0;
   presoldata->ncancels = 0;
   presoldata->noldcancels = 0;
   presoldata->time = 0.0;
   presoldata->nfailures = 0;
   presoldata->nwaitingcalls = 0;
   presoldata->isinit = TRUE;

   return SCIP_OKAY;
}

/** creates the multiagg presolver and includes it in SCIP */
SCIP_RETCODE SCIPincludePresolMultiagg(
      SCIP*                 scip                /**< SCIP data structure */
      )
{
   SCIP_PRESOLDATA* presoldata;
   SCIP_PRESOL* presol;

   /* create multiagg presolver data */
   SCIP_CALL( SCIPallocBlockMemory(scip, &presoldata) );

   presoldata->isinit = FALSE;

   /* include presolver */
   SCIP_CALL( SCIPincludePresolBasic(scip, &presol, PRESOL_NAME, PRESOL_DESC, PRESOL_PRIORITY, PRESOL_MAXROUNDS,
            PRESOL_TIMING, presolExecMultiagg, presoldata) );

   SCIP_CALL( SCIPsetPresolCopy(scip, presol, presolCopyMultiagg) );
   SCIP_CALL( SCIPsetPresolFree(scip, presol, presolFreeMultiagg) );
   SCIP_CALL( SCIPsetPresolInit(scip, presol, presolInitMultiagg) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/multiagg/enablecopy",
            "should multiagg presolver be copied to sub-SCIPs?",
            &presoldata->enablecopy, TRUE, PRESOL_ENABLECOPY, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/multiagg/maxcolnonzeros",
            "maximal number of considered nonzeros within one column (-1: no limit)",
            &presoldata->maxcolnonzeros, TRUE, DEFAULT_MAX_COLNONZEROS, -1, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/multiagg/maxrownonzeros",
            "maximal number of considered nonzeros within one row (-1: no limit)",
            &presoldata->maxrownonzeros, TRUE, DEFAULT_MAX_ROWNONZEROS, -1, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
            "presolving/multiagg/min_elim",
            "minimal eliminated nonzeros in general",
            &presoldata->mineliminate, FALSE, MIN_ELIM, -INT_MAX, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip,
            "presolving/multiagg/waitingfac",
            "number of calls to wait until next execution as a multiple of the number of useless calls",
            &presoldata->waitingfac, TRUE, DEFAULT_WAITINGFAC, 0.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/multiagg/isprint",
            "should we print the aggregation process?",
            &presoldata->isprint, TRUE, DEBUG_ISPRINT, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/multiagg/isaccurate",
            "whether the number of nonzeros is accurately computed?",
            &presoldata->isaccurate, TRUE, PRESOL_ISACCURATE, NULL, NULL) );

   SCIP_CALL( SCIPaddBoolParam(scip,
            "presolving/multiagg/preservetwo",
            "should we preserve small cons?",
            &presoldata->preservevarbound, TRUE, DEFAULT_PRESERVE_VARBOUND, NULL, NULL) );
   return SCIP_OKAY;
}
