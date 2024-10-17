BEGIN{
   # set print level
   # 0    : print overall table of all instances
   # 1    : print each instances
   # 2    : performance profile graph (for software origin) (multiple files can be processed)
   # 3    : end gap graph (for software origin) (multiple files can be processed)
   # 4    : print summary table of affected instances
   # 5    : print instance-wise presolve compare table

   ########## input parameters ##########
   # print level
   printlevel = 0;
   if( LEVEL > 0 )
      printlevel = LEVEL;
   # print latex
   printlatex = 0;
   if( LATEX == 1 )
      printlatex = LATEX;
   # whether to print non-affected problems?
   printnonaff = 0;
   if( NONAFF == 1 )
      printnonaff = NONAFF;
   # whether to print error problems?
   printerror = 0;
   if( ERROR == 1 )
      printerror = ERROR;
   # reduction is used to strictly determine whether an instance is affected or not
   affaccurate = 0;
   if( AA == 1 )
      affaccurate = AA;
   # compare the results under different seeds with the same other parameters
   compdiffseed = 0;
   if( COMPDIFFSEED == 1 )
      compdiffseed = COMPDIFFSEED;
   # set time limit
   maxtime = 7200;
   if( TIME > 0 )
      maxtime = TIME;
   # set compare ratio
   cmp = 0.1;
   if( CMP > 0 )
      cmp = CMP;
   # set compare ratio for affected instances
   cmpaff = 0.1;
   if( CMPAFF > 0 )
      cmpaff = CMPAFF;
   # default shift
   shift = 1;
   if( SHIFT > 0 )
      shift = SHIFT;
   # set time shift
   timeshift = 1;
   if( TIMESHIFT > 0 )
      timeshift = TIMESHIFT;
   # set node shift
   nodeshift = 1;
   if( NODESHIFT > 0 )
      nodeshift = NODESHIFT;
   ######################################

   # init the num of problem
   probnum = 0;
   filenum = 0;
   # init number of problem (1) all file correct
   ncor = 0;
}

function max(x,y)
{
   return (x) > (y) ? (x) : (y);
}

function min(x,y)
{
   return (x) < (y) ? (x) : (y);
}

function floor(x)
{
   return int(x);
}

function round(x)
{
   return int(x+0.5);
}

function ceil(x)
{
   return int(x) + ((x%1)!=0);
}

function abs(x)
{
   return (x >= 0 ? x : -x);
}

# define correctly calculated problem "cor", affected problem "aff", and get optimal solstatus's problem "opt"
# (B) cor[x] == 1 means that all results of problem x is exist and correctly calculated
# (I) opt[x] == n means that n file contain optimal value,(if cor[x] equal 0, opt[x] must be 0)
# (B) aff[x] == 1 means that either (i) exist one correct and wrong result or (ii) all results correct (at least one in timelimit) and their computing outcome different
# (B) lps[x] == 1 means that problem x is lp-solvable (at least one file)
# their computing outcome different means that only one get optimal or their computing process different
# cor[x] == 1 && aff[x] == 1 means that all results of problem x is correctly calculated and their computing outcome different
# opt[x] == n && aff[x] == 1 means that n results of problem x get optimal and exist one different solving process
function Init(aff, cor, opt)
{
   ncor = probnum;
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      # affected
      aff[prob] = 0;
      # correct
      cor[prob] = 1;
      # optimal
      opt[prob] = 0;
      lps[prob] = 0;
      nfilesolved = 0;
      nfilecorrect = 2;
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         # file is not exit
         if( probfile[prob","file ] != 1)
         {
            printf("Warning: Missing Data! %s\n", prob);
            nfilesolved = 0;
            nfilecorrect = 0;
            break;
         }
         # memlim is considered as abort
         if( status[prob","file] == "abort" || status[prob","file] == "memlim" || solstatus[prob","file] == "mismatch" || solstatus[prob","file] == "error" )
         {
            nfilecorrect--;
            continue;
         }
         if( status[prob","file] == "ok" )
         {
            nfilesolved++;
            opt[prob]++;
         }
      }
      # do not count incorrect instances
      if( nfilecorrect == 0 )
      {
         aff[prob] = 0;
         cor[prob] = 0;
         ncor -= 1;
         opt[prob] = 0;
         lps[prob] = 0;
      }
      else if( nfilecorrect == 1 )
      {
         aff[prob] = 1;
         cor[prob] = 0;
         ncor -= 1;
         opt[prob] = 0;
         lps[prob] = 0;
      }
      else
      {
         cor[prob] = 1;
         for( j = 0; j < filenum; j++ )
         {
            if( flptime[prob","filelist[j]] < maxtime )
            {
               lps[prob] = 1;
               break;
            }
         }
         if( affaccurate == 0 )
         {
            if( nfilesolved >= 1 && nfilesolved < filenum )
            {
               aff[prob] = 1;
            }
            else
            {
               for( j = 1; j < filenum; j++ )
               {
                  # we focus the whole MIP problem
                  if( (node[prob","filelist[0]] != node[prob","filelist[j]] || totlpiter[prob","filelist[0]] != totlpiter[prob","filelist[j]]) \
                          && (time[prob","filelist[0]] < maxtime || time[prob","filelist[j]] < maxtime) )
                  {
                     aff[prob] = 1;
                     break;
                  }
               }
            }
         }
         else
         {
            # use our own output as a criterion for aff
            for( j = 0; j < filenum; j++ )
            {
               if( myexec[prob","filelist[j]] != 0 )
               {
                  aff[prob] = 1;
                  break;
               }
            }
         }
      }
   }
}

