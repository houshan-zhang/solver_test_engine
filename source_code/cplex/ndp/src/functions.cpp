#include "functions.h"

double Max(double a, double b)
{
   return a >= b ? a : b;
}

double Min(double a, double b)
{
   return a <= b ? a : b;
}

double Abs(double a)
{
   return a >= 0 ? a : -a;
}

bool IsEq(double a, double b)
{
   return Abs(a-b) <= eps;
}

void free_and_null(void** ptr)
{
   if( *ptr != NULL )
   {
      free(*ptr);
      *ptr = NULL;
   }
}

int CPXPUBLIC CheckCutOff(
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
      )
{
   HeurData* heurdata = (HeurData*)cbhandle;
   if( heurdata->incval - heurdata->refval <= heurdata->mipgap * heurdata->refval )
   {
      *useraction_p = CPX_CALLBACK_FAIL;
      return 0;
   }

   *useraction_p = CPX_CALLBACK_DEFAULT;
   return 0;
}

/* integer solution cannot be passed directly to cplex */
int CPXPUBLIC GetIncumbent(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double objval,
      double* x,
      int* isfeas_p,
      int* useraction_p
      )
{
   //printf("Enter GetInc\n");
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;

   /* initialize useraction to indicate no user action taken */
   *useraction_p = CPX_CALLBACK_DEFAULT;
   /* replace the current incument in cplex */
   *isfeas_p = 1;
   /* replace the current incument in heurdata */
   if( heurdata->incval > objval )
   {
      heurdata->update_incsol = true;
      heurdata->update1 = true;
      heurdata->update2 = true;
      heurdata->incval = objval;
      for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
   }
   return 0;
};

