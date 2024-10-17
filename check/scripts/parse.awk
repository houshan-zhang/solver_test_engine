# status
# 1) abort              program interrupt
# 2) timlim             time limit reached
# 3) memlim             memory limit reached
# 4) ok                 solver thinks the minimum gap has been reached

# solstatus
# 1) error              program interrupt
# 2) --                 no solution(may be infeasible problem)
# 3) ok                 exist correct solution
# 4) mismatch           exist incorrect solution

BEGIN {
   cmpseed = 0;
   if( CMPSEED == 1 )
      cmpseed = 1;

   infty = +1e+20;
   eps = 1e-04;
   largegap = 1e+04;
   reltol = 1e-06;

   # initialize summary data
   ninstance = 0;
   nsolved = 0;
   ntimlim = 0;
   nmemlim = 0;
   nfailed = 0;

   # initialize data to be set in parse_<solver>.awk
   solver = "?";
   solverversion = "?";

   # init avetime and avenode
   shift = 1;
   shifttime = 1;
   shiftnodes = 1;
   geoavetime = 1;
   geoavenodes = 1;
   geoub = 1;
   geolb = 1;
   geogap = 1;

   # print title
   if( cmpseed )
   {
      printf("----------------------------+-------+-------+--------------+--------------+-------+---------+--------+--------+---------\n");
   }
   else
   {
      printf("----------------------------+-------+-------+--------------+--------------+-------+---------+--------+--------+---------\n");
      printf("Name                        | Ncons | Nvars |  Dual Bound  | Primal Bound |  Gap%% |  Nodes  |  Time  | Status | Solution \n");
      printf("----------------------------+-------+-------+--------------+--------------+-------+---------+--------+--------+---------\n");
   }
}

function abs(x)
{
   return x < 0 ? -x : x;
}
function min(x,y)
{
   return (x) < (y) ? (x) : (y);
}
function max(x,y)
{
   return (x) > (y) ? (x) : (y);
}

## parse solution status from solution file (optional)
/=opt=/  { knownsolstatus[$2] = "opt"; knownsolvalue[$2] = $3; }     ## get the optimum solution ccalue
/=best=/ { knownsolstatus[$2] = "best"; knownsolvalue[$2] = $3; }    ## get the best known solution value
/=inf=/  { knownsolstatus[$2] = "inf"; }                             ## problem infeasible (no feasible solution exists)
/=unbd=/ { knownsolstatus[$2] = "unbd"; }                            ## knowing the problem is unbound
/=unkn=/ { knownsolstatus[$2] = "unkn"; }                            ## knowing nothing

# name of each instance
/^@01/ {
   n  = split ($3, a, "/");
   m = split(a[n], b, ".");
   prob = b[1];
   while( b[m] == "gz," || b[m] == "mps," )
      m--;
   for( i = 2; i < m; i++ )
      prob = prob "." b[i];

   n = split(prob, b, "#");
   prob = b[1];
   seed = $5;

   # initialize data to be set in parse.awk of each instance
   maxtim = 0;
   maxgap = 0;
   time = 0.0;

   # initialize data to be set parse_<solver>.awk of each instance
   bbnodes = 0;
   pb = +infty;
   db = -infty;
   abort = 1;
   timlim = 0;
   memlim = 0;
   solstatus = "error";
}
# time
/@04/ { maxtim = $3; }
# gap
/@06/ {
   maxgap = $3;
   if( maxgap > 0 )
      reltol = maxgap;
}

# scip can write a .sol file even if the problem is infeasible
# the content of .sol file are as follows
#
#     solution status: infeasible
#     no solution available
#
# but cplex can not, it just threw an error "CPLEX Error  1217: No solution exists."