function AddName(name, list, str, number)
{
   if( name[str] != 1)
   {
      name[str] = 1;
      list[number] = str;
      number++;
   }
   return number;
}

# print the title of afftcted instances
function EachInstanceTitle()
{
   # print filename
   printf(" %-15s | ","Name");
   for( j = 0; j < filenum; j++ )
   {
      printf("%-34.34s | ", filelist[j]);
   }
   printf("\n");

   # print nsolved, time, node
   printf("%-15.15s  | "," ");
   for( j = 0; j < filenum; j++ )
   {
      printf("%-12.12s ", "time");
      printf("%-12.12s ", "node");
      printf("%-8.8s ", "gap");
      if( j > 0 )
      {
         printf("| %-6.6s ", "timp");
         printf("%-8.8s ", "nimp");
      }
      printf("| ");
   }
   printf("\n");
}

# print information about each instance in each method, including node, time and gap
function EachInstance()
{
   EachInstanceTitle()
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      if( printerror == 0 && cor[prob] == 0 )
         continue;
      if( printnonaff == 0 && aff[prob] == 0 )
         continue;
      if( aff[prob] )
         printf("*");
      else
         printf(" ");
      printf("%-15.15s | ", prob);
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         printf("%-12.2f ", time[prob","file]);
         printf("%-12d ", node[prob","file]);
         printf("%-8s ", gap[prob","file]);
         if( j>=1 )
         {
            if(time[prob","filelist[0]] > 0)
               printf("| %-6.2f ", time[prob","file]/time[prob","filelist[0]]);
            else
               printf("%-6s ", " -- ");
            if(node[prob","filelist[0]] > 0)
               printf("%-8.2f ", node[prob","file]/node[prob","filelist[0]]);
            else
               printf("%-8s ", " -- ");
         }
         printf("| ");
      }
      printf("\n");
   }
}

function PerformanceProfileGraph()
{
   #--------------------
   # set time range(s)
   timemin = 0;
   timemax = 7200;
   #--------------------
   # time print interval
   xspace = 100;
   # time vector length
   xlen = (timemax-timemin)/xspace + 1;
   # init
   for( i = 0; i < filenum; i++ )
   {
      for( j = 0; j < xlen; j++ )
      {
         # file i in time (timemax+xspace*j) can solve y[i,j] instances
         y[i","j] = 0;
      }
   }
   for( k = 0; k < probnum; k++ )
   {
      prob = problist[k];
      if( cor[prob] == 0 )
         continue;
      for( i = 0; i < filenum; i++ )
      {
         file = filelist[i];
         fillstart = max(0, ceil((time[prob","file]-timemin)/xspace));
         if( time[prob","file] >= maxtime )
            fillstart = xlen;
         for( j = fillstart; j < xlen; j++ )
            y[i","j] += 1;
      }
   }
   # print
   for( j = 0; j < xlen; j++ )
   {
      printf("%-10.5f", timemin + xspace*j);
      for( i = 0; i < filenum; i++ )
         printf(" \t%-4.2f", 100*y[i","j]/ncor);
      printf("\n");
   }
}

function EndGapGraph()
{
   #--------------------
   # set gap range(%)
   gapmin = 1;
   gapmax = 100;
   #--------------------
   # gap print interval
   xspace = 0.1;
   # gap vector length
   xlen = (gapmax-gapmin)/xspace + 1;
   # init
   for( i = 0; i < filenum; i++ )
   {
      for( j = 0; j < xlen; j++ )
      {
         # y[i,j] instances' end gap is less or equal than (gapmin+xspace*j) in file i
         y[i","j] = 0;
      }
   }
   for( k = 0; k < probnum; k++ )
   {
      prob = problist[k];
      if( cor[prob] == 0 )
         continue;
      for( i = 0; i < filenum; i++ )
      {
         file = filelist[i];
         if( gap[prob","file] != "--" && gap[prob","file] != "Large")
            fillstart = max(0, ceil((gap[prob","file]-gapmin)/xspace));
         else
            fillstart = xlen;
         for( j = fillstart; j < xlen; j++ )
            y[i","j] += 1;
      }
   }
   # print
   for( j = 0; j < xlen; j++ )
   {
      printf("%-10.5f", gapmin + xspace*j);
      for( i = 0; i < filenum; i++ )
         printf(" \t %-4.2f", 100*y[i","j]/ncor);
      printf("\n");
   }
}

