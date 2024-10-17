# solver name
BEGIN {
   solver = "SCIP";
}
# solver version
/^SCIP version/ {
   solverversion = $3;
}
# branch and bound nodes
/^  nodes \(total\)    :/ {
   bbnodes = $4;
}
# dual and primal bound
/^  Primal Bound     :/ {
   if( $4 == "infeasible" )
   {
      pb = +infty;
      db = +infty;
   }
   else if( $4 == "-" )
      pb = +infty;
   else
      pb = $4;
}
/^  Dual Bound       :/ {
   if( $4 != "-" )
      db = $4;
}
# solving status
/^SCIP Status        :/ {
   abort = 0;
}
/solving was interrupted/ {
   timlim = 1;
}
# gap limit reached are not considered stopped
/solving was interrupted \[gap limit reached\]/ {
   timlim = 0;
}
/solving was interrupted \[memory limit reached\]/ {
   timlim = 0;
   memlim = 1;
}
/problem is solved/ {
   timlim = 0;
   memlim = 0;
}
/Solving Time \(sec\) :/ {
   time = $5;
}
/^  presolving       :/ {
   presoltime = $3;
}

## my presolving method multiagg
/^  multiagg         :/ {
   myptime = $3;
}

/^  primal LP        :/ {
   primallpiter = $6;
}
/^  dual LP          :/ {
   duallpiter = $6;
   duallpiterpersec = $8;
   duallptime = $4;
   duallpiterpercall = $7;
}
/^original problem has/ {
   ovar = $4;
   ocon = $15;
}
/^  Variables        :/ {
   tvar = $3;
}
/^  Constraints      :/ {
   tcon = $3;
}
/^  Nonzeros         :/ {
   tnnz = $3;
}
/^MultiAggregation:/ {
   myexec = $7;
}
/^  First LP value   :/ {
   firstlpvalue = $5;
}
/^  First LP Time    :/ {
   firstlptime = $5;
}
/^  First LP Iters   :/ {
   firstlpiter = $5;
   split($6, v, "(");
         firstlpspeed = v[2];
}
