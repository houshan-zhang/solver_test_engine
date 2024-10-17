# solver version
/^CPX Callback NDP/ {
   solver = "CPX Callback NDP";
   solverversion = $4;
}
/^CPX Return Code: 0/ {
   abort = 0;
}
/Total \(root\+branch&cut\)/ {
   time = $4;
}
/CPX Callback Results:/ {
   pb = $5;
   db = $7;
   bbnodes = $9;
}
/Problem Size/ {
   ocon = $4;
   ovar = $6;
}