function PresolveInstanceCompare()
{
   idx = -1;
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      if( printerror == 0 && cor[prob] == 0 )
         continue;
      if( printnonaff == 0 && aff[prob] == 0 )
         continue;
      idx += 1;
      split(prob, temp, "[");
      originname[idx] = temp[1];
      if( idx == 0 )
         printf("%-25s & ", originname[idx]);
      else if( idx >= 1 && originname[idx] == originname[idx-1])
         printf(" & ");
      else
      {
         printf("\\hline\n");
         printf("%-25s & ", originname[idx]);
      }
      for( j = filenum-1; j >= 0; j-- )
      {
         file = filelist[j];
         if ( status[prob","file] == "abort" || solstatus[prob","file] == "mismatch" || solstatus[prob","file] == "error" )
            print("error & ")
         else if( time[prob","file] >= 7200 )
            print("limit & ")
         else
            printf("%12.2f & ", time[prob","file]);
         if ( status[prob","file] == "abort" || solstatus[prob","file] == "mismatch" || solstatus[prob","file] == "error" )
            printf("%12s & ", "error");
         else
            printf("%12d & ", node[prob","file]);
         if( j==0 )
         {
            if( time[prob","filelist[1]] > 0 )
            {
               if( cor[prob] == 0 )
                  printf("%6s & ", " -- ");
               else if( time[prob","file]/time[prob","filelist[1]] < 1 - cmpaff )
                  printf("\\textcolor{blue}{%6.2f} & ", time[prob","file]/time[prob","filelist[1]]);
               else if( time[prob","file]/time[prob","filelist[1]] > 1 + cmpaff)
                  printf("\\textcolor{red}{%6.2f} & ", time[prob","file]/time[prob","filelist[1]]);
               else
                  printf("%6.2f & ", time[prob","file]/time[prob","filelist[1]]);
            }
            else
               printf("%6s & ", " -- ");
            if( node[prob","filelist[1]] > 0 )
            {
               if( cor[prob] == 0 )
                  printf("%6s & ", " -- ");
               else if( node[prob","file]/node[prob","filelist[1]] < 1 - cmpaff )
                  printf("\\textcolor{blue}{%8.2f} & ", node[prob","file]/node[prob","filelist[1]]);
               else if( node[prob","file]/node[prob","filelist[1]] > 1 + cmpaff)
                  printf("\\textcolor{red}{%8.2f} & ", node[prob","file]/node[prob","filelist[1]]);
               else
                  printf("%8.2f & ", node[prob","file]/node[prob","filelist[1]]);
            }
            else
               printf("%8s & ", " -- ");
            if( tcon[prob","filelist[1]] > 0 )
            {
               if( tcon[prob","file]/tcon[prob","filelist[1]] < 1 - cmpaff )
                  printf("\\textcolor{blue}{%4.2f} & ", tcon[prob","file]/tcon[prob","filelist[1]]);
               else if( tcon[prob","file]/tcon[prob","filelist[1]] > 1 + cmpaff )
                  printf("\\textcolor{red}{%4.2f} & ", tcon[prob","file]/tcon[prob","filelist[1]]);
               else
                  printf("%4.2f & ", tcon[prob","file]/tcon[prob","filelist[1]]);
            }
            else
               printf("%4s & ", " -- ");
            if( tvar[prob","filelist[1]] > 0 )
            {
               if( tvar[prob","file]/tvar[prob","filelist[1]] < 1 - cmpaff )
                  printf("\\textcolor{blue}{%4.2f} & ", tvar[prob","file]/tvar[prob","filelist[1]]);
               else if( tvar[prob","file]/tvar[prob","filelist[1]] > 1 + cmpaff )
                  printf("\\textcolor{red}{%4.2f} & ", tvar[prob","file]/tvar[prob","filelist[1]]);
               else
                  printf("%4.2f & ", tvar[prob","file]/tvar[prob","filelist[1]]);
            }
            else
               printf("%4s & ", " -- ");
            if( tnnz[prob","filelist[1]] > 0 )
            {
               if( tnnz[prob","file]/tnnz[prob","filelist[1]] < 1 - cmpaff )
                  printf("\\textcolor{blue}{%4.2f} & ", tnnz[prob","file]/tnnz[prob","filelist[1]]);
               else if( tnnz[prob","file]/tnnz[prob","filelist[1]] > 1 + cmpaff )
                  printf("\\textcolor{red}{%4.2f} & ", tnnz[prob","file]/tnnz[prob","filelist[1]]);
               else
                  printf("%4.2f & ", tnnz[prob","file]/tnnz[prob","filelist[1]]);
            }
            else
               printf("%4s & ", " -- ");
            printf("%5.2f \\\\", myptime[prob","file]);
         }
      }
      printf("\n");
   }
}

