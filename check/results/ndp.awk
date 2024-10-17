BEGIN{
   # set print level
   # 0    : print overall table of all instances
   # 1    : print each instances
   # 2    : performance profile graph (for software origin) (multiple files can be processed)
   # 3    : end gap graph (for software origin) (multiple files can be processed)

   ########## input parameters ##########
   # print level
   printlevel = 0;
   if( LEVEL > 0 )
      printlevel = LEVEL;
   # print latex
   printlatex = 0;
   if( LATEX == 1 )
      printlatex = LATEX;
   # whether to print error problems?
   printerror = 0;
   if( ERROR == 1 )
      printerror = ERROR;
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

# define correctly calculated problem "cor", and get optimal solstatus's problem "opt"
# (B) cor[x] == 1 means that all results of problem x is exist and correctly calculated
# (I) opt[x] == n means that n file contain optimal value,(if cor[x] equal 0, opt[x] must be 0)
# (B) lps[x] == 1 means that problem x is lp-solvable (at least one file)
# their computing outcome different means that only one get optimal or their computing process different
function Init(cor, opt)
{
   ncor = probnum;
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
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
         cor[prob] = 0;
         ncor -= 1;
         opt[prob] = 0;
         lps[prob] = 0;
      }
      else if( nfilecorrect == 1 )
      {
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

# print the title of each instances
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
   xspace = 10;
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
   gapmin = 0;
   gapmax = 15;
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

function OverallTableTitle()
{
   if( printlatex )
   {
      printf("\\begin{table}[h]\n");
      printf("\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}lc");
      for( j = 0; j < filenum; j++ )
      {
         printf("ccc");
      }
      printf("}\n");
      printf("\\toprule\n");
      printf("& & \\multicolumn{3}{@{}c@{}}{\\texttt{LNS}}");
      for( j = 1; j < filenum; j++ )
      {
         printf(" & \\multicolumn{3}{@{}c@{}}{\\texttt{CPX-%d}}",j);
      }
      printf("\\\\");
      for( j = 0; j < filenum; j++ )
      {
         printf("\\cmidrule{%d-%d}", 3*j+3, 3*j+5);
      }
      printf("\n");
      printf("Bracket & Total");
      for( j = 0; j < filenum; j++ )
      {
         printf(" & Solved & Nodes & Time");
      }
      printf("\\\\\n");
   }
   else
   {
      printf("\t\t\t\tLNS");
      for( j = 1; j < filenum; j++ )
      {
         printf("\t\t\t\tCPX-%d", j);
      }
      printf("\n");
      printf("Bracket\t\tTotal");
      for( j = 0; j < filenum; j++ )
      {
         printf("\t\tSolved\tNodes\tTime");
      }
      printf("\n");
   }
}

function PrintOverallAll()
{
   # num of models in bracket
   num = 0;
   nfaster = 0;
   nslower = 0;
   # init
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] = 1;
      geo_node[filelist[j]] = 1;
      ntimlim[filelist[j]] = 0;
   }
   for( i = 0; i < probnum; i++ )
   {
      prob = problist[i];
      # just count all the results that are correct, otherwise the table is meaningless
      if( cor[prob] == 0 )
      {
         continue;
      }
      # set shifted geometric means of time and node
      for( j = 0; j < filenum; j++ )
      {
         file = filelist[j];
         if( status[prob","file] == "timlim" )
            ntimlim[file]++;
         if( status[prob","file] == "ok" || status[prob","file] == "timlim" )
         {
            geo_time[file] = (time[prob","file]+timeshift)^(1/(num+1)) * geo_time[file]^(num/(num+1));
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
   }
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] -= timeshift;
      geo_node[filelist[j]] -= nodeshift;
   }
   if( printlatex )
   {
      printf("\\midrule\n");
      printf("\\texttt{All} & %d", num);
      for( j = 0; j < filenum; j++ )
      {
         printf(" & \\textbf{%d} & \\textbf{%d} & \\textbf{%.2f}", num-ntimlim[filelist[j]], geo_node[filelist[j]], geo_time[filelist[j]]);
      }
      printf("\\\\\n");
   }
   else
   {
      printf("All\t\t%d", num);
      for( j = 0; j < filenum; j++ )
      {
         printf("\t\t%d\t%d\t%.2f", num-ntimlim[filelist[j]], geo_node[filelist[j]], geo_time[filelist[j]]);
      }
      printf("\n");
   }
}

function OverallBracket(a)
{
   # num of models in bracket
   num = 0;
   nfaster = 0;
   nslower = 0;
   # init
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] = 1;
      geo_node[filelist[j]] = 1;
      ntimlim[filelist[j]] = 0;
   }
   for( i = 0; i < probnum; i++)
   {
      prob = problist[i];
      # just count all the results that are correct, and exist one minimum gap has been reached
      if( cor[prob] == 0 || opt[prob] == 0 )
         continue;
      # up to here, prob must :1) all files are correctly calculated, and 2) one file is optimized
      nfilesolvedinbracket = 0;
      for( j = 0; j < filenum; j++ )
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
         if( status[prob","file] == "timlim" )
            ntimlim[file]++;
         if( status[prob","file] == "ok" || status[prob","file] == "timlim" )
         {
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
      num++;
   }
   for( j = 0; j < filenum; j++ )
   {
      geo_time[filelist[j]] -= timeshift;
      geo_node[filelist[j]] -= nodeshift;
   }
   if( printlatex )
   {
      printf("$\\geq$ %d & %d", a, num);
      for( j = 0; j < filenum; j++ )
      {
         printf(" & %d & %d & %.2f", num-ntimlim[filelist[j]], geo_node[filelist[j]], geo_time[filelist[j]]);
      }
      printf("\\\\\n");
   }
   else
   {
      printf(">=%d\t\t%d", a, num);
      for( j = 0; j < filenum; j++ )
      {
         printf("\t\t%d\t%d\t%.2f", num-ntimlim[filelist[j]], geo_node[filelist[j]], geo_time[filelist[j]]);
      }
      printf("\n");
   }
}

# main function, assign value for all data
{
   # compare simple table
   if( NF == 10 )
   {
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
   # check filenum
   if( filenum < 2 )
   {
      print("error! please input at least two file");
      exit 0;
   }
   Init(cor, opt);
   # basic table
   if( printlevel == 0 )
   {
      # print the title of the basic table
      OverallTableTitle();
      # print related information by time interval
      PrintOverallAll();
      OverallBracket(900);
      OverallBracket(1800);
      OverallBracket(3600);
      if( printlatex )
      {
         printf("\\botrule\n");
         printf("\\end{tabular*}\n");
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
}