# solution status
/Read SOL:/ {
   solstatus = "--";
}
/solution file is empty/ {
   solstatus = "--";
}
/Check SOL:/ {
   intcheck = $4;
   conscheck = $6;
   objcheck = $8;
   if( intcheck && conscheck && objcheck )
      solstatus = "ok";
   else
      solstatus = "mismatch";
}
/^= over =/ {
   # determine solving status
   if( time >= maxtim )
      timlim = 1;
   if( abort )
      status = "abort";
   else if( timlim )
      status = "timlim";
   else if( memlim )
      status = "memlim";
   else
      status = "ok";

   # fix solstatus w.r.t. consistency to solution file (optional)
   if( knownsolstatus[prob] != "" && solstatus != "error" && solstatus != "mismatch" )
   {
      knownsolstat = knownsolstatus[prob];
      knownsol =  knownsolvalue[prob];
      abstol = LINTOL;

      # check if solver claims infeasible (NOTE currently only working with MINIMIZATION)
      infeasible = ( pb >= infty && db >= infty );

      # We check different know solution status.
      #
      # 1) in case of knowing an optimal solution, we check if the solution      # =opt=
      #    reported by the solver in consistent with tolerances
      # 2) in case of knowing an best solution, we check if the solution         # =best=
      #    reported by the solver is less or equal than it
      # 3) in case of knowing infeasibility, we check if the solution            # =inf=
      #    exist
      # 4) in case of knowing problem unbound                                    # =unbd=
      #     nothing to do
      # 5) in case of knowing nothing no consistency check                       # =unkn=

      ### check feasibility ###
      if( knownsolstat == "opt" || knownsolstat == "best" )
      {
         if( infeasible )
         {
            # solver claimed infeasible, but we know a solution
            solstatus = "mismatch";
         }
      }
      else if( knownsolstat == "inf" )
      {
         if( solstatus != "--" )
         {
            # solver claimed feasible, but we know a infeasible
            solstatus = "mismatch";
         }
      }
      ### check primal bound ###
      if( knownsolstat == "opt" || knownsolstat == "best" )
      {
         # the minimum gap has been reached
         if( !timlim && !memlim && !abort )
         {
            # check absolute error
            if( abs(knownsol) <= 100 )
            {
               if( pb - knownsol > abstol )
                  solstatus = "mismatch";
            }
            # check relative error
            else
            {
               if( (pb - knownsol)/abs(knownsol) > reltol )
                  solstatus = "mismatch";
            }
         }
      }
   }

   # determine overall status from solving status and solution status:
   # instance solved correctly (including case that no solution was found)
   if( status == "ok" && (solstatus == "ok" || solstatus == "--") )
   {
      geoavetime = (time+shifttime)^(1/(ninstance+1)) * geoavetime^( (ninstance)/(ninstance+1));
      geoavenodes = (bbnodes+shiftnodes)^(1/(ninstance+1)) * geoavenodes^( (ninstance)/(ninstance+1));
      nsolved++;
   }
   # incorrect solving process or infeasible solution (including errors with solution checker)
   else if( status == "abort" || (solstatus == "error" || solstatus == "mismatch") )
   {
      geoavetime = (maxtim+shifttime)^(1/(ninstance+1)) * geoavetime^( (ninstance)/(ninstance+1));
      geoavenodes = (bbnodes+shiftnodes)^(1/(ninstance+1)) * geoavenodes^( (ninstance)/(ninstance+1));
      nfailed++;
   }
   # stopped due to imposed limits
   else if( status == "memlim" )
   {
      geoavetime = (maxtim+shifttime)^(1/(ninstance+1)) * geoavetime^( (ninstance)/(ninstance+1));
      geoavenodes = (bbnodes+shiftnodes)^(1/(ninstance+1)) * geoavenodes^( (ninstance)/(ninstance+1));
      nmemlim++;
   }
   else
   {
      geoavetime = (maxtim+shifttime)^(1/(ninstance+1)) * geoavetime^( (ninstance)/(ninstance+1));
      geoavenodes = (bbnodes+shiftnodes)^(1/(ninstance+1)) * geoavenodes^( (ninstance)/(ninstance+1));
      ntimlim++;
   }
   ninstance++;

   # compute gap
   temp = pb;
   pb = 1.0*temp;
   temp = db;
   db = 1.0*temp;

   geoub = (pb+shift)^(1/(ninstance)) * geoub^( (ninstance-1)/(ninstance));
   geolb = (db+shift)^(1/(ninstance)) * geolb^( (ninstance-1)/(ninstance));

   # we follow the gap definition in article "Progress in Presolving for Mixed Integer Programming"
   if( abs(pb - db) < eps && pb < +infty )
   {
      gap = 0.0;
      geogap = (gap+shift)^(1/(ninstance)) * geogap^( (ninstance-1)/(ninstance));
   }
   else if( abs(db) < eps || abs(pb) < eps)
      gap = -1.0;
   else if( pb*db < 0.0 )
      gap = -1.0;
   else if( abs(db) >= +infty || abs(pb) >= +infty )
      gap = -1.0;
   else
   {
      gap = 100.0*abs((pb-db)/max(abs(db), abs(pb)));
      geogap = (gap+shift)^(1/(ninstance)) * geogap^( (ninstance-1)/(ninstance));
   }

   if( gap < 0.0 )
      gapstr = "    --";
   else if( gap < largegap )
      gapstr = sprintf("%6.2f", gap);
   else
      gapstr = " Large";
   if( cmpseed )
   {
      #problem name
      prob = prob "[" seed "]";
      printf("%-28s ", prob);
      #dual bound
      printf("%14.8g ", db);
      #primal bound
      printf("%14.8g ", pb);
      #gap
      printf("%7s ", gapstr);
      #nodes
      printf("%9d ", bbnodes);
      #solving time
      printf("%8.2f ", time);
      #status
      printf("%8s ", status);
      #solution status
      printf("%9s ", solstatus);
      #number of origproblem's constraints
      printf("%7d ", ocon);
      #number of origproblem's variables
      printf("%7d ", ovar);
      #number of transproblem's constraints
      printf("%7d ", tcon);
      #number of transproblem's variables
      printf("%7d ", tvar);
      #number of transproblem's nnonzeros
      printf("%10d ", tnnz);
      #total presolve time
      printf("%5.2f ", presoltime);
      #my presolve time
      printf("%5.2f ", myptime);
      #first lp time
      printf("%5.2f ", firstlptime);
      #num of first lp iteration
      printf("%7d ", firstlpiter);
      #first lp iteration speed
      printf("%10.2f ", firstlpspeed);
      #first lp value
      printf("%10.2f ", firstlpvalue);
      #total num of lp iteration
      printf("%7d ", primallpiter + duallpiter)
      #my plugin output
      printf("%7d", myexec);
      printf("\n");
   }
   else
      printf("%-28s %7d %7d %14.8g %14.8g %7s %9d %8.2f %8s %9s\n", prob, ocon, ovar, db, pb, gapstr, bbnodes, time, status, solstatus);
}
END {
   printf("----------------------------+-------+-------+--------------+--------------+-------+---------+--------+--------+---------\n");
   printf("nsolved/ntimlim/nfailed/nmemlim: %d/%d/%d/%d\n", nsolved, ntimlim, nfailed, nmemlim);
   printf("geotime: %f\n", geoavetime-shifttime);
   printf("geonodes: %f\n", geoavenodes-shiftnodes);
   printf("\n");
   printf("geoub: %f\n", geoub-shift);
   printf("geolb: %f\n", geolb-shift);
   printf("geogap: %f\n", geogap-shift);
   printf("\n");
   printf("maxtim: %g\n", maxtim);
   printf("%s(%s)\n", solver, solverversion);
}