function OnlineSupplement()
{
   # loop through the problist
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      split(prob, temp, "[");
      originname[i] = temp[1];
      # set segment len; seed num
      seglen = 7;
      seednum = 5;
      # show the name
      seg[i] = substr(originname[i], ((i)%(seednum))*seglen+1, seglen);
      # replace "_" with "\_"
      gsub(/_/, "\\_", seg[i]);
      # if the name of the current prob is different from that of the previous prob, the hline is displayed
      if( i >= 1 && originname[i] != originname[i-1])
         printf("\\hline\n");
      # if a seed is affected, mark it
      isaff = 0;
      for( seedidx = 0; seedidx < seednum; seedidx++ )
      {
         if( aff[problist[floor(i/seednum)*seednum + seedidx]] == 1 )
         {
            isaff = 1;
            break;
         }
      }
      if( isaff == 1 )
         printf("\\textbf{%s} & ", seg[i]);
      else
         printf("%s & ", seg[i]);
      # each file shows the corresponding information
      for( j = filenum-1; j >= 0; j-- )
      {
         file = filelist[j];
         if( aff[prob] == 1 && cor[prob] == 1 && j == 0 )
         {
            # time TODO
            if( time[prob","file] >= 7200 )
            {
               if( gap[prob","file] != "--" && gap[prob","file] != "Large")
                  printf("%.2f\\%% & ", gap[prob","file])
               else
                  printf("%s & ", gap[prob","file])
            }
            else
            {
               if( time[prob","file]/time[prob","filelist[1]] <= 1-cmp || time[prob","filelist[1]] >= maxtime )
                  printf("\\blue{%.2f} & ", time[prob","file]);
               else if( time[prob","filelist[1]]/time[prob","file] <= 1-cmp )
                  printf("\\red{%.2f} & ", time[prob","file]);
               else
                  printf("%.2f & ", time[prob","file]);
            }
            # node
            if( node[prob","filelist[1]] != 0 && node[prob","file]/node[prob","filelist[1]] <= 1-cmp )
               printf("\\blue{%d} & ", node[prob","file]);
            else if( node[prob","file] != 0 && node[prob","filelist[1]]/node[prob","file] <= 1-cmp )
               printf("\\red{%d} & ", node[prob","file]);
            else
               printf("%d & ", node[prob","file]);
            # ptime
            printf("%d & ", ptime[prob","file]);
            # tcon
            if( tcon[prob","file]/tcon[prob","filelist[1]] <= 1-cmp )
               printf("\\blue{%d} & ", tcon[prob","file]);
            else if( tcon[prob","filelist[1]]/tcon[prob","file] <= 1-cmp )
               printf("\\red{%d} & ", tcon[prob","file]);
            else
               printf("%d & ", tcon[prob","file]);
            # tvar
            if( tvar[prob","file]/tvar[prob","filelist[1]] <= 1-cmp )
               printf("\\blue{%d} & ", tvar[prob","file]);
            else if( tvar[prob","filelist[1]]/tvar[prob","file] <= 1-cmp )
               printf("\\red{%d} & ", tvar[prob","file]);
            else
               printf("%d & ", tvar[prob","file]);
            # tnnz
            if( tnnz[prob","file]/tnnz[prob","filelist[1]] <= 1-cmp )
               printf("\\blue{%d} & ", tnnz[prob","file]);
            else if( tnnz[prob","filelist[1]]/tnnz[prob","file] <= 1-cmp )
               printf("\\red{%d} & ", tnnz[prob","file]);
            else
               printf("%d & ", tnnz[prob","file]);
            # flptime
            if( flptime[prob","filelist[1]] != 0 && flptime[prob","file]/flptime[prob","filelist[1]] <= 1-cmp )
               printf("\\blue{%.2f} & ", flptime[prob","file]);
            else if( flptime[prob","file] != 0 && flptime[prob","filelist[1]]/flptime[prob","file] <= 1-cmp )
               printf("\\red{%.2f} & ", flptime[prob","file]);
            else
               printf("%.2f & ", flptime[prob","file]);
            # flpspeed
            if( flpspeed[prob","filelist[1]] != 0 && flpspeed[prob","file]/flpspeed[prob","filelist[1]] <= 1-cmp )
               printf("\\red{%.2f} & ", flpspeed[prob","file]);
            else if( flpspeed[prob","file] != 0 && flpspeed[prob","filelist[1]]/flpspeed[prob","file] <= 1-cmp )
               printf("\\blue{%.2f} & ", flpspeed[prob","file]);
            else
               printf("%.2f & ", flpspeed[prob","file]);
         }
         else
         {
            if( status[prob","file] == "abort" || status[prob","file] == "memlim" || solstatus[prob","file] == "mismatch" || solstatus[prob","file] == "error" )
               printf("error & ")
            else if( time[prob","file] >= 7200 )
            {
               if( gap[prob","file] != "--" && gap[prob","file] != "Large")
                  printf("%.2f\\%% & ", gap[prob","file])
               else
                  printf("%s & ", gap[prob","file])
            }
            else
               printf("%.2f & ", time[prob","file]);
            printf("%d & ", node[prob","file]);
            printf("%.2f & ", ptime[prob","file]);
            printf("%d & ", tcon[prob","file]);
            printf("%d & ", tvar[prob","file]);
            printf("%d & ", tnnz[prob","file]);
            printf("%.2f & ", flptime[prob","file]);
            printf("%.2f & ", flpspeed[prob","file]);
         }
         if( j == 0 )
            printf("%.2f & ", myptime[prob","file]);
      }
      printf("\\\\\n");
   }
}

function OverallTableTitle()
{
   # print the names of two methods
   if( printlevel == 0 )
   {
      if( printlatex )
      {
         printf("\\begin{table}[H]\n");
         printf("\\begin{tabular}{|cc|c|ccccc|cc|}\n");
         printf("\\hline\n");
         printf("\\multicolumn{2}{|c|}{} & \\Adv & \\multicolumn{5}{c|}{\\Def} & \\multicolumn{2}{c|}{\\Aff} \\\\ \\hline\n");
         printf("\\multicolumn{1}{|c|}{Brackets} & Models & Tilim & \\multicolumn{1}{c|}{Tilim} & \\multicolumn{1}{c|}{Faster} & \\multicolumn{1}{c|}{Slower} & \\multicolumn{1}{c|}{Time} & Nodes & \\multicolumn{1}{c|}{Models} & Time \\\\ \\hline\n");
         }
         else
         {
            printf("\t\tAdvanced\tDefault\t\t\t\t\tAffected\n");
         printf("Bracket\tModels\tTilim\t\tTilim\tFaster\tSlower\tTime\tNodes\tModels\tTime\n");
      }
   }
   else
   {
      if( printlatex )
      {
         printf("\\begin{table}[H]\n");
         printf("\\begin{tabular}{|cc|c|ccccc|ccc|}\n");
         printf("\\hline\n");
         printf("\\multicolumn{2}{|c|}{} & \\Adv & \\multicolumn{5}{c|}{\\Def} & \\multicolumn{3}{c|}{\\Aff} \\\\ \\hline\n");
         printf("\\multicolumn{1}{|c|}{Brackets} & Models & Tilim & \\multicolumn{1}{c|}{Tilim} & \\multicolumn{1}{c|}{Faster} & \\multicolumn{1}{c|}{Slower} & \\multicolumn{1}{c|}{Time} & Nodes & \\multicolumn{1}{c|}{Models} & \\multicolumn{1}{c|}{Time} & Nodes \\\\ \\hline\n");
         }
         else
         {
            printf("\t\tAdvanced\tDefault\t\t\t\t\tAffected\n");
         printf("Bracket\tModels\tTilim\t\tTilim\tFaster\tSlower\tTime\tNodes\tModels\tTime\tNodes\n");
      }
   }
}

