# solver name
#TODO check
BEGIN {
   solver = "CPLEX";
   solver_objsense = 1;
   solver_type = "MIP";
}

# dual and primal bound, and solving status
/^MIP - / { solver_type = $1; $0 = substr($0, 7, length($0)-6); }
/^Barrier - / { solver_type = $1; $0 = substr($0, 11, length($0)-10); }
/^Primal simplex - / { solver_type = $1; $0 = substr($0, 18, length($0)-17); }
/^Dual simplex - / { solver_type = $1; $0 = substr($0, 16, length($0)-15); }
/^Populate - / { solver_type = "MIP"; $0 = substr($0, 12, length($0)-11); }
/^Presolve - / { solver_type = $1; $0 = substr($0, 12, length($0)-11); }

# solver version
/^Welcome to.* Interactive Optimizer/ {
   solverversion = $NF;
}

# objective sense
/^Problem is a minimization problem./ {
   solver_objsense = 1;
}
/^Problem is a maximization problem./ {
   solver_objsense = -1;
}

# branch and bound nodes
/^Solution time/ {
   bbnodes = $11;
   abort = 0;
}

/^MIP - Error termination/ {
   timlim = 1;
}

/^Integer / {
   if ($2 ~ /infeasible/ )
   {
      db = solver_objsense * infty;
      pb = solver_objsense * infty;
   }
   else
   {
      db = $NF;
      pb = $NF;
   }
}
/MIP - Integer optimal solution:  Objective = / {
   pb = $NF;
   db = $NF;
}
/^Optimal:  Objective = / {
   pb = $NF;
   db = $NF;
}
/^Non-optimal:  Objective = / {
   pb = $NF;
   db = $NF;
}
/^Unbounded or infeasible./ {
   db = solver_objsense * infty;
   pb = solver_objsense * infty;
}
/^Infeasible/ {
   db = solver_objsense * infty;
   pb = solver_objsense * infty;
}
/^Unbounded/ {
   db = solver_objsense * infty;
   pb = solver_objsense * infty;
}
/^Dual objective limit exceeded/ {
   db = solver_objsense * infty;
   pb = solver_objsense * infty;
}
/^Primal objective limit exceeded/ {
   db = solver_objsense * infty;
   pb = solver_objsense * infty;
}
/^Solution limit exceeded/ {
   pb = $NF;
}
/^Time limit exceeded/ {
   pb = solver_objsense * infty;
   if ( solver_type == "MIP" && $4 != "no" )
      pb = $8;
   timlim = 1;
}
/^Integer optimal, tolerance/ {
   pb = $7;
}
/^Memory limit exceeded/ {
   pb = solver_objsense * infty;
   if ( solver_type == "MIP" && $4 != "no" )
      pb = $8;
   memlim = 1;
}
/^Node limit exceeded/ {
   pb = ($4 == "no") ? solver_objsense * infty : $8;
   timlim = 1;
}
/^Tree / {
   pb = ($4 == "no") ? solver_objsense * infty : $8;
   timlim = 1;
}
/^Aborted, / {
   pb = solver_objsense * infty;
   if ( solver_type == "MIP" && $2 != "no" )
      pb = $6;
   abort = 1;
}
/^Error / {
   pb = solver_objsense * infty;
   if ( solver_type == "MIP" && $3 != "no" )
      pb = $7;
   abort = 1;
}
/^Unknown status / {
   pb = solver_objsense * infty;
   if ( solver_type == "MIP" && $4 == "Objective" )
      pb = $6;
}
/^All reachable solutions enumerated, integer / {
   if ($6 ~ /infeasible/ )
   {
      db = solver_objsense * infty;
      pb = solver_objsense * infty;
   }
   else
   {
      db = $NF;
      pb = $NF;
   }
}
/^Populate solution limit exceeded/ {
   pb = $NF;
}
/^Current MIP best bound =/ {
   db = $6;
}
/^CPLEX Error  1001: Out of memory./ {
   memlim = 1;
}
/Total \(root\+branch&cut\)/ {
   time = $4;
}
/^Variables            :.*\[/ {
   ovar = $3;
   }
/^Linear constraints   :.*\[/ {
   ocon = $4;
}
