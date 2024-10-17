BEGIN {
   solver = "Gurobi";
}
# solver version
/^Gurobi Optimizer version/ {
   solverversion = $4;
}
# branch and bound nodes
/^Explored/ {
   bbnodes = $2;
}
# infeasible model
/^Model is infeasible/ {
   db = pb;
}
# dual and primal bound
/^Best objective/ {
   if( $3 != "-," )
      pb = $3 + 0.0;
   if( $6 != "-," )
      db = $6 + 0.0;
}
# solving status
/^Explored/ {
   abort = 0;
}
/^Solved/ {
   abort = 0;
}
/^Time limit reached/ {
   timlim = 1;
}
/^Solve interrupted/ {
   timlim = 1;
}
/^Optimal solution found/ {
   timlim = 0;
}