function PrintOverallAll()
{
   # num of models in bracket
   num = 0;
   num_aff = 0;
   ntimlim0 = 0;
   ntimlim1 = 0;
   nfaster = 0;
   nslower = 0;
   # init
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] = 1;
      geo_node[filelist[j]] = 1;
      geo_time_aff[filelist[j]] = 1;
      geo_node_aff[filelist[j]] = 1;
      solved[filelist[j]] = 0;
   }
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      # just count all the results that are correct, otherwise the table is meaningless
      if( cor[prob] == 0 )
      {
         continue;
      }
      if( status[prob","filelist[0]] == "timlim" )
         ntimlim0++;
      if( status[prob","filelist[1]] == "timlim" )
         ntimlim1++;
      # set shifted geometric means of time and node
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         if( status[prob","file] == "ok" )
         {
            geo_time[file] = (time[prob","file]+timeshift)^(1/(num+1)) * geo_time[file]^(num/(num+1));
            geo_node[file] = (node[prob","file]+nodeshift)^(1/(num+1)) * geo_node[file]^(num/(num+1));
         }
         else if( status[prob","file] == "timlim" )
         {
            geo_time[file] = (maxtime+timeshift)^(1/(num+1)) * geo_time[file]^(num/(num+1));
            geo_node[file] = (node[prob","file]+nodeshift)^(1/(num+1)) * geo_node[file]^(num/(num+1));
         }
         else
            print "error !!! " status[prob","file];
      }
      num++;
      if( time[prob","filelist[0]] > 0 && time[prob","filelist[1]] > 0 )
      {
         if( time[prob","filelist[1]]/time[prob","filelist[0]] <= 1-cmp )
            nfaster++;
         else if( time[prob","filelist[0]]/time[prob","filelist[1]] <= 1-cmp )
            nslower++;
      }
      if( affaccurate )
      {
         if( aff[prob] == 0 )
            continue;
         # up to here, prob must :1) all files are correctly calculated, 2) one file is optimized, 3) the calculation time of one file is in bracket, affected problem
         for( j = 0; j < filenum; j++ )
         {
            file = filelist[j];
            if( status[prob","file] == "ok" || status[prob","file] == "timlim" )
            {
               geo_time_aff[file] = (time[prob","file]+timeshift)^(1/(num_aff+1)) * geo_time_aff[file]^(num_aff/(num_aff+1));
               geo_node_aff[file] = (node[prob","file]+nodeshift)^(1/(num_aff+1)) * geo_node_aff[file]^(num_aff/(num_aff+1));
            }
            else
               print "error !!! " status[prob","file];
         }
         num_aff++;
      }
   }
   file0 = filelist[0];
   file1 = filelist[1];
   geo_time[file0] -= timeshift;
   geo_time[file1] -= timeshift;
   geo_node[file0] -= nodeshift;
   geo_node[file1] -= nodeshift;
   if( affaccurate )
   {
      geo_time_aff[file0] -= timeshift;
      geo_time_aff[file1] -= timeshift;
      geo_node_aff[file0] -= nodeshift;
      geo_node_aff[file1] -= nodeshift;
   }
   if( affaccurate )
   {
      if( printlevel == 0 )
      {
         if( LATEX )
         {
            printf("\\multicolumn{1}{|l|}{All} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{%d} & %.2f \\\\ \\hline\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0]);
         }
         else
         {
            printf("All\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%.2f\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0]);
         }
      }
      else
      {
         if( LATEX )
         {
            printf("\\multicolumn{1}{|l|}{All} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f \\\\ \\hline\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0], geo_node_aff[file1]/geo_node_aff[file0]);
         }
         else
         {
            printf("All\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%.2f\t%.2f\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0], geo_node_aff[file1]/geo_node_aff[file0]);
         }
      }
   }
   else
   {
      if( printlevel == 0 )
      {
         if( LATEX )
         {
            printf("\\multicolumn{1}{|l|}{All} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{--} & -- \\\\ \\hline\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0]);
         }
         else
         {
            printf("All\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t--\t--\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0]);
         }
      }
      else
      {
         if( LATEX )
         {
            printf("\\multicolumn{1}{|l|}{All} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{--} & \\multicolumn{1}{c|}{--} & -- \\\\ \\hline\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0]);
         }
         else
         {
            printf("All\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t--\t--\t--\n", num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0]);
         }
      }
   }
}

function OverallBracket(a)
{
   # num of models in bracket
   num = 0;
   # num of affected models in bracket
   num_aff = 0;
   ntimlim0 = 0;
   ntimlim1 = 0;
   nfaster = 0;
   nslower = 0;
   # init
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] = 1;
      geo_time_aff[filelist[j]] = 1;
      geo_node_aff[filelist[j]] = 1;
      geo_node[filelist[j]] = 1;
      solved[filelist[j]] = 0;
   }
   for( i = 0; i < probnum; i++)
   {
      prob = problist[i];
      # just count all the results that are correct, and exist one minimum gap has been reached
      if( cor[prob] == 0 || opt[prob] == 0 )
         continue;
      # up to here, prob must :1) all files are correctly calculated, and 2) one file is optimized
      if(status[prob","filelist[0]] == "timlim")
         ntimlim0++;
      if(status[prob","filelist[1]] == "timlim")
         ntimlim1++;
      nfilesolvedinbracket = 0;
      for( j = 0; j<filenum; j++ )
      {
         file = filelist[j];
         if( time[prob","file] >= a )
            nfilesolvedinbracket++;
      }
      if( nfilesolvedinbracket == 0 )
         continue;
      # up to here, prob must :1) all files are correctly calculated, 2) one file is optimized, and 3) the calculation time of one file is in bracket
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         if( status[prob","file] == "ok" || status[prob","file] == "timlim" )
         {
            solved[file]++;
            geo_time[file] = (time[prob","file]+timeshift)^(1/(num+1)) * geo_time[file]^(num/(num+1));
            geo_node[file] = (node[prob","file]+nodeshift)^(1/(num+1)) * geo_node[file]^(num/(num+1));
         }
         else
            print "error !!! " status[prob","file];
      }
      if( time[prob","filelist[0]] > 0 && time[prob","filelist[1]] > 0 )
      {
         if( time[prob","filelist[1]]/time[prob","filelist[0]] <= 1-cmp )
            nfaster++;
         else if( time[prob","filelist[0]]/time[prob","filelist[1]] <= 1-cmp )
            nslower++;
      }
      #if( time[prob","filelist[0]]/time[prob","filelist[1]] <= 0.8 )
       # print problist[i] " --- " time[prob","filelist[0]] " --- " time[prob","filelist[1]]
      num++;
      if( aff[prob] == 0 )
         continue;
      # up to here, prob must :1) all files are correctly calculated, 2) one file is optimized, 3) the calculation time of one file is in bracket, affected problem
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         if( status[prob","file] == "ok" || status[prob","file] == "timlim" )
         {
            geo_time_aff[file] = (time[prob","file]+timeshift)^(1/(num_aff+1)) * geo_time_aff[file]^(num_aff/(num_aff+1));
            geo_node_aff[file] = (node[prob","file]+nodeshift)^(1/(num_aff+1)) * geo_node_aff[file]^(num_aff/(num_aff+1));
         }
         else
            print "error !!! " status[prob","file];
      }
      num_aff++;
   }
   file0 = filelist[0];
   file1 = filelist[1];
   geo_time[file0] -= timeshift;
   geo_time[file1] -= timeshift;
   geo_time_aff[file0] -= timeshift;
   geo_time_aff[file1] -= timeshift;
   geo_node[file0] -= nodeshift;
   geo_node[file1] -= nodeshift;
   geo_node_aff[file0] -= nodeshift;
   geo_node_aff[file1] -= nodeshift;
   if( printlevel == 0 )
   {
      if( printlatex )
      {
         printf("\\multicolumn{1}{|l|}{$\\geq$ %d} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{%d} & %.2f \\\\ \\hline\n", a, num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0]);
      }
      else
      {
         printf(">=%d\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%.2f\n", a, num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0]);
      }
   }
   else
   {
      if( printlatex )
      {
         printf("\\multicolumn{1}{|l|}{$\\geq$ %d} & %d & %d & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f & \\multicolumn{1}{c|}{%d} & \\multicolumn{1}{c|}{%.2f} & %.2f \\\\ \\hline\n", a, num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0], geo_node_aff[file1]/geo_node_aff[file0]);
      }
      else
      {
         printf(">=%d\t%d\t%d\t\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%.2f\t%.2f\n", a, num, ntimlim0, ntimlim1, nfaster, nslower, geo_time[file1]/geo_time[file0], geo_node[file1]/geo_node[file0], num_aff, geo_time_aff[file1]/geo_time_aff[file0], geo_node_aff[file1]/geo_node_aff[file0]);
      }
   }
}