int CPXPUBLIC Heuristic(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double* objval_p,                /* the input is the obj value of LP relaxation, and the output is the obj value found by heuristics */
      double* x,                       /* the input is the solution of LP relaxation, and the output is the solution found by heuristics */
      int* checkfeas_p,
      int* useraction_p
      )
{
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;
   if( heurdata->heurmode <= 0 )
      return 0;

   Time ctime;
   bool time_delay = false;
   GetTime(&ctime);
   if( CalTime(&heurdata->previoustime, &ctime) >= 200 ) { time_delay = true; }

   //printf("%d  %d  %d\n", heurdata->nheurcall, heurdata->nheurlimit, heurdata->update_incsol);
   //if( !heurdata->update_incsol && heurdata->nheurcall > heurdata->nheurlimit && !time_delay )
   if( !heurdata->update1 && heurdata->nheurcall > heurdata->nheurlimit && (!heurdata->update2 || !time_delay) )
      return 0;

   Time start_time;
   GetTime(&start_time);

   if( !heurdata->update1 )
      heurdata->nheurcall += 1;

   int status = 0;
   double origobjval;
   double bestbound;
   double cutoff;
   bool success = false;
   /* initialize useraction to indicate no user action taken */
   *useraction_p = CPX_CALLBACK_DEFAULT;
   /* get pointer */
   CPXCLPptr origmodel;
   /* get the original MIP problem model */
   status = CPXgetcallbacklp(origenv, cbdata, wherefrom, &origmodel);

   /* heuristic */
   /* the first method  */
   Time start_time1;
   GetTime(&start_time1);
   /* change variable bound of submodel */
   if( !heurdata->update1 )
      heurdata->ChgClassVarBnd('C', 'B', x);
   else
   {
      heurdata->ChgClassVarBnd('C', 'B', heurdata->incsol);
   }
   /* get the cutoff of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_CUTOFF, &cutoff);
   /* get the obj value of incumbent of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_INTEGER, &origobjval);
   /* get the best bound of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_REMAINING, &bestbound);
   /* set cutoff of subproblem */
   heurdata->SetCutoff(cutoff);

   /* solve submodel */
   //TODO
   if( heurdata->nexec1 == 0 && false )
   {
      for( int i = 0; i < heurdata->nint; i++ )
      {
         if( x[heurdata->intidx[i]] >= 1e-5 )
         {
            *objval_p += (ceil(x[heurdata->intidx[i]]) - x[heurdata->intidx[i]]) * heurdata->objcoef[heurdata->intidx[i]];
            x[heurdata->intidx[i]] = ceil(x[heurdata->intidx[i]]);
            success = true ? false : heurdata->incval > *objval_p;
         }
      }
   }
   else
      success = heurdata->Optimize(x, objval_p);
   heurdata->nexec1 += 1;
   /* recover the bounds of submodel */
   heurdata->RcvClassVarBnd('C');
#if 0
   double* llb = (double*)malloc(heurdata->ncol*sizeof(double));
   double* uub = (double*)malloc(heurdata->ncol*sizeof(double));
   CPXgetlb(heurdata->subenv, heurdata->submodel, llb, 0, heurdata->ncol-1);
   CPXgetub(heurdata->subenv, heurdata->submodel, uub, 0, heurdata->ncol-1);
   for( int i = 0; i < heurdata->ncol; i++ )
   {
      if( heurdata->ctype[i] == CPX_CONTINUOUS &&  llb[i] >= 0.1 )
         printf("llb: %f, uub: %f, lb: %f, ub: %f, name: %s\n", llb[i], uub[i], heurdata->lb[i], heurdata->ub[i], heurdata->colname[i]);
   }
   Free(llb);
   Free(uub);
#endif

   /* if we get a better incumbent solution */
   if( success )
   {
      heurdata->nsucc1 += 1;
      if( !heurdata->update1 )
      {
         /* update heur limit  */
         heurdata->nheurlimit += heurdata->nheuradd;
         if( origobjval >= 1e+10 )
            printf("=  C  = - Old  - INF -   New %10.2f\n", *objval_p);
         else
            printf("=  C  = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
      }
      else
         printf("= C.I = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
      /* update cutoff of subproblem */
      heurdata->SetCutoff(*objval_p);
      /* tell CPLEX that a solution is being returned */
      *useraction_p = CPX_CALLBACK_SET;
      /* update the current incument in heurdata */
      if( heurdata->incval > *objval_p )
      {
         heurdata->update2 = true;
         heurdata->incval = *objval_p;
         origobjval = *objval_p;
         for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
      }
      else { PrintErr(); }
   }
   //TODO
   heurdata->update1 = false;

   Time end_time1;
   GetTime(&end_time1);
   heurdata->time1 += CalTime(&start_time1, &end_time1);

   /* the method 2 */
   Time start_time2;
   GetTime(&start_time2);
   int range = 2;
   if( heurdata->update2 && heurdata->nlink >= 1 && heurdata->heurmode >= 2 && time_delay )
   {
      //printf("Enter Cycle-Based Neighborhood Search, incsol objval: %f\n", heurdata->incval);

      /* ========== Cycle-Based Neighborhood Search ========== */
      heurdata->update2 = false;
      bool stop = false;
      int centrallink = heurdata->GetBestLink();
      for( int i = 0; i < heurdata->nlink; i++ ) { heurdata->used[i] = false; }
      int linkidx;
      int nfail = 0;
      int nStepTwo = 0;
      while( !stop )
      {
         linkidx = heurdata->FindClosestLink(centrallink);
         if( linkidx == -1 ) { break; }
         heurdata->nexec2 += 1;
         stop = !heurdata->NeiSearch(objval_p, x, linkidx, range);

         /* find a better incumbent solution */
         if( !stop )
         {
            /* update the current incument in heurdata */
            if( heurdata->incval > *objval_p )
            {
               heurdata->update1 = true;
               heurdata->update2 = true;
               heurdata->incval = *objval_p;
               for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
               heurdata->nsucc2 += 1;
               printf("=  I  = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
               heurdata->SetCutoff(*objval_p);
               origobjval = *objval_p;
               *useraction_p = CPX_CALLBACK_SET;
            }
            else { PrintErr(); }
         }

         nStepTwo ++;
         if( stop ) { nfail++; } else { nfail = 0; }
         if( heurdata->heurmode >= 3 ) { stop = false; }
         //TODO
         if( nfail >= 5 || nStepTwo >= 10 ) { stop = true; }
         //if( nfail >= 2 ) { stop = true; }
         if( heurdata->refval >= 1e+20 )
         {
            if( (heurdata->incval-bestbound) <= (heurdata->mipgap*bestbound) ) { stop = true; }
         }
         else
         {
            if( (heurdata->incval-heurdata->refval) <= (heurdata->mipgap*heurdata->refval) ) { stop = true; }
         }

         Time current_time;
         GetTime(&current_time);
         double cost_time = CalTime(&heurdata->init_time, &current_time);
         if( cost_time >= heurdata->timelimit )
            break;
      }
      GetTime(&heurdata->previoustime);
   }
   Time end_time2;
   GetTime(&end_time2);
   heurdata->time2 += CalTime(&start_time2, &end_time2);

   /* check the solution for integer feasibility */
   *checkfeas_p = 1;

   Time end_time;
   GetTime(&end_time);
   heurdata->time += CalTime(&start_time, &end_time);

   return status;
};

bool HeurData::NeiSearch(double* objval_p, double* x, int linkidx, int range)
{
   if( used[linkidx] ) { PrintErr(); }
   /* change variable bound of submodel */
   ChgClassVarBnd('I', 'B', incsol);
   /* select neighbourhood */
   int nfree = 1;
   int nfreemax = 30;
   std::unordered_set<int> freelinkset;
   freelinkset.insert(linkidx);
   for( int i = 0; i < nlink; i++ )
   {
#if 1
      if( nfree >= nfreemax ) { break; }
      if( linkdismat[linkidx][i] == 1 )
      {
         for( int j = 0; j < nlink; j++ )
         {
            if( nfree >= nfreemax ) { break; }
            if( linkdismat[i][j] <= 1 ) { continue; }
            if( linkdismat[linkidx][j] == 1 )
            {
               for( int k = 0; k < nlink; k++ )
               {
                  if( nfree >= nfreemax ) { break; }
                  if( k == linkidx || k == i || k == j ) { continue; }
                  if( linkdismat[i][k] + linkdismat[j][k] == range )
                  {
                     if(freelinkset.count(i) == 0 ) { freelinkset.insert(i); nfree++; }
                     if(freelinkset.count(j) == 0 ) { freelinkset.insert(j); nfree++; }
                     if(freelinkset.count(k) == 0 ) { freelinkset.insert(k); nfree++; }
                  }
               }
            }
         }
      }
#else
      if( linkdismat[linkidx][i] <= 1 )
      {
         if(freelinkset.count(i) == 0 ) { freelinkset.insert(i); nfree++; }
      }
#endif
   }
   for( auto iter = freelinkset.begin(); iter != freelinkset.end(); iter++ )
   {
      RcvVarBnd(link2var[*iter], linksize[*iter]);
      used[*iter] = true;
   }
   //printf("nfreelink: %d\n", nfree);
   bool success = Optimize(x, objval_p);
   RcvClassVarBnd('I');
   return success;
}

void HeurData::GetName()
{
   int storespace;
   int surplus;
   /* get col names */
   CPXgetcolname(subenv, submodel, NULL, NULL, 0, &surplus, 0, ncol-1);
   storespace = -surplus;
   colname = (char**)malloc(ncol*sizeof(char*));
   colnamestore = (char*)malloc(storespace*sizeof(char));
   heurstatus = CPXgetcolname(subenv, submodel, colname, colnamestore, storespace, &surplus, 0, ncol-1);
   if( heurstatus || surplus < 0 ) { PrintErr(); }
   /* get row names */
   CPXgetrowname(subenv, submodel, NULL, NULL, 0, &surplus, 0, nrow-1);
   storespace = -surplus;
   rowname = (char**)malloc(nrow*sizeof(char*));
   rownamestore = (char*)malloc(storespace*sizeof(char));
   heurstatus = CPXgetrowname(subenv, submodel, rowname, rownamestore, storespace, &surplus, 0, nrow-1);
   if( heurstatus || surplus < 0 ) { PrintErr(); }
}

HeurData::HeurData()
{
   heurmode = 0;
   cpxmode = 0;
   mipgap = 1e-4;
   timelimit = 7200.0;
   heurstatus = 0;
   subenv = NULL;
   submodel = NULL;

   nheurcall = 0;
   nheurlimit = 0;
   nheuradd = 0;
   update_incsol = false;
   update1 = false;
   update2 = false;
   incval = 1e+75;
   refval = 1e+75;
   cutoff = 1e+75;
   incsol = NULL;
   nrow = 0;
   ncol = 0;
   nnz = 0;
   ncont = 0;
   nbin = 0;
   nint = 0;
   nlink = 0;
   ctype = NULL;
   contidx = NULL;
   binidx = NULL;
   intidx = NULL;
   lb = NULL;
   ub = NULL;
   binlb = NULL;
   binub = NULL;
   intlb = NULL;
   intub = NULL;
   contlb = NULL;
   contub = NULL;
   L_sign = NULL;
   U_sign = NULL;
   B_sign = NULL;

   rmatbeg = NULL;
   rmatind = NULL;
   rmatval = NULL;
   cmatbeg = NULL;
   cmatind = NULL;
   cmatval = NULL;
   rhs = NULL;

   colname = NULL;
   rowname = NULL;
   colnamestore = NULL;
   rownamestore = NULL;

   var2link = NULL;
   link2var = NULL;
   link2row = NULL;
   row2link = NULL;
   linksize = NULL;
   linkdismat = NULL;
   used = NULL;

   time = 0.0;
   time1 = 0.0;
   time2 = 0.0;
   nexec1 = 0;
   nexec2 = 0;
   nsucc1 = 0;
   nsucc2 = 0;
   GetTime(&previoustime);
   previoustime.tv_sec -= 7200;
}

void HeurData::Init(char** argv, Set* set)
{
   nheurlimit = 500;
   nheuradd = 100;

   heurmode = set->heurmode;
   cpxmode = set->cpxmode;
   mipgap = set->mipgap;
   timelimit = double(set->timelimit);
   subenv = CPXopenCPLEX(&heurstatus);
   submodel = CPXcreateprob(subenv, &heurstatus, "heur");
   heurstatus = CPXreadcopyprob(subenv, submodel, argv[1], NULL);
   if( heurmode >= 21 )
   {
      lpenv = CPXopenCPLEX(&heurstatus);
      lpmodel = CPXcreateprob(lpenv, &heurstatus, "heur");
      heurstatus = CPXreadcopyprob(lpenv, lpmodel, argv[1], NULL);
   }

   nrow = CPXgetnumrows(subenv, submodel);
   ncol = CPXgetnumcols(subenv, submodel);
   nnz = CPXgetnumnz(subenv, submodel);
   nbin = CPXgetnumbin(subenv, submodel);
   nint = CPXgetnumint(subenv, submodel) + nbin;
   ncont = ncol-nint-CPXgetnumsemicont(subenv, submodel)-CPXgetnumsemiint(subenv, submodel);
   incsol = (double*)malloc(ncol*sizeof(double));
   ctype = (char*)malloc(ncol*sizeof(char));
   heurstatus = CPXgetctype(subenv, submodel, ctype, 0, ncol-1);
   binidx = (int*)malloc((nbin)*sizeof(int));
   intidx = (int*)malloc((nint)*sizeof(int));
   contidx = (int*)malloc((ncont)*sizeof(int));
   lb = (double*)malloc(ncol*sizeof(double));
   ub = (double*)malloc(ncol*sizeof(double));
   objcoef = (double*)malloc(ncol*sizeof(double));
   binlb = (double*)malloc(nbin*sizeof(double));
   binub = (double*)malloc(nbin*sizeof(double));
   intlb = (double*)malloc(nint*sizeof(double));
   intub = (double*)malloc(nint*sizeof(double));
   contlb = (double*)malloc(ncont*sizeof(double));
   contub = (double*)malloc(ncont*sizeof(double));
   L_sign = (char*)malloc(ncol*sizeof(char));
   U_sign = (char*)malloc(ncol*sizeof(char));
   B_sign = (char*)malloc(ncol*sizeof(char));
   for( int i = 0; i < ncol; i++ )
   {
      L_sign[i] = 'L';
      U_sign[i] = 'U';
      B_sign[i] = 'B';
   }
   heurstatus = CPXgetlb(subenv, submodel, lb, 0, ncol-1);
   heurstatus = CPXgetub(subenv, submodel, ub, 0, ncol-1);
   heurstatus = CPXgetobj(subenv, submodel, objcoef, 0, ncol-1);
   int cidx = 0;
   int bidx = 0;
   int iidx = 0;
   for( int i = 0; i < ncol; i++ )
   {
      if( ctype[i] == CPX_BINARY )
      {
         binidx[bidx] = i;
         binlb[bidx] = lb[i];
         binub[bidx] = ub[i];
         bidx++;
         intidx[iidx] = i;
         intlb[iidx] = lb[i];
         intub[iidx] = ub[i];
         iidx++;
      }
      else if( ctype[i] == CPX_INTEGER )
      {
         intidx[iidx] = i;
         intlb[iidx] = lb[i];
         intub[iidx] = ub[i];
         iidx++;
      }
      else if( ctype[i] == CPX_CONTINUOUS )
      {
         contidx[cidx] = i;
         contlb[cidx] = lb[i];
         contub[cidx] = ub[i];
         cidx++;
      }
   }

   GetName();

   GetModelInfo();

   /* get the number of links (constraint whose integer variable are all singletons and exist integer variable) */
   int firstr;
   int endr;
   int firstc;
   int endc;
   bool isallsingleton;
   linksize = (int*)malloc(nint*sizeof(int));
   link2row = (int*)malloc(nint*sizeof(int));
   row2link = (int*)malloc(nrow*sizeof(int));
   for( int j = 0; j < nrow; j++ )
      row2link[j] = -1;
   for( int j = 0; j < nrow; j++ )
   {
      isallsingleton = false;
      firstc = rmatbeg[j];
      if( j != nrow-1 )
         endc = rmatbeg[j+1]-1;
      else
         endc = nnz-1;
      linksize[nlink] = 0;
      for( ; firstc <= endc; firstc++ )
      {
         int colidx = rmatind[firstc];
         if( ctype[colidx] != CPX_BINARY && ctype[colidx] != CPX_INTEGER )
            continue;
         /* check whether integer variable is singleton */
         firstr = cmatbeg[colidx];
         if( colidx != ncol-1 )
            endr = cmatbeg[colidx+1]-1;
         else
            endr = nnz-1;
         if( firstr == endr )
         {
            isallsingleton = true;
            linksize[nlink]++;
            link2row[nlink] = j;
         }
         else
         {
            isallsingleton = false;
            break;
         }
      }
      if( isallsingleton )
      {
         row2link[j] = nlink;
         nlink++;
      }
   }
   /* resize linksize and link2row */
   linksize = (int*)realloc(linksize, nlink*sizeof(int));
   link2row = (int*)realloc(link2row, nlink*sizeof(int));

   /* get var2link, lin2var */
   var2link = (int*)malloc(ncol*sizeof(int));
   /* integer and binary variables have corresponding links */
   for( int i = 0; i < ncol; i++ )
      var2link[i] = -1;
   link2var = (int**)malloc(nlink*sizeof(int*));
   for( int i = 0; i < nlink; i++ )
   {
      link2var[i] = (int*)malloc(linksize[i]*sizeof(int));
      int rowidx = link2row[i];
      firstc = rmatbeg[rowidx];
      if( rowidx != nrow-1 )
         endc = rmatbeg[rowidx+1]-1;
      else
         endc = nnz-1;
      int tmp = 0;
      for( ; firstc <= endc; firstc++ )
      {
         int colidx = rmatind[firstc];
         if( ctype[colidx] != CPX_BINARY && ctype[colidx] != CPX_INTEGER )
            continue;
         var2link[colidx] = i;
         link2var[i][tmp] = colidx;
         tmp++;
      }
   }
   /* ---------- */

   /* get distance between each links */
   linkdismat = (int**)malloc(nlink*sizeof(int*));
   for( int i = 0; i < nlink; i++ )
   {
      linkdismat[i] = (int*)malloc(nlink*sizeof(int));
      /* for a connection graph, the distance between two links must be less than nlink */
      for( int j = 0; j < nlink; j++ )
         linkdismat[i][j] = nlink;
   }

   /* init the link adjacent relationship */
   const char* prefix = "facility_";
   int x1;
   int y1;
   int x2;
   int y2;
   for( int i = 0; i < nlink; i++ )
   {
      int rowidx = link2row[i];
      if( strncmp(rowname[rowidx], prefix, strlen(prefix)) == 0 )
      {
         const char *x1_str = rowname[rowidx] + strlen(prefix);
         x1 = atoi(x1_str);
         const char *underscore1 = strchr(x1_str, '_');
         if( underscore1 == NULL ) { PrintErr(); }
         const char* y1_str = underscore1 + 1;
         y1 = atoi(y1_str);
         for( int jidx = 0; jidx < nrow; jidx++ )
         {
            if( jidx != rowidx && strncmp(rowname[jidx], prefix, strlen(prefix)) == 0 )
            {
               if( row2link[jidx] == -1 ) { PrintErr(); }
               const char *x2_str = rowname[jidx] + strlen(prefix);
               x2 = atoi(x2_str);
               const char *underscore2 = strchr(x2_str, '_');
               if( underscore2 == NULL ) { PrintErr(); }
               const char* y2_str = underscore2 + 1;
               y2 = atoi(y2_str);
               if( x1 == x2 || x1 == y2 || y1 == x2 || y1 == y2 )
               {
                  linkdismat[i][row2link[jidx]] = 1;
                  linkdismat[row2link[jidx]][i] = 1;
                  //printf("Facility1: %d, %d; Facility2: %d, %d\n", x1, y1, x2, y2);
               }
            }
         }
      }
   }

   /* Floyd-Warshall algorithm */
   for( int i = 0; i < nlink; i++ )
   {
      for( int j = 0; j < nlink; j++ )
      {
#if 0
         /* save time and memory */
         if( linkdismat[i][j] >= 10 )
            continue;
#endif
         for( int k = 0; k < nlink; k++ )
         {
            linkdismat[j][k] = Min(linkdismat[j][k], linkdismat[i][j] + linkdismat[i][k]);
            linkdismat[k][j] = linkdismat[j][k];
         }
      }
   }
   for( int i = 0; i < nlink; i++ )
      linkdismat[i][i] = 0;

   /* ---------- */
   used = (bool*)malloc(nlink*sizeof(bool));
   for( int i = 0; i < nlink; i++ ) { used[i] = false; }
   SetSubProb();
}

int HeurData::GetBestLink()
{
   int bestlink = -1;
   double bestscore = 0.0;
   for( int i = 0; i < nlink; i++ )
   {
      if( GetActBndRes(link2row[i]) > bestscore )
      {
         bestscore = GetActBndRes(link2row[i]);
         bestlink = i;
      }
   }
   return bestlink;
}

double HeurData::GetActBndRes(int rowidx)
{
   int firstc;
   int endc;
   double lhstmp = 0.0;
   firstc = rmatbeg[rowidx];
   if( rowidx != nrow-1 )
      endc = rmatbeg[rowidx+1]-1;
   else
      endc = nnz-1;
   for( ; firstc <= endc; firstc++ )
      lhstmp += rmatval[firstc]*incsol[rmatind[firstc]];
   if( rhs[rowidx]-lhstmp <= -eps ) { PrintErr(); }
   return rhs[rowidx]-lhstmp;
}

void HeurData::SetSubProb()
{
   /* CPX_OFF is default value of ScreenOutput in callback mode */
   heurstatus = CPXsetintparam(subenv, CPXPARAM_ScreenOutput, CPX_OFF);
   heurstatus = CPXsetdblparam(subenv, CPXPARAM_MIP_Tolerances_MIPGap, (double)1e-2);
   /* set threads is critical */
   heurstatus = CPXsetintparam(subenv, CPXPARAM_Threads, 1);
   heurstatus = CPXsetdblparam(subenv, CPXPARAM_TimeLimit, 60);
}

void HeurData::GetModelInfo()
{
   rmatbeg = (int*)malloc(nrow*sizeof(int));
   rmatind = (int*)malloc(nnz*sizeof(int));
   rmatval = (double*)malloc(nnz*sizeof(double));
   cmatbeg = (int*)malloc(ncol*sizeof(int));
   cmatind = (int*)malloc(nnz*sizeof(int));
   cmatval = (double*)malloc(nnz*sizeof(double));
   rhs = (double*)malloc(nrow*sizeof(double));
   int surplus;
   int nnztmp;
   /* all parameters of CPXgetrows have to be non-NULL */
   heurstatus = CPXgetrows(subenv, submodel, &nnztmp, rmatbeg, rmatind, rmatval, nnz, &surplus, 0, nrow-1);
   if( heurstatus || surplus < 0 || nnztmp != nnz ) { PrintErr(); }
   /* all parameters of CPXgetcols have to be non-NULL */
   heurstatus = CPXgetcols(subenv, submodel, &nnztmp, cmatbeg, cmatind, cmatval, nnz, &surplus, 0, ncol-1);
   heurstatus = CPXgetrhs(subenv, submodel, rhs, 0, nrow-1);
   if( heurstatus || surplus < 0 || nnztmp != nnz ) { PrintErr(); }
}

int HeurData::FindClosestLink(int centrallink)
{
   int idx = -1;
   int distance = nlink+1;
   for( int i = 0; i < nlink; i++ )
   {
      if( !used[i] && linkdismat[centrallink][i] < distance )
      {
         idx = i;
         distance = linkdismat[centrallink][i];
      }
   }
   return idx;
}

void HeurData::RcvClassVarBnd(char vartype)
{
#if 0
   /* check */
   printf("== integer ==\n");
   for( int i = 0; i < nint; i++ )
      printf("type: %c, lb: %f, ub: %f, name: %s\n", ctype[intidx[i]], lb[intidx[i]], ub[intidx[i]], colname[intidx[i]]);
   printf("== continuous ==\n");
   for( int i = 0; i < ncont; i++ )
      printf("type: %c, lb: %f, ub: %f, name: %s\n", ctype[contidx[i]], lb[contidx[i]], ub[contidx[i]], colname[contidx[i]]);
#endif
   /* specifies the number of bound to change */
   int num;
   int* indices;
   double* lbd;
   double* ubd;
   switch( vartype )
   {
      case 'C':
         num = ncont;
         indices = contidx;
         lbd = contlb;
         ubd = contub;
         break;
      case 'B':
         num = nbin;
         indices = binidx;
         lbd = binlb;
         ubd = binub;
         break;
      case 'I':
         num = nint;
         indices = intidx;
         lbd = intlb;
         ubd = intub;
         break;
      default:
         PrintErr();
         return;
   }

   /* change variable bound of submodel */
   heurstatus = CPXchgbds(subenv, submodel, num, indices, L_sign, lbd);
   heurstatus = CPXchgbds(subenv, submodel, num, indices, U_sign, ubd);
}

HeurData::~HeurData()
{
   Free(ctype);
   Free(binidx);
   Free(intidx);
   Free(contidx);
   Free(incsol);
   Free(lb);
   Free(ub);
   Free(objcoef);
   Free(binlb);
   Free(binub);
   Free(intlb);
   Free(intub);
   Free(contlb);
   Free(contub);
   Free(L_sign);
   Free(U_sign);
   Free(B_sign);
   for( int i = 0; i < ncol; i++ )
      colname[i] = NULL;
   for( int i = 0; i < nlink; i++ )
      Free(linkdismat[i]);
   Free(linkdismat);
   Free(used);
   Free(colname);
   Free(colnamestore);
   Free(var2link);
   for( int i = 0; i < nlink; i++ )
      Free(link2var[i]);
   Free(link2var);
   Free(link2row);
   Free(row2link);
   Free(linksize);
   for( int j = 0; j < nrow; j++ )
      rowname[j] = NULL;
   Free(rmatbeg);
   Free(rmatind);
   Free(rmatval);
   Free(cmatbeg);
   Free(cmatind);
   Free(cmatval);
   Free(rhs);
   Free(rowname);
   Free(rownamestore);
   heurstatus = CPXfreeprob(subenv, &submodel);
   heurstatus = CPXcloseCPLEX(&subenv);
   if( heurmode >= 21 )
   {
      heurstatus = CPXfreeprob(lpenv, &lpmodel);
      heurstatus = CPXcloseCPLEX(&lpenv);
   }
}

void HeurData::RcvVarBnd(int* indices, int num)
{
   double* lbd = (double*)malloc(num*sizeof(double));
   double* ubd = (double*)malloc(num*sizeof(double));
   for( int i = 0; i < num; i++ )
   {
      lbd[i] = lb[indices[i]];
      ubd[i] = ub[indices[i]];
   }
   CPXchgbds(subenv, submodel, num, indices, L_sign, lbd);
   CPXchgbds(subenv, submodel, num, indices, U_sign, ubd);
   Free(lbd);
   Free(ubd);
}

void HeurData::ChgClassVarBnd(char vartype, char boundtype, double* bound)
{
   /* specifies the number of bound to change */
   int num;
   int* indices;
   switch( vartype )
   {
      case 'C':
         num = ncont;
         indices = contidx;
         break;
      case 'B':
         num = nbin;
         indices = binidx;
         break;
      case 'I':
         num = nint;
         indices = intidx;
         break;
      default:
         PrintErr();
         return;
   }
   double* bd = (double*)malloc(num*sizeof(double));
   for( int i = 0; i < num; i++ )
      bd[i] = bound[indices[i]];

   /* specifies the lower or upper bound */
   char* lu = NULL;
   switch( boundtype )
   {
      case 'L':
         lu = L_sign;
         break;
      case 'U':
         lu = U_sign;
         break;
      case 'B':
         lu = B_sign;
         break;
      default:
         PrintErr();
         return;
   }

   /* change variable bound of submodel */
   heurstatus = CPXchgbds(subenv, submodel, num, indices, lu, bd);
   Free(bd);
}

bool HeurData::SetCutoff(double _cutoff)
{
   cutoff = Min(cutoff, _cutoff);
   return true;
}

bool HeurData::Optimize(double* x, double* objval)
{
   //heurstatus = CPXsetdblparam(subenv, CPXPARAM_MIP_Tolerances_UpperCutoff, cutoff-Min(1e-2, (cutoff - *objval)/10.0));
   heurstatus = CPXsetdblparam(subenv, CPXPARAM_MIP_Tolerances_UpperCutoff, cutoff-100);
   heurstatus = CPXmipopt(subenv, submodel);
   if( CPXgetsolnpoolnumsolns(subenv, submodel) >= 1 )
   {
      heurstatus = CPXgetobjval(subenv, submodel, objval);
      heurstatus = CPXgetx(subenv, submodel, x, 0, ncol-1);
      return true;
   }
   return false;
}

void GetTime(timespec* time)
{
   clock_gettime(CLOCK_MONOTONIC, time);
}

double CalTime(timespec* start_time, timespec* end_time)
{
   return (end_time->tv_sec-start_time->tv_sec) + (end_time->tv_nsec-start_time->tv_nsec)/1e9;
}

int CPXPUBLIC Alone(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double* objval_p,
      double* x,
      int* checkfeas_p,
      int* useraction_p
      )
{
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;
   /* initialize useraction to indicate no user action taken */
   *useraction_p = CPX_CALLBACK_DEFAULT;

   Time start_time;
   GetTime(&start_time);

   int status = 0;
   double origobjval;
   double bestbound;
   double cutoff;
   bool success;
   /* get pointer */
   CPXCLPptr origmodel;
   /* get the original MIP problem model */
   status = CPXgetcallbacklp(origenv, cbdata, wherefrom, &origmodel);

   /* heuristic */
   /* the first method  */
   Time start_time1;
   GetTime(&start_time1);
   /* change variable bound of submodel */
   heurdata->ChgClassVarBnd('C', 'B', x);
   /* get the cutoff of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_CUTOFF, &cutoff);
   /* get the obj value of incumbent of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_INTEGER, &origobjval);
   /* get the best bound of original problem */
   CPXgetcallbackinfo(origenv, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_REMAINING, &bestbound);
   /* set cutoff of subproblem */
   heurdata->SetCutoff(cutoff);

   /* solve submodel */
   success = heurdata->Optimize(x, objval_p);
   heurdata->nexec1 += 1;
   /* recover the bounds of submodel */
   heurdata->RcvClassVarBnd('C');
#if 0
   double* llb = (double*)malloc(heurdata->ncol*sizeof(double));
   double* uub = (double*)malloc(heurdata->ncol*sizeof(double));
   CPXgetlb(heurdata->subenv, heurdata->submodel, llb, 0, heurdata->ncol-1);
   CPXgetub(heurdata->subenv, heurdata->submodel, uub, 0, heurdata->ncol-1);
   for( int i = 0; i < heurdata->ncol; i++ )
   {
      if( heurdata->ctype[i] == CPX_CONTINUOUS &&  llb[i] >= 0.1 )
         printf("llb: %f, uub: %f, lb: %f, ub: %f, name: %s\n", llb[i], uub[i], heurdata->lb[i], heurdata->ub[i], heurdata->colname[i]);
   }
   Free(llb);
   Free(uub);
#endif

   /* NOW - get a incumbent solution */
   if( success )
   {
      heurdata->nsucc1 += 1;
      if( origobjval >= 1e+10 )
         printf("=  C  = - Old  - INF -   New %10.2f\n", *objval_p);
      else
         printf("=  C  = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
      /* update cutoff of subproblem */
      heurdata->SetCutoff(*objval_p);
      /* tell CPLEX that a solution is being returned */
      *useraction_p = CPX_CALLBACK_SET;
      /* update the current incument in heurdata */
      heurdata->update_incsol = true;
      heurdata->incval = *objval_p;
      origobjval = *objval_p;
      for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
   }
   else
   {
      PrintErr();
   }

   Time end_time1;
   GetTime(&end_time1);
   heurdata->time1 += CalTime(&start_time1, &end_time1);

   /* the method 2 */
   //while( heurdata->nlink >= 1 && heurdata->update_incsol )
   while( true )
   {
      //printf("Enter Cycle-Based Neighborhood Search, incsol objval: %f\n", heurdata->incval);

      /* ========== Cycle-Based Neighborhood Search ========== */
      Time start_time2;
      GetTime(&start_time2);
      int range = 2;
      heurdata->update_incsol = false;
      bool stop = false;
      int centrallink = heurdata->GetBestLink();
      for( int i = 0; i < heurdata->nlink; i++ ) { heurdata->used[i] = false; }
      int linkidx;
      int nfail = 0;
      while( !stop )
      {
         linkidx = heurdata->FindClosestLink(centrallink);
         if( linkidx == -1 ) { break; }
         heurdata->nexec2 += 1;
         stop = !heurdata->NeiSearch(objval_p, x, linkidx, range);

         /* find a better iincumbent solution */
         if( !stop )
         {
            /* update the current incument in heurdata */
            if( heurdata->incval > *objval_p + 1e-2 )
            {
               heurdata->update_incsol = true;
               heurdata->incval = *objval_p;
               for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
               heurdata->nsucc2 += 1;
               printf("=  I  = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
               heurdata->SetCutoff(*objval_p);
               origobjval = *objval_p;
            }
            else { PrintErr(); }
         }

         if( stop ) { nfail++; } else { nfail = 0; } stop = false;
         if( nfail >= 10 ) { stop = true; }

         Time current_time;
         GetTime(&current_time);
         double cost_time = CalTime(&heurdata->init_time, &current_time);
         if( cost_time >= heurdata->timelimit )
            break;
      }
      Time end_time2;
      GetTime(&end_time2);
      heurdata->time2 += CalTime(&start_time2, &end_time2);

      // ----- Method 1 ----- //
      Time start_time1;
      GetTime(&start_time1);
      /* change variable bound of submodel */
      heurdata->ChgClassVarBnd('C', 'B', heurdata->incsol);

      /* solve submodel */
      success = heurdata->Optimize(x, objval_p);
      heurdata->nexec1 += 1;
      /* recover the bounds of submodel */
      heurdata->RcvClassVarBnd('C');

      /* update the current incument in heurdata */
      if( success )
      {
         /* update cutoff of subproblem */
         heurdata->SetCutoff(*objval_p);
         heurdata->nsucc1 += 1;
         printf("= C.I = - Old %10.2f New %10.2f\n", origobjval, *objval_p);
         heurdata->update_incsol = true;
         heurdata->incval = *objval_p;
         origobjval = *objval_p;
         for( int i = 0; i < heurdata->ncol; i++ ) { heurdata->incsol[i] = x[i]; }
      }

      Time end_time1;
      GetTime(&end_time1);
      heurdata->time1 += CalTime(&start_time1, &end_time1);
      // ----- Method 1 ----- //

      Time current_time;
      GetTime(&current_time);
      double cost_time = CalTime(&heurdata->init_time, &current_time);
      if( cost_time >= heurdata->timelimit )
         break;
   }
   /* check the solution for integer feasibility */
   *checkfeas_p = 1;

   Time end_time;
   GetTime(&end_time);
   heurdata->time += CalTime(&start_time, &end_time);

   return status;
};

int CPXPUBLIC LPBH1(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double* objval_p,
      double* x,
      int* checkfeas_p,
      int* useraction_p
      )
{
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;

   /* check the solution for integer feasibility */
   *checkfeas_p = 1;
   /* tell CPLEX that a solution is being returned */
   *useraction_p = CPX_CALLBACK_SET;

   Time start_time;
   GetTime(&start_time);

   int status = 0;
   double origobjval = 1e+20;
   *objval_p = 1e+20;
   /* get pointer */
   CPXCLPptr origmodel;
   /* get the original MIP problem model */
   status = CPXgetcallbacklp(origenv, cbdata, wherefrom, &origmodel);

   /* heuristic */
   Time start_time2;
   GetTime(&start_time2);
   double* tmpx = (double*)malloc(heurdata->ncol*sizeof(double));
   double* tmpobj = (double*)malloc(heurdata->nint*sizeof(double));
   for( int i = 0; i < heurdata->nint; i++ )
      tmpobj[i] = heurdata->objcoef[heurdata->intidx[i]];

   /* change variable type of lpmodel */
   char* c_symbol = (char*)malloc(heurdata->nint*sizeof(char));
   for( int i = 0; i < heurdata->nint; i++ )
      c_symbol[i] = CPX_CONTINUOUS;
   CPXchgctype(heurdata->lpenv, heurdata->lpmodel, heurdata->nint, heurdata->intidx, c_symbol);
   Free(c_symbol);

   int iter = 0;
   for( ; ; iter++ )
   {
      /* solve lpmodel */
      //heurdata->Optimize(tmpx, &heurdata->incval);
      CPXmipopt(heurdata->lpenv, heurdata->lpmodel);
      if( CPXgetsolnpoolnumsolns(heurdata->lpenv, heurdata->lpmodel) >= 1 )
      {
         CPXgetobjval(heurdata->lpenv, heurdata->lpmodel, &heurdata->incval);
         CPXgetx(heurdata->lpenv, heurdata->lpmodel, tmpx, 0, heurdata->ncol-1);
      }
      else { PrintErr(); }
      heurdata->nexec1 += 1;
      for( int i = 0; i < heurdata->nint; i++ )
      {
         if( tmpx[heurdata->intidx[i]] >= 1e-2 )
         {
            /* change objective function */
            double newobj = heurdata->objcoef[heurdata->intidx[i]] * ceil(tmpx[heurdata->intidx[i]]) / tmpx[heurdata->intidx[i]];
            CPXchgobj(heurdata->lpenv, heurdata->lpmodel, 1, &heurdata->intidx[i], &newobj);
            /* round up to get a incumbent solution */
            heurdata->incval -= tmpx[heurdata->intidx[i]] * tmpobj[i];
            heurdata->incval += ceil(tmpx[heurdata->intidx[i]]) * heurdata->objcoef[heurdata->intidx[i]];
            tmpx[heurdata->intidx[i]] = ceil(tmpx[heurdata->intidx[i]]);
            tmpobj[i] = newobj;
         }
      }

      if( *objval_p > heurdata->incval + 1e-2 )
      {
         heurdata->nsucc1 += 1;
         if( origobjval >= 1e+10 )
            printf("=  C  = - Old  - INF -   New %10.2f\n", heurdata->incval);
         else
            printf("=  C  = - Old %10.2f New %10.2f\n", *objval_p, heurdata->incval);
         /* update the current incument in heurdata */
         *objval_p = heurdata->incval;
         for( int i = 0; i < heurdata->ncol; i++ )
            x[i] = tmpx[i];
      }

      printf("last %10.2f next %10.2f\n", origobjval, heurdata->incval);
      //if( Abs(origobjval - heurdata->incval) <= Max(origobjval, heurdata->incval)*1e-5 ) { break; }
      origobjval = heurdata->incval;

      Time current_time;
      GetTime(&current_time);
      double cost_time = CalTime(&heurdata->init_time, &current_time);
      if( cost_time >= heurdata->timelimit )
         break;
   }
   printf("Number of iterations of slope scaling: %d\n\n", iter);

   Free(tmpx);
   Free(tmpobj);
   Time end_time2;
   GetTime(&end_time2);
   heurdata->time2 += CalTime(&start_time2, &end_time2);

   Time end_time;
   GetTime(&end_time);
   heurdata->time += CalTime(&start_time, &end_time);

   return status;
};

int CPXPUBLIC LPBH2(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double* objval_p,
      double* x,
      int* checkfeas_p,
      int* useraction_p
      )
{
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;

   /* check the solution for integer feasibility */
   *checkfeas_p = 1;
   /* tell CPLEX that a solution is being returned */
   *useraction_p = CPX_CALLBACK_SET;

   Time start_time;
   GetTime(&start_time);

   int status = 0;
   double origobjval = 1e+20;
   *objval_p = 1e+20;
   /* get pointer */
   CPXCLPptr origmodel;
   /* get the original MIP problem model */
   status = CPXgetcallbacklp(origenv, cbdata, wherefrom, &origmodel);

   /* heuristic */
   Time start_time2;
   GetTime(&start_time2);
   double* tmpx = (double*)malloc(heurdata->ncol*sizeof(double));
   double* tmpobj = (double*)malloc(heurdata->nint*sizeof(double));
   for( int i = 0; i < heurdata->nint; i++ )
      tmpobj[i] = heurdata->objcoef[heurdata->intidx[i]];

   /* change variable type of lpmodel */
   char* c_symbol = (char*)malloc(heurdata->nint*sizeof(char));
   for( int i = 0; i < heurdata->nint; i++ )
      c_symbol[i] = CPX_CONTINUOUS;
   CPXchgctype(heurdata->lpenv, heurdata->lpmodel, heurdata->nint, heurdata->intidx, c_symbol);
   Free(c_symbol);

   int iter = 0;
   for( ; ; iter++ )
   {
      /* solve lpmodel */
      //heurdata->Optimize(tmpx, &heurdata->incval);
      CPXmipopt(heurdata->lpenv, heurdata->lpmodel);
      if( CPXgetsolnpoolnumsolns(heurdata->lpenv, heurdata->lpmodel) >= 1 )
      {
         CPXgetobjval(heurdata->lpenv, heurdata->lpmodel, &heurdata->incval);
         CPXgetx(heurdata->lpenv, heurdata->lpmodel, tmpx, 0, heurdata->ncol-1);
      }
      else { PrintErr(); }
      heurdata->nexec1 += 1;
      for( int i = 0; i < heurdata->nint; i++ )
      {
         if( tmpx[heurdata->intidx[i]] >= 1e-2 )
         {
            /* change objective function */
            double newobj = heurdata->objcoef[heurdata->intidx[i]] * ceil(tmpx[heurdata->intidx[i]]) / tmpx[heurdata->intidx[i]];
            CPXchgobj(heurdata->lpenv, heurdata->lpmodel, 1, &heurdata->intidx[i], &newobj);
            /* solve knapsack problem get a incumbent solution */
            Time start_time1;
            GetTime(&start_time1);
            heurdata->ChgClassVarBnd('C', 'B', tmpx);
            CPXmipopt(heurdata->subenv, heurdata->submodel);
            if( CPXgetsolnpoolnumsolns(heurdata->subenv, heurdata->submodel) >= 1 )
            {
               CPXgetobjval(heurdata->subenv, heurdata->submodel, &heurdata->incval);
               CPXgetx(heurdata->subenv, heurdata->submodel, tmpx, 0, heurdata->ncol-1);
            }
            else { PrintErr(); }
            Time end_time1;
            GetTime(&end_time1);
            heurdata->time1 += CalTime(&start_time1, &end_time1);
            tmpobj[i] = newobj;
         }
      }

      if( *objval_p > heurdata->incval + 1e-2 )
      {
         heurdata->nsucc1 += 1;
         if( origobjval >= 1e+10 )
            printf("=  C  = - Old  - INF -   New %10.2f\n", heurdata->incval);
         else
            printf("=  C  = - Old %10.2f New %10.2f\n", *objval_p, heurdata->incval);
         /* update the current incument in heurdata */
         *objval_p = heurdata->incval;
         for( int i = 0; i < heurdata->ncol; i++ )
            x[i] = tmpx[i];
      }

      printf("last %10.2f next %10.2f\n", origobjval, heurdata->incval);
      //if( Abs(origobjval - heurdata->incval) <= Max(origobjval, heurdata->incval)*1e-5 ) { break; }
      origobjval = heurdata->incval;

      Time current_time;
      GetTime(&current_time);
      double cost_time = CalTime(&heurdata->init_time, &current_time);
      if( cost_time >= heurdata->timelimit )
         break;
   }
   printf("Number of iterations of slope scaling: %d\n\n", iter);

   Free(tmpx);
   Free(tmpobj);
   Time end_time2;
   GetTime(&end_time2);
   heurdata->time2 += CalTime(&start_time2, &end_time2);

   Time end_time;
   GetTime(&end_time);
   heurdata->time += CalTime(&start_time, &end_time);

   return status;
};

int CPXPUBLIC LPBH3(
      CPXCENVptr origenv,
      void* cbdata,
      int wherefrom,
      void* cbhandle,
      double* objval_p,
      double* x,
      int* checkfeas_p,
      int* useraction_p
      )
{
   /* type casting */
   HeurData* heurdata = (HeurData*)cbhandle;

   /* check the solution for integer feasibility */
   *checkfeas_p = 1;
   /* tell CPLEX that a solution is being returned */
   *useraction_p = CPX_CALLBACK_SET;

   Time start_time;
   GetTime(&start_time);

   int status = 0;
   double origobjval = 1e+20;
   *objval_p = 1e+20;
   /* get pointer */
   CPXCLPptr origmodel;
   /* get the original MIP problem model */
   status = CPXgetcallbacklp(origenv, cbdata, wherefrom, &origmodel);

   /* heuristic */
   Time start_time2;
   GetTime(&start_time2);
   double* tmpx = (double*)malloc(heurdata->ncol*sizeof(double));
   double* tmpobj = (double*)malloc(heurdata->nint*sizeof(double));
   for( int i = 0; i < heurdata->nint; i++ )
      tmpobj[i] = heurdata->objcoef[heurdata->intidx[i]];

   /* change variable type of lpmodel */
   char* c_symbol = (char*)malloc(heurdata->nint*sizeof(char));
   for( int i = 0; i < heurdata->nint; i++ )
      c_symbol[i] = CPX_CONTINUOUS;
   CPXchgctype(heurdata->lpenv, heurdata->lpmodel, heurdata->nint, heurdata->intidx, c_symbol);
   Free(c_symbol);

   for( int i = 0; i < heurdata->nlink; i++ )
      heurdata->used[i] = false;

   int iter = 0;
   double cost_time = 0;
   for( ; iter < 10; iter++ )
   {
      /* solve lpmodel */
      //heurdata->Optimize(tmpx, &heurdata->incval);
      CPXmipopt(heurdata->lpenv, heurdata->lpmodel);
      if( CPXgetsolnpoolnumsolns(heurdata->lpenv, heurdata->lpmodel) >= 1 )
      {
         CPXgetobjval(heurdata->lpenv, heurdata->lpmodel, &heurdata->incval);
         CPXgetx(heurdata->lpenv, heurdata->lpmodel, tmpx, 0, heurdata->ncol-1);
      }
      else { PrintErr(); }
      for( int i = 0; i < heurdata->nint; i++ )
      {
         if( tmpx[heurdata->intidx[i]] >= 1e-2 )
         {
            /* change objective function */
            double newobj = heurdata->objcoef[heurdata->intidx[i]] * ceil(tmpx[heurdata->intidx[i]]) / tmpx[heurdata->intidx[i]];
            CPXchgobj(heurdata->lpenv, heurdata->lpmodel, 1, &heurdata->intidx[i], &newobj);
            /* round up to get a incumbent solution */
            heurdata->incval -= tmpx[heurdata->intidx[i]] * tmpobj[i];
            heurdata->incval += ceil(tmpx[heurdata->intidx[i]]) * heurdata->objcoef[heurdata->intidx[i]];
            tmpobj[i] = newobj;

            heurdata->used[heurdata->var2link[heurdata->intidx[i]]] = true;
         }
      }

      if( Abs(origobjval - heurdata->incval) <= Max(origobjval, heurdata->incval)*1e-5 )
         break;
      else
         origobjval = heurdata->incval;

      Time current_time;
      GetTime(&current_time);
      cost_time = CalTime(&heurdata->init_time, &current_time);
      if( cost_time >= heurdata->timelimit / 2.0 )
         break;
   }
   printf("Number of iterations of slope scaling: %d\n\n", iter);

   // -----
   /* solve restricted NDP get a incumbent solution */
   heurdata->nexec1 = 1;
   for( int i = 0; i < heurdata->nlink; i++ )
   {
      if( !heurdata->used[i] )
      {
         for( int j = 0; j < heurdata->linksize[i]; j++ )
         {
            char sign = 'U';
            double ub = 0.0;
            CPXchgbds(heurdata->subenv, heurdata->submodel, 1, &heurdata->link2var[i][j], &sign, &ub);
         }
      }
   }
   CPXsetdblparam(heurdata->subenv, CPXPARAM_TimeLimit, Max(60.0, heurdata->timelimit - cost_time + 1));
   CPXmipopt(heurdata->subenv, heurdata->submodel);
   if( CPXgetsolnpoolnumsolns(heurdata->subenv, heurdata->submodel) >= 1 )
   {
      CPXgetobjval(heurdata->subenv, heurdata->submodel, objval_p);
      CPXgetx(heurdata->subenv, heurdata->submodel, x, 0, heurdata->ncol-1);
      heurdata->nsucc1 = 1;
   }
   else { *useraction_p = CPX_CALLBACK_DEFAULT; }
   // -----

   Free(tmpx);
   Free(tmpobj);
   Time end_time2;
   GetTime(&end_time2);
   heurdata->time2 += CalTime(&start_time2, &end_time2);

   Time end_time;
   GetTime(&end_time);
   heurdata->time += CalTime(&start_time, &end_time);

   return status;
};
