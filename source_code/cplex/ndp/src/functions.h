#ifndef __FUNCTIONS_HEADER__
#define __FUNCTIONS_HEADER__

#include "../../../../ThirdParty/cplex/include/cplex.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <unordered_set>
#include <time.h>
#include "paramset.h"

#define PrintErr()                  printf("ERROR! [%s:%d]\n", __FILE__, __LINE__);
#define Free(ptr)                   free_and_null((void**)(&ptr));
#define eps                         1e-6

typedef struct timespec Time;

extern double Max(double a, double b);
extern double Min(double a, double b);

extern void free_and_null(void** ptr);

extern int CPXPUBLIC CheckCutOff(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      int type,
      int sos,
      int nodecnt,
      int bdcnt,
      const int *nodebeg,
      const int *indices,
      const char *lu,
      const double *bd,
      const double *nodeest,
      int* useraction_p
      );

extern int CPXPUBLIC GetIncumbent(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double objval,                /* objective value */
      double *x,                    /* pointer to an array contains solution */
      int *isfeas_p,                /* whether use x to replace the current incumbent */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

extern int CPXPUBLIC Heuristic(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double *objval_p,             /* pointer to objective value */
      double *x,                    /* pointer to an array contains solution */
      int *checkfeas_p,             /* whether check integer feasibility */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

extern int CPXPUBLIC Alone(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double *objval_p,             /* pointer to objective value */
      double *x,                    /* pointer to an array contains solution */
      int *checkfeas_p,             /* whether check integer feasibility */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

extern int CPXPUBLIC LPBH1(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double *objval_p,             /* pointer to objective value */
      double *x,                    /* pointer to an array contains solution */
      int *checkfeas_p,             /* whether check integer feasibility */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

extern int CPXPUBLIC LPBH2(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double *objval_p,             /* pointer to objective value */
      double *x,                    /* pointer to an array contains solution */
      int *checkfeas_p,             /* whether check integer feasibility */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

extern int CPXPUBLIC LPBH3(
      CPXCENVptr origenv,           /* pointer to the CPLEX origenvironment */
      void *cbdata,                 /* pointer to pass to functions that obtain callback-specific information */
      int wherefrom,                /* integer value reporting at which point in the optimization this function was called */
      void *cbhandle,               /* pointer to user private data */
      double *objval_p,             /* pointer to objective value */
      double *x,                    /* pointer to an array contains solution */
      int *checkfeas_p,             /* whether check integer feasibility */
      int *useraction_p             /* specifier of the action to be taken on completion of the user callback */
      );

class HeurData
{
   public:
      /* CPX data */
      int heurstatus;
      CPXENVptr subenv;
      CPXLPptr submodel;
      CPXENVptr lpenv;
      CPXLPptr lpmodel;

      /* settings */
      int heurmode;
      int cpxmode;
      double mipgap;
      double timelimit;

      int* rmatbeg;
      int* rmatind;
      double* rmatval;
      int* cmatbeg;
      int* cmatind;
      double* cmatval;
      double* rhs;

      char** colname;
      char* colnamestore;
      char** rowname;
      char* rownamestore;

      /* heuristic data */
      int nheurcall;
      int nheurlimit;
      int nheuradd;
      bool update_incsol;                    /* whether incumbent is updated */
      bool update1;
      bool update2;
      double incval;                   /* current incumbent obj value*/
      double refval;                   /* reference solution obj value */
      double* incsol;                  /* current incumbent solution */
      int nrow;
      int ncol;
      int nnz;
      int ncont;
      int nbin;
      int nint;                        /* general integer plus binary */
      int nlink;
      char* ctype;                     /* variable type */
      int* contidx;
      int* binidx;
      int* intidx;
      double* lb;
      double* ub;
      double* binlb;
      double* binub;
      double* intlb;
      double* intub;
      double* contlb;
      double* contub;
      double* objcoef;
      /* strings consisted of single characters, used to change variable bounds */
      char* L_sign;
      char* U_sign;
      char* B_sign;
      double cutoff;

      int* var2link;
      int** link2var;
      int* link2row;
      int* row2link;
      int* linksize;                   /* the number of variable in each links */
      int** linkdismat;                /* distant matrix; see [GINS] */
      bool* used;                      /* mark links that have been used */

      /* statistic data */
      double time;
      double time1;
      double time2;
      Time init_time;
      int nexec1;
      int nexec2;
      int nsucc1;
      int nsucc2;
      Time previoustime;

      /* class definition functions */
      HeurData();
      ~HeurData();
      void Init(char** argv, Set* set);
      void GetModelInfo();
      void SetSubProb();
      /* change the bounds of a class of variables in submodel */
      void ChgClassVarBnd(char vartype, char boundtype, double* bound );
      /* recover the bounds of a class of variable in submodel */
      void RcvClassVarBnd(char vartype);
      void RcvVarBnd(int* indices, int num);
      bool Optimize(double* x, double* objval);
      bool SetCutoff(double cutoff);
      void GetName();
      /* the heuristic potential with colidx as the center and size as the neighborhood range; see [GINS] */
      int GetBestLink();
      /* get the activity bound residuals of incsol */
      double GetActBndRes(int rowidx);
      /* neighborhood search */
      bool NeiSearch(double* objval_p, double* x, int linkidx, int range);
      /* find the unused link closest to the central link */
      int FindClosestLink(int centrallink);
};

/* infrastructure */
void GetTime(timespec* time);
double CalTime(timespec* start_time, timespec* end_time);

#endif