# presolve reduction table(just statistic affected instances)
function PresolveReduction()
{
   # num of affected models in bracket
   numaff = 0;
   # num of correct models(may have additional requirements)
   numall = 0;
   # init
   for( j = 0; j < filenum; j++)
   {
      nflpvaluebigger = 0;
      nflpvaluesmaller = 0;
      geo_ptime[filelist[j]] = 1;
      geo_con[filelist[j]] = 1;
      geo_var[filelist[j]] = 1;
      geo_nnz[filelist[j]] = 1;
      geo_flpiter[filelist[j]] = 1;
      geo_flpspeed[filelist[j]] = 1;
      geo_flptime[filelist[j]] = 1;
   }
   # loop all problems
   for( i = 0; i < probnum; i++)
   {
      prob = problist[i];
      # just count the problems that all results is correct
      if( cor[prob] == 0 )
         continue;
      # statistic n file solved or get first LP
      nfilesolvedinbracket = 0;
      # we require that the prob get first LP in both file
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         # only count the instances that enter first simplex iteration
         if( flpiter[prob","file] > 0 )
            nfilesolvedinbracket++;
      }
      if( nfilesolvedinbracket != filenum || lps[prob] == 0 )
         continue;
      numall++;
      # only count lp-solvable and affected problem
      if( aff[prob] == 0 )
         continue;
      #printf("%s\n", prob)
      # print information we additional needs
      for( j = 0; j < filenum; j++)
      {
         file = filelist[j];
         geo_con[file] = (tcon[prob","file]+shift)^(1/(numaff+1)) * geo_con[file]^(numaff/(numaff+1));
         geo_var[file] = (tvar[prob","file]+shift)^(1/(numaff+1)) * geo_var[file]^(numaff/(numaff+1));
         geo_nnz[file] = (tnnz[prob","file]+shift)^(1/(numaff+1)) * geo_nnz[file]^(numaff/(numaff+1));
         geo_ptime[file] = (ptime[prob","file]+timeshift)^(1/(numaff+1)) * geo_ptime[file]^(numaff/(numaff+1));
         geo_flpiter[file] = (flpiter[prob","file]+shift)^(1/(numaff+1)) * geo_flpiter[file]^(numaff/(numaff+1));
         geo_flpspeed[file] = (flpspeed[prob","file]+shift)^(1/(numaff+1)) * geo_flpspeed[file]^(numaff/(numaff+1));
         geo_flptime[file] = (flptime[prob","file]+timeshift)^(1/(numaff+1)) * geo_flptime[file]^(numaff/(numaff+1));
      }
      if( flpvalue[prob","filelist[0]] > flpvalue[prob","filelist[1]]+0.0001*abs(flpvalue[prob","filelist[1]]) )
         nflpvaluebigger++;
      else if( flpvalue[prob","filelist[1]] > flpvalue[prob","filelist[0]]+0.0001*abs(flpvalue[prob","filelist[0]]))
         nflpvaluesmaller++;
      numaff++;
      # output affected instances name, used to collect the output information written by myself
      # TODO think about where it should be
      if( 1 )
      {
         # obtain absolute path
         cmd = "realpath " ARGV[1];
         cmd | getline absolute_path;
         close(cmd);
         # absolute_path is delimited with "/" and the result is stored in the path_arr array
         split(absolute_path, path_arr, "/");
         # get the text after the last "/"
         tempname = path_arr[length(path_arr)];
         # get the testset, solver, nthreads, time limit
         split(tempname, tempname_arr, "\\.");
         # replace ".res" with empty
         gsub("\\.res$", "", tempname);
         # split the prob variable into an array prob_arr with the "[" and "]" delimiters
         split(prob, prob_arr, "\\[|\\]");
         # integrate the text after the last "/"
         output_tempname = tempname_arr[1] "." prob_arr[1] "." prob_arr[2] "." tempname_arr[2] "." tempname_arr[3] "." tempname_arr[4] ".out.gz";
         # change absolute_path
         sub("\\.res", ".out.gz", absolute_path);
         sub("/results\\.", "/", absolute_path);
         sub("/[^/]+$", "/" output_tempname, absolute_path);
         printf("-Affected- %s\n", absolute_path);
      }
   }
   file0 = filelist[0];
   file1 = filelist[1];
   geo_ptime[file0] -= timeshift;
   geo_ptime[file1] -= timeshift;
   geo_con[file0] -= shift;
   geo_con[file1] -= shift;
   geo_var[file0] -= shift;
   geo_var[file1] -= shift;
   geo_nnz[file0] -= shift;
   geo_nnz[file1] -= shift;
   geo_flpiter[file0] -= shift;
   geo_flpiter[file1] -= shift;
   geo_flpspeed[file0] -= shift;
   geo_flpspeed[file1] -= shift;
   geo_flptime[file0] -= timeshift;
   geo_flptime[file1] -= timeshift;
   r_ptime = (geo_ptime[file0]-geo_ptime[file1])/geo_ptime[file1];
   r_con = (geo_con[file0]-geo_con[file1])/geo_con[file1];
   r_var = (geo_var[file0]-geo_var[file1])/geo_var[file1];
   r_nnz = (geo_nnz[file0]-geo_nnz[file1])/geo_nnz[file1];
   r_flpiter = (geo_flpiter[file0]-geo_flpiter[file1])/geo_flpiter[file1];
   r_flpspeed = (geo_flpspeed[file0]-geo_flpspeed[file1])/geo_flpspeed[file1];
   r_flptime = (geo_flptime[file0]-geo_flptime[file1])/geo_flptime[file1];
   if( printlatex )
   {
      printf("\\begin{table}[H]\n");
      printf("%%\\setlength{\\tabcolsep}{1.5mm}\n");
      printf("{\n");
      printf("\\begin{tabular}{cccccccc}\n");
      printf("\\toprule[1pt]\n");
      printf("& PTime & Conss & Vars & NNZs & LPTime & LPIter &LPSpeed \\\\\n");
      printf("\\toprule[0.8pt]\n");
      printf("\\Def & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f \\\\\n", \
                geo_ptime[file1], geo_con[file1], geo_var[file1], geo_nnz[file1], geo_flptime[file1], geo_flpiter[file1], geo_flpspeed[file1]);
      printf("\\Adv & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f & %-.2f \\\\\n", \
                geo_ptime[file0], geo_con[file0], geo_var[file0], geo_nnz[file0], geo_flptime[file0], geo_flpiter[file1], geo_flpspeed[file0]);
      printf("%%Rat & %-6.2f & %-6.2f & %-6.2f & %-6.2f & %-6.2f & %-6.2f & %-6.2f \\\\\n", \
             r_ptime, r_con, r_var, r_nnz, r_flptime, r_flpiter, r_flpspeed);
      printf("%%numall: %d, numaff: %d\n", numall, numaff);
      printf("\\toprule[1pt]\n");
      printf("\\end{tabular}\n");
      printf("}\n");
      printf("\\end{table}\n");
   }
   else
   {
      printf("n_flp_value_bigger:   %d\n", nflpvaluebigger);
      printf("n_flp_value_smaller:  %d\n", nflpvaluesmaller);
      printf("geo_flptime0 %f\n", geo_flptime[file0]);
      printf("geo_flptime1 %f\n", geo_flptime[file1]);
      printf("numall: %d, numaff: %d\n", numall, numaff);
      printf("%-4dPTime  Conss    Vars     NNZs     LPTime   LPIter   LPSpeed\n", a);
      #printf("Def %-6.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f\n", \
      #          geo_ptime[file1], geo_con[file1], geo_var[file1], geo_nnz[file1], geo_flptime[file1], geo_flpspeed[file1]);
      #printf("Adv %-6.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f\n", \
      #          geo_ptime[file0], geo_con[file0], geo_var[file0], geo_nnz[file0], geo_flptime[file0], geo_flpspeed[file0]);
      printf("Def %-6.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f\n", \
                geo_ptime[file1], geo_con[file1], geo_var[file1], geo_nnz[file1], geo_flptime[file1], geo_flpiter[file1], geo_flpspeed[file1]);
      printf("Adv %-6.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f\n", \
                geo_ptime[file0], geo_con[file0], geo_var[file0], geo_nnz[file0], geo_flptime[file0], geo_flpiter[file0], geo_flpspeed[file0]);
      printf("Rat %-6.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f\n", \
             r_ptime, r_con, r_var, r_nnz, r_flptime, r_flpiter, r_flpspeed);
   }
}

# main function, assign value for all data
{
   # compare simple table
   if( NF == 10 )
   {
      result_table_type = 1;
      # remove seed from name
      if( compdiffseed )
      {
         split($1, v, "[");
         $1 = v[1];
      }
      # init the probnum, problist, filenum, filelist
      probnum = AddName(probname, problist, $1, probnum);
      filenum = AddName(filename, filelist, FILENAME, filenum);
      # whether the file for this example exists
      probfile[$1","FILENAME] = 1;
      # number of origproblem's constraints
      ocon[$1","FILENAME] = $2;
      # number of origproblem's variables
      ovar[$1","FILENAME] = $3;
      # dual bound
      db[$1","FILENAME] = $4;
      # primal bound
      pb[$1","FILENAME] = $5;
      # gap (Note! data type is string)
      gap[$1","FILENAME] = $6;
      # nodes
      node[$1","FILENAME] = $7;
      # total run time
      # it is important (use "<" rather than "<=")
      if( $8 < maxtime )
         time[$1","FILENAME] = $8;
      else
         time[$1","FILENAME] = maxtime;
      # status
      status[$1","FILENAME] = $9;
      # solution status
      solstatus[$1","FILENAME] = $10;
   }
   # compare compelete table( just for scip )
   else if( NF == 21 && ( /\[.*\]/ ) )
   #else if( NF == 19 && ( /\[0\]/ || /\[1\]/ || /\[2\]/ ) )
   #else if( NF == 19 && ( /\[0\]/ ) )
   {
      result_table_type = 2;
      # remove seed from name
      if( compdiffseed )
      {
         split($1, v, "[");
         $1 = v[1];
      }
      # init the probnum, problist, filenum, filelist
      probnum = AddName(probname, problist, $1, probnum);
      filenum = AddName(filename, filelist, FILENAME, filenum);
      # whether the file for this example exists
      probfile[$1","FILENAME] = 1;
      # dual bound
      db[$1","FILENAME] = $2;
      # primal bound
      pb[$1","FILENAME] = $3;
      # gap (Note! data type is string)
      gap[$1","FILENAME] = $4;
      # nodes
      node[$1","FILENAME] = $5;
      # total run time
      # it is important (use "<" rather than "<=")
      if( $6 < maxtime )
         time[$1","FILENAME] = $6;
      else
         time[$1","FILENAME] = maxtime;
      # status
      status[$1","FILENAME] = $7;
      # solution status
      solstatus[$1","FILENAME] = $8;
      # number of origproblem's constraints
      ocon[$1","FILENAME] = $9;
      # number of origproblem's variables
      ovar[$1","FILENAME] = $10;
      # number of tranproblem's constraints
      tcon[$1","FILENAME] = $11;
      # number of tranproblem's variables
      tvar[$1","FILENAME] = $12;
      # number of tranproblem's nonzeros
      tnnz[$1","FILENAME] = $13;
      # total presolve time
      ptime[$1","FILENAME] = $14;
      # my presolve time
      myptime[$1","FILENAME] = $15;
      # first lp time
      flptime[$1","FILENAME] = $16
      # num of first lp iteration
      flpiter[$1","FILENAME] = $17;
      # first lp iteration speed
      flpspeed[$1","FILENAME] = $18;
      # first lp value
      flpvalue[$1","FILENAME] = $19;
      #total num of lp iteration
      totlpiter[$1","FILENAME] = $20;
      # my plugin output
      myexec[$1","FILENAME] = $21;
   }
   else if( $1 == "nsolved/ntimlim/nfailed/nmemlim:")
   {
      split($2, s, "/");
      nsolved[FILENAME] = s[1];
      ntimlim[FILENAME] = s[2];
      nfailed[FILENAME] = s[3];
      nnmemlim[FILENAME] = s[4];
   }
}

END{
   # check table type
   if( result_table_type == 0 )
   {
      print("error! may you check you .res file or run .sh file again");
      exit 0;
   }
   else if( result_table_type == 1 && printlevel >= 4 )
   {
      print("error! simple result table can not be compared under level greater then 3");
      exit 0;
   }
   # check filenum
   if( filenum < 2 )
   {
      print("error! please input at least two file");
      exit 0;
   }
   Init(aff, cor, opt);
   # basic table
   if( printlevel == 0 || printlevel == 0.1 )
   {
      # print the title of the basic table
      OverallTableTitle();
      # print related information by time interval
      PrintOverallAll();
      OverallBracket(0);
      OverallBracket(1);
      OverallBracket(10);
      OverallBracket(100);
      OverallBracket(1000);
      if( printlatex )
      {
         printf("\\end{tabular}\n");
         printf("\\end{table}\n");
      }
   }
   else if( printlevel == 1 )
   {
      EachInstance();
   }
   else if( printlevel == 2 )
   {
      PerformanceProfileGraph();
   }
   else if( printlevel == 3 )
   {
      EndGapGraph();
   }
   else if( printlevel == 4 )
   {
      # print related information by time interval
      PresolveReduction();
   }
   else if( printlevel == 5 )
   {
      # instance-wise presolve reduction and effectiveness compare
      PresolveInstanceCompare();
   }
   else if( printlevel == 6 )
   {
      OnlineSupplement();
   }
}
